#ifndef CHART_H
#define CHART_H

#include <QColor>
#include <QObject>
#include <QStringList>

///
/// \brief The ChartType enum encodes the type of chart.
///
enum class ChartType { NONE, LINE, STACK, SCOPE, ALERT };

///
/// \brief Convert a string to a ChartType. Returns NONE if conversion fails.
///
ChartType string_to_chart_type(QString string);

///
/// \brief Convert a ChartType to a string. Because c++.
///
QString chart_type_to_string(ChartType);


///
/// \brief The Chart class models the basic requirements of a chart
///
class Chart {
public:
    QString title   = "Chart";
    QString x_label = "Time";
    QString y_label = "Value";

    QString type;

    int chart_col = 0;
    int chart_row = 0;

    int chart_col_span = 1;
    int chart_row_span = 1;

    QStringList variables;

    QColor chart_tint;

    bool hide_legend = false;

    bool  use_value_min = false;
    float value_min     = 0;

    bool  use_value_max = false;
    float value_max     = 0;

public:
    Chart();
    Chart(QJsonObject const&);
    ~Chart();

    QJsonObject to_json() const;
};

#endif // CHART_H
