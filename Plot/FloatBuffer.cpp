#include "FloatBuffer.h"

#include <cstddef>

FloatBuffer::FloatBuffer(QObject* parent)
    : QObject(parent),
      m_secondsPerSample(0.00001f),
      m_length(0),
      m_first_samples_ignored(0),
      m_ignored_requested(0)
{
}

size_t FloatBuffer::validSizeUnlocked() const
{
    const size_t ignored = std::min(m_first_samples_ignored, m_length);
    return m_length - ignored;
}

size_t FloatBuffer::dataIndexUnlocked(size_t index) const
{
    if (m_data.empty() || index >= validSizeUnlocked()) {
        return 0;
    }
    return std::min(m_first_samples_ignored, m_length) + index;
}

bool FloatBuffer::validSignalIndex(int signal_index) const
{
    return signal_index >= 0 && signal_index < 4;
}

unsigned FloatBuffer::timeToIndexUnlocked(double time) const
{
    if (!std::isfinite(time) || time <= 0.0 || m_secondsPerSample <= 0.0f) {
        return 0;
    }
    const double raw_index = time / static_cast<double>(m_secondsPerSample);
    if (!std::isfinite(raw_index) || raw_index <= 0.0) {
        return 0;
    }
    const size_t maximum = validSizeUnlocked();
    if (raw_index >= static_cast<double>(maximum)) {
        return static_cast<unsigned>(maximum);
    }
    return static_cast<unsigned>(raw_index);
}

double FloatBuffer::indexToTimeUnlocked(unsigned index) const
{
    return static_cast<double>(index) * static_cast<double>(m_secondsPerSample);
}

unsigned FloatBuffer::countPointsBetween(double start, double end) const
{
    if (!std::isfinite(start) || !std::isfinite(end) || end <= start) {
        return 0;
    }

    QReadLocker locker(&m_lock);
    const unsigned first = timeToIndexUnlocked(start);
    const unsigned last = timeToIndexUnlocked(end);
    return last > first ? last - first : 0;
}

unsigned FloatBuffer::size() const
{
    QReadLocker locker(&m_lock);
    return static_cast<unsigned>(validSizeUnlocked());
}

unsigned long FloatBuffer::ignoredFirstSamplesCount() const
{
    QReadLocker locker(&m_lock);
    return static_cast<unsigned long>(m_ignored_requested);
}

void FloatBuffer::setIgnoredFirstSamplesCount(unsigned long count)
{
    bool changed = false;
    {
        QWriteLocker locker(&m_lock);
        const size_t requested = static_cast<size_t>(count);
        changed = m_ignored_requested != requested;
        m_ignored_requested = requested;
        m_first_samples_ignored = std::min(requested, m_data.size());
    }
    if (changed) {
        emit dataChanged();
    }
}

float FloatBuffer::get(unsigned index) const
{
    QReadLocker locker(&m_lock);
    if (index >= validSizeUnlocked()) {
        return 0.0f;
    }
    return m_data[dataIndexUnlocked(index)];
}

void FloatBuffer::append_samples(const std::vector<std::array<float, 4>>& samples,
                                  int signal_index)
{
    if (!validSignalIndex(signal_index) || samples.empty()) {
        return;
    }

    size_t count = 0;
    {
        QWriteLocker locker(&m_lock);
        if (m_data.empty()) {
            return;
        }
        m_length = std::min(m_length, m_data.size());
        count = std::min(samples.size(), m_data.size() - m_length);
        for (size_t i = 0; i < count; ++i) {
            m_data.push_back(samples[i][static_cast<size_t>(signal_index)]);
        }
        m_length += count;
    }
    if (count > 0) {
        emit dataChanged();
    }
}

void FloatBuffer::append_samples_circular(const std::vector<std::array<float, 4>>& samples,
                                           int signal_index)
{
    if (!validSignalIndex(signal_index) || samples.empty()) {
        return;
    }

    {
        QWriteLocker locker(&m_lock);
        if (m_data.empty()) {
            return;
        }
        m_length = std::min(m_length, m_data.size());
        const size_t capacity = m_data.size();
        const size_t count = std::min(samples.size(), capacity);
        const size_t first = samples.size() - count;
        const size_t keep = capacity - count;
        if (m_data.size() > keep) {
            const size_t removed = m_data.size() - keep;
            m_first_samples_ignored = m_first_samples_ignored > removed
                ? m_first_samples_ignored - removed : 0;
            m_data.erase(m_data.begin(), m_data.begin() + static_cast<std::ptrdiff_t>(removed));
            m_length = std::min(m_length, keep);
        }
        for (size_t i = first; i < samples.size(); ++i) {
            m_data.push_back(samples[i][static_cast<size_t>(signal_index)]);
        }
        m_length = std::min(capacity, m_length + count);
    }
    emit dataChanged();
}

void FloatBuffer::toVertexData(double start, double end, QSGGeometry::Point2D* vertices,
                               unsigned n_vertices) const
{
    if (!vertices || n_vertices == 0 || !std::isfinite(start) || !std::isfinite(end)
            || end <= start) {
        return;
    }

    QReadLocker locker(&m_lock);
    const unsigned first = timeToIndexUnlocked(start);
    const unsigned last = timeToIndexUnlocked(end);
    if (last <= first) {
        return;
    }

    const unsigned available = last - first;
    const unsigned count = std::min(available, n_vertices);
    for (unsigned i = 0; i < count; ++i) {
        unsigned index = first + i;
        if (available > n_vertices && n_vertices > 1) {
            index = first + static_cast<unsigned>((static_cast<double>(i) * available) / n_vertices);
        }
        vertices[i].set(indexToTimeUnlocked(index), m_data[dataIndexUnlocked(index)]);
    }
}

void FloatBuffer::setRate(float seconds_per_sample)
{
    if (!std::isfinite(seconds_per_sample) || seconds_per_sample <= 0.0f) {
        return;
    }

    {
        QWriteLocker locker(&m_lock);
        m_secondsPerSample = seconds_per_sample;
    }
    emit dataChanged();
}

void FloatBuffer::allocate(unsigned length)
{
    {
        QWriteLocker locker(&m_lock);
        m_data.assign(length, 0.0f);
        m_length = 0;
        m_first_samples_ignored = std::min(m_ignored_requested, m_data.size());
    }
    emit dataChanged();
}

QList<qreal> FloatBuffer::getData() const
{
    QReadLocker locker(&m_lock);
    const size_t count = validSizeUnlocked();
    QList<qreal> data;
    data.reserve(static_cast<int>(count));
    for (size_t i = 0; i < count; ++i) {
        data.append(m_data[dataIndexUnlocked(i)]);
    }
    return data;
}

std::vector<float> FloatBuffer::snapshot() const
{
    QReadLocker locker(&m_lock);
    const size_t count = validSizeUnlocked();
    std::vector<float> data;
    data.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        data.push_back(m_data[dataIndexUnlocked(i)]);
    }
    return data;
}

void FloatBuffer::startSweep()
{
    {
        QWriteLocker locker(&m_lock);
        const size_t length = m_data.size();
        m_data.clear();
        m_data.resize(length);
        m_length = 0;
        m_first_samples_ignored = std::min(m_ignored_requested, m_data.size());
    }
    emit dataChanged();
}

double FloatBuffer::mean() const
{
    QReadLocker locker(&m_lock);
    const size_t count = validSizeUnlocked();
    if (count == 0) {
        return 0.0;
    }
    double total = 0.0;
    for (size_t i = 0; i < count; ++i) {
        total += m_data[dataIndexUnlocked(i)];
    }
    return total / static_cast<double>(count);
}

double FloatBuffer::rms() const
{
    QReadLocker locker(&m_lock);
    const size_t count = validSizeUnlocked();
    if (count == 0) {
        return 0.0;
    }
    double average = 0.0;
    for (size_t i = 0; i < count; ++i) {
        average += m_data[dataIndexUnlocked(i)];
    }
    average /= static_cast<double>(count);

    double total = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double difference = m_data[dataIndexUnlocked(i)] - average;
        total += difference * difference;
    }
    return std::sqrt(total / static_cast<double>(count));
}

double FloatBuffer::peak_to_peak() const
{
    QReadLocker locker(&m_lock);
    const size_t count = validSizeUnlocked();
    if (count == 0) {
        return 0.0;
    }
    float minimum = m_data[dataIndexUnlocked(0)];
    float maximum = minimum;
    for (size_t i = 1; i < count; ++i) {
        const float value = m_data[dataIndexUnlocked(i)];
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }
    return static_cast<double>(maximum - minimum);
}

std::vector<float> FloatBuffer::dif_mean(double average) const
{
    const std::vector<float> values = snapshot();
    std::vector<float> result;
    result.reserve(values.size());
    for (float value : values) {
        result.push_back(static_cast<float>(value - average));
    }
    return result;
}
