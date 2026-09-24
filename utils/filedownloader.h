#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class FileDownloader : public QObject
{
    Q_OBJECT

public:
    explicit FileDownloader(const QUrl& url, QObject* parent = nullptr);
    ~FileDownloader() override;

    QByteArray downloadedData() const;

signals:
    void downloaded();
    void failed(const QString& error);

private slots:
    void fileDownloaded(QNetworkReply* reply);

private:
    QNetworkAccessManager m_webCtrl;
    QByteArray m_downloadedData;
    bool m_abortedForSize;
};

#endif // FILEDOWNLOADER_H
