#include "SMU.h"

#include "Plot/FloatBuffer.h"
#include "Plot/PhosphorRender.h"
#include "utils/fileio.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

using namespace smu;

namespace {
constexpr unsigned MaxSampleCount = 5000000;
constexpr double MaxWaveformSamples = 1000000.0;
constexpr qint64 MaxFirmwareBytes = 4 * 1024 * 1024;

bool sameDevice(const Device* first, const Device* second)
{
    if (first == nullptr || second == nullptr) {
        return false;
    }
    return first == second
        || (!first->m_serial.empty() && first->m_serial == second->m_serial)
        || (first->m_usb_addr != std::make_pair<uint8_t, uint8_t>(0, 0)
            && first->m_usb_addr == second->m_usb_addr);
}

bool containsDevice(const QList<DeviceItem*>& devices, const Device* device)
{
    for (DeviceItem* item : devices) {
        if (item && sameDevice(item->rawDevice(), device)) {
            return true;
        }
    }
    return false;
}
}

void registerTypes()
{
    qmlRegisterType<SessionItem>();
    qmlRegisterType<DeviceItem>();
    qmlRegisterType<ChannelItem>();
    qmlRegisterType<SignalItem>();
    qmlRegisterType<SrcItem>();

    qmlRegisterType<PhosphorRender>("Plot", 1, 0, "PhosphorRender");
    qmlRegisterType<FloatBuffer>("Plot", 1, 0, "FloatBuffer");

    qRegisterMetaType<uint64_t>("uint64_t");
}

void SessionItem::discardDevice(Device* device)
{
    if (!m_session || !device || m_retired_devices.contains(device)
            || m_session->m_devices.find(device) != m_session->m_devices.end()) {
        return;
    }
    for (DeviceItem* item : m_devices) {
        if (item && item->m_device == device) {
            return;
        }
    }
    auto& available = m_session->m_available_devices;
    available.erase(std::remove(available.begin(), available.end(), device), available.end());
    if (m_session->m_devices.find(device) != m_session->m_devices.end()) {
        return;
    }
    for (Device* current : m_session->m_available_devices) {
        if (current != device && sameDevice(current, device)) {
            return;
        }
    }
    for (DeviceItem* item : m_devices) {
        if (item && item->m_device != device && sameDevice(item->m_device, device)) {
            return;
        }
    }
    delete device;
    m_retired_devices.insert(device);
}

void SessionItem::reconcileUnownedDevices(const std::vector<Device*>& previousDevices)
{
    if (!m_session) {
        return;
    }
    for (Device* oldDevice : previousDevices) {
        if (!oldDevice || m_retired_devices.contains(oldDevice)) {
            continue;
        }
        bool referenced = false;
        for (Device* current : m_session->m_available_devices) {
            if (current == oldDevice || sameDevice(current, oldDevice)) {
                referenced = true;
                break;
            }
        }
        if (!referenced && m_session->m_devices.find(oldDevice) != m_session->m_devices.end()) {
            referenced = true;
        }
        if (!referenced) {
            for (DeviceItem* item : m_devices) {
                if (item && (item->m_device == oldDevice || sameDevice(item->m_device, oldDevice))) {
                    referenced = true;
                    break;
                }
            }
        }
        if (!referenced) {
            discardDevice(oldDevice);
        }
    }
}

QString SessionItem::getTmpPathForFirmware()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/pixelpulse2/m1k_firmware");
    QDir directory(path);
    if (!directory.exists() && !QDir().mkpath(path)) {
        return QString();
    }
    return directory.path();
}

SessionItem::SessionItem()
    : m_session(new Session),
      m_active(false),
      m_continuous(false),
      m_sample_rate(0),
      m_sample_count(0),
      m_sample_time(0.1),
      m_queue_size(1000000),
      m_data_logger(new DataLogger(-1.0f, this)),
      m_logging(0),
      m_firmware_fd(nullptr),
      m_scan_enabled(true),
      sweepTimer(new QTimer(this))
{
    connect(this, &SessionItem::finished, this, &SessionItem::onFinished, Qt::QueuedConnection);
    connect(this, &SessionItem::sampleCountChanged, this, &SessionItem::onSampleCountChanged);
    connect(this, &SessionItem::loggingChanged, this, &SessionItem::onLoggingChanged);
    connect(this, &SessionItem::sampleTimeChanged, this, &SessionItem::onSampleTimeChanged);
    connect(&timer, &QTimer::timeout, this, &SessionItem::getSamples);
    connect(sweepTimer, &QTimer::timeout, this, &SessionItem::beginNewSweep);
    connect(&deviceScanTimer, &QTimer::timeout, this, &SessionItem::scanDevices);

    deviceScanTimer.setInterval(2000);
    deviceScanTimer.start();
    QTimer::singleShot(0, this, &SessionItem::scanDevices);
}

SessionItem::~SessionItem()
{
    deviceScanTimer.stop();
    timer.stop();
    if (sweepTimer) {
        sweepTimer->stop();
    }
    closeAllDevices();
    if (m_firmware_fd) {
        delete m_firmware_fd;
        m_firmware_fd = nullptr;
    }
    if (m_session) {
        const std::vector<Device*> available = m_session->m_available_devices;
        for (Device* device : available) {
            discardDevice(device);
        }
    }
    m_session.reset();
}

void SessionItem::openAllDevices()
{
    m_scan_enabled = true;
    if (m_active || !m_session) {
        return;
    }

    m_session->m_queue_size = m_queue_size;
    const std::vector<Device*> previousDevices = m_session->m_available_devices;
    try {
        const int result = m_session->scan();
        if (result < 0) {
            qWarning() << "Unable to open devices:" << result;
            reconcileUnownedDevices(previousDevices);
            return;
        }
    } catch (const std::exception& error) {
        qWarning() << "Unable to open devices:" << error.what();
        reconcileUnownedDevices(previousDevices);
        return;
    }

    for (Device* device : m_session->m_available_devices) {
        if (device) {
            onAttached(device);
        }
    }
    reconcileUnownedDevices(previousDevices);
    devicesChanged();
}

void SessionItem::closeAllDevices()
{
    m_scan_enabled = false;
    if (!m_session) {
        return;
    }

    if (m_active && !cancel()) {
        return;
    }
    timer.stop();
    if (sweepTimer) {
        sweepTimer->stop();
    }

    QList<DeviceItem*> devices;
    QList<DeviceItem*> failedDevices;
    devices.swap(m_devices);
    if (m_data_logger) {
        m_data_logger->clearData();
    }
    for (DeviceItem* device : devices) {
        if (!device) {
            continue;
        }
        bool removed = false;
        try {
            const int result = m_session->remove(device->m_device);
            removed = result == 0;
            if (result != 0) {
                qWarning() << "Unable to remove device:" << result;
            }
        } catch (const std::exception& error) {
            qWarning() << "Unable to remove device:" << error.what();
        }
        if (removed) {
            delete device;
        } else {
            failedDevices.append(device);
        }
    }
    m_devices.append(failedDevices);
    if (!devices.isEmpty()) {
        devicesChanged();
    }
}

void SessionItem::resumeDeviceScanning()
{
    m_scan_enabled = true;
    deviceScanTimer.start();
    scanDevices();
}

void SessionItem::start(bool continuous)
{
    if (!m_session || m_active || m_devices.isEmpty() || m_sample_rate == 0
            || m_sample_count > MaxSampleCount
            || (!continuous && m_sample_count == 0)) {
        return;
    }

    try {
        m_session->flush();
        const int configuredRate = m_session->configure(m_sample_rate);
        if (configuredRate < 0) {
            qWarning() << "Unable to configure session:" << configuredRate;
            return;
        }
        if (configuredRate > 0 && configuredRate != m_sample_rate) {
            m_sample_rate = static_cast<unsigned>(configuredRate);
            emit sampleRateChanged();
        }

        m_continuous = continuous;
        const unsigned displayCount = continuous
            ? std::min(MaxSampleCount, std::max(1u, static_cast<unsigned>(
                static_cast<double>(m_sample_rate) * m_sample_time)))
            : m_sample_count;
        for (DeviceItem* device : m_devices) {
            device->setSamplesAdded(0);
            for (ChannelItem* channel : device->m_channels) {
                if (device->m_device->set_mode(channel->m_index, channel->m_mode) < 0
                        || !channel->buildTxBuffer()) {
                    throw std::runtime_error("Unable to configure device channel");
                }
                for (SignalItem* signal : channel->m_signals) {
                    signal->m_buffer->setRate(1.0f / static_cast<float>(m_sample_rate));
                    signal->m_buffer->allocate(displayCount);
                    signal->m_buffer->startSweep();
                }
            }
            if (!device->write()) {
                throw std::runtime_error("Unable to configure device output");
            }
        }

        const int result = m_session->start(continuous ? 0 : m_sample_count);
        if (result != 0) {
            throw std::runtime_error("Unable to start session");
        }
    } catch (const std::exception& error) {
        qWarning() << "Session start failed:" << error.what();
        try {
            m_session->cancel();
            m_session->end();
        } catch (const std::exception& cleanupError) {
            qWarning() << "Session cleanup failed:" << cleanupError.what();
        }
        m_active = false;
        emit activeChanged();
        return;
    }

    timer.start(0);
    m_active = true;
    emit activeChanged();
}

void SessionItem::onAttached(Device* device)
{
    if (!device || !m_session) {
        return;
    }
    if (m_active && !cancel()) {
        return;
    }
    for (int i = 0; i < m_devices.size(); ++i) {
        DeviceItem* item = m_devices.at(i);
        if (!item) {
            continue;
        }
        if (item->m_device == device) {
            return;
        }
        if (sameDevice(item->m_device, device)) {
            m_devices.removeAt(i);
            if (m_data_logger) {
                m_data_logger->clearData();
            }
            int removeResult = -1;
            try {
                removeResult = m_session->remove(item->m_device, true);
            } catch (const std::exception& error) {
                qWarning() << "Unable to replace device:" << error.what();
            }
            if (removeResult != 0) {
                qWarning() << "Unable to replace device:" << removeResult;
                m_devices.insert(i, item);
                return;
            }
            discardDevice(item->m_device);
            delete item;
            break;
        }
    }

    int result = -1;
    try {
        result = m_session->add(device);
    } catch (const std::exception& error) {
        qWarning() << "Unable to add device:" << error.what();
    }
    if (result == 0) {
        m_devices.append(new DeviceItem(this, device));
        devicesChanged();
    } else {
        qWarning() << "Unable to add device:" << result;
        discardDevice(device);
    }
}

void SessionItem::onDetached(Device* device)
{
    if (!device || !m_session) {
        return;
    }

    for (int i = 0; i < m_devices.size(); ++i) {
        DeviceItem* item = m_devices.at(i);
        if (!sameDevice(item->m_device, device)) {
            continue;
        }
        if (m_active && !cancel()) {
            return;
        }
        m_devices.removeAt(i);
        if (m_data_logger) {
            m_data_logger->clearData();
        }
        int result = -1;
        try {
            result = m_session->remove(item->m_device, true);
        } catch (const std::exception& error) {
            qWarning() << "Unable to remove detached device:" << error.what();
        }
        if (result != 0) {
            qWarning() << "Unable to remove detached device:" << result;
            m_devices.insert(i, item);
        } else {
            discardDevice(item->m_device);
            delete item;
        }
        devicesChanged();
        return;
    }
}

void SessionItem::scanDevices()
{
    if (!m_scan_enabled || !m_session) {
        return;
    }

    const std::vector<Device*> previousDevices = m_session->m_available_devices;
    m_retired_devices.clear();
    int result = 0;
    try {
        result = m_session->scan();
    } catch (const std::exception& error) {
        qWarning() << "Unable to scan for devices:" << error.what();
        return;
    }
    if (result < 0) {
        qWarning() << "Unable to scan for devices:" << result;
        reconcileUnownedDevices(previousDevices);
        return;
    }

    QList<Device*> available;
    for (Device* device : m_session->m_available_devices) {
        if (device) {
            available.append(device);
            m_retired_devices.remove(device);
        }
    }

    QList<DeviceItem*> detached;
    for (DeviceItem* item : m_devices) {
        bool found = false;
        for (Device* device : available) {
            if (sameDevice(item->m_device, device)) {
                found = true;
                break;
            }
        }
        if (!found) {
            detached.append(item);
        }
    }
    for (DeviceItem* item : detached) {
        if (item) {
            onDetached(item->m_device);
        }
    }

    for (Device* device : available) {
        if (!containsDevice(m_devices, device)) {
            onAttached(device);
        }
    }

    reconcileUnownedDevices(previousDevices);
    m_retired_devices.clear();
}

void SessionItem::onSampleCountChanged()
{
    if (m_active) {
        restart();
    }
}

void SessionItem::onSampleTimeChanged()
{
    if (m_data_logger) {
        m_data_logger->setSampleTime(static_cast<float>(m_sample_time));
    }
}

void SessionItem::toggleLogging()
{
    m_logging = m_logging == 0 ? 1 : 0;
    if (m_data_logger) {
        delete m_data_logger;
        m_data_logger = new DataLogger(m_logging == 1 ? static_cast<float>(m_sample_time) : -1.0f, this);
    }
    emit loggingChanged();
}

void SessionItem::onLoggingChanged()
{
    if (m_logging == 1 && m_active && m_data_logger) {
        m_data_logger->setSampleTime(static_cast<float>(m_sample_time));
    }
}

void SessionItem::handleDownloadedFirmware()
{
    if (!m_firmware_fd) {
        return;
    }

    FileDownloader* downloader = m_firmware_fd;
    m_firmware_fd = nullptr;
    const QString directory = getTmpPathForFirmware();
    const QString path = directory.isEmpty() ? QString()
        : QDir(directory).filePath(QStringLiteral("firmware.bin"));
    if (path.isEmpty() || !FileIO().writeRawByFilename(path, downloader->downloadedData())) {
        emit firmwareDownloadFailed(QStringLiteral("Unable to save downloaded firmware"));
    } else {
        emit firmwareDownloaded();
    }
    downloader->deleteLater();
}

bool SessionItem::cancel()
{
    const bool resumeTimer = timer.isActive();
    timer.stop();
    if (sweepTimer) {
        sweepTimer->stop();
    }
    if (!m_active || !m_session) {
        return true;
    }

    int cancelResult = 0;
    int endResult = 0;
    try {
        cancelResult = m_session->cancel();
        endResult = m_session->end();
    } catch (const std::exception& error) {
        qWarning() << "Session cancellation failed:" << error.what();
        if (resumeTimer) {
            timer.start(0);
        }
        return false;
    }
    if (cancelResult != 0 || endResult != 0 || m_session->m_active_devices != 0
            || !m_session->cancelled()) {
        qWarning() << "Session cancellation returned" << cancelResult << endResult
                   << "with" << m_session->m_active_devices << "active devices";
        if (resumeTimer) {
            timer.start(0);
        }
        return false;
    }
    m_active = false;
    emit activeChanged();
    return true;
}

void SessionItem::restart()
{
    if (!m_active) {
        return;
    }
    const bool continuous = m_continuous;
    if (!cancel()) {
        return;
    }
    start(continuous);
}

void SessionItem::onFinished()
{
    updateAllMeasurements();
}

void SessionItem::updateMeasurements()
{
    for (DeviceItem* device : m_devices) {
        for (ChannelItem* channel : device->m_channels) {
            for (SignalItem* signal : channel->m_signals) {
                signal->updateMeasurementLatest();
            }
        }
    }
}

void SessionItem::updateAllMeasurements()
{
    for (DeviceItem* device : m_devices) {
        for (ChannelItem* channel : device->m_channels) {
            for (SignalItem* signal : channel->m_signals) {
                signal->updateMeasurementMean();
                signal->updateMeasurementLatest();
                signal->updatePeakToPeak();
                signal->updateRms();
            }
        }
    }
}

void SessionItem::downloadFromUrl(const QString& url)
{
    const QUrl parsedUrl(url);
    if (!parsedUrl.isValid() || (parsedUrl.scheme() != QStringLiteral("http")
                                 && parsedUrl.scheme() != QStringLiteral("https"))) {
        emit firmwareDownloadFailed(QStringLiteral("Invalid firmware URL"));
        return;
    }
    if (m_firmware_fd) {
        delete m_firmware_fd;
        m_firmware_fd = nullptr;
    }

    m_firmware_fd = new FileDownloader(QUrl(url), this);
    connect(m_firmware_fd, &FileDownloader::downloaded,
            this, &SessionItem::handleDownloadedFirmware);
    connect(m_firmware_fd, &FileDownloader::failed, this,
            [this](const QString& error) {
        if (m_firmware_fd && sender() == m_firmware_fd) {
            FileDownloader* downloader = m_firmware_fd;
            m_firmware_fd = nullptr;
            downloader->deleteLater();
            emit firmwareDownloadFailed(error);
        }
    });
}

QString SessionItem::flash_firmware(const QString& url)
{
    if (!m_session) {
        return QStringLiteral("Session is not available");
    }
    if (m_active || !m_devices.isEmpty() || m_session->m_active_devices != 0) {
        return QStringLiteral("Close active devices before updating firmware");
    }
    const QUrl firmwareUrl(url);
    const QString firmwarePath = firmwareUrl.isLocalFile() ? firmwareUrl.toLocalFile() : url;
    const QFileInfo firmwareInfo(firmwarePath);
    if (!firmwareInfo.isFile() || !firmwareInfo.isReadable()
            || firmwareInfo.size() <= 0 || firmwareInfo.size() > MaxFirmwareBytes) {
        return QStringLiteral("Firmware file is missing, unreadable, or too large");
    }
    try {
        const int flashed = m_session->flash_firmware(firmwarePath.toStdString());
        m_session->m_available_devices.clear();
        if (flashed <= 0) {
            return QStringLiteral("No devices were updated");
        }
        return QString();
    } catch (const std::exception& error) {
        m_session->m_available_devices.clear();
        return QString::fromUtf8(error.what());
    }
}

void SessionItem::getSamples()
{
    if (!m_active || !m_session) {
        return;
    }
    if (m_session->cancelled()) {
        cancel();
        return;
    }

    for (DeviceItem* device : m_devices) {
        std::vector<std::array<float, 4>> rxbuf;
        int result = 0;
        try {
            result = device->m_device->read(rxbuf, m_queue_size, 0);
        } catch (const std::system_error& error) {
            qWarning() << "Sample read failed:" << error.what();
            cancel();
            return;
        } catch (const std::runtime_error& error) {
            qWarning() << "Sample read failed:" << error.what();
            cancel();
            return;
        }

        if (result < 0) {
            qWarning() << "Sample read returned" << result;
            cancel();
            return;
        }
        if (result == 0) {
            continue;
        }
        if (static_cast<size_t>(result) > rxbuf.size()) {
            qWarning() << "Sample read returned an invalid sample count";
            cancel();
            return;
        }

        device->setSamplesAdded(device->samplesAdded() + static_cast<size_t>(result));
        size_t channelOffset = 0;
        for (ChannelItem* channel : device->m_channels) {
            if (!channel) {
                continue;
            }
            for (SignalItem* signal : channel->m_signals) {
                if (!signal || !signal->m_signal) {
                    cancel();
                    return;
                }
                const size_t signalIndex = channelOffset + static_cast<size_t>(signal->m_index);
                if (signalIndex >= rxbuf.front().size()) {
                    qWarning() << "Sample signal index is out of range";
                    cancel();
                    return;
                }
                if (m_continuous) {
                    signal->m_buffer->append_samples_circular(rxbuf, static_cast<int>(signalIndex));
                } else {
                    signal->m_buffer->append_samples(rxbuf, static_cast<int>(signalIndex));
                }
            }
            channelOffset += channel->m_signals.size();
        }

        if (m_logging == 1 && m_data_logger) {
            m_data_logger->addBulkData(device, rxbuf);
        }
    }

    bool allComplete = !m_devices.isEmpty();
    for (DeviceItem* device : m_devices) {
        if (device->samplesAdded() < static_cast<size_t>(m_sample_count)) {
            allComplete = false;
            break;
        }
    }

    if (!m_continuous && allComplete) {
        timer.stop();
        for (DeviceItem* device : m_devices) {
            device->setSamplesAdded(0);
        }
        try {
            const int result = m_session->end();
            if (result != 0) {
                qWarning() << "Session end returned" << result;
                cancel();
                return;
            }
        } catch (const std::exception& error) {
            qWarning() << "Session end failed:" << error.what();
            cancel();
            return;
        }
        if (!m_active || m_session->m_active_devices != 0) {
            return;
        }
        emit finished(0);
        if (sweepTimer) {
            sweepTimer->setInterval(100);
            sweepTimer->setSingleShot(true);
            sweepTimer->start();
        }
    }
}

void SessionItem::beginNewSweep()
{
    if (!m_active || !m_session) {
        return;
    }

    try {
        for (DeviceItem* device : m_devices) {
            device->setSamplesAdded(0);
            for (ChannelItem* channel : device->m_channels) {
                if (device->m_device->set_mode(channel->m_index, channel->m_mode) < 0
                        || !channel->buildTxBuffer()) {
                    throw std::runtime_error("Unable to configure repeated sweep");
                }
                for (SignalItem* signal : channel->m_signals) {
                    signal->m_buffer->startSweep();
                }
            }
            if (!device->write()) {
                throw std::runtime_error("Unable to configure repeated sweep output");
            }
        }
        if (m_session->start(m_sample_count) != 0) {
            throw std::runtime_error("Unable to start repeated sweep");
        }
    } catch (const std::exception& error) {
        qWarning() << "Repeated sweep failed:" << error.what();
        cancel();
        return;
    }
    timer.start(0);
}

int SessionItem::programmingModeDeviceExists()
{
    if (!m_session) {
        return 0;
    }
    try {
        std::vector<libusb_device*> sambaDevices;
        return m_session->scan_samba_devs(sambaDevices);
    } catch (const std::exception& error) {
        qWarning() << "Unable to scan programming devices:" << error.what();
        return 0;
    }
}

int SessionItem::getAvailableDevices() const
{
    return m_scan_enabled && m_session ? static_cast<int>(m_session->m_available_devices.size()) : 0;
}

int SessionItem::getActiveDevices() const
{
    return m_session ? static_cast<int>(m_session->m_devices.size()) : 0;
}

bool SessionItem::isContinuous() const
{
    return m_continuous;
}

bool SessionItem::getActive() const
{
    return m_active;
}

QQmlListProperty<DeviceItem> SessionItem::getDevices()
{
    return QQmlListProperty<DeviceItem>(this, m_devices);
}

DeviceItem::DeviceItem(SessionItem* parent, Device* device)
    : QObject(parent),
      m_device(device),
      m_led_step(0),
      m_samples_added(0)
{
    ledTimer.setSingleShot(true);
    connect(&ledTimer, &QTimer::timeout, this, &DeviceItem::advanceLed);

    if (!m_device || !m_device->info()) {
        return;
    }
    const sl_device_info* deviceInfo = m_device->info();
    for (unsigned channelIndex = 0; channelIndex < deviceInfo->channel_count; ++channelIndex) {
        m_channels.append(new ChannelItem(this, m_device, channelIndex));
    }
}

QQmlListProperty<ChannelItem> DeviceItem::getChannels()
{
    return QQmlListProperty<ChannelItem>(this, m_channels);
}

QString DeviceItem::getLabel() const
{
    return m_device && m_device->info() ? QString::fromUtf8(m_device->info()->label) : QString();
}

QString DeviceItem::getFWVer() const
{
    return m_device ? QString::fromStdString(m_device->m_fwver) : QString();
}

QString DeviceItem::getHWVer() const
{
    return m_device ? QString::fromStdString(m_device->m_hwver) : QString();
}

QString DeviceItem::getDevSN() const
{
    return m_device ? QString::fromStdString(m_device->m_serial) : QString();
}

int DeviceItem::getDefaultRate() const
{
    return m_device ? m_device->get_default_rate() : 0;
}

int DeviceItem::ctrl_transfer(int x, int y, int z)
{
    if (!m_device) {
        return -1;
    }
    return m_device->ctrl_transfer(0x40, x, y, z, nullptr, 0, 100);
}

void DeviceItem::blinkLeds()
{
    m_led_step = 0;
    ledTimer.stop();
    advanceLed();
}

size_t DeviceItem::samplesAdded() const
{
    return m_samples_added;
}

void DeviceItem::setSamplesAdded(size_t count)
{
    m_samples_added = count;
}

Device* DeviceItem::rawDevice() const
{
    return m_device;
}

bool DeviceItem::write(ChannelItem* channel)
{
    if (!m_device) {
        return false;
    }
    for (ChannelItem* current : m_channels) {
        if (channel != nullptr && current != channel) {
            continue;
        }
        const unsigned mode = current->property("mode").toUInt();
        if (mode > SIMV_SPLIT) {
            qWarning() << "Device channel mode is out of range";
            return false;
        }
        if (mode != SVMI && mode != SIMV) {
            continue;
        }
        try {
            if (m_device->write(current->m_tx_data, current->m_index, true) != 0) {
                return false;
            }
        } catch (const std::exception& error) {
            qWarning() << "Device write failed:" << error.what();
            return false;
        }
    }
    return true;
}

void DeviceItem::advanceLed()
{
    if (!m_device) {
        return;
    }
    setLed((m_led_step % 2) == 0 ? 1 : 2);
    ++m_led_step;
    if (m_led_step >= 4) {
        m_led_step = 0;
        ledTimer.stop();
    } else {
        ledTimer.start(500);
    }
}

void DeviceItem::setLed(unsigned value)
{
    try {
        if (m_device && m_device->set_led(value) != 0) {
            qWarning() << "Unable to set device LED";
        }
    } catch (const std::exception& error) {
        qWarning() << "Unable to set device LED:" << error.what();
    }
}

ChannelItem::ChannelItem(DeviceItem* parent, Device* device, unsigned channelIndex)
    : QObject(parent),
      m_device(device),
      m_index(channelIndex),
      m_mode(0),
      timer(nullptr)
{
    if (!m_device) {
        return;
    }
    const sl_channel_info* channelInfo = m_device->channel_info(m_index);
    if (!channelInfo) {
        return;
    }
    for (unsigned signalIndex = 0; signalIndex < channelInfo->signal_count; ++signalIndex) {
        m_signals.append(new SignalItem(this, static_cast<int>(signalIndex), m_device->signal(m_index, signalIndex)));
    }
    timer = new TimerItem(this, parent);
}

QQmlListProperty<SignalItem> ChannelItem::getSignals()
{
    return QQmlListProperty<SignalItem>(this, m_signals);
}

QString ChannelItem::getLabel() const
{
    if (!m_device) {
        return QString();
    }
    const sl_channel_info* info = m_device->channel_info(m_index);
    return info && info->label ? QString::fromUtf8(info->label) : QString();
}

bool ChannelItem::buildTxBuffer()
{
    m_tx_data.clear();
    if (!m_device || m_mode > SIMV_SPLIT) {
        return false;
    }
    if (m_mode != SVMI && m_mode != SIMV) {
        return true;
    }

    const size_t signalIndex = m_mode == SVMI ? 0 : 1;
    if (signalIndex >= m_signals.size()) {
        return false;
    }
    SignalItem* txSignal = m_signals.at(static_cast<int>(signalIndex));
    if (!txSignal) {
        return false;
    }

    const QString source = txSignal->getSrc()->property("src").toString();
    const double v1 = txSignal->getSrc()->property("v1").toDouble();
    const double v2 = txSignal->getSrc()->property("v2").toDouble();
    const double period = txSignal->getSrc()->property("period").toDouble();
    const double phase = txSignal->getSrc()->property("phase").toDouble();
    const double duty = txSignal->getSrc()->property("duty").toDouble();
    if (!std::isfinite(v1) || !std::isfinite(v2) || !std::isfinite(phase)) {
        return false;
    }
    if (source == QLatin1String("constant")) {
        m_tx_data.clear();
        txSignal->m_signal->constant(m_tx_data, 1, static_cast<float>(v1));
        return true;
    }
    if (source != QLatin1String("square") && source != QLatin1String("sawtooth")
            && source != QLatin1String("stairstep") && source != QLatin1String("sine")
            && source != QLatin1String("triangle")) {
        return false;
    }
    if (!std::isfinite(period) || period <= 0.0
            || period > MaxWaveformSamples
            || !std::isfinite(duty) || duty < 0.0 || duty > 1.0) {
        return false;
    }

    const int samples = static_cast<int>(period);
    if (samples < 1) {
        return false;
    }
    if (source == QLatin1String("square")) {
        txSignal->m_signal->square(m_tx_data, samples, static_cast<float>(v1),
                                   static_cast<float>(v2), period, phase, duty);
    } else if (source == QLatin1String("sawtooth")) {
        txSignal->m_signal->sawtooth(m_tx_data, samples, static_cast<float>(v1),
                                     static_cast<float>(v2), period, phase);
    } else if (source == QLatin1String("stairstep")) {
        txSignal->m_signal->stairstep(m_tx_data, samples, static_cast<float>(v1),
                                      static_cast<float>(v2), period, phase);
    } else if (source == QLatin1String("sine")) {
        txSignal->m_signal->sine(m_tx_data, samples, static_cast<float>(v1),
                                 static_cast<float>(v2), period, phase);
    } else {
        txSignal->m_signal->triangle(m_tx_data, samples, static_cast<float>(v1),
                                     static_cast<float>(v2), period, phase);
    }
    return true;
}

SignalItem::SignalItem(ChannelItem* parent, int index, Signal* signal)
    : QObject(parent),
      m_index(index),
      m_channel(parent),
      m_signal(signal),
      m_buffer(new FloatBuffer(this)),
      m_src(new SrcItem(this)),
      m_measurement(0.0),
      m_peak_to_peak(0.0),
      m_rms(0.0),
      m_mean(0.0)
{
    connect(m_channel, &ChannelItem::modeChanged, this, &SignalItem::onParentModeChanged);
}

FloatBuffer* SignalItem::getBuffer() const
{
    return m_buffer;
}

QString SignalItem::getLabel() const
{
    return m_signal && m_signal->info() ? QString::fromUtf8(m_signal->info()->label) : QString();
}

double SignalItem::getMin() const
{
    return m_signal && m_signal->info() ? m_signal->info()->min : 0.0;
}

double SignalItem::getMax() const
{
    return m_signal && m_signal->info() ? m_signal->info()->max : 0.0;
}

double SignalItem::getResolution() const
{
    return m_signal && m_signal->info() ? m_signal->info()->resolution : 0.0;
}

SrcItem* SignalItem::getSrc() const
{
    return m_src;
}

bool SignalItem::getIsOutput() const
{
    return m_signal && m_signal->info()
        && (m_signal->info()->outputModes & (1u << m_channel->m_mode));
}

bool SignalItem::getIsInput() const
{
    return m_signal && m_signal->info()
        && (m_signal->info()->inputModes & (1u << m_channel->m_mode));
}

double SignalItem::getMeasurement() const
{
    return m_measurement;
}

double SignalItem::getPeak() const
{
    return m_peak_to_peak;
}

double SignalItem::getRms() const
{
    return m_rms;
}

double SignalItem::getMean() const
{
    return m_mean;
}

void SignalItem::onParentModeChanged(int)
{
    emit isOutputChanged(getIsOutput());
    emit isInputChanged(getIsInput());
}

void SignalItem::updateMeasurementMean()
{
    m_mean = m_buffer->mean();
    emit meanChanged(m_mean);
}

void SignalItem::updateMeasurementLatest()
{
    const unsigned count = m_buffer->size();
    m_measurement = count == 0 ? 0.0 : static_cast<double>(m_buffer->get(count - 1));
    emit measurementChanged(m_measurement);
}

void SignalItem::updatePeakToPeak()
{
    m_peak_to_peak = m_buffer->peak_to_peak();
    emit peakChanged(m_peak_to_peak);
}

void SignalItem::updateRms()
{
    m_rms = m_buffer->rms();
    emit rmsChanged(m_rms);
}

SrcItem::SrcItem(SignalItem* parent)
    : QObject(parent),
      m_src(QStringLiteral("constant")),
      m_v1(0.0),
      m_v2(0.0),
      m_period(0.0),
      m_phase(0.0),
      m_duty(0.5),
      m_parent(parent)
{
    connect(this, &SrcItem::srcChanged, this, &SrcItem::changed);
    connect(this, &SrcItem::v1Changed, this, &SrcItem::changed);
    connect(this, &SrcItem::v2Changed, this, &SrcItem::changed);
    connect(this, &SrcItem::periodChanged, this, &SrcItem::changed);
    connect(this, &SrcItem::phaseChanged, this, &SrcItem::changed);
    connect(this, &SrcItem::dutyChanged, this, &SrcItem::changed);
}

void SrcItem::setPhase(double phase)
{
    if (m_src == QLatin1String("constant") || !std::isfinite(phase)
            || !std::isfinite(m_period) || m_period <= 0.0) {
        return;
    }
    const double normalized = std::fmod(std::fmod(phase, m_period) + m_period, m_period);
    if (normalized != m_phase) {
        m_phase = normalized;
        emit phaseChanged(m_phase);
    }
}

void SrcItem::update()
{
}

TimerItem::TimerItem(ChannelItem* channel, DeviceItem* device)
    : QObject(channel),
      m_channel(channel),
      m_device(device),
      m_session(device ? static_cast<SessionItem*>(device->parent()) : nullptr)
{
    m_changeBufferTimer.setSingleShot(true);
    m_changeBufferTimer.setInterval(100);
    connect(&m_changeBufferTimer, &QTimer::timeout, this, &TimerItem::needChangeBuffer);
    if (m_channel) {
        for (SignalItem* signal : m_channel->m_signals) {
            if (signal) {
                connect(signal->m_src, &SrcItem::changed,
                        this, &TimerItem::parameterChanged);
            }
        }
    }
}

void TimerItem::parameterChanged()
{
    if (m_session && m_session->isContinuous()) {
        m_changeBufferTimer.start();
    }
}

void TimerItem::needChangeBuffer()
{
    if (m_channel && m_device && m_channel->buildTxBuffer() && m_device->write(m_channel)) {
        return;
    }
    qWarning() << "Unable to update output buffer";
}

DataLogger::DataLogger(float sample_time, QObject* parent)
    : QObject(parent),
      sampleTime(sample_time),
      startTime(std::chrono::system_clock::now()),
      lastLog(startTime)
{
    if (sampleTime <= 0.0f || std::fabs(sampleTime - 0.01f) < 0.00001f
            || std::fabs(sampleTime - 0.1f) < 0.00001f) {
        return;
    }

    createLoggingFolder();
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const char* timeString = std::ctime(&now);
    const QString path = directory + QStringLiteral("/logging/PP_Log_")
        + QString::fromStdString(modifyDateTime(timeString ? std::string(timeString) : std::string()))
        + QStringLiteral(".csv");
    fileStream.open(path.toStdString().c_str(), std::ios::out);
    if (!fileStream.is_open()) {
        qWarning() << "Unable to open data log" << path;
        return;
    }
    fileStream << "Timestamp,Device serial,Min V ch A,Min A ch A,Min V ch B,Min A ch B,Max V ch A,Max A ch A,Max V ch B,Max A ch B,Avg V ch A,Avg A ch A,Avg V ch B,Avg A ch B\n";
    fileStream.flush();
}

DataLogger::~DataLogger()
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (fileStream.is_open()) {
        fileStream.flush();
        fileStream.close();
    }
}

std::string DataLogger::modifyDateTime(const std::string& date_time)
{
    if (date_time.size() < 24) {
        return "00000000000000";
    }
    const std::string year = date_time.substr(20, 4);
    const std::string month = date_time.substr(4, 3);
    const std::map<std::string, std::string> months = {
        {"Jan", "01"}, {"Feb", "02"}, {"Mar", "03"}, {"Apr", "04"},
        {"May", "05"}, {"Jun", "06"}, {"Jul", "07"}, {"Aug", "08"},
        {"Sep", "09"}, {"Oct", "10"}, {"Nov", "11"}, {"Dec", "12"}
    };
    const auto monthIt = months.find(month);
    const std::string monthValue = monthIt == months.end() ? "00" : monthIt->second;
    return year + monthValue + date_time.substr(8, 2) + date_time.substr(11, 2)
        + date_time.substr(14, 2) + date_time.substr(17, 2);
}

void DataLogger::clearData()
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    dataCounter.clear();
    minimum.clear();
    maximum.clear();
    sum.clear();
    lastLog = std::chrono::system_clock::now();
}

void DataLogger::updateMinimum(DeviceItem* device, const std::array<float, 4>& samples)
{
    auto& values = minimum[device];
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = std::min(values[i], samples[i]);
    }
}

void DataLogger::updateMaximum(DeviceItem* device, const std::array<float, 4>& samples)
{
    auto& values = maximum[device];
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = std::max(values[i], samples[i]);
    }
}

void DataLogger::updateSum(DeviceItem* device, const std::array<float, 4>& samples)
{
    auto& values = sum[device];
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] += samples[i];
    }
}

std::array<float, 4> DataLogger::computeAverageUnlocked(DeviceItem* device) const
{
    const auto counter = dataCounter.find(device);
    const auto total = sum.find(device);
    if (counter == dataCounter.end() || total == sum.end() || counter->second == 0) {
        return {{0.0f, 0.0f, 0.0f, 0.0f}};
    }
    std::array<float, 4> result = {{0.0f, 0.0f, 0.0f, 0.0f}};
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = total->second[i] / static_cast<float>(counter->second);
    }
    return result;
}

void DataLogger::resetData(DeviceItem* device)
{
    dataCounter[device] = 0;
    minimum[device] = {{0.0f, 0.0f, 0.0f, 0.0f}};
    maximum[device] = {{0.0f, 0.0f, 0.0f, 0.0f}};
    sum[device] = {{0.0f, 0.0f, 0.0f, 0.0f}};
}

void DataLogger::addData(DeviceItem* device, const std::array<float, 4>& samples)
{
    if (!device) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_logMutex);
    auto counter = dataCounter.find(device);
    if (counter == dataCounter.end() || counter->second == 0) {
        resetData(device);
        minimum[device] = samples;
        maximum[device] = samples;
    } else {
        updateMinimum(device, samples);
        updateMaximum(device, samples);
    }
    updateSum(device, samples);
    dataCounter[device] += 1;

    if (sampleTime <= 0.0f || !fileStream.is_open()) {
        return;
    }
    const std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - lastLog;
    if (elapsed.count() < static_cast<double>(sampleTime)) {
        return;
    }
    lastLog = std::chrono::system_clock::now();
    for (auto& entry : sum) {
        DeviceItem* current = entry.first;
        if (dataCounter[current] > 0) {
            printDataUnlocked(current);
            resetData(current);
        }
    }
}

void DataLogger::addBulkData(DeviceItem* device, const std::vector<std::array<float, 4>>& samples)
{
    doAddBulkData(device, samples);
}

void DataLogger::doAddBulkData(DeviceItem* device, const std::vector<std::array<float, 4>>& samples)
{
    for (const std::array<float, 4>& sample : samples) {
        addData(device, sample);
    }
}

void DataLogger::printData(DeviceItem* device)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    printDataUnlocked(device);
}

void DataLogger::printDataUnlocked(DeviceItem* device)
{
    if (!device || !fileStream.is_open()) {
        return;
    }
    const auto counter = dataCounter.find(device);
    if (counter == dataCounter.end() || counter->second == 0) {
        return;
    }
    std::string serial = device->m_device ? device->m_device->m_serial : std::string();
    if (serial.size() > 5) {
        serial = serial.substr(serial.size() - 5);
    }
    const std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - startTime;
    const std::array<float, 4> average = computeAverageUnlocked(device);
    const auto& min = minimum[device];
    const auto& max = maximum[device];
    fileStream << std::setprecision(3) << std::fixed << elapsed.count() << "," << serial << ","
               << min[0] << "," << min[1] << "," << min[2] << "," << min[3] << ","
               << max[0] << "," << max[1] << "," << max[2] << "," << max[3] << ","
               << average[0] << "," << average[1] << "," << average[2] << "," << average[3] << '\n';
    fileStream.flush();
}

void DataLogger::setSampleTime(float sample_time)
{
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (std::isfinite(sample_time) && sample_time > 0.0f) {
        sampleTime = sample_time;
    }
}

void DataLogger::createLoggingFolder()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/logging");
    if (!QDir().mkpath(directory)) {
        qWarning() << "Unable to create data log directory" << directory;
    }
}
