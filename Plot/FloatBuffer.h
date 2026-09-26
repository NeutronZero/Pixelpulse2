#pragma once

#include <QObject>
#include <QList>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QtQuick/qsgnode.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

class FloatBuffer : public QObject
{
    Q_OBJECT

public:
    explicit FloatBuffer(QObject* parent = nullptr);

    unsigned countPointsBetween(double start, double end) const;
    unsigned size() const;

    Q_INVOKABLE unsigned long ignoredFirstSamplesCount() const;
    Q_INVOKABLE void setIgnoredFirstSamplesCount(unsigned long count);

    Q_INVOKABLE float get(unsigned index) const;

    void append_samples(const std::vector<std::array<float, 4>>& samples, int signal_index);
    void append_samples_circular(const std::vector<std::array<float, 4>>& samples, int signal_index);

    void toVertexData(double start, double end, QSGGeometry::Point2D* vertices,
                      unsigned n_vertices) const;

    void setRate(float seconds_per_sample);
    void allocate(unsigned length);

    Q_INVOKABLE QList<qreal> getData() const;
    std::vector<float> snapshot() const;

    void startSweep();

    double rms() const;
    double peak_to_peak() const;
    double mean() const;
    std::vector<float> dif_mean(double average) const;

signals:
    void dataChanged();

public slots:
    QObject* getObject()
    {
        return this;
    }

private:
    size_t validSizeUnlocked() const;
    size_t dataIndexUnlocked(size_t index) const;
    unsigned timeToIndexUnlocked(double time) const;
    double indexToTimeUnlocked(unsigned index) const;
    bool validSignalIndex(int signal_index) const;

    float m_secondsPerSample;
    std::vector<float> m_data;
    size_t m_length;
    size_t m_first_samples_ignored;
    size_t m_ignored_requested;
    mutable QReadWriteLock m_lock;
};
