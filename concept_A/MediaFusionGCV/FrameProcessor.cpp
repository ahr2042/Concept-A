#include "FrameProcessor.h"
#include "Algorithms.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

FrameProcessor::FrameProcessor()
{
    gst_video_info_init(&m_info);

    filterElement = gst_element_factory_make("capsfilter", "fp-bgr");
    if (!filterElement)
        return;

    // Force BGR so each buffer maps directly to a cv::Mat CV_8UC3; the upstream
    // videoconvert negotiates to satisfy this.
    GstCaps* bgr = gst_caps_new_simple("video/x-raw",
                                       "format", G_TYPE_STRING, "BGR", NULL);
    g_object_set(filterElement, "caps", bgr, NULL);
    gst_caps_unref(bgr);

    // In-place processing happens as buffers leave the capsfilter.
    GstPad* src = gst_element_get_static_pad(filterElement, "src");
    m_probeId = gst_pad_add_probe(src, GST_PAD_PROBE_TYPE_BUFFER,
                                  &FrameProcessor::onBuffer, this, nullptr);
    gst_object_unref(src);
}

FrameProcessor::~FrameProcessor()
{
    // The probe is torn down with the pad when the element is freed. Mirror the
    // other elements' ownership: drop our ref (the bin holds its own).
    if (filterElement) { gst_object_unref(filterElement); filterElement = nullptr; }
}

bool FrameProcessor::setAlgorithms(const std::vector<std::string>& names)
{
    // Validate the whole request before building anything: a chain is
    // all-or-nothing, so a typo cannot half-apply and cannot pass silently.
    for (const auto& n : names)
        if (!findAlgorithm(n))
            return false;

    DetectorConfig                         cfg = detectorConfig();
    AccelBackend                           accel;
    std::map<std::string, AlgorithmParams> params;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        accel  = m_accel;
        params = m_algoParams;
    }

    std::vector<std::unique_ptr<Algorithm>> built;
    built.reserve(names.size());
    for (const auto& n : names) {
        auto a = makeAlgorithm(n);
        // Pick the resolved backend before loading so a detector configures the
        // right engine on the spot; plain filters ignore it.
        a->setAccel(accel);
        // Replay the operator's tuning onto the fresh stage, which would
        // otherwise come up on its compiled-in defaults.
        const auto it = params.find(n);
        if (it != params.end() && !it->second.empty())
            a->setParams(it->second);
        // A detector joining the chain inherits the model chosen earlier;
        // loading happens here, off the streaming thread.
        if (auto* det = dynamic_cast<DetectorAlgorithm*>(a.get()))
            det->configure(cfg);
        built.push_back(std::move(a));
    }

    std::lock_guard<std::mutex> lk(m_mutex);
    m_algos = std::move(built);
    return true;
}

bool FrameProcessor::setAlgorithmParams(const std::string& algo, const AlgorithmParams& values)
{
    if (!findAlgorithm(algo))
        return false;

    // ::-qualified: the member of the same name returns current values, this is
    // the registry's schema.
    const std::vector<AlgorithmParam> schema = ::algorithmParams(algo);

    // Clamp against the schema here, so every stage can trust what it receives
    // and no client has to reimplement the ranges.
    AlgorithmParams clamped;
    for (const auto& [key, value] : values) {
        const auto p = std::find_if(schema.begin(), schema.end(),
                                    [&key](const AlgorithmParam& s) { return s.key == key; });
        if (p == schema.end())
            return false;
        clamped[key] = p->clamp(value);
    }

    std::lock_guard<std::mutex> lk(m_mutex);
    for (const auto& [key, value] : clamped)
        m_algoParams[algo][key] = value;

    // Push to a matching stage already streaming. The streaming thread waits on
    // this mutex meanwhile, which is what makes a live tweak safe.
    for (const auto& a : m_algos)
        if (algo == a->name())
            a->setParams(clamped);
    return true;
}

AlgorithmParams FrameProcessor::algorithmParams(const std::string& algo) const
{
    // Schema defaults with the operator's overrides on top, so a caller always
    // sees a complete, current value set.
    AlgorithmParams out = defaultParams(::algorithmParams(algo));

    std::lock_guard<std::mutex> lk(m_mutex);
    const auto it = m_algoParams.find(algo);
    if (it != m_algoParams.end())
        for (const auto& [key, value] : it->second)
            out[key] = value;
    return out;
}

void FrameProcessor::setAccel(AccelBackend backend)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_accel = backend;
    // Re-target any stage already in the chain. For a detector this means the
    // next configure()/reload runs on the new engine.
    for (const auto& a : m_algos)
        a->setAccel(backend);
}

bool FrameProcessor::setDetectorConfig(const DetectorConfig& cfg)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_detectorConfig = cfg;

    // Loading a graph takes a moment and the streaming thread waits on this
    // mutex meanwhile — a model swap costs a frame or two, which is the
    // expected behaviour for changing models mid-stream.
    bool ok = true;
    for (const auto& a : m_algos)
        if (auto* det = dynamic_cast<DetectorAlgorithm*>(a.get()))
            ok = det->configure(cfg) && ok;
    return ok;
}

DetectorConfig FrameProcessor::detectorConfig() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_detectorConfig;
}

std::vector<InferenceStats> FrameProcessor::stageStats() const
{
    std::lock_guard<std::mutex> lk(m_mutex);

    std::vector<InferenceStats> out;
    for (const auto& a : m_algos) {
        InferenceStats st;
        // snapshotStats() clears its output first, so the name goes on after.
        if (a->snapshotStats(st)) {
            st.stage = a->name();
            out.push_back(std::move(st));
        }
    }
    return out;
}

std::vector<std::string> FrameProcessor::activeAlgorithms() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<std::string> out;
    out.reserve(m_algos.size());
    for (const auto& a : m_algos) out.emplace_back(a->name());
    return out;
}

GstPadProbeReturn FrameProcessor::onBuffer(GstPad* pad, GstPadProbeInfo* info, gpointer user)
{
    return static_cast<FrameProcessor*>(user)->processBuffer(pad, info);
}

GstPadProbeReturn FrameProcessor::processBuffer(GstPad* pad, GstPadProbeInfo* info)
{
    GstBuffer* buf = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buf)
        return GST_PAD_PROBE_OK;

    // Learn the (fixed BGR) geometry once from the pad caps.
    if (!m_haveInfo) {
        GstCaps* caps = gst_pad_get_current_caps(pad);
        if (caps) {
            m_haveInfo = gst_video_info_from_caps(&m_info, caps);
            gst_caps_unref(caps);
        }
        if (!m_haveInfo)
            return GST_PAD_PROBE_OK;
    }

    // Modify pixels in place; make_writable keeps the buffer's (downstream-
    // negotiated, e.g. memfd) memory rather than substituting our own.
    buf = gst_buffer_make_writable(buf);
    GST_PAD_PROBE_INFO_DATA(info) = buf;

    GstVideoFrame frame;
    if (!gst_video_frame_map(&frame, &m_info, buf, GST_MAP_READWRITE))
        return GST_PAD_PROBE_OK;

    const int w      = GST_VIDEO_FRAME_WIDTH(&frame);
    const int h      = GST_VIDEO_FRAME_HEIGHT(&frame);
    const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
    auto*     data   = static_cast<guint8*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));

    cv::Mat mat(h, w, CV_8UC3, data, stride);   // wraps buffer memory (no copy)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (const auto& a : m_algos) a->apply(mat);
    }

    gst_video_frame_unmap(&frame);
    return GST_PAD_PROBE_OK;
}
