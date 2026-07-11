#include "GStreamerSinkApplication.h"

#include <atomic>
#include <unistd.h>

std::string GStreamerSinkApplication::makeUniqueSocketPath()
{
    // Unique per process + instance so concurrent pipelines never collide on a
    // shared socket (the previous fixed default did).
    static std::atomic<unsigned> counter{0};
    return "/tmp/mediafusiongcv-" + std::to_string(getpid())
         + "-" + std::to_string(counter.fetch_add(1)) + ".sock";
}

GStreamerSinkApplication::GStreamerSinkApplication(const std::string& socketPath)
    : m_socketPath(socketPath.empty() ? makeUniqueSocketPath() : socketPath)
{
    sinkElement = gst_element_factory_make("unixfdsink", "sink");
    if (sinkElement) {
        g_object_set(sinkElement,
            "socket-path", m_socketPath.c_str(),
            // sync=false: forward each buffer to the socket as soon as it arrives
            // instead of waiting on the clock — the GUI's display sink does the
            // final pacing, so this removes up to one frame interval of latency.
            "sync",  FALSE,
            // async=false: don't block the pipeline's PAUSED transition waiting
            // for a downstream peer to connect; the sink may start with no client.
            "async", FALSE,
            NULL);
    }
}

errorState GStreamerSinkApplication::setSinkElement(const std::string& socketPath)
{
    if (!sinkElement)
        return errorState::NULLPTR_ERR;

    // Empty string -> keep the path chosen at construction (default or explicit).
    if (!socketPath.empty()) {
        m_socketPath = socketPath;
        // socket-path is only settable in NULL/READY; setSinkElement is called
        // during configuration, before startStreaming() sets the pipeline PLAYING.
        g_object_set(sinkElement, "socket-path", m_socketPath.c_str(), NULL);
    }
    return errorState::NO_ERR;
}
