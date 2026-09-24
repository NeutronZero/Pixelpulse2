#pragma once

#include <QSet>
#include <QTimer>
#include <QtQuick/QQuickItem>

#include <libsmu/libsmu.hpp>

#include <array>
#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "utils/filedownloader.h"

class SessionItem;
class DeviceItem;
class ChannelItem;
class SignalItem;
class ModeItem;
class SrcItem;
class TimerItem;
class FloatBuffer;
class DataLogger;

class SessionItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<DeviceItem> devices READ getDevices NOTIFY devicesChanged)
    Q_PROPERTY(bool active READ getActive NOTIFY activeChanged)
    Q_PROPERTY(unsigned sampleRate MEMBER m_sample_rate NOTIFY sampleRateChanged)
    Q_PROPERTY(unsigned sampleCount MEMBER m_sample_count NOTIFY sampleCountChanged)
    Q_PROPERTY(double sampleTime MEMBER m_sample_time NOTIFY sampleTimeChanged)
    Q_PROPERTY(unsigned logging MEMBER m_logging NOTIFY loggingChanged)
    Q_PROPERTY(int activeDevices READ getActiveDevices NOTIFY activeChanged)
    Q_PROPERTY(int availableDevices READ getAvailableDevices NOTIFY devicesChanged)
    Q_PROPERTY(int queueSize MEMBER m_queue_size CONSTANT)

public:
    SessionItem();
    ~SessionItem() override;

    Q_INVOKABLE void openAllDevices();
    Q_INVOKABLE void closeAllDevices();
    Q_INVOKABLE void resumeDeviceScanning();
    Q_INVOKABLE void start(bool continuous);
    Q_INVOKABLE bool cancel();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void toggleLogging();

    int getAvailableDevices() const;
    int getActiveDevices() const;
    bool isContinuous() const;
    bool getActive() const;
    QQmlListProperty<DeviceItem> getDevices();

    Q_INVOKABLE void updateMeasurements();
    Q_INVOKABLE void updateAllMeasurements();

    Q_INVOKABLE void downloadFromUrl(const QString& url);
    Q_INVOKABLE QString flash_firmware(const QString& url);
    Q_INVOKABLE QString getTmpPathForFirmware();
    Q_INVOKABLE int programmingModeDeviceExists();

signals:
    void devicesChanged();
    void activeChanged();
    void sampleRateChanged();
    void sampleCountChanged();
    void sampleTimeChanged();
    void loggingChanged();
    void finished(unsigned status);
    void attached(smu::Device* device);
    void detached(smu::Device* device);
    void firmwareDownloaded();
    void firmwareDownloadFailed(const QString& error);

protected slots:
    void onFinished();
    void onAttached(smu::Device* device);
    void onDetached(smu::Device* device);
    void scanDevices();
    void handleDownloadedFirmware();
    void onSampleCountChanged();
    void onSampleTimeChanged();
    void onLoggingChanged();
    void getSamples();
    void beginNewSweep();

protected:
    void discardDevice(smu::Device* device);
    void reconcileUnownedDevices(const std::vector<smu::Device*>& previousDevices);

    std::unique_ptr<smu::Session> m_session;
    bool m_active;
    bool m_continuous;
    unsigned m_sample_rate;
    unsigned m_sample_count;
    double m_sample_time;
    unsigned m_queue_size;
    DataLogger* m_data_logger;
    unsigned m_logging;
    FileDownloader* m_firmware_fd;
    bool m_scan_enabled;
    QSet<smu::Device*> m_retired_devices;
    QList<DeviceItem*> m_devices;
    QTimer timer;
    QTimer* sweepTimer;
    QTimer deviceScanTimer;
};

class DeviceItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<ChannelItem> channels READ getChannels CONSTANT)
    Q_PROPERTY(QString label READ getLabel CONSTANT)
    Q_PROPERTY(QString FWVer READ getFWVer CONSTANT)
    Q_PROPERTY(QString HWVer READ getHWVer CONSTANT)
    Q_PROPERTY(int DefaultRate READ getDefaultRate CONSTANT)
    Q_PROPERTY(QString UUID READ getDevSN CONSTANT)

public:
    DeviceItem(SessionItem* parent, smu::Device* device);

    QQmlListProperty<ChannelItem> getChannels();
    QString getLabel() const;
    QString getFWVer() const;
    QString getHWVer() const;
    QString getDevSN() const;
    int getDefaultRate() const;

    Q_INVOKABLE int ctrl_transfer(int x, int y, int z);
    Q_INVOKABLE void blinkLeds();

    size_t samplesAdded() const;
    void setSamplesAdded(size_t count);
    bool write(ChannelItem* channel = nullptr);
    smu::Device* rawDevice() const;

protected:
    smu::Device* const m_device;
    QList<ChannelItem*> m_channels;
    QTimer ledTimer;
    unsigned m_led_step;
    friend class DataLogger;
    friend class SessionItem;

private slots:
    void advanceLed();

private:
    void setLed(unsigned value);

protected:
    size_t m_samples_added;
};

class ChannelItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<SignalItem> signals READ getSignals CONSTANT)
    Q_PROPERTY(QString label READ getLabel CONSTANT)
    Q_PROPERTY(unsigned mode MEMBER m_mode NOTIFY modeChanged)

public:
    ChannelItem(DeviceItem* parent, smu::Device* device, unsigned index);

    QQmlListProperty<SignalItem> getSignals();
    QString getLabel() const;
    bool buildTxBuffer();

signals:
    void modeChanged(unsigned mode);

protected:
    smu::Device* const m_device;
    const unsigned m_index;
    unsigned m_mode;
    QList<ModeItem*> m_modes;
    QList<SignalItem*> m_signals;
    std::vector<float> m_tx_data;
    TimerItem* timer;

    friend class SessionItem;
    friend class DeviceItem;
    friend class SignalItem;
    friend class TimerItem;
};

class SignalItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(FloatBuffer* buffer READ getBuffer CONSTANT)
    Q_PROPERTY(QString label READ getLabel CONSTANT)
    Q_PROPERTY(double min READ getMin CONSTANT)
    Q_PROPERTY(double max READ getMax CONSTANT)
    Q_PROPERTY(double resolution READ getResolution CONSTANT)
    Q_PROPERTY(SrcItem* src READ getSrc CONSTANT)
    Q_PROPERTY(bool isOutput READ getIsOutput NOTIFY isOutputChanged)
    Q_PROPERTY(bool isInput READ getIsInput NOTIFY isInputChanged)
    Q_PROPERTY(double measurement READ getMeasurement NOTIFY measurementChanged)
    Q_PROPERTY(double peak_to_peak READ getPeak NOTIFY peakChanged)
    Q_PROPERTY(double rms READ getRms NOTIFY rmsChanged)
    Q_PROPERTY(double mean READ getMean NOTIFY meanChanged)

public:
    SignalItem(ChannelItem* parent, int index, smu::Signal* signal);

    FloatBuffer* getBuffer() const;
    QString getLabel() const;
    double getMin() const;
    double getMax() const;
    double getResolution() const;
    SrcItem* getSrc() const;
    bool getIsOutput() const;
    bool getIsInput() const;
    double getMeasurement() const;
    double getPeak() const;
    double getRms() const;
    double getMean() const;

signals:
    void isOutputChanged(bool value);
    void isInputChanged(bool value);
    void measurementChanged(double value);
    void peakChanged(double value);
    void rmsChanged(double value);
    void meanChanged(double value);

protected slots:
    void onParentModeChanged(int mode);

protected:
    int const m_index;
    ChannelItem* const m_channel;
    smu::Signal* const m_signal;
    FloatBuffer* m_buffer;
    SrcItem* m_src;
    double m_measurement;
    double m_peak_to_peak;
    double m_rms;
    double m_mean;

    friend class SessionItem;
    friend class ChannelItem;
    friend class SrcItem;
    friend class TimerItem;

    void updateMeasurementMean();
    void updateMeasurementLatest();
    void updatePeakToPeak();
    void updateRms();
};

class SrcItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString src MEMBER m_src NOTIFY srcChanged)
    Q_PROPERTY(double v1 MEMBER m_v1 NOTIFY v1Changed)
    Q_PROPERTY(double v2 MEMBER m_v2 NOTIFY v2Changed)
    Q_PROPERTY(double period MEMBER m_period NOTIFY periodChanged)
    Q_PROPERTY(double phase MEMBER m_phase WRITE setPhase NOTIFY phaseChanged)
    Q_PROPERTY(double duty MEMBER m_duty NOTIFY dutyChanged)

public:
    explicit SrcItem(SignalItem* parent);
    Q_INVOKABLE void update();

    void setPhase(double phase);

signals:
    void srcChanged(QString value);
    void v1Changed(double value);
    void v2Changed(double value);
    void periodChanged(double value);
    void phaseChanged(double value);
    void dutyChanged(double value);
    void changed();

protected:
    QString m_src;
    double m_v1;
    double m_v2;
    double m_period;
    double m_phase;
    double m_duty;
    SignalItem* m_parent;

    friend class TimerItem;
};

class TimerItem : public QObject
{
    Q_OBJECT

public:
    TimerItem(ChannelItem* channel, DeviceItem* device);

public slots:
    void parameterChanged();

private slots:
    void needChangeBuffer();

private:
    ChannelItem* m_channel;
    DeviceItem* m_device;
    SessionItem* m_session;
    QTimer m_changeBufferTimer;
};

class DataLogger : public QObject
{
    Q_OBJECT

public:
    explicit DataLogger(float sample_time, QObject* parent = nullptr);
    ~DataLogger() override;

    void addData(DeviceItem* device, const std::array<float, 4>& samples);
    void addBulkData(DeviceItem* device, const std::vector<std::array<float, 4>>& samples);
    void clearData();
    void printData(DeviceItem* device);
    void setSampleTime(float sample_time);

private:
    void doAddBulkData(DeviceItem* device, const std::vector<std::array<float, 4>>& samples);
    void printDataUnlocked(DeviceItem* device);
    std::array<float, 4> computeAverageUnlocked(DeviceItem* device) const;
    void updateMinimum(DeviceItem* device, const std::array<float, 4>& samples);
    void updateMaximum(DeviceItem* device, const std::array<float, 4>& samples);
    void updateSum(DeviceItem* device, const std::array<float, 4>& samples);
    void resetData(DeviceItem* device);
    static std::string modifyDateTime(const std::string& date_time);
    void createLoggingFolder();

    float sampleTime;
    std::ofstream fileStream;
    std::map<DeviceItem*, int> dataCounter;
    std::map<DeviceItem*, std::array<float, 4>> minimum;
    std::map<DeviceItem*, std::array<float, 4>> maximum;
    std::map<DeviceItem*, std::array<float, 4>> sum;
    std::chrono::time_point<std::chrono::system_clock> startTime;
    std::chrono::time_point<std::chrono::system_clock> lastLog;
    std::mutex m_logMutex;
};

void registerTypes();
