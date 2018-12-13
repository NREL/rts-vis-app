#include "chart.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

static auto none_lit  = QStringLiteral("none");
static auto line_lit  = QStringLiteral("line");
static auto stack_lit = QStringLiteral("stack");
static auto scope_lit = QStringLiteral("scope");
static auto alert_lit = QStringLiteral("alert");

ChartType string_to_chart_type(QString string) {
    if (string.startsWith(line_lit, Qt::CaseInsensitive)) {
        return ChartType::LINE;
    } else if (string.startsWith(stack_lit, Qt::CaseInsensitive)) {
        return ChartType::STACK;
    } else if (string.startsWith(scope_lit, Qt::CaseInsensitive)) {
        return ChartType::SCOPE;
    } else if (string.startsWith(alert_lit, Qt::CaseInsensitive)) {
        return ChartType::ALERT;
    }
    return ChartType::NONE;
}

QString chart_type_to_string(ChartType ct) {
    switch (ct) {
    case ChartType::NONE: return none_lit;
    case ChartType::LINE: return line_lit;
    case ChartType::STACK: return stack_lit;
    case ChartType::SCOPE: return scope_lit;
    case ChartType::ALERT: return alert_lit;
    }

    Q_UNREACHABLE();
}

Chart::Chart() = default;

Chart::Chart(QJsonObject const& object) {
    title   = object["title"].toString("Untitled");
    x_label = object["x_axis_name"].toString("Time");
    y_label = object["y_axis_name"].toString("Value");

    type = object["type"].toString();

    if (type.isEmpty()) {
        type = object["plot_type"].toString();
    }

    chart_col = object["chart_col"].toInt(0);
    chart_row = object["chart_row"].toInt(0);

    chart_col_span = object["chart_col_span"].toInt(1);
    chart_row_span = object["chart_row_span"].toInt(1);

    chart_tint = object["chart_tint"].toString();

    hide_legend = object["hide_legend"].toBool(false);

    use_value_min = object.contains("min_value");
    value_min     = object["min_value"].toDouble();

    use_value_max = object.contains("max_value");
    value_max     = object["max_value"].toDouble();

    for (auto v : object["variables"].toArray()) {
        auto obj = v.toObject();

        auto vuuid          = obj["id"].toString();
        auto old_style_axis = obj["axis"].toString();
        if (old_style_axis.startsWith("x")) {
            continue;
        }
        if (vuuid.isEmpty()) {
            qCritical() << "Empty id for chart!";
        }
        variables.push_back(vuuid);
    }
}

Chart::~Chart() = default;

QJsonObject Chart::to_json() const {
    QJsonObject object;
    object["title"]       = title;
    object["x_axis_name"] = x_label;
    object["y_axis_name"] = y_label;

    object["type"] = type;

    object["chart_col"] = chart_col;
    object["chart_row"] = chart_row;

    object["chart_col_span"] = chart_col_span;
    object["chart_row_span"] = chart_row_span;

    QJsonArray var_list;

    for (auto const& v : variables) {
        QJsonObject link;
        link["id"] = v;
        var_list.push_back(link);
    }
    return object;
}
