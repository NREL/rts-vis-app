#ifndef DATACONTROL_H
#define DATACONTROL_H

#include <QDataStream>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUuid>

#include <array>
#include <memory>
#include <vector>

class Chart;
class QJsonObject;

///
/// \brief Convert a UUID to a QString, minus the brackets.
///
/// This is a hack to get around a flaw in the QUUID api.
/// There is a fix, but only in 5.11.
///
QString uuid_to_qstring(QUuid id);

///
/// \brief Write a binary dump of a given primitive
///
template <class T>
void write_primitive(QDataStream& stream, T prim) {
    stream.writeRawData(reinterpret_cast<char*>(&prim), sizeof(T));
}

///
/// \brief Write a binary dump of an array of primitives, prefixed with the
/// array count
///
template <class T>
void write_primitive_array(QDataStream& stream, T* data, size_t count) {
    write_primitive(stream, count);
    stream.writeRawData(reinterpret_cast<char const*>(data), sizeof(T) * count);
}

//==============================================================================

///
/// \brief The FrameVar struct models a variable inside a data frame
///
struct FrameVar {
    QString id;
    QString name;
    QString message_topic;
    size_t  index;
    QString data_type;
    QString legend_text;
    QString bus_location;
    QUuid   uuid;


    // assigned by us. check alpha to see if this is a valid color.
    std::array<uint8_t, 4> color;
    size_t                 global_index;

    FrameVar() = default;
    FrameVar(QJsonObject const&, size_t global_id);
};

using FrameVarPtr = std::shared_ptr<FrameVar>;

//==============================================================================

///
/// \brief The FrameDefinition struct describes a frame of data
///
struct FrameDefinition {
    QString                  frame_id;
    std::vector<FrameVarPtr> variables;
};

using FrameDefinitionPtr = std::shared_ptr<FrameDefinition>;

//==============================================================================

///
/// \brief The SourceConfig struct describes data source sample rates
///
struct SourceConfig {
    size_t estimated_samples_per_sec;
    size_t target_samples_per_sec;
};

///
/// \brief The ExperimentDefinition struct models an experiment as defined in
/// the experiment.json file
///
struct ExperimentDefinition {
    QHash<QString, FrameDefinitionPtr> frame_map;
    size_t                             num_vars = 0;

    QHash<QString, size_t>     uuid_to_global_varid_mapping;
    QHash<size_t, FrameVarPtr> global_to_var_mapping;

    std::vector<Chart> charts;

    ExperimentDefinition();
    ExperimentDefinition(QJsonObject const&);
    ~ExperimentDefinition();

    ExperimentDefinition(ExperimentDefinition const&) = delete;
    ExperimentDefinition& operator=(ExperimentDefinition const&) = delete;

    ExperimentDefinition(ExperimentDefinition&&) = delete;
    ExperimentDefinition& operator=(ExperimentDefinition&&) = delete;

    auto begin() const { return frame_map.begin(); }
    auto end() const { return frame_map.end(); }
};

using ExperimentPtr = std::shared_ptr<ExperimentDefinition const>;

//==============================================================================

///
/// \brief Given a list of variable uuids, find the data frame definition that
/// they all belong to
///
FrameDefinitionPtr get_common_frame(ExperimentPtr const& definition,
                                    QStringList const&   var_uuids);


#endif // DATACONTROL_H
