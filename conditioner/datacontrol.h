#ifndef DATACONTROL_H
#define DATACONTROL_H

#include <QDataStream>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <memory>
#include <vector>

class QJsonObject;

///
/// \brief Read a json floating point value array.
///
/// Needed when we started having some performance issues with the QT reader.
/// Consider removing when more tests are done.
///
void read_json_float_array(QByteArray const& array, std::vector<float>& data);

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

    size_t global_index; // assigned by us

    FrameVar() = default;
    FrameVar(QJsonObject const&, size_t global_id);
};

//==============================================================================

///
/// \brief The FrameDefinition struct describes a frame of data
///
struct FrameDefinition {
    QString               frame_id;
    std::vector<FrameVar> variables;
};

using FrameDefinitionPtr = std::shared_ptr<FrameDefinition const>;

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
    QHash<QString, std::shared_ptr<FrameDefinition>> map;
    size_t                                           num_vars;

    QHash<QString, size_t> global_var_mapping;
    QJsonObject            mapping_object;

    ExperimentDefinition() = default;
    ExperimentDefinition(QJsonObject const&);

    auto begin() const { return map.begin(); }
    auto end() const { return map.end(); }
};

#endif // DATACONTROL_H
