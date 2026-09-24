#include <iostream>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QUrl>
#include <QtGlobal>
#include "SMU.h"

#include "utils/fileio.h"
#include "config.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("ADI");
    QCoreApplication::setApplicationName("Pixelpulse2");

    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QGuiApplication app(argc, argv);

    if (argc == 2 && (QString::fromLocal8Bit(argv[1]) == QStringLiteral("-v")
                      || QString::fromLocal8Bit(argv[1]) == QStringLiteral("--version"))) {
        std::cout << GIT_VERSION << ": Built on " << BUILD_DATE << std::endl;
        return 0;
    }

    FileIO fileIO;
    SessionItem smu_session;
    QQmlApplicationEngine engine;

    registerTypes();

    engine.rootContext()->setContextProperty("session", &smu_session);

    QVariantMap versions;
    versions.insert("build_date", BUILD_DATE);
    versions.insert("git_version", GIT_VERSION);
    engine.rootContext()->setContextProperty("versions", versions);
    engine.rootContext()->setContextProperty("fileio", &fileIO);

    QUrl qmlUrl = QStringLiteral("qrc:/qml/main.qml");
    if (argc > 1) {
        qmlUrl = QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1]));
    }
    engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
    engine.load(qmlUrl);
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    app.setWindowIcon(QIcon(":/icons/pp2.ico"));

    int result = app.exec();
    smu_session.closeAllDevices();

    return result;
}
