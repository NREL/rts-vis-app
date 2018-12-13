#ifndef CHARTDATA_H
#define CHARTDATA_H

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

#include <array>
#include <chrono>
#include <memory>

struct ExperimentDefinition;
using ExperimentPtr = std::shared_ptr<ExperimentDefinition const>;

class QOpenGLFunctions_3_2_Core;
class QOpenGLWidget;
class LineChartWidget;
struct DelayedVarBlock;

///
/// \brief The Vertex struct models a GPU vertex.
///
struct Vertex {
    glm::vec2              position;
    std::array<uint8_t, 4> color;

    Vertex() = default;
    Vertex(float x, float y)
        : position(x, y), color({ { 255, 255, 255, 255 } }) {}

    Vertex(float x, float y, std::array<uint8_t, 4> c)
        : position(x, y), color(c) {}
};

///
/// \brief The LinePrimitive struct models a line index tuple
///
struct LinePrimitive {
    uint16_t a;
    uint16_t b;
};

///
/// \brief The TrianglePrimitive struct models a triangle index tuple
///
struct TrianglePrimitive {
    uint16_t a;
    uint16_t b;
    uint16_t c;
};

///
/// \brief The DataRef struct is a lightweight reference to a new frame of data.
///
struct DataRef {
    float* source = nullptr;
    size_t count  = 0; ///< Number of floats in this frame

    double server_time = 0; ///< Timestamp of these data

    size_t server_ms_delay = 0; ///< Sample rate

    float get_var(size_t var_id) const { return source[var_id]; }
};

///
/// \brief The BufferShard struct is a parent type to ease creation of GL
/// buffers
///
struct BufferShard {
    /// number of samples (verts) per line, or stack
    size_t        num_line_samples = 0;
    QOpenGLBuffer vertex_info;
    QOpenGLBuffer index_info;

    // because you cannot copy vaos
    std::unique_ptr<QOpenGLVertexArrayObject> vao;

    /// Global var ids for this shard
    std::vector<size_t> var_ids;

    /// Cached color for each var
    std::vector<std::array<uint8_t, 4>> var_colors;

    std::vector<Vertex> vertex_source;
    std::vector<Vertex> new_vertex_cache;

    BufferShard() = default;

    BufferShard(BufferShard const&) = delete;
    BufferShard& operator=(BufferShard const&) = delete;

    BufferShard(BufferShard&&) = default;
    BufferShard& operator=(BufferShard&&) = default;
};

//==============================================================================

///
/// \brief The ChartLineShard struct is a GL buffer representation for line
/// plots.
///
struct ChartLineShard : public BufferShard {
    std::vector<LinePrimitive> index_source;

    float var_max = std::numeric_limits<float>::lowest();
    float var_min = std::numeric_limits<float>::max();

    ///
    /// \brief Given a sample id, provide the indexing offset
    ///
    size_t frame_offset(size_t tid) const;

    ///
    /// \brief Given a shard-local varible index, and the sample index, provide
    /// the vertex offset in the vertex buffer.
    ///
    uint16_t vertex_index(size_t vid, size_t tid) const;

    void initialize(QOpenGLWidget*             context,
                    QOpenGLFunctions_3_2_Core* functions,
                    size_t                     num_samples);

    void add(DataRef const& ref, size_t cache_index, float time);

    void draw(size_t cache_index);
};


///
/// \brief The ChartLineData class holds all the GL state for a line chart
///
class ChartLineData {
    ExperimentPtr m_exp_data;
    bool          m_rebuild = true;

    ///
    /// \brief Global var ids
    ///
    std::vector<size_t> m_all_var_ids;

    std::vector<ChartLineShard> m_gpu_buffers;

    size_t m_history_ms         = 4000;
    size_t m_server_ms_delay    = 1000;
    size_t m_num_cached_samples = 1;

    size_t m_cache_index = 0; // where to place a new timestep

    size_t m_num_timesteps   = 1;
    double m_last_local_time = 0;


    void rebuild(QOpenGLWidget*             context,
                 QOpenGLFunctions_3_2_Core* functions,
                 DataRef const&             ref);

public:
    ChartLineData(ExperimentPtr              exp_data,
                  std::vector<size_t> const& var_ids,
                  size_t                     history_ms);

    void add(QOpenGLWidget*             context,
             QOpenGLFunctions_3_2_Core* functions,
             DataRef const&             ref);

    void draw();

    float min_time() const;
    float recent_time() const;
    float var_max();
    float var_min();
};

//==============================================================================

///
/// \brief The ChartStackShard struct is a GL buffer representation for stack
/// plots.
///
struct ChartStackShard : public BufferShard {
    std::vector<char> is_pos_var_ids; // check if this is still needed

    std::vector<TrianglePrimitive> index_source;

    size_t frame_offset(size_t tid) const;

    ///
    /// \brief Get the index of a vertex in the vertex buffer
    /// \param vid Variable index in this shard
    /// \param tid Sample, or time index
    /// \param upper Stacks have an upper and lower line.
    ///
    uint16_t vertex_index(size_t vid, size_t tid, bool upper) const;

    void initialize(QOpenGLWidget*             context,
                    QOpenGLFunctions_3_2_Core* functions,
                    size_t                     num_samples);

    void add(DataRef const& ref,
             size_t         cache_index,
             float          time,
             float&         pos_sum,
             float&         neg_sum);

    void draw(size_t cache_index);
};

class ChartStackData {
    ExperimentPtr m_exp_data;
    bool          m_rebuild = true;

    std::vector<size_t> m_all_var_ids;

    std::vector<ChartStackShard> m_gpu_buffers;

    size_t m_history_ms         = 4000;
    size_t m_server_ms_delay    = 1000;
    size_t m_num_cached_samples = 1;

    size_t   frame_offset(size_t tid) const;
    uint16_t vertex_index(size_t vid, size_t tid, bool upper) const;

    std::vector<Vertex>            m_vertex_source;
    std::vector<TrianglePrimitive> m_index_source;


    float               m_data_max = std::numeric_limits<float>::lowest();
    float               m_data_min = std::numeric_limits<float>::max();
    std::vector<float>  m_new_value_cache;
    std::vector<Vertex> m_new_vertex_cache;

    size_t m_cache_index = 0; // where to place a new timestep

    size_t m_num_timesteps   = 1;
    double m_last_local_time = 0;


    void rebuild(QOpenGLWidget*             context,
                 QOpenGLFunctions_3_2_Core* functions,
                 DataRef const&             ref);

public:
    ChartStackData(ExperimentPtr              exp_data,
                   std::vector<size_t> const& var_ids,
                   size_t                     history_ms);

    void add(QOpenGLWidget*             context,
             QOpenGLFunctions_3_2_Core* functions,
             DataRef const&             ref);

    void draw();

    float recent_time() const;
    float var_max() const;
    float var_min() const;
};

//==============================================================================

///
/// \brief The ChartScopeShard struct is a GL buffer representation for scope
/// plots.
///
struct ChartScopeShard : public BufferShard {
    std::vector<LinePrimitive> index_source;

    float var_max = std::numeric_limits<float>::lowest();
    float var_min = std::numeric_limits<float>::max();

    size_t   frame_offset(size_t tid) const;
    uint16_t vertex_index(size_t vid, size_t tid) const;

    void initialize(QOpenGLWidget*             context,
                    QOpenGLFunctions_3_2_Core* functions,
                    size_t                     num_samples);

    void add(DelayedVarBlock const& ref);

    void draw();
};

class ChartScopeData {
    ExperimentPtr m_exp_data;
    QStringList   m_var_uuids;
    bool          m_rebuild = true;

    float m_first_ts = 0;
    float m_last_ts  = 1;

    std::vector<size_t> m_all_var_ids;

    std::vector<ChartScopeShard> m_gpu_buffers;

    void rebuild(QOpenGLWidget*             context,
                 QOpenGLFunctions_3_2_Core* functions,
                 DelayedVarBlock const&     ref);

public:
    ChartScopeData(ExperimentPtr const&       e,
                   QStringList                var_uuids,
                   std::vector<size_t> const& var_ids);
    ~ChartScopeData();

    void add(QOpenGLWidget*             context,
             QOpenGLFunctions_3_2_Core* functions,
             DelayedVarBlock const&     ref);

    void draw();

    float min_time() const;
    float max_time() const;
    float var_max() const;
    float var_min() const;
};

#endif // CHARTDATA_H
