#include "filedownloader.h"

#include <QUrl>

namespace {
constexpr qint64 MaxDownloadBytes = 16 * 1024 * 1024;
}

FileDownloader::FileDownloader(const QUrl& url, QObject* parent)
    : QObject(parent),
      m_abortedForSize(false)
{
    connect(&m_webCtrl, &QNetworkAccessManager::finished,
            this, &FileDownloader::fileDownloaded);

    if (!url.isValid() || (url.scheme() != QStringLiteral("http")
                           && url.scheme() != QStringLiteral("https"))) {
        emit failed(tr("Invalid download URL"));
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QVariant::fromValue(QNetworkRequest::NoLessSafeRedirectPolicy));
    request.setMaximumRedirectsAllowed(5);
    QNetworkReply* reply = m_webCtrl.get(request);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (m_abortedForSize)
            return;
        const qint64 available = reply->bytesAvailable();
        const qint64 current = static_cast<qint64>(m_downloadedData.size());
        if (available < 0 || current > MaxDownloadBytes || available > MaxDownloadBytes - current) {
            m_abortedForSize = true;
            m_downloadedData.clear();
            reply->abort();
            return;
        }
        m_downloadedData += reply->readAll();
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64 received, qint64 total) {
        Q_UNUSED(received);
        if (total > MaxDownloadBytes && !m_abortedForSize) {
            m_abortedForSize = true;
            m_downloadedData.clear();
            reply->abort();
        }
    });
}

FileDownloader::~FileDownloader() = default;

void FileDownloader::fileDownloaded(QNetworkReply* reply)
{
    if (!reply) {
        return;
    }

    if (m_abortedForSize) {
        reply->deleteLater();
        emit failed(tr("Downloaded data is too large"));
        return;
    }

    const QNetworkReply::NetworkError error = reply->error();
    const QVariant statusValue = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const int status = statusValue.isValid() ? statusValue.toInt() : 0;
    const qint64 contentLength = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    if (contentLength > MaxDownloadBytes) {
        reply->deleteLater();
        emit failed(tr("Downloaded data is too large"));
        return;
    }

    if (error != QNetworkReply::NoError || status < 200 || status >= 300) {
        const QString message = status >= 200 && status < 300
            ? reply->errorString()
            : tr("Download failed with HTTP status %1").arg(status);
        reply->deleteLater();
        emit failed(message);
        return;
    }

    const qint64 current = static_cast<qint64>(m_downloadedData.size());
    const qint64 remaining = reply->bytesAvailable();
    if (remaining < 0 || current > MaxDownloadBytes || remaining > MaxDownloadBytes - current) {
        m_downloadedData.clear();
        reply->deleteLater();
        emit failed(tr("Downloaded data is too large"));
        return;
    }
    m_downloadedData += reply->readAll();
    reply->deleteLater();
    if (m_downloadedData.isEmpty()) {
        emit failed(tr("Download returned no data"));
        return;
    }
    if (m_downloadedData.size() > MaxDownloadBytes) {
        m_downloadedData.clear();
        emit failed(tr("Downloaded data is too large"));
        return;
    }

    emit downloaded();
}

QByteArray FileDownloader::downloadedData() const
{
    return m_downloadedData;
}
