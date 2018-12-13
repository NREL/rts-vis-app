#include "samplebuffer.h"

#include <QDebug>
#include <QVector>

SampleBuffer::SampleBuffer(FrameDefinitionPtr const& definition,
                           QObject*                  object)
    : QThread(object), m_definition(definition) {
    m_variable_store.resize(definition->variables.size());
    m_variable_cache.resize(definition->variables.size());

    for (auto const& fvar : definition->variables) {
        if (fvar.data_type != "float") {
            m_signal_var_offsets.push_back(fvar.index);
        }
    }
}

SampleBuffer::~SampleBuffer() {}

static bool is_next_char(char c) {
    return !std::isspace(c) and c != ',' and c != '"';
}

///
/// \brief Fill an already allocated and sized array, BUT NO MORE. Underflow
/// shall not change the rest of the array
///
static void restricted_read_json_float_array(QByteArray const&   array,
                                             std::vector<float>& data) {

    // qDebug() << array;

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

void SampleBuffer::on_new_data(QVector<QByteArray> data_list) {

    QByteArray array;

    if (data_list.empty()) return;

    if (data_list.size() == 1) {
        array = data_list.value(0);
    } else {
        array = data_list.value(1); // multipart, estimate first might be topic
    }

    restricted_read_json_float_array(array, m_variable_cache);


    // probe the array. value at [0] should be the mono counter

    float timestamp = m_variable_cache.at(0);

    if (timestamp < m_last_timestamp) {
        // this has happened for some misbehaving sources
        qDebug() << "Dropping frame from the past!" << timestamp
                 << m_last_timestamp;
        return;
    }

    m_last_timestamp = timestamp;

    // sanitize signals
    // some bool values are sent as floats

    for (size_t offset : m_signal_var_offsets) {
        float value              = m_variable_cache[offset];
        m_variable_cache[offset] = static_cast<float>(value > .5f);
    }

    // swap to the next buffer
    std::swap(m_variable_cache, m_variable_store);

    Q_ASSERT(m_variable_cache.size() == m_variable_store.size() and
             m_variable_cache.size() == m_definition->variables.size());
}

void SampleBuffer::on_sample_request() {
    // qDebug() << this << Q_FUNC_INFO;
    auto to_emit = QVector<float>::fromStdVector(m_variable_store);

    emit new_state_vector(to_emit, m_definition);
}


//==============================================================================

SampleCollector::SampleCollector(ExperimentDefinition const& definition,
                                 QObject*                    object)
    : QThread(object), m_definition(definition) {
    m_total_variable_store.resize(definition.num_vars);
}
SampleCollector::~SampleCollector() {}

void SampleCollector::on_new_state_subvector(QVector<float>     v,
                                             FrameDefinitionPtr p) {
    auto const& vars = p->variables;
    for (size_t vi = 0; vi < p->variables.size(); vi++) {
        size_t place                  = vars[vi].global_index;
        m_total_variable_store[place] = v[vi];
    }
}


void SampleCollector::on_sample_request() {
    // this pulls a copy of the current state
    auto nv = QVector<float>::fromStdVector(m_total_variable_store);
    emit new_state_vector(nv);
}
