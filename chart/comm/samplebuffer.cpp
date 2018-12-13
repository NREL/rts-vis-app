#include "samplebuffer.h"

#include <QDebug>
#include <QVector>

#include <chrono>
#include <cstdlib>

SampleBuffer::SampleBuffer(FrameDefinitionPtr const&  definition,
                           SampleBufferOptions const& options,
                           QObject*                   object)
    : QThread(object), m_definition(definition), m_options(options) {
    m_variable_store.resize(definition->variables.size());
    m_variable_cache.resize(definition->variables.size());

    qDebug() << Q_FUNC_INFO
             << (options.override_time ? "override time" : "using source time");

    for (auto const& fvar : definition->variables) {
        if (fvar->data_type != "float") {
            m_signal_var_offsets.push_back(fvar->index);
        }
    }
}

SampleBuffer::~SampleBuffer() = default;

bool is_next_char(char c) { return !std::isspace(c) and c != ',' and c != '"'; }

///
/// \brief Fill an already allocated and sized array, BUT NO MORE. Underflow
/// shall not change the rest of the array
///
void restricted_read_json_float_array(QByteArray const&   array,
                                      std::vector<float>& data) {

    char const*       ptr = array.data();
    char const* const end = array.data() + array.size();

    // scan until we hit a '['

    ptr = std::find(ptr, end, '[');
    ptr++;

    if (ptr == end) return;

    // find our first
    ptr = std::find_if(ptr, end, is_next_char);

    size_t counter = 0;

    while (ptr != end) {
        char* lend;
        float val = std::strtof(ptr, &lend);
        // qDebug() << val;

        if (ptr == lend) {
            qDebug() << "cannot convert" << array;
            qDebug() << *ptr;
            // couldn't convert
            return;
        }

        // conversion ok
        data[counter] = val;
        counter++;

        if (counter == data.size()) {
            break;
        }

        // scan, discarding whitespace, ',', and "\"
        ptr = lend;

        ptr = std::find_if(ptr, end, is_next_char);

        if (*ptr == ']') {
            // done
            // qDebug() << "done";
            break;
        }
    }
}

static auto application_start_time = std::chrono::high_resolution_clock::now();

static double get_since_start() {
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now() -
               application_start_time)
        .count();
}

void SampleBuffer::on_new_data(QVector<QByteArray> data_list) {
    m_got_first_packet = true;

    QByteArray array;

    if (data_list.empty()) return;

    if (data_list.size() == 1) {
        array = data_list.value(0);
    } else {
        array = data_list.value(1); // multipart, estimate first might be topic
    }

    restricted_read_json_float_array(array, m_variable_cache);

    // qDebug() << this << Q_FUNC_INFO << m_variable_cache[0]
    //         << m_variable_cache[1];

    // probe the array. value at [1] should be the mono counter

    double timestamp = 0;

    double time_delta_since_start = get_since_start();

    m_time_delta_last_received = time_delta_since_start;

    if (m_options.override_time) {
        timestamp = time_delta_since_start;
    } else {
        timestamp = static_cast<double>(m_variable_cache.at(1));
    }


    if (timestamp <= m_last_timestamp) {
        qWarning() << "Dropping frame from the past!"
                   << "New time" << timestamp << " <= "
                   << "Old Time" << m_last_timestamp;
        return;
    }

    m_last_timestamp = timestamp;

    // sanitize signals

    for (size_t offset : m_signal_var_offsets) {
        float value              = m_variable_cache[offset];
        m_variable_cache[offset] = static_cast<float>(value > .5f);
    }

    std::swap(m_variable_cache, m_variable_store);

    Q_ASSERT(m_variable_cache.size() == m_variable_store.size() and
             m_variable_cache.size() == m_definition->variables.size());
}

void SampleBuffer::on_sample_request() {

    double time_since_start = get_since_start();

    // if we havent seen something for a while, let people know
    double delta = time_since_start - m_time_delta_last_received;

    if (delta > 3.0 and m_got_first_packet) {
        qWarning() << this << "No packet for" << delta << "seconds!";
    }

    auto to_emit = QVector<float>::fromStdVector(m_variable_store);

    emit new_state_vector(to_emit, m_last_timestamp, m_definition);
}


//==============================================================================

SampleCollector::SampleCollector(ExperimentPtr const& definition,
                                 QObject*             object)
    : QThread(object), m_definition(definition) {
    m_total_variable_store.resize(definition->num_vars);
}
SampleCollector::~SampleCollector() {}

void SampleCollector::on_new_state_subvector(QVector<float>     v,
                                             double             timestamp,
                                             FrameDefinitionPtr p) {
    m_last_timestamp = std::max(timestamp, m_last_timestamp);

    auto const& vars = p->variables;
    for (size_t vi = 0; vi < p->variables.size(); vi++) {
        size_t place                  = vars[vi]->global_index;
        m_total_variable_store[place] = v[vi];
    }
}


void SampleCollector::on_sample_request() {
    //    qDebug() << this << Q_FUNC_INFO << m_last_timestamp;
    auto nv = QVector<float>::fromStdVector(m_total_variable_store);
    emit new_state_vector(nv, m_last_timestamp);
}

//==============================================================================


LineDelayBuffer::LineDelayBuffer(FrameDefinitionPtr const& definition,
                                 QObject*                  object)
    : QThread(object), m_definition(definition) {

    m_num_vars = definition->variables.size();

    //    qDebug() << Q_FUNC_INFO << m_num_vars;

    m_cache.resize(m_num_vars);

    for (auto const& fvar : definition->variables) {
        // qDebug() << fvar.data_type;
        if (fvar->data_type != "float") {
            m_signal_var_offsets.push_back(fvar->index);
        }
    }

    // TODO: we are assuming the number of samples in a second! we have no way
    // of getting this info for now.
    m_num_samples_max = 1000;

    realloc_storage();
}

void LineDelayBuffer::realloc_storage() {
    m_data.num_samples     = m_num_samples_max;
    m_data.num_vars        = m_num_vars;
    m_data.variables_store = QVector<float>(m_num_samples_max * m_num_vars);
}

void LineDelayBuffer::flush_storage() {
    m_curr_sample_num = 0;

    emit block_ready(m_data);

    realloc_storage();
}

void LineDelayBuffer::on_new_data(QVector<QByteArray> data_list) {
    // TODO: consolidate with above
    QByteArray array;

    if (data_list.empty()) return;

    if (data_list.size() == 1) {
        array = data_list.value(0);
    } else {
        array = data_list.value(1); // multipart, estimate first might be topic
    }

    restricted_read_json_float_array(array, m_cache);

    // probe the array. value at [1] should be the sim counter

    float timestamp = m_cache.at(1);

    if (timestamp < m_last_timestamp) {
        qWarning() << "Dropping frame from the past!" << timestamp
                   << m_last_timestamp;
        return;
    }

    m_last_timestamp = timestamp;

    // sanitize signals
    for (size_t offset : m_signal_var_offsets) {
        float value     = m_cache[offset];
        m_cache[offset] = static_cast<float>(value > .5f);
    }

    // now, burn this into our cache

    size_t position = m_curr_sample_num * m_num_vars;

    std::copy(m_cache.begin(),
              m_cache.end(),
              m_data.variables_store.begin() + position);

    m_curr_sample_num++;

    if (m_curr_sample_num >= m_num_samples_max) {
        flush_storage();
    }
}
