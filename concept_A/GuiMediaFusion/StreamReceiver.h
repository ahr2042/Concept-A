#pragma once

// ── GUI-side stream receiver ──────────────────────────────────────────────────
// Displays the backend's stream by connecting `unixfdsrc` to the socket that the
// backend's GStreamerSinkApplication (`unixfdsink`) listens on. Frames render
// directly into this QWidget via GstVideoOverlay — no extra top-level window,
// GPU-accelerated through Mesa GL on the Radeon. Caps arrive in-band over the
// unixfd transport, so no format negotiation is required here.
//
// VISION_OS additions on top of the original sketch:
//   * a buffer pad-probe on the video sink counts frames/bytes and captures the
//     negotiated caps; a 1 s GUI-thread timer turns that into fps / MB/s stats
//     (statsTick) that feed the telemetry strip and analytics charts.
//   * the same timer polls the pipeline bus for ERROR/EOS (no GLib main-loop
//     dependency) and emits streamError/streamEnded.
//
// gst_init() must have been called once in this process (main() does it).

#include <QWidget>
#include <gst/gst.h>

#include <atomic>
#include <string>

class StreamReceiver : public QWidget
{
    Q_OBJECT
public:
    explicit StreamReceiver(QWidget* parent = nullptr);
    ~StreamReceiver() override;

    // Connect to the backend's unixfdsink socket and begin displaying.
    // socketPath MUST match the path returned by the daemon's `start` reply
    // (per-pipeline unique). The backend must already be streaming.
    bool start(const std::string& socketPath);
    void stop();
    bool running() const { return m_pipeline != nullptr; }

signals:
    // Once per second while running: current fps, current MB/s, frame size.
    void statsTick(double fps, double mbps, QSize resolution, quint64 totalFrames);
    void streamError(const QString& message);
    void streamEnded();

protected:
    void timerEvent(QTimerEvent*) override;

private:
    static GstBusSyncReply onBusSync(GstBus*, GstMessage*, gpointer self);
    static GstPadProbeReturn onBuffer(GstPad*, GstPadProbeInfo*, gpointer self);
    void pollBusAndStats();

    GstElement* m_pipeline = nullptr;
    GstElement* m_sink     = nullptr;   // the video sink embedded into this widget
    guintptr    m_winId    = 0;         // cached on the GUI thread (see start())
    gulong      m_probeId  = 0;
    GstPad*     m_probePad = nullptr;

    int m_timerId = 0;

    // written by the streaming thread, read by the GUI timer
    std::atomic<quint64> m_frames{0};
    std::atomic<quint64> m_bytes{0};
    std::atomic<int>     m_width{0};
    std::atomic<int>     m_height{0};
    quint64 m_lastFrames = 0;
    quint64 m_lastBytes  = 0;
};
