#include "datacontrol.h"

#include "chart.h"

#include <QColor>
#include <QDataStream>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <cmath>

QString uuid_to_qstring(QUuid id) {
    auto s = id.toString();
    return s.mid(1, s.size() - 2);
}

/// Color choices from the colorbrewer website
static const std::vector<std::array<uint8_t, 4>> colorbrewer = {
    { { 166, 206, 227, 255 } }, { { 31, 120, 180, 255 } },
    { { 178, 223, 138, 255 } }, { { 51, 160, 44, 255 } },
    { { 251, 154, 153, 255 } }, { { 227, 26, 28, 255 } },
    { { 253, 191, 111, 255 } }, { { 255, 127, 0, 255 } },
    { { 202, 178, 214, 255 } }, { { 106, 61, 154, 255 } },
    { { 255, 255, 153, 255 } }, { { 177, 89, 40, 255 } },
    { { 141, 211, 199, 255 } }, { { 255, 255, 179, 255 } },
    { { 190, 186, 218, 255 } }, { { 251, 128, 114, 255 } },
    { { 128, 177, 211, 255 } }, { { 253, 180, 98, 255 } },
    { { 179, 222, 105, 255 } }, { { 252, 205, 229, 255 } },
    { { 217, 217, 217, 255 } }, { { 188, 128, 189, 255 } },
    { { 204, 235, 197, 255 } }, { { 255, 237, 111, 255 } },

};

std::array<uint8_t, 4> generate_color(size_t global_id) {
    return colorbrewer[global_id % colorbrewer.size()];
}

static std::array<uint8_t, 4> get_or_generate_color(QJsonObject const& object,
                                                    size_t global_id) {
    auto iter = object.find("color");
    if (iter == object.end()) {
        return generate_color(global_id);
    }

    if (iter->isString()) {
        QColor color(iter->toString());

        if (!color.isValid()) return generate_color(global_id);

        return { { static_cast<uint8_t>(color.red()),
                   static_cast<uint8_t>(color.green()),
                   static_cast<uint8_t>(color.blue()),
                   1 } };
    }

    return generate_color(global_id);
}

FrameVar::FrameVar(QJsonObject const& object, size_t global_id)
    : id(object["id"].toString()),
      name(object["variable_name"].toString("Unnamed")),
      message_topic(object["message_topic"].toString()),
      index(object["message_index"].toString().toUInt()),
      data_type(object["data_type"].toString()),
      legend_text(object["legend_text"].toString()),
      bus_location(object["bus_location"].toString()),
      uuid(uuid_to_qstring(QUuid(object["uuid"].toString()))),
      color(get_or_generate_color(object, global_id)),
      global_index(global_id) {}


ExperimentDefinition::ExperimentDefinition(QJsonObject const& obj) {
    size_t global_id_counter = 0;

    auto next_id = [&global_id_counter]() { return global_id_counter++; };

    auto vars = obj["variables"].toArray();

    for (auto vdef : vars) {
        FrameVar frame_var(vdef.toObject(), next_id());

        uuid_to_global_varid_mapping[uuid_to_qstring(frame_var.uuid)] =
            frame_var.global_index;

        auto iter = frame_map.find(frame_var.message_topic);

        if (iter == frame_map.end()) {
            iter = frame_map.insert(frame_var.message_topic,
                                    std::make_shared<FrameDefinition>());
        }

        auto svar = std::make_shared<FrameVar>(frame_var);

        (*iter)->variables.emplace_back(svar);

        global_to_var_mapping[frame_var.global_index] = svar;
    }

    for (auto& v : frame_map) {
        v->frame_id = v->variables.front()->message_topic;

        std::sort(
            v->variables.begin(),
            v->variables.end(),
            [](auto const& a, auto const& b) { return a->index < b->index; });
    }

    num_vars = global_id_counter;

    for (auto rc : obj["plots"].toArray()) {
        charts.emplace_back(rc.toObject());
    }
}


ExperimentDefinition::ExperimentDefinition()  = default;
ExperimentDefinition::~ExperimentDefinition() = default;

//==============================================================================

FrameDefinitionPtr get_common_frame(ExperimentPtr const& definition,
                                    QStringList const&   var_uuids) {

    auto const& def = *definition;

    QString frame;

    for (auto const& uuid : var_uuids) {
        auto iter = def.uuid_to_global_varid_mapping.find(uuid);
        if (iter == def.uuid_to_global_varid_mapping.end()) {
            throw std::runtime_error("inconsistent variable state!");
        }
        auto vid = *iter;

        Q_ASSERT(def.global_to_var_mapping.contains(vid));

        auto const& vinfo = def.global_to_var_mapping[vid];

        if (frame.isEmpty()) {
            frame = vinfo->message_topic;
        }

        if (frame != vinfo->message_topic) return {};
    }

    Q_ASSERT(def.frame_map.contains(frame));

    return def.frame_map[frame];
}
