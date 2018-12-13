#ifndef SAMPLEBUFFER_H
#define SAMPLEBUFFER_H

#include "datacontrol.h"

#include <QObject>
#include <QThread>

///
/// \brief The SampleBuffer class handles a last-value-seen buffer from a topic
///
class SampleBuffer : public QThread {
    Q_OBJECT
    FrameDefinitionPtr m_definition;

    // double buffer the incoming data
    std::vector<float> m_variable_store;
    std::vector<float> m_variable_cache;

    std::vector<size_t> m_signal_var_offsets;

    float m_last_timestamp = 0;

public:
    SampleBuffer(FrameDefinitionPtr const& definition, QObject* object);
    ~SampleBuffer();


public slots:
    void on_new_data(QVector<QByteArray>);
    void on_sample_request();

signals:
    void new_state_vector(QVector<float>, FrameDefinitionPtr);
};

//==============================================================================

///
/// \brief The SampleCollector class collects sampled frames from a number of
/// sample buffers
///
class SampleCollector : public QThread {
    Q_OBJECT
    ExperimentDefinition m_definition;

    std::vector<float> m_total_variable_store;

public:
    SampleCollector(ExperimentDefinition const& definition, QObject* object);
    ~SampleCollector();


public slots:
    void on_new_state_subvector(QVector<float>, FrameDefinitionPtr);
    void on_sample_request();

signals:
    void new_state_vector(QVector<float>);
};

#endif // SAMPLEBUFFER_H
