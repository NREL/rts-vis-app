#include "chartdata.h"

#include "chartwidget.h"
#include "comm/samplebuffer.h"

#include <qopenglfunctions_3_2_core.h>

// Verts should be a known size; 3 * floats
static_assert(sizeof(Vertex) == 3 * sizeof(float), "");

// GL attribute locations
constexpr int VERTEX_LOCATION = 0;
constexpr int COLOR_LOCATION  = 1;

/// \brief Check for gl errors and throw if any are found.
static void check_gl_errors(char const* context, unsigned int line) {
    auto err = glGetError();

    if (err != GL_NO_ERROR) {
        throw std::runtime_error(std::string("GL ERROR IN ") + context + " " +
                                 std::to_string(line) + " " +
                                 std::to_string(err));
    }
}

///
/// \brief Create a new GL buffer. A context MUST BE ACTIVE.
///
template <class T>
QOpenGLBuffer create_new_buffer(QOpenGLBuffer::Type   type,
                                std::vector<T> const& source) {
    QOpenGLBuffer buffer(type);
    buffer.create();
    buffer.bind();
    buffer.allocate(reinterpret_cast<char const*>(source.data()),
                    static_cast<int>(source.size() * sizeof(T)));

    buffer.release();

    check_gl_errors(Q_FUNC_INFO, __LINE__);

    return buffer;
}

static auto global_start_time = std::chrono::high_resolution_clock::now();

//==============================================================================


size_t ChartLineShard::frame_offset(size_t tid) const {
    return var_ids.size() * tid;
}

uint16_t ChartLineShard::vertex_index(size_t vid, size_t tid) const {
    assert(vid < var_ids.size());

    return static_cast<uint16_t>(frame_offset(tid) + vid);
}

void ChartLineShard::initialize(QOpenGLWidget*             context,
                                QOpenGLFunctions_3_2_Core* functions,
                                size_t                     num_samples) {

    num_line_samples = num_samples;
    // ordering is [time 0 samples * Nvar] [time 1 samples * Nvar] etc

    vertex_source.clear();
    index_source.clear();

    vertex_source.resize(var_ids.size() * num_samples);

    assert(vertex_source.size() < std::numeric_limits<uint16_t>::max());

    index_source.reserve(vertex_source.size());

    for (size_t frame_i = 0; frame_i < num_samples - 1; frame_i++) {

        for (size_t vi = 0; vi < var_ids.size(); vi++) {
            index_source.push_back(
                { vertex_index(vi, frame_i), vertex_index(vi, frame_i + 1) });

            assert(index_source.back().a < vertex_source.size());
            assert(index_source.back().b < vertex_source.size());
        }
    }

    for (size_t vi = 0; vi < var_ids.size(); vi++) {
        index_source.push_back(
            { vertex_index(vi, 0), vertex_index(vi, num_samples - 1) });

        assert(index_source.back().a < vertex_source.size());
        assert(index_source.back().b < vertex_source.size());
    }


    context->makeCurrent();

    vertex_info = create_new_buffer(QOpenGLBuffer::VertexBuffer, vertex_source);
    index_info  = create_new_buffer(QOpenGLBuffer::IndexBuffer, index_source);


    assert(context->context()->isValid());

    vao = std::make_unique<QOpenGLVertexArrayObject>();
    vao->create();
    vao->bind();
    vertex_info.bind();
    index_info.bind();
    functions->glVertexAttribPointer(VERTEX_LOCATION,
                                     2,
                                     GL_FLOAT,
                                     GL_FALSE,
                                     sizeof(Vertex),
                                     (void*)offsetof(Vertex, position));
    functions->glEnableVertexAttribArray(VERTEX_LOCATION);

    functions->glVertexAttribPointer(COLOR_LOCATION,
                                     3,
                                     GL_UNSIGNED_BYTE,
                                     GL_TRUE,
                                     sizeof(Vertex),
                                     (void*)offsetof(Vertex, color));
    functions->glEnableVertexAttribArray(COLOR_LOCATION);


    // note the order here, make sure we keep the index buffer bound
    vao->release();

    index_info.release();
    vertex_info.release();

    // context->doneCurrent();
}

void ChartLineShard::add(DataRef const& ref, size_t cache_index, float time) {
    size_t num_vars = var_ids.size();

    new_vertex_cache.resize(num_vars);

    for (size_t i = 0; i < num_vars; i++) {
        size_t vid       = var_ids[i];
        float  var_value = ref.get_var(vid);

        var_max = std::max(var_max, var_value);
        var_min = std::min(var_min, var_value);

        auto id1 = vertex_index(i, 0);

        new_vertex_cache[id1] =
            Vertex(static_cast<float>(time), var_value, var_colors[i]);
    }

    // now upload

    size_t byte_offset = frame_offset(cache_index) * sizeof(Vertex);

    vertex_info.bind();

    vertex_info.write(
        static_cast<int>(byte_offset),
        new_vertex_cache.data(),
        static_cast<int>(new_vertex_cache.size() * sizeof(Vertex)));

    vertex_info.release();

    std::copy(new_vertex_cache.begin(),
              new_vertex_cache.end(),
              vertex_source.begin() + frame_offset(cache_index));
}

static void issue_draw_lines(int start_line, int line_count) {
    Q_ASSERT(start_line >= 0);
    Q_ASSERT(line_count > 0);
    glDrawElements(GL_LINES,
                   line_count * 2,
                   GL_UNSIGNED_SHORT,
                   (void*)(start_line * sizeof(LinePrimitive)));
}

void ChartLineShard::draw(size_t cache_index) {
    assert(!!vao);

    vao->bind();

    size_t num_vars = var_ids.size();

    auto blocks_to_offset = [=](size_t i) { return num_vars * i; };

    size_t line_start;
    size_t line_count;

    // for each var
    // we have to knock out the one at the pointer, and the one behind,
    // since they share verts

    // cases, n = 4
    // 0 0 x x, counter == n-1
    // x 0 0 x, counter == 0
    // x x 0 0, counter == 1
    // 0 x x 0, else
    //     ^

    if (cache_index == 0) {
        // one draw, in the middle.
        line_start = blocks_to_offset(1);
        line_count = blocks_to_offset(num_line_samples - 2);

        issue_draw_lines(line_start, line_count);
    } else if (cache_index == 1) {
        line_start = blocks_to_offset(2);
        line_count = blocks_to_offset(num_line_samples - 2);

        issue_draw_lines(line_start, line_count);

    } else if (cache_index == num_line_samples - 1) {
        line_start = blocks_to_offset(0);
        line_count = blocks_to_offset(num_line_samples - 2);

        issue_draw_lines(line_start, line_count);
    } else {
        // two draws
        line_start = blocks_to_offset(0);
        line_count = blocks_to_offset(cache_index - 1);

        issue_draw_lines(line_start, line_count);

        line_start = blocks_to_offset(cache_index + 1);
        line_count = blocks_to_offset(num_line_samples - (cache_index + 1));

        issue_draw_lines(line_start, line_count);
    }


    vao->release();

    check_gl_errors(Q_FUNC_INFO, __LINE__);
}

ChartLineData::ChartLineData(ExperimentPtr              exp_data,
                             std::vector<size_t> const& var_ids,
                             size_t                     history_ms)
    : m_exp_data(exp_data), m_all_var_ids(var_ids), m_history_ms(history_ms) {}


void ChartLineData::rebuild(QOpenGLWidget*             context,
                            QOpenGLFunctions_3_2_Core* functions,
                            DataRef const&             ref) {
    m_num_cached_samples = (m_history_ms / ref.server_ms_delay) * 1.5;
    m_server_ms_delay    = ref.server_ms_delay;

    qDebug() << "Rebuilding VBO" << m_num_cached_samples << "samples needed";

    // so we need to split our lines across multiple shards based on how many
    // verts we are going to need.

    // how many lines can we fit in one shard?
    size_t lines_per_shard = std::numeric_limits<uint16_t>::max() /
                             m_num_cached_samples; // int math intended

    qDebug() << "lines per shard" << lines_per_shard << "with"
             << m_all_var_ids.size() << "lines";

    size_t needed_shards =
        std::ceil(m_all_var_ids.size() / static_cast<float>(lines_per_shard));

    qDebug() << "Building" << needed_shards << "shards";

    assert(needed_shards > 0);


    // now lets split our vars up to each shard

    m_gpu_buffers.resize(needed_shards);

    for (size_t vid_iter = 0; vid_iter < m_all_var_ids.size(); vid_iter++) {
        size_t shard_num = vid_iter / lines_per_shard; // int math

        auto& shard = m_gpu_buffers.at(shard_num);

        auto global_vid = m_all_var_ids[vid_iter];

        shard.var_ids.push_back(global_vid);
        shard.var_colors.push_back(
            m_exp_data->global_to_var_mapping[global_vid]->color);
    }

    // now that they have all the var ids, lets init each one

    for (auto& shard : m_gpu_buffers) {
        shard.initialize(context, functions, m_num_cached_samples);
    }


    check_gl_errors(Q_FUNC_INFO, __LINE__);

    m_rebuild = false;
}

void ChartLineData::add(QOpenGLWidget*             context,
                        QOpenGLFunctions_3_2_Core* functions,
                        DataRef const&             ref) {

    if (m_rebuild or m_server_ms_delay != ref.server_ms_delay)
        rebuild(context, functions, ref);

    context->makeCurrent();

    m_last_local_time = ref.server_time;

    // install new samples at index
    // note that these are raw samples. we need to turn them into verts

    for (auto& shard : m_gpu_buffers) {
        shard.add(ref, m_cache_index, m_last_local_time);
    }


    m_cache_index = (m_cache_index + 1) % m_num_cached_samples;
}

void ChartLineData::draw() {

    for (auto& shard : m_gpu_buffers) {
        shard.draw(m_cache_index);
    }

    check_gl_errors(Q_FUNC_INFO, __LINE__);
}

float ChartLineData::recent_time() const { return m_last_local_time; }

float ChartLineData::var_max() {
    constexpr float bad_val = .0001f;
    if (m_gpu_buffers.empty()) return bad_val;

    auto iter = std::max_element(
        m_gpu_buffers.begin(),
        m_gpu_buffers.end(),
        [](auto const& a, auto const& b) { return a.var_max < b.var_max; });

    if (iter == m_gpu_buffers.end()) return bad_val;

    return iter->var_max;
}
float ChartLineData::var_min() {
    constexpr float bad_val = .0001f;
    if (m_gpu_buffers.empty()) return bad_val;

    auto iter = std::min_element(
        m_gpu_buffers.begin(),
        m_gpu_buffers.end(),
        [](auto const& a, auto const& b) { return a.var_min < b.var_min; });

    if (iter == m_gpu_buffers.end()) return bad_val;

    return iter->var_min;
}

//==============================================================================

size_t ChartStackShard::frame_offset(size_t tid) const {
    return var_ids.size() * 2 * tid;
}

uint16_t
ChartStackShard::vertex_index(size_t vid, size_t tid, bool upper) const {
    assert(vid < var_ids.size());
    return frame_offset(tid) + (2 * vid + upper);
}


void ChartStackShard::initialize(QOpenGLWidget*             context,
                                 QOpenGLFunctions_3_2_Core* functions,
                                 size_t                     num_samples) {
    qDebug() << Q_FUNC_INFO;

    num_line_samples = num_samples;

    // we are going to build a huge vbo
    // with TWO verticies per sample!

    // ordering is [ v0t0 v0t0 v1t0 v1t0 , v0t1 v0t1 v1t1 v1t1 ]
    // this is to support different colors

    // so
    // time offset = num_vars * 2 * time_index;
    // to build index buffers
    // vid = 2 * vid + upper?

    vertex_source.clear();
    index_source.clear();

    size_t num_vars = var_ids.size();

    vertex_source.resize(num_vars * num_line_samples * 2);

    Q_ASSERT(vertex_source.size() < std::numeric_limits<uint16_t>::max());

    // each stack line is num_samples * 2 triangles, plus 2 for wrapraround

    index_source.reserve(num_vars * (num_line_samples * 2 + 2));


    for (size_t frame_i = 0; frame_i < num_line_samples - 1; frame_i++) {

        for (size_t vi = 0; vi < num_vars; vi++) {

            index_source.push_back({ vertex_index(vi, frame_i, false),
                                     vertex_index(vi, frame_i + 1, true),
                                     vertex_index(vi, frame_i, true) });

            assert(index_source.back().a < vertex_source.size());
            assert(index_source.back().b < vertex_source.size());
            assert(index_source.back().c < vertex_source.size());

            index_source.push_back({ vertex_index(vi, frame_i, false),
                                     vertex_index(vi, frame_i + 1, false),
                                     vertex_index(vi, frame_i + 1, true) });

            assert(index_source.back().a < vertex_source.size());
            assert(index_source.back().b < vertex_source.size());
            assert(index_source.back().c < vertex_source.size());
        }
    }

    for (size_t vi = 0; vi < num_vars; vi++) {

        index_source.push_back({ vertex_index(vi, 0, false),
                                 vertex_index(vi, num_line_samples - 1, true),
                                 vertex_index(vi, 0, true) });

        index_source.push_back(
            { vertex_index(vi, 0, false),
              vertex_index(vi, num_line_samples - 1, false),
              vertex_index(vi, num_line_samples - 1, true) });
    }


    context->makeCurrent();

    assert(context->isValid());

    vertex_info = create_new_buffer(QOpenGLBuffer::VertexBuffer, vertex_source);
    index_info  = create_new_buffer(QOpenGLBuffer::IndexBuffer, index_source);

    vao = std::make_unique<QOpenGLVertexArrayObject>();

    vao->create();
    vao->bind();
    vertex_info.bind();
    index_info.bind();
    functions->glVertexAttribPointer(VERTEX_LOCATION,
                                     2,
                                     GL_FLOAT,
                                     GL_FALSE,
                                     sizeof(Vertex),
                                     (void*)offsetof(Vertex, position));
    functions->glEnableVertexAttribArray(VERTEX_LOCATION);

    functions->glVertexAttribPointer(COLOR_LOCATION,
                                     3,
                                     GL_UNSIGNED_BYTE,
                                     GL_TRUE,
                                     sizeof(Vertex),
                                     (void*)offsetof(Vertex, color));
    functions->glEnableVertexAttribArray(COLOR_LOCATION);


    // note the order here, make sure we keep the index buffer bound
    vao->release();

    index_info.release();
    vertex_info.release();

    context->doneCurrent();
}

void ChartStackShard::add(DataRef const& ref,
                          size_t         cache_index,
                          float          time,
                          float&         pos_sum,
                          float&         neg_sum) {

    // so, some vars can go positive and negative, between frames.
    // we will sort them on positive and negative, and also by global var id
    // to get some measure of consistency.

    size_t num_vars = var_ids.size();

    new_vertex_cache.resize(num_vars * 2);

    // figure out whos a positive and whos a negative
    {
        if (is_pos_var_ids.size() != var_ids.size()) {
            is_pos_var_ids.resize(var_ids.size());
        }

        for (size_t i = 0; i < num_vars; i++) {
            size_t vid   = var_ids[i];
            float  value = ref.get_var(vid);

            is_pos_var_ids[i] = value > 0;
        }
    }

    // ok now we have to build verts

    for (size_t iter = 0; iter < var_ids.size(); iter++) {
        bool   is_pos    = is_pos_var_ids[iter];
        size_t vid       = var_ids[iter];
        float  var_value = ref.get_var(vid);

        if (is_pos) {
            var_value = std::abs(var_value);
        } else {
            var_value = (var_value < 0 ? var_value : -var_value);
        }

        auto id1 = vertex_index(iter, 0, false);
        auto id2 = vertex_index(iter, 0, true);

        auto color = var_colors[iter];

        assert(id1 < new_vertex_cache.size());
        assert(id2 < new_vertex_cache.size());

        float last_value = is_pos ? pos_sum : neg_sum;

        float next_value = last_value + var_value;

        new_vertex_cache[id1] = Vertex(time, last_value, color);
        new_vertex_cache[id2] = Vertex(time, next_value, color);

        if (is_pos) {
            pos_sum = next_value;
        } else {
            neg_sum = next_value;
        }
    }

    // now upload

    size_t byte_offset = frame_offset(cache_index) * sizeof(Vertex);

    vertex_info.bind();

    vertex_info.write(
        static_cast<int>(byte_offset),
        new_vertex_cache.data(),
        static_cast<int>(new_vertex_cache.size() * sizeof(Vertex)));

    vertex_info.release();

    std::copy(new_vertex_cache.begin(),
              new_vertex_cache.end(),
              vertex_source.begin() + frame_offset(cache_index));
}


static void issue_draw_quads_from_tris(int start_quad_offset, int quad_count) {
    // qDebug() << Q_FUNC_INFO << start_quad_offset << quad_count
    //         << CACHED_SAMPLES_PER_VAR;
    assert(start_quad_offset >= 0);
    assert(quad_count > 0);
    glDrawElements(
        GL_TRIANGLES,
        quad_count * 3 * 2,
        GL_UNSIGNED_SHORT,
        (void*)(start_quad_offset * (sizeof(TrianglePrimitive) * 2)));
}

void ChartStackShard::draw(size_t cache_index) {
    assert(!!vao);

    vao->bind();

    // we have to knock out the one at the pointer, and the one behind,
    // since they share verts

    // here the ringbuffer is global. so we have blocks of
    // [ vars at time 1 ] [ vars at time 2 ]

    size_t num_vars = var_ids.size();

    auto blocks_to_offset = [=](size_t i) { return num_vars * i; };

    size_t quad_start;
    size_t quad_count;

    // cases, n = 4
    // 0 0 x x, counter == n-1
    // x 0 0 x, counter == 0
    // x x 0 0, counter == 1
    // 0 x x 0, else
    //

    if (cache_index == 0) {
        // one draw, in the middle.
        quad_start = blocks_to_offset(1);
        quad_count = blocks_to_offset(num_line_samples - 2);
        issue_draw_quads_from_tris(quad_start, quad_count);
    } else if (cache_index == 1) {
        // one draw
        quad_start = blocks_to_offset(2);
        quad_count = blocks_to_offset(num_line_samples - 2);
        issue_draw_quads_from_tris(quad_start, quad_count);
    } else if (cache_index == num_line_samples - 1) {
        quad_start = blocks_to_offset(0);
        quad_count = blocks_to_offset(num_line_samples - 2);
        issue_draw_quads_from_tris(quad_start, quad_count);
    } else {
        // two draws
        quad_start = blocks_to_offset(0);
        quad_count = blocks_to_offset(cache_index - 1);
        issue_draw_quads_from_tris(quad_start, quad_count);

        quad_start = blocks_to_offset(cache_index + 1);
        quad_count = blocks_to_offset(num_line_samples - (cache_index + 1));
        issue_draw_quads_from_tris(quad_start, quad_count);
    }
    vao->release();

    check_gl_errors(Q_FUNC_INFO, __LINE__);
}


ChartStackData::ChartStackData(ExperimentPtr              exp_data,
                               std::vector<size_t> const& var_ids,
                               size_t                     history_ms)
    : m_exp_data(exp_data), m_all_var_ids(var_ids), m_history_ms(history_ms) {
    assert(var_ids.size() >= 2);
}

void ChartStackData::rebuild(QOpenGLWidget*             context,
                             QOpenGLFunctions_3_2_Core* functions,
                             DataRef const&             ref) {
    m_num_cached_samples = (m_history_ms / ref.server_ms_delay) * 1.5;
    m_server_ms_delay    = ref.server_ms_delay;

    qDebug() << "Rebuilding STACK VBO" << m_num_cached_samples
             << "samples needed";


    // how many lines can we fit in one shard?
    size_t lines_per_shard =
        std::numeric_limits<uint16_t>::max() / (m_num_cached_samples * 2);

    qDebug() << "lines per shard" << lines_per_shard;

    size_t needed_shards =
        std::ceil(m_all_var_ids.size() / static_cast<float>(lines_per_shard));

    qDebug() << "Building" << needed_shards << "shards";

    assert(needed_shards > 0);


    m_gpu_buffers.resize(needed_shards);

    for (size_t vid_iter = 0; vid_iter < m_all_var_ids.size(); vid_iter++) {
        size_t shard_num = vid_iter / lines_per_shard; // int math

        auto& shard = m_gpu_buffers.at(shard_num);

        auto global_vid = m_all_var_ids[vid_iter];

        shard.var_ids.push_back(global_vid);
        shard.var_colors.push_back(
            m_exp_data->global_to_var_mapping[global_vid]->color);
    }

    // now that they have all the var ids, lets init each one

    for (auto& shard : m_gpu_buffers) {
        shard.initialize(context, functions, m_num_cached_samples);
    }


    check_gl_errors(Q_FUNC_INFO, __LINE__);

    m_rebuild = false;
}


void ChartStackData::add(QOpenGLWidget*             context,
                         QOpenGLFunctions_3_2_Core* functions,
                         DataRef const&             ref) {
    if (m_rebuild or m_server_ms_delay != ref.server_ms_delay)
        rebuild(context, functions, ref);

    context->makeCurrent();

    m_last_local_time = ref.server_time;

    // install new samples at index
    // note that these are raw samples. we need to turn them into verts

    float pos_sum = 0;
    float neg_sum = 0;

    for (auto& shard : m_gpu_buffers) {
        shard.add(ref, m_cache_index, m_last_local_time, pos_sum, neg_sum);
        assert(pos_sum >= 0);
        assert(neg_sum <= 0);
    }

    m_data_max = std::max(m_data_max, pos_sum);
    m_data_min = std::min(m_data_min, neg_sum);

    // move next

    m_cache_index = (m_cache_index + 1) % m_num_cached_samples;
}


void ChartStackData::draw() {
    glDisable(GL_CULL_FACE);

    for (auto& shard : m_gpu_buffers) {
        shard.draw(m_cache_index);
    }

    check_gl_errors(Q_FUNC_INFO, __LINE__);
}

float ChartStackData::recent_time() const { return m_last_local_time; }

float ChartStackData::var_max() const { return m_data_max; }

float ChartStackData::var_min() const { return m_data_min; }

//==============================================================================


size_t ChartScopeShard::frame_offset(size_t tid) const {
    return var_ids.size() * tid;
}

uint16_t ChartScopeShard::vertex_index(size_t vid, size_t tid) const {
    assert(vid < var_ids.size());
    return frame_offset(tid) + vid;
}

void ChartScopeShard::initialize(QOpenGLWidget*             context,
                                 QOpenGLFunctions_3_2_Core* functions,
                                 size_t                     num_samples) {

    num_line_samples = num_samples;
    // ordering is [time 0 samples * Nvar] [time 1 samples * Nvar] etc

    vertex_source.clear();
    index_source.clear();

    vertex_source.resize(var_ids.size() * num_samples);

    assert(vertex_source.size() < std::numeric_limits<uint16_t>::max());

    index_source.reserve(vertex_source.size());

    for (size_t frame_i = 0; frame_i < num_samples - 1; frame_i++) {

        for (size_t vi = 0; vi < var_ids.size(); vi++) {
            index_source.push_back(
                { vertex_index(vi, frame_i), vertex_index(vi, frame_i + 1) });

            // qDebug() << index_source.back().a << vertex_source.size();
            assert(index_source.back().a < vertex_source.size());
            assert(index_source.back().b < vertex_source.size());
        }
    }

    for (size_t vi = 0; vi < var_ids.size(); vi++) {
        index_source.push_back(
            { vertex_index(vi, 0), vertex_index(vi, num_samples - 1) });

        assert(index_source.back().a < vertex_source.size());
        assert(index_source.back().b < vertex_source.size());
    }


    context->makeCurrent();

    vertex_info = create_new_buffer(QOpenGLBuffer::VertexBuffer, vertex_source);
    index_info  = create_new_buffer(QOpenGLBuffer::IndexBuffer, index_source);


    assert(context->context()->isValid());

    vao = std::make_unique<QOpenGLVertexArrayObject>();
    vao->create();
    vao->bind();
    vertex_info.bind();
    index_info.bind();
    functions->glVertexAttribPointer(VERTEX_LOCATION,
                                     2,
                                     GL_FLOAT,
                                     GL_FALSE,
                                     sizeof(Vertex),
                                     (void*)offsetof(Vertex, position));
    functions->glEnableVertexAttribArray(VERTEX_LOCATION);

    functions->glVertexAttribPointer(COLOR_LOCATION,
                                     3,
                                     GL_UNSIGNED_BYTE,
                                     GL_TRUE,
                                     sizeof(Vertex),
                                     (void*)offsetof(Vertex, color));
    functions->glEnableVertexAttribArray(COLOR_LOCATION);


    // note the order here, make sure we keep the index buffer bound
    vao->release();

    index_info.release();
    vertex_info.release();
}

void ChartScopeShard::add(DelayedVarBlock const& ref) {
    size_t num_vars = var_ids.size();

    new_vertex_cache.resize(ref.num_samples * num_vars);

    Q_ASSERT(ref.num_samples == 1000);

    for (size_t i = 0; i < num_vars; i++) {

        auto vid   = var_ids[i];
        auto color = var_colors[i];

        for (size_t s_i = 0; s_i < ref.num_samples; s_i++) {

            size_t index = ref.index(vid, s_i);

            float var_value = ref.variables_store[index];

            var_max = std::max(var_max, var_value);
            var_min = std::min(var_min, var_value);

            float time_value = ref.get_var(1, s_i);

            auto id1 = vertex_index(i, s_i);

            Q_ASSERT(id1 < new_vertex_cache.size());

            new_vertex_cache[id1] = Vertex(time_value, var_value, color);
        }
    }

    // now upload

    // we are uploading the whole damn thing.

    size_t byte_offset = 0;

    vertex_info.bind();

    vertex_info.write(
        static_cast<int>(byte_offset),
        new_vertex_cache.data(),
        static_cast<int>(new_vertex_cache.size() * sizeof(Vertex)));

    vertex_info.release();

    assert(new_vertex_cache.size() == vertex_source.size());

    std::copy(new_vertex_cache.begin(),
              new_vertex_cache.end(),
              vertex_source.begin());
}


void ChartScopeShard::draw() {
    assert(!!vao);

    vao->bind();

    size_t num_vars = var_ids.size();

    auto blocks_to_offset = [=](size_t i) { return num_vars * i; };

    size_t line_start = blocks_to_offset(0);
    size_t line_count = blocks_to_offset(num_line_samples - 1);

    issue_draw_lines(line_start, line_count);

    vao->release();
}

void ChartScopeData::rebuild(QOpenGLWidget*             context,
                             QOpenGLFunctions_3_2_Core* functions,
                             DelayedVarBlock const& /*ref*/) {
    size_t num_cached_samples = 1000; // TODO: fix this hardcode

    qDebug() << "Rebuilding VBO" << num_cached_samples << "samples needed";

    // so we need to split our lines across multiple shards based on how many
    // verts we are going to need. hello, integer math.

    // how many lines can we fit in one shard?
    size_t lines_per_shard =
        std::numeric_limits<uint16_t>::max() / num_cached_samples;

    qDebug() << "lines per shard" << lines_per_shard << "with"
             << m_all_var_ids.size() << "lines";

    size_t needed_shards =
        std::ceil(m_all_var_ids.size() / static_cast<float>(lines_per_shard));

    qDebug() << "Building" << needed_shards << "shards";

    assert(needed_shards > 0);


    // now lets split our vars up to each shard

    m_gpu_buffers.resize(needed_shards);

    auto frame_ptr = get_common_frame(m_exp_data, m_var_uuids);

    qDebug() << "New scope data targets frame: " << frame_ptr->frame_id;

    auto get_local_id = [&frame_ptr](size_t global_id) {
        for (auto const& var_info : frame_ptr->variables) {
            if (var_info->global_index == global_id) return var_info->index;
        }
        qFatal("Unable to map global to local var ids.");
    };

    for (size_t vid_iter = 0; vid_iter < m_all_var_ids.size(); vid_iter++) {
        size_t shard_num = vid_iter / lines_per_shard; // int math

        auto& shard = m_gpu_buffers.at(shard_num);

        // now we need to remap the global var id to the frame local id,
        // as we only support one frame per scope

        // TODO: generalize, and make less dumb

        auto global_vid = m_all_var_ids[vid_iter];

        size_t frame_index = get_local_id(global_vid);

        shard.var_ids.push_back(frame_index);
        shard.var_colors.push_back(
            m_exp_data->global_to_var_mapping[global_vid]->color);
    }

    // now that they have all the var ids, lets init each one

    for (auto& shard : m_gpu_buffers) {
        shard.initialize(context, functions, num_cached_samples);
    }


    check_gl_errors(Q_FUNC_INFO, __LINE__);

    m_rebuild = false;
}

ChartScopeData::ChartScopeData(ExperimentPtr const&       e,
                               QStringList                var_uuids,
                               std::vector<size_t> const& var_ids)
    : m_exp_data(e), m_var_uuids(var_uuids), m_all_var_ids(var_ids) {}

ChartScopeData::~ChartScopeData() = default;

void ChartScopeData::add(QOpenGLWidget*             context,
                         QOpenGLFunctions_3_2_Core* functions,
                         DelayedVarBlock const&     ref) {
    if (m_rebuild) rebuild(context, functions, ref);


    context->makeCurrent();

    // qDebug() << "Adding new state vector";

    // TODO: remove these hardcodes
    m_first_ts = ref.get_var(1, 0);
    m_last_ts  = ref.get_var(1, 999); // 1 is the time var, 999 is last sample

    for (auto& shard : m_gpu_buffers) {
        shard.add(ref);
    }
}

void ChartScopeData::draw() {

    for (auto& shard : m_gpu_buffers) {
        shard.draw();
    }

    check_gl_errors(Q_FUNC_INFO, __LINE__);
}

float ChartScopeData::min_time() const { return m_first_ts; }
float ChartScopeData::max_time() const { return m_last_ts; }
float ChartScopeData::var_max() const {
    constexpr float bad_val = .0001f;
    if (m_gpu_buffers.empty()) return bad_val;

    auto iter = std::max_element(
        m_gpu_buffers.begin(),
        m_gpu_buffers.end(),
        [](auto const& a, auto const& b) { return a.var_max < b.var_max; });

    if (iter == m_gpu_buffers.end()) return bad_val;

    return iter->var_max;
}
float ChartScopeData::var_min() const {
    constexpr float bad_val = .0001f;
    if (m_gpu_buffers.empty()) return bad_val;

    auto iter = std::min_element(
        m_gpu_buffers.begin(),
        m_gpu_buffers.end(),
        [](auto const& a, auto const& b) { return a.var_min < b.var_min; });

    if (iter == m_gpu_buffers.end()) return bad_val;

    return iter->var_min;
}
