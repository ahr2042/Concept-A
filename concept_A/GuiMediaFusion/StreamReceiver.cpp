#include "StreamReceiver.h"

#include <gst/video/videooverlay.h>

StreamReceiver::StreamReceiver(QWidget* parent) : QWidget(parent)
{
    // Give the widget a real native window so a video sink can render into it,
    // and stop Qt from compositing over that region (less tearing/flicker).
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_PaintOnScreen);   // X11; harmless under Wayland
    setMinimumSize(320, 240);
}

StreamReceiver::~StreamReceiver() { stop(); }

bool StreamReceiver::start(const std::string& socketPath)
{
    stop();

    // Cache the native handle now, on the GUI thread — onBusSync() runs on a
    // GStreamer streaming thread and must not call winId() there.
    m_winId = static_cast<guintptr>(winId());

    // queue (shallow, leaky=downstream) bounds latency by dropping under load
    // rather than accumulating delay; sync=false renders frames as they arrive.
    // glimagesink = GPU path via Mesa GL — swap for waylandsink / xvimagesink to
    // match the session if you prefer.
    const std::string desc =
        "unixfdsrc socket-path=" + socketPath + " ! "
        "queue max-size-buffers=3 leaky=downstream ! "
        "videoconvert ! "
        "glimagesink name=videosink sync=false";

    GError* err = nullptr;
    m_pipeline = gst_parse_launch(desc.c_str(), &err);
    if (!m_pipeline) {
        qWarning("StreamReceiver: parse failed: %s", err ? err->message : "(unknown)");
        if (err) g_error_free(err);
        return false;
    }
    m_sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "videosink");

    // Frame/byte counting probe on the sink's input pad (statsTick source).
    m_frames = 0; m_bytes = 0; m_width = 0; m_height = 0;
    m_lastFrames = 0; m_lastBytes = 0;
    if (m_sink) {
        m_probePad = gst_element_get_static_pad(m_sink, "sink");
        if (m_probePad)
            m_probeId = gst_pad_add_probe(m_probePad, GST_PAD_PROBE_TYPE_BUFFER,
                                          &StreamReceiver::onBuffer, this, nullptr);
    }

    // Answer the sink's window-handle request the instant it asks (the correct,
    // race-free way to embed video overlay).
    GstBus* bus = gst_element_get_bus(m_pipeline);
    gst_bus_set_sync_handler(bus, &StreamReceiver::onBusSync, this, nullptr);
    gst_object_unref(bus);

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING)
            == GST_STATE_CHANGE_FAILURE) {
        qWarning("StreamReceiver: failed to PLAY (is the backend listening on %s?)",
                 socketPath.c_str());
        stop();
        return false;
    }

    m_timerId = startTimer(1000);
    return true;
}

void StreamReceiver::stop()
{
    if (m_timerId) { killTimer(m_timerId); m_timerId = 0; }
    if (m_probePad) {
        if (m_probeId) gst_pad_remove_probe(m_probePad, m_probeId);
        gst_object_unref(m_probePad);
        m_probePad = nullptr;
        m_probeId  = 0;
    }
    if (m_sink)     { gst_object_unref(m_sink); m_sink = nullptr; }
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}

void StreamReceiver::timerEvent(QTimerEvent*)
{
    pollBusAndStats();
}

void StreamReceiver::pollBusAndStats()
{
    if (!m_pipeline)
        return;

    // Non-blocking bus poll — no GLib main loop needed.
    GstBus* bus = gst_element_get_bus(m_pipeline);
    while (GstMessage* msg = gst_bus_pop_filtered(
               bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS))) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* err = nullptr;
            gst_message_parse_error(msg, &err, nullptr);
            const QString text = err ? QString::fromUtf8(err->message)
                                     : QStringLiteral("unknown pipeline error");
            if (err) g_error_free(err);
            gst_message_unref(msg);
            gst_object_unref(bus);
            stop();
            emit streamError(text);
            return;
        }
        gst_message_unref(msg);
        gst_object_unref(bus);
        stop();
        emit streamEnded();
        return;
    }
    gst_object_unref(bus);

    const quint64 frames = m_frames.load();
    const quint64 bytes  = m_bytes.load();
    const double fps  = double(frames - m_lastFrames);
    const double mbps = double(bytes - m_lastBytes) / (1024.0 * 1024.0);
    m_lastFrames = frames;
    m_lastBytes  = bytes;
    emit statsTick(fps, mbps, QSize(m_width.load(), m_height.load()), frames);
}

// Runs on a GStreamer streaming thread — only touch atomics here.
GstPadProbeReturn StreamReceiver::onBuffer(GstPad* pad, GstPadProbeInfo* info, gpointer self)
{
    auto* r = static_cast<StreamReceiver*>(self);
    GstBuffer* buf = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buf) {
        r->m_frames.fetch_add(1, std::memory_order_relaxed);
        r->m_bytes.fetch_add(gst_buffer_get_size(buf), std::memory_order_relaxed);
    }
    if (r->m_width.load(std::memory_order_relaxed) == 0) {
        if (GstCaps* caps = gst_pad_get_current_caps(pad)) {
            if (const GstStructure* s = gst_caps_get_structure(caps, 0)) {
                int w = 0, h = 0;
                gst_structure_get_int(s, "width", &w);
                gst_structure_get_int(s, "height", &h);
                r->m_width.store(w, std::memory_order_relaxed);
                r->m_height.store(h, std::memory_order_relaxed);
            }
            gst_caps_unref(caps);
        }
    }
    return GST_PAD_PROBE_OK;
}

// Runs on a GStreamer streaming thread. Hand the sink this widget's native id
// when it requests one. Use the value cached in start() — never call winId()
// from here.
GstBusSyncReply StreamReceiver::onBusSync(GstBus*, GstMessage* msg, gpointer self)
{
    if (!gst_is_video_overlay_prepare_window_handle_message(msg))
        return GST_BUS_PASS;

    auto* r = static_cast<StreamReceiver*>(self);
    gst_video_overlay_set_window_handle(
        GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(msg)), r->m_winId);

    gst_message_unref(msg);
    return GST_BUS_DROP;
}
