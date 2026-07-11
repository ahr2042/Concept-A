#include "ControlClient.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

ControlClient::~ControlClient() { disconnect(); }

bool ControlClient::connect(const std::string& socketPath)
{
    disconnect();
    m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0)
        return false;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socketPath.size() >= sizeof(addr.sun_path)) { disconnect(); return false; }
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        disconnect();
        return false;
    }
    return true;
}

void ControlClient::disconnect()
{
    if (m_fd >= 0) { close(m_fd); m_fd = -1; }
}

bool ControlClient::sendLine(const std::string& line)
{
    if (m_fd < 0) return false;
    std::string out = line;
    if (out.empty() || out.back() != '\n') out.push_back('\n');
    size_t sent = 0;
    while (sent < out.size()) {
        ssize_t n = write(m_fd, out.data() + sent, out.size() - sent);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool ControlClient::readReply(std::string& out)
{
    out.clear();
    if (m_fd < 0) return false;
    char buf[4096];
    while (true) {
        ssize_t n = read(m_fd, buf, sizeof(buf));
        if (n <= 0) return false;
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\0') return true;     // reply terminator
            out.push_back(buf[i]);
        }
    }
}

bool ControlClient::command(const std::string& cmd, std::string& response)
{
    return sendLine(cmd) && readReply(response);
}

long ControlClient::create(const std::string& source, const std::string& sink, const std::string& name)
{
    std::string resp;
    if (!command("create " + source + " " + sink + " " + name, resp)) return -1;
    if (resp.rfind("OK", 0) != 0) return -1;
    auto pos = resp.find("id=");
    if (pos == std::string::npos) return -1;
    try { return std::stol(resp.substr(pos + 3)); } catch (...) { return -1; }
}

bool ControlClient::setDevice(long id, int deviceIndex, int capIndex)
{
    std::ostringstream c;
    c << "set-device " << id << " " << deviceIndex << " " << capIndex;
    std::string resp;
    return command(c.str(), resp) && resp.rfind("OK", 0) == 0;
}

bool ControlClient::setAlgorithms(long id, const std::string& csv)
{
    std::ostringstream c;
    c << "algos " << id << " " << csv;
    std::string resp;
    return command(c.str(), resp) && resp.rfind("OK", 0) == 0;
}

bool ControlClient::start(long id, std::string& videoSocketPath)
{
    videoSocketPath.clear();
    std::ostringstream c;
    c << "start " << id;
    std::string resp;
    if (!command(c.str(), resp) || resp.rfind("OK", 0) != 0) return false;
    std::istringstream iss(resp);              // "OK <path>" or "OK"
    std::string ok, path;
    iss >> ok >> path;
    videoSocketPath = path;
    return true;
}

bool ControlClient::stop(long id)
{
    std::ostringstream c;
    c << "stop " << id;
    std::string resp;
    return command(c.str(), resp) && resp.rfind("OK", 0) == 0;
}

std::string ControlClient::devices(long id)
{
    std::ostringstream c;
    c << "devices " << id;
    std::string resp;
    command(c.str(), resp);
    return resp;
}
