#pragma once

#include "GStreamerSink.h"
#include <string>

// IPC sink: ships GstBuffers to another process (the GUI) over a Unix socket
// using unixfdsink (GStreamer >= 1.24). The buffer's underlying file
// descriptor(s) — dmabuf when the source is zero-copy, otherwise a memfd —
// are passed via SCM_RIGHTS, so the pixel payload is not streamed through the
// socket. Caps and buffer meta travel in-band, so the receiver (unixfdsrc)
// needs no out-of-band format negotiation.
//
// unixfdsink is the LISTENING end: it creates/owns the socket at socketPath().
// The receiving process connects to that same path, therefore the backend must
// reach at least READY before the GUI's unixfdsrc connects.
class GStreamerSinkApplication : public GStreamerSink
{
public:
    // socketPath: filesystem path of the control socket the sink listens on.
    // NOTE: each concurrent pipeline needs its OWN path — pass a unique one per
    // stream (the default is a single-stream convenience only).
    // Empty socketPath -> a unique per-process path is generated (recommended:
    // distinct pipelines must not share a socket). Read it back via socketPath().
    explicit GStreamerSinkApplication(const std::string& socketPath = std::string());

    const std::string& socketPath() const { return m_socketPath; }
    std::string        endpoint()   const override { return m_socketPath; }

    static std::string makeUniqueSocketPath();

private:
    // Reuses the base "set sink element" entry point as "set socket path": the
    // sink-config string coming through the C API (mediaLib_init's sink arg) is
    // interpreted as the socket path for this sink type.
    errorState setSinkElement(const std::string&) override;
    errorState setCapsFilterElement(int32_t, int32_t) override { return errorState::NO_ERR; }

    std::string m_socketPath;
};
