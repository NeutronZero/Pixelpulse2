#include "PhosphorRender.h"
#include <algorithm>
#include <cmath>

#include <QtQuick/qsgnode.h>
#include <QtQuick/qsgflatcolormaterial.h>
#include <iostream>

#include <QOpenGLFunctions>

class Shader : public QSGMaterialShader
{
public:
    const char *vertexShader() const {
        return
        "attribute highp vec4 vertex;          \n"
        "uniform highp mat4 matrix;            \n"
        "uniform highp float pointSize;        \n"
        "void main() {                         \n"
        "    gl_PointSize = pointSize;          \n"
        "    gl_Position = matrix * vertex;    \n"
        "}";
    }

    const char *fragmentShader() const {
        return
        "#version 120 \n"
        "uniform lowp float opacity;"
        "uniform lowp vec4 color;"
        "void main() {"
        "    float dist = length(gl_PointCoord - vec2(0.5))*2;"
        "    gl_FragColor = color * opacity * (1-dist);"
       //"    if(dist > 1)"
       //"        discard;"
        "}";
    }

    char const *const *attributeNames() const
    {
        static char const *const names[] = { "vertex", 0 };
        return names;
    }

    void initialize()
    {
        QSGMaterialShader::initialize();
        m_id_matrix = program()->uniformLocation("matrix");
        m_id_pointSize = program()->uniformLocation("pointSize");
        m_id_opacity = program()->uniformLocation("opacity");
        m_id_color = program()->uniformLocation("color");
        m_glFuncs = QOpenGLContext::currentContext()->functions();
    }

    void updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial);

    void deactivate() {
        m_glFuncs->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_glFuncs->glDisable(GL_POINT_SPRITE);
    }

private:
    int m_id_matrix;
    int m_id_pointSize;
    int m_id_opacity;
    int m_id_color;
    QOpenGLFunctions *m_glFuncs;
};

class Material : public QSGMaterial
{
public:
    QSGMaterialType *type() const { static QSGMaterialType type; return &type; }
    QSGMaterialShader *createShader() const { return new Shader; }
    QSGMaterial::Flags  flags() const { return QSGMaterial::Blending; }

    QMatrix4x4 transformation;
    float pointSize;
    QColor color;
};

void Shader::updateState(const RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial)
{
    Q_UNUSED(oldMaterial);
    Q_ASSERT(program()->isLinked());

    Material* m = static_cast<Material*>(newMaterial);
    program()->setUniformValue(m_id_matrix, state.combinedMatrix()*m->transformation);
    program()->setUniformValue(m_id_pointSize, std::max(1.0f, m->pointSize));

    if (state.isOpacityDirty()) {
        program()->setUniformValue(m_id_opacity, state.opacity());
    }

    program()->setUniformValue(m_id_color, m->color);

    m_glFuncs->glBlendFunc(GL_ONE, GL_ONE);
    m_glFuncs->glEnable(GL_POINT_SPRITE);
    // glPointSize(m->pointSize); // Commenting this out since it is not available in QOpenGLFunctions (OpenGL ES 2.0) and apparently does not affect the point size
}

PhosphorRender::PhosphorRender(QQuickItem *parent)
    : QQuickItem(parent), m_ybuffer(NULL), m_xbuffer(NULL),
    m_xmin(0), m_xmax(1), m_ymin(0), m_ymax(1), m_pointSize(0),
    m_color(0.03*255, 0.3*255, 0.03*255, 1*255)
{
    setFlag(ItemHasContents, true);
}

PhosphorRender::~PhosphorRender()
{
}

QSGNode *PhosphorRender::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_ybuffer) {
        return nullptr;
    }

    const QRectF bounds = boundingRect();
    if (!std::isfinite(m_xmin) || !std::isfinite(m_xmax)
            || !std::isfinite(m_ymin) || !std::isfinite(m_ymax)
            || m_xmax <= m_xmin || m_ymax <= m_ymin
            || !std::isfinite(bounds.width()) || !std::isfinite(bounds.height())
            || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
        return oldNode;
    }

    const std::vector<float> yData = m_ybuffer->snapshot();
    std::vector<float> xData;
    if (m_xbuffer) {
        xData = m_xbuffer->snapshot();
    }

    unsigned nPoints = m_xbuffer
        ? static_cast<unsigned>(std::min(xData.size(), yData.size()))
        : m_ybuffer->countPointsBetween(m_xmin, m_xmax);
    nPoints = std::min(nPoints, 65767U);
    if (nPoints == 0) {
        return nullptr;
    }

    QSGGeometryNode* node = nullptr;
    QSGGeometry* geometry = nullptr;
    Material* material = nullptr;
    if (!oldNode) {
        node = new QSGGeometryNode;
        geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), nPoints);
        geometry->setDrawingMode(GL_POINTS);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        material = new Material;
        material->setFlag(QSGMaterial::Blending);
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    } else {
        node = static_cast<QSGGeometryNode*>(oldNode);
        geometry = node->geometry();
        geometry->allocate(nPoints);
        material = static_cast<Material*>(node->material());
    }

    material->transformation.setToIdentity();
    material->transformation.scale(bounds.width() / (m_xmax - m_xmin),
                                  bounds.height() / (m_ymin - m_ymax));
    material->transformation.translate(-m_xmin, -m_ymax);
    material->pointSize = std::max(1.0, m_pointSize);
    material->color = m_color;

    QSGGeometry::Point2D* vertices = geometry->vertexDataAsPoint2D();
    if (m_xbuffer) {
        for (unsigned i = 0; i < nPoints; ++i) {
            vertices[i].set(xData[i], yData[i]);
        }
    } else {
        m_ybuffer->toVertexData(m_xmin, m_xmax, vertices, nPoints);
    }
    node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    return node;
}
