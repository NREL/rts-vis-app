#include "chartwidget.h"

#include "chartdata.h"
#include "comm/datacontrol.h"
#include "comm/session.h"
#include "flowlayout.h"
#include "verticallabel.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QDateTime>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QOpenGLBuffer>
#include <QVBoxLayout>

#include <chrono>
#include <limits>
#include <unordered_set>

static char const* vertex_source = R"(
#version 330

uniform mat4 sys_mvp;

layout(location = 0) in vec4 raw_position;
layout(location = 1) in vec4 raw_color;

out vec4 int_color;

void main() {
    vec4 pos = sys_mvp*raw_position;
    gl_Position = pos;
    int_color = raw_color;
}
)";

static char const* frag_source = R"(
#version 330

//layout(location = 2) uniform vec4 mat_color;

in  vec4 int_color;
out vec4 sys_color;

void main() {
    //sys_color = int_color*mat_color;
    sys_color = int_color;
}
)";


static QFont make_fixed_font() {
    static QFont f("Courier", 10);
    return f;
}

///
/// \brief Check for any GL errors, and explode if found.
///
static void check_gl_errors(char const* context) {
    auto err = glGetError();

    if (err != GL_NO_ERROR) {
        qFatal("GL ERROR IN %s %i", context, err);
    }
}

static glm::vec3 make_background_color(QColor tint) {
    if (!tint.isValid()) {
        tint = QColor("#000a12");
    }

    return { tint.redF(), tint.greenF(), tint.blueF() };
}

GLPoweredChart::GLPoweredChart(Chart const& chart)
    : m_background_color(make_background_color(chart.chart_tint)) {}

GLPoweredChart::~GLPoweredChart() = default;

void GLPoweredChart::add(DataRef const&) {}

void GLPoweredChart::initializeGL() {
    initializeOpenGLFunctions();
    // this makes the background match the surrounding widget background
    glClearColor(0, 0.039f, 0.070f, 1);

    bool ok = false;

    ok =
        m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_source);
    Q_ASSERT(ok && "Unable to compile vertex shader!");
    ok =
        m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, frag_source);
    Q_ASSERT(ok && "Unable to compile fragment shader!");

    ok = m_program.link();
    Q_ASSERT(ok && "Unable to link program!");

    m_mvp_location = m_program.uniformLocation("sys_mvp");
}

void GLPoweredChart::resizeGL(int /*w*/, int /*h*/) { update_projection(); }

void GLPoweredChart::update_projection() {
    glClearColor(
        m_background_color.r, m_background_color.g, m_background_color.b, 1);

    auto bounds = get_bounds();

    // add a margin to the data, so we add 10% to the value

    auto range = bounds.max_value - bounds.min_value;
    range *= .1;

    bounds.max_value += range;
    bounds.min_value -= range;

    m_projection = glm::ortho<float>(bounds.min_time,
                                     bounds.max_time,
                                     bounds.min_value,
                                     bounds.max_value,
                                     -1,
                                     1);
}

// Line Chart ==================================================================


LineChartWidget::LineChartWidget(ChartWidgetOptions const& opts)
    : GLPoweredChart(opts.chart),
      m_options(opts),
      m_from(std::make_unique<ChartLineData>(opts.experiment_info,
                                             opts.server_ids,
                                             opts.history_ms)) {}

LineChartWidget::~LineChartWidget() {}

void LineChartWidget::add(DataRef const& ref) { m_from->add(this, this, ref); }

ChartBounds LineChartWidget::get_bounds() const {
    float max_time = m_from->recent_time();
    float min_time = max_time - (m_options.history_ms / 1000);

    float var_min = m_from->var_min();
    float var_max = m_from->var_max();

    if (m_options.chart.use_value_min) {
        var_min = m_options.chart.value_min;
    }

    if (m_options.chart.use_value_max) {
        var_max = m_options.chart.value_max;
    }

    if (std::abs(var_min - var_max) < std::numeric_limits<float>::epsilon()) {
        var_max = var_min + 1;
    }

    return { min_time, max_time, var_min, var_max };
}

void LineChartWidget::paintGL() {

    update_projection(); // hmm
    glClear(GL_COLOR_BUFFER_BIT);

    m_program.bind();

    // setup uniforms

    glUniformMatrix4fv(m_mvp_location, 1, false, glm::value_ptr(m_projection));

    m_from->draw();


    m_program.release();

    check_gl_errors(Q_FUNC_INFO);
}

// Stack Widget ================================================================

StackChartWidget::StackChartWidget(ChartWidgetOptions const& opts)
    : GLPoweredChart(opts.chart),
      m_options(opts),
      m_from(std::make_unique<ChartStackData>(m_options.experiment_info,
                                              m_options.server_ids,
                                              opts.history_ms)) {}

StackChartWidget::~StackChartWidget() {}

ChartBounds StackChartWidget::get_bounds() const {
    float max_time = m_from->recent_time();
    float min_time = max_time - (m_options.history_ms / 1000);
    float var_min  = m_from->var_min();
    float var_max  = m_from->var_max();

    if (m_options.chart.use_value_min) {
        var_min = m_options.chart.value_min;
    }

    if (m_options.chart.use_value_max) {
        var_max = m_options.chart.value_max;
    }

    if (std::abs(var_min - var_max) < std::numeric_limits<float>::epsilon()) {
        var_max = var_min + 1;
    }

    return { min_time, max_time, var_min, var_max };
}

void StackChartWidget::add(DataRef const& ref) { m_from->add(this, this, ref); }

void StackChartWidget::paintGL() {

    update_projection();

    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_program.bind()) {
        qFatal("Unable to bind shaders!");
    }

    // setup uniforms

    glUniformMatrix4fv(m_mvp_location, 1, false, glm::value_ptr(m_projection));

    m_from->draw();

    m_program.release();

    check_gl_errors(Q_FUNC_INFO);
}

// Scope Widget ================================================================

ScopeChartWidget::ScopeChartWidget(ChartWidgetOptions const& options)
    : GLPoweredChart(options.chart),
      m_options(options),
      m_data(std::make_unique<ChartScopeData>(options.experiment_info,
                                              options.chart.variables,
                                              options.server_ids)) {}

ScopeChartWidget::~ScopeChartWidget() {}

ChartBounds ScopeChartWidget::get_bounds() const {
    return { m_data->min_time(),
             m_data->max_time(),
             m_data->var_min(),
             m_data->var_max() };
}

void ScopeChartWidget::paintGL() {
    update_projection(); // hmm
    // qDebug() << Q_FUNC_INFO;
    glClear(GL_COLOR_BUFFER_BIT);

    m_program.bind();

    // setup uniforms

    glUniformMatrix4fv(m_mvp_location, 1, false, glm::value_ptr(m_projection));

    m_data->draw();


    m_program.release();

    check_gl_errors(Q_FUNC_INFO);
}

void ScopeChartWidget::block_ready(DelayedVarBlock block) {
    m_data->add(this, this, block);
}

//==============================================================================

static QString const panel_sheet = R"(
QWidget {
    background: "%1";
    border-radius: 2px;
}
)";

static QString get_stylesheet(QColor tint = QColor()) {
    if (!tint.isValid()) {
        tint = QColor("#000a12");
    }

    return panel_sheet.arg(tint.name());
}

Panel::Panel(QWidget* p) : QWidget(p) { this->setStyleSheet(get_stylesheet()); }
Panel::~Panel() {}

void Panel::update() {}

//==============================================================================

template <class Ptr>
auto checked_deref(Ptr const& p) -> decltype(*p) const & {
    assert(!!p);
    return *p;
}

AlertChartWidget::AlertChartWidget(ChartWidgetOptions const& options,
                                   QWidget*                  p)
    : Panel(p), m_options(options) {

    QVBoxLayout* layout = new QVBoxLayout();

    FlowLayout* alert_flow_layout = new FlowLayout();

    ExperimentDefinition const& def = checked_deref(m_options.experiment_info);

    for (size_t gid : options.server_ids) {
        auto var_info = def.global_to_var_mapping[gid];

        auto* l = new QLabel(var_info->name);

        m_labels.push_back(l);

        alert_flow_layout->addWidget(l);
    }

    m_last_value.resize(options.server_ids.size(), 0.0f);

    qDebug() << "Created alert panel, watching" << options.server_ids.size()
             << "vars";

    QFont font;
    font.setPixelSize(18);

    QLabel* title = new QLabel(options.chart.title);
    title->setFont(font);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    layout->addWidget(title);

    layout->addLayout(alert_flow_layout);

    this->setLayout(layout);
}

AlertChartWidget::~AlertChartWidget() {}

void AlertChartWidget::add(DataRef const& ref) {
    for (size_t i = 0; i < m_options.server_ids.size(); i++) {
        size_t gid      = m_options.server_ids[i];
        m_last_value[i] = ref.get_var(gid);

        if (m_last_value[i] > .5f) {
            QLabel* l = m_labels[i];
            l->setStyleSheet("QWidget { color: red; }");
        }
    }
}
void AlertChartWidget::update() {}

//==============================================================================

constexpr int legend_width = 50;

ChartWidget::ChartWidget(ChartWidgetOptions const& options,
                         Session*                  session,
                         QWidget*                  p)
    : Panel(p) {

    if (options.chart.chart_tint.isValid()) {
        this->setStyleSheet(get_stylesheet(options.chart.chart_tint));
    }

    m_last.min_value = std::numeric_limits<float>::lowest();
    m_last.max_value = std::numeric_limits<float>::max();
    m_last.min_time  = 0;
    m_last.max_time  = 0;

    // create chart

    auto type = string_to_chart_type(options.chart.type);

    switch (type) {
    case ChartType::LINE: m_chart = new LineChartWidget(options); break;
    case ChartType::STACK: m_chart = new StackChartWidget(options); break;
    case ChartType::SCOPE: {
        ScopeChartWidget* lp = new ScopeChartWidget(options);
        m_chart              = lp;

        // get buffer that supports this widget

        auto ptr =
            get_common_frame(options.experiment_info, options.chart.variables);

        auto* buffer = session->buffer_for_frame(ptr->frame_id);

        assert(buffer);

        connect(buffer,
                &LineDelayBuffer::block_ready,
                lp,
                &ScopeChartWidget::block_ready);
        break;
    }
    default: qFatal("Chart type is not a GL chart type!");
    }

    // now lets build the surrounding widgets

    QGridLayout* core_layout = new QGridLayout();

    // legend
    if (!options.chart.hide_legend) {
        QListWidget* label_list = new QListWidget();

        label_list->setFocusPolicy(Qt::FocusPolicy::NoFocus);

        QFont f;
        f.setPointSize(9);

        label_list->setFont(f);

        label_list->setMinimumWidth(legend_width);
        label_list->setMaximumWidth(legend_width);
        label_list->setHorizontalScrollBarPolicy(
            Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        label_list->setVerticalScrollBarPolicy(
            Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

        ExperimentDefinition const& def =
            checked_deref(options.experiment_info);

        for (auto vid : options.server_ids) {
            auto   var_info  = def.global_to_var_mapping[vid];
            auto   raw_color = var_info->color;
            QColor var_color(raw_color[0], raw_color[1], raw_color[2]);

            auto text = var_info->name;
            label_list->addItem(text);
            auto* item = label_list->item(label_list->count() - 1);
            if (item) item->setForeground(var_color);
        }

        core_layout->addWidget(label_list, 1, 0);
    } else {
        core_layout->addWidget(new QWidget(), 1, 0);
    }

    auto fixed_font = make_fixed_font();

    // time elements
    {
        QHBoxLayout* time_layout = new QHBoxLayout();

        time_layout->setMargin(0);

        m_min_time_label = new QLabel("T-");
        m_max_time_label = new QLabel("T+");

        m_min_time_label->setFont(fixed_font);
        m_max_time_label->setFont(fixed_font);

        time_layout->addWidget(m_min_time_label);
        time_layout->addStretch(1);
        time_layout->addWidget(m_max_time_label);

        core_layout->addLayout(time_layout, 2, 1);
    }

    // bound elements
    {

        QWidget* bounds_widget = new QWidget();
        bounds_widget->setMinimumWidth(legend_width);
        bounds_widget->setMaximumWidth(legend_width);

        QVBoxLayout* bounds_layout = new QVBoxLayout();
        m_min_label                = new QLabel("Min");
        m_max_label                = new QLabel("Max");

        bounds_layout->setMargin(0);

        m_min_label->setFont(fixed_font);
        m_max_label->setFont(fixed_font);

        bounds_layout->addWidget(m_max_label);

        // user specified labels
        {
            QFont font;
            font.setPixelSize(9);

            QLabel* value_type = new VerticalLabel(options.chart.y_label);
            value_type->setFont(font);

            bounds_layout->addWidget(value_type, 1);
        }

        bounds_layout->addWidget(m_min_label);

        bounds_widget->setLayout(bounds_layout);

        core_layout->addWidget(bounds_widget, 1, 2);
    }

    QFont font;
    font.setPixelSize(16);

    QLabel* title = new QLabel(options.chart.title);
    title->setFont(font);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    title->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    core_layout->addWidget(title, 0, 1);
    core_layout->addWidget(m_chart, 1, 1);

    core_layout->setColumnStretch(0, 0);
    core_layout->setColumnStretch(1, 1);
    core_layout->setColumnStretch(2, 0);

    core_layout->setRowStretch(0, 0);
    core_layout->setRowStretch(1, 1);
    core_layout->setRowStretch(2, 0);

    core_layout->setMargin(3);
    core_layout->setSpacing(3);

    setLayout(core_layout);
}

ChartWidget::~ChartWidget() {}

void ChartWidget::add(DataRef const& ref) { m_chart->add(ref); }


// Qt's date and time stuff would like to work with real times. Thus, if we have
// a large sim time with no known start date, we can't use their API, we just
// get invalid times.
static QString time_to_string(float seconds) {
    int64_t minutes = seconds / 60;
    int64_t hours   = minutes / 60;
    QString s       = QString::number(fmod(seconds, 60), 'f', 4);

    return QString("%1:%2:%3")
        .arg(QString::number(hours), 2, '0')
        .arg(QString::number(minutes % 60), 2, '0')
        .arg(s);
}

static void update_fixed_width_label(QLabel* l,
                                     bool    as_time,
                                     float   value,
                                     float&  last_value) {
    if (std::abs(last_value - value) < std::numeric_limits<float>::epsilon())
        return;

    last_value = value;

    QString number;

    if (as_time) {
        number = time_to_string(value);
    } else {
        number = QString::number(value, 'g', 6);
    }

    l->setText(number);
}

void ChartWidget::update() {
    m_chart->update();

    auto b = m_chart->get_bounds();

    update_fixed_width_label(m_min_label, false, b.min_value, m_last.min_value);
    update_fixed_width_label(m_max_label, false, b.max_value, m_last.max_value);

    update_fixed_width_label(
        m_min_time_label, true, b.min_time, m_last.min_time);
    update_fixed_width_label(
        m_max_time_label, true, b.max_time, m_last.max_time);

    m_last = b;
}


Panel* make_panel(ChartWidgetOptions const& options,
                  Session*                  session,
                  QWidget*                  parent) {
    auto type = string_to_chart_type(options.chart.type);

    qDebug() << "Creating new" << options.chart.type << "chart";

    switch (type) {
    case ChartType::NONE: qFatal("Attempting to make an empty chart");
    case ChartType::LINE:
    case ChartType::STACK:
    case ChartType::SCOPE: return new ChartWidget(options, session, parent);
    case ChartType::ALERT: return new AlertChartWidget(options, parent);
    }

    Q_UNREACHABLE();
}
