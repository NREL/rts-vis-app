#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include "chart.h"
#include "comm/samplebuffer.h"

#include <glm/glm.hpp>

#include <QLabel>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QWidget>
#include <qopenglfunctions_3_2_core.h>

#include <memory>

// forward decls

struct DataRef;
struct ExperimentDefinition;
using ExperimentPtr = std::shared_ptr<ExperimentDefinition const>;
class ChartLineData;
class ChartStackData;
class ChartScopeData;
class Session;

struct ChartBounds {
    float min_time;
    float max_time;

    float min_value;
    float max_value;
};

///
/// \brief The GLPoweredChart class is the base class for any chart that uses
/// OpenGL.
///
class GLPoweredChart : public QOpenGLWidget, public QOpenGLFunctions_3_2_Core {
protected:
    QOpenGLShaderProgram m_program;
    int                  m_mvp_location = -1; ///< Modelview Proj mat shader loc
    glm::mat4            m_projection;
    glm::vec3            m_background_color;

    void update_projection();

public:
    GLPoweredChart(Chart const& chart);
    ~GLPoweredChart() override;

    ///
    /// \brief Get the data and time bounds of this chart
    ///
    virtual ChartBounds get_bounds() const = 0;

    virtual void add(DataRef const& ref);

    void initializeGL() override;

    void resizeGL(int w, int h) override;
};

// Chart Widget Options ========================================================

struct ChartWidgetOptions {
    Chart               chart; ///< Chart definition
    ExperimentPtr       experiment_info;
    std::vector<size_t> server_ids;         ///< global var ids the chart uses
    size_t              history_ms = 10000; ///< history to show, in ms

    ChartWidgetOptions() = default;
    ChartWidgetOptions(Chart const& t, std::vector<size_t>&& vids)
        : chart(t), server_ids(std::move(vids)) {}
};

// Line Widget =================================================================

class LineChartWidget : public GLPoweredChart {
    ChartWidgetOptions             m_options;
    std::unique_ptr<ChartLineData> m_from;

public:
    LineChartWidget(ChartWidgetOptions const&);
    ~LineChartWidget() override;

    void add(DataRef const& ref) override;

    ChartBounds get_bounds() const override;

protected:
    void paintGL() override;
};

// Stack Widget ================================================================

class StackChartWidget : public GLPoweredChart {
    ChartWidgetOptions              m_options;
    std::unique_ptr<ChartStackData> m_from;

public:
    StackChartWidget(ChartWidgetOptions const&);
    ~StackChartWidget() override;

    void add(DataRef const& ref) override;

    ChartBounds get_bounds() const override;

protected:
    void paintGL() override;
};

// Scope Widget ================================================================

class ScopeChartWidget : public GLPoweredChart {
    Q_OBJECT

    ChartWidgetOptions              m_options;
    std::unique_ptr<ChartScopeData> m_data;

public:
    ScopeChartWidget(ChartWidgetOptions const&);
    ~ScopeChartWidget() override;

    ChartBounds get_bounds() const override;

protected:
    void paintGL() override;

public slots:
    // scopes have a different data source
    void block_ready(DelayedVarBlock);
};

// Panel =======================================================================

///
/// \brief The Panel class is the top level chart class.
///
/// Panel
/// | - AlertChartWidget
/// | - ChartWidget
/// |   | -> GLChart
///
/// \todo Rework the add and update functions to signals and slots
///
class Panel : public QWidget {
public:
    Panel(QWidget* p);
    virtual ~Panel();

    virtual void add(DataRef const& ref) = 0;
    virtual void update();
};

// Alert Widget ================================================================

class AlertChartWidget : public Panel {
    ChartWidgetOptions m_options;

    std::vector<QLabel*> m_labels;
    std::vector<float>   m_last_value;

public:
    AlertChartWidget(ChartWidgetOptions const&, QWidget* p = nullptr);
    ~AlertChartWidget() override;

    void add(DataRef const& ref) override;
    void update() override;
};


// Chart Widget ================================================================

class ChartWidget : public Panel {
    GLPoweredChart* m_chart = nullptr;

    QLabel* m_min_label;
    QLabel* m_max_label;

    QLabel* m_min_time_label;
    QLabel* m_max_time_label;

    ChartBounds m_last;

public:
    ChartWidget(ChartWidgetOptions const& options,
                Session*                  session,
                QWidget*                  p = nullptr);
    ~ChartWidget() override;

    void add(DataRef const& ref) override;
    void update() override;
};


///
/// \brief Helper function to make a new Panel, given the options
///
Panel* make_panel(ChartWidgetOptions const& options,
                  Session*                  session,
                  QWidget*                  parent = nullptr);

#endif // CHARTWIDGET_H
