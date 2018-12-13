#ifndef SAMPLEBUFFER_H
#define SAMPLEBUFFER_H

#include "datacontrol.h"

#include <QObject>
#include <QThread>
#include <QVector>

struct SampleBufferOptions {
    bool override_time = false;
};

///
/// \brief The SampleBuffer class handles a last-value-seen buffer from a topic
///
class SampleBuffer : public QThread {
    Q_OBJECT
    FrameDefinitionPtr  m_definition;
    SampleBufferOptions m_options;

    std::vector<float> m_variable_store;
    std::vector<float> m_variable_cache;

    std::vector<size_t> m_signal_var_offsets;

    bool   m_got_first_packet         = false;
    double m_time_delta_last_received = 0;

    double m_last_timestamp = 0;

public:
    SampleBuffer(FrameDefinitionPtr const&  definition,
                 SampleBufferOptions const& options,
                 QObject*                   object);
    ~SampleBuffer();

public slots:
    ///
    /// \brief Handle a new frame of data
    ///
    void on_new_data(QVector<QByteArray>);

    ///
    /// \brief Handle a new request to sample the last value seen buffer
    ///
    void on_sample_request();

signals:
    ///
    /// \brief Emitted when a new sample is ready
    ///
    void new_state_vector(QVector<float>, double, FrameDefinitionPtr);
};

//==============================================================================

///
/// \brief The SampleCollector class collects sampled frames from a number of
/// sample buffers
///
class SampleCollector : public QThread {
    Q_OBJECT
    ExperimentPtr m_definition;

    std::vector<float> m_total_variable_store;
    double             m_last_timestamp = 0;

public:
    SampleCollector(ExperimentPtr const& definition, QObject* object);
    ~SampleCollector();

    auto const& definition() const { return *m_definition; }

public slots:
    void on_new_state_subvector(QVector<float>, double, FrameDefinitionPtr);
    void on_sample_request();

signals:
    void new_state_vector(QVector<float>, double);
};

//==============================================================================

///
/// \brief The DelayedVarBlock struct is to store all the samples from high rate
/// variables that we are going to time delay to get a scope like chart
///
/// This class should have value semantics
///
struct DelayedVarBlock {
    QVector<float> variables_store;

    size_t num_vars;
    size_t num_samples;

    size_t index(size_t internal_var_id, size_t sample_id) const {
        return num_vars * sample_id + internal_var_id;
    }

    // an id here is not the global var id
    float get_var(size_t internal_var_id, size_t sample_id) const {
        return variables_store[index(internal_var_id, sample_id)];
    }
};

//==============================================================================

///
/// \brief The LineDelayBuffer class handles buffering high rate data for scope
/// plots
///
class LineDelayBuffer : public QThread {
    Q_OBJECT

    FrameDefinitionPtr  m_definition;
    std::vector<size_t> m_signal_var_offsets;
    size_t              m_num_vars;

    std::vector<float> m_cache;
    DelayedVarBlock    m_data;

    size_t m_curr_sample_num = 0;
    size_t m_num_samples_max;

    float m_last_timestamp = 0;

    void realloc_storage();
    void flush_storage();

public:
    LineDelayBuffer(FrameDefinitionPtr const& definition, QObject* object);

    void on_new_data(QVector<QByteArray>);

signals:
    void block_ready(DelayedVarBlock);
};

#endif // SAMPLEBUFFER_H
