#pragma once

#include <string>

// Client for the MediaFusionGCV control daemon (started with `--serve <path>`).
// Pure POSIX + std::string, no Qt, so it builds/tests standalone and the GUI's
// Model can wrap it. One persistent connection carries many commands; each
// request is a '\n'-terminated line, each reply is text ending in a NUL byte.
//
// This is the client half of the two-process split: the GUI no longer links the
// pipeline logic in-process — it drives the daemon over this socket and learns
// the per-stream video socket path from start(), which it hands to StreamReceiver.
class ControlClient
{
public:
    ControlClient() = default;
    ~ControlClient();

    bool connect(const std::string& socketPath);
    void disconnect();
    bool connected() const { return m_fd >= 0; }

    // Low-level: send one command, receive the full reply (NUL stripped).
    bool command(const std::string& cmd, std::string& response);

    // Typed helpers mirroring the protocol.
    long create(const std::string& source, const std::string& sink, const std::string& name); // id, or -1
    bool setDevice(long id, int deviceIndex, int capIndex);
    bool setAlgorithms(long id, const std::string& csv);       // "" disables
    bool start(long id, std::string& videoSocketPath);         // path empty for non-app sinks
    bool stop(long id);
    std::string devices(long id);                              // raw listing text

private:
    bool sendLine(const std::string& line);
    bool readReply(std::string& out);

    int m_fd = -1;
};
