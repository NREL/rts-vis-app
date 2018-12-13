#include "datacontrol.h"

#include <QDataStream>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <cmath>

FrameVar::FrameVar(QJsonObject const& object, size_t global_id)
    : id(object["id"].toString()),
      name(object["variable_name"].toString()),
      message_topic(object["message_topic"].toString()),
      index(object["message_index"].toString().toUInt()),
      data_type(object["data_type"].toString()),
      legend_text(object["legend_text"].toString()),
      bus_location(object["bus_location"].toString()),
      uuid(QUuid(object["uuid"].toString())),
      global_index(global_id) {}


ExperimentDefinition::ExperimentDefinition(QJsonObject const& obj) {
    size_t global_id_counter = 0;

    auto next_id = [&global_id_counter]() { return global_id_counter++; };

    auto vars = obj["variables"].toArray();

    for (auto vdef : vars) {
        FrameVar frame_var(vdef.toObject(), next_id());

        global_var_mapping[frame_var.uuid.toString()] = frame_var.global_index;

        auto iter = map.find(frame_var.message_topic);

        if (iter == map.end()) {
            iter = map.insert(frame_var.message_topic,
                              std::make_shared<FrameDefinition>());
        }

        (*iter)->variables.emplace_back(frame_var);
    }

    for (auto& v : map) {
        v->frame_id = v->variables.front().message_topic;

        std::sort(
            v->variables.begin(),
            v->variables.end(),
            [](auto const& a, auto const& b) { return a.index < b.index; });
    }

    // compute some nice lookups
    num_vars = global_id_counter;

    auto iter = global_var_mapping.constBegin();

    while (iter != global_var_mapping.constEnd()) {
        mapping_object[iter.key()] = static_cast<int>(iter.value());
        iter++;
    }
}
