#ifndef FILEIO_H
#define FILEIO_H

#include <QObject>
#include <QFile>
#include <QSaveFile>
#include <QUrl>
#include <QTextStream>

class FileIO : public QObject
{
    Q_OBJECT

public slots:
    bool writeByURI(const QUrl& destination, const QString& data) {
        return writeByFilename(destination.toLocalFile(), data);
    }

    bool writeByFilename(const QString& source, const QString& data) {
        if (source.isEmpty()) {
            return false;
        }

        QSaveFile file(source);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit writeFailed(source, file.errorString());
            return false;
        }

        QTextStream out(&file);
        out << data << "\n";
        out.flush();
        if (out.status() != QTextStream::Ok || !file.commit()) {
            emit writeFailed(source, file.errorString());
            return false;
        }
        return true;
    }

    bool writeRawByURI(const QUrl& destination, const QByteArray& data) {
        return writeRawByFilename(destination.toLocalFile(), data);
    }

    bool writeRawByFilename(const QString& source, const QByteArray& data) {
        if (source.isEmpty()) {
            return false;
        }

        QSaveFile file(source);
        if (!file.open(QIODevice::WriteOnly)) {
            emit writeFailed(source, file.errorString());
            return false;
        }
        if (file.write(data) != data.size() || !file.commit()) {
            emit writeFailed(source, file.errorString());
            return false;
        }
        return true;
    }

    QString readByURI(const QUrl& source) {
        const QString path = source.toLocalFile();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit readFailed(path, file.errorString());
            return QString();
        }

        QTextStream in(&file);
        const QString data = in.readAll();
        if (in.status() != QTextStream::Ok) {
            emit readFailed(path, file.errorString());
            return QString();
        }
        return data;
    }

signals:
    void writeFailed(const QString& path, const QString& error);
    void readFailed(const QString& path, const QString& error);

public:
    FileIO() {}
};

#endif // FILEIO_H
