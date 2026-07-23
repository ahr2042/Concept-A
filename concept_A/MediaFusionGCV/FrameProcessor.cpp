#include "FrameProcessor.h"
#include "Algorithms.h"

#include <opencv2/imgproc.hpp>

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

void FrameProcessor::setAlgorithms(const std::vector<std::string>& names)
{
    DetectorConfig cfg = detectorConfig();

    std::vector<std::unique_ptr<Algorithm>> built;
    built.reserve(names.size());
    for (const auto& n : names) {
        auto a = makeAlgorithm(n);
        if (!a)
            continue;
        // A detector joining the chain inherits the model chosen earlier;
        // loading happens here, off the streaming thread.
        if (auto* det = dynamic_cast<DetectorAlgorithm*>(a.get()))
            det->configure(cfg);
        built.push_back(std::move(a));
    }

    std::lock_guard<std::mutex> lk(m_mutex);
    m_algos = std::move(built);
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

bool FrameProcessor::inferenceStats(InferenceStats& out) const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    for (const auto& a : m_algos)
        if (a->snapshotStats(out))
            return true;
    return false;
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
