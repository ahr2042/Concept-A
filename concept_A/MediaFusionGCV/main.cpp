#include "MediaFusionGCV_API.h"
#include "AccelBackend.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unistd.h>

// For --serve (Unix-domain control socket) mode.
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <cerrno>

struct PipelineMeta {
    std::string name;
    SourceType  src;
    SinkType    snk;
};

static std::vector<PipelineMeta> pipelineMetas;

static const char* kSourceNames[] = {
    "none", "file", "camera", "network", "screen", "test", "custom"
};
static const char* kSinkNames[] = {
    "none", "screen", "file", "network", "hardware", "app", "test", "media"
};

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string errStr(errorState e)
{
    switch (e) {
        case errorState::NO_ERR:                    return "NO_ERR";
        case errorState::NULLPTR_ERR:               return "NULLPTR_ERR";
        case errorState::EMPTY_STRING_ERR:          return "EMPTY_STRING_ERR";
        case errorState::OBJECT_CREATION_ERR:       return "OBJECT_CREATION_ERR";
        case errorState::BUILD_PIPELINE_FAILED:     return "BUILD_PIPELINE_FAILED";
        case errorState::START_STREAMING_FAILED:    return "START_STREAMING_FAILED";
        case errorState::STOP_STREAMING_FAILED:     return "STOP_STREAMING_FAILED";
        case errorState::LOAD_MODEL_ERR:            return "LOAD_MODEL_ERR";
        case errorState::SET_SOURCE_ELEMENT_ERR:    return "SET_SOURCE_ELEMENT_ERR";
        case errorState::SET_SINK_ELEMENT_ERR:      return "SET_SINK_ELEMENT_ERR";
        case errorState::SET_SOURCE_CAPS_ERR:       return "SET_SOURCE_CAPS_ERR";
        case errorState::NO_VIDEO_DEVICE_FOUND_ERR: return "NO_VIDEO_DEVICE_FOUND_ERR";
        case errorState::NOT_IMPLEMENTED_YET_ERR:   return "NOT_IMPLEMENTED_YET_ERR";
        default:                                    return "UNKNOWN_ERR";
    }
}

static bool parseSourceType(const std::string& token, SourceType& out)
{
    try {
        int v = std::stoi(token);
        if (v >= 0 && v <= 6) { out = static_cast<SourceType>(v); return true; }
        return false;
    } catch (...) {}
    std::string low = toLower(token);
    for (int i = 0; i <= 6; i++) {
        if (low == kSourceNames[i]) { out = static_cast<SourceType>(i); return true; }
    }
    return false;
}

static bool parseSinkType(const std::string& token, SinkType& out)
{
    try {
        int v = std::stoi(token);
        if (v >= 0 && v <= 7) { out = static_cast<SinkType>(v); return true; }
        return false;
    } catch (...) {}
    std::string low = toLower(token);
    if (low == "application") low = "app";
    if (low == "debugging"  ) low = "test";
    if (low == "media_and_streaming") low = "media";
    for (int i = 0; i <= 7; i++) {
        if (low == kSinkNames[i]) { out = static_cast<SinkType>(i); return true; }
    }
    return false;
}

static std::string helpText()
{
    std::ostringstream o;
    o   << "MediaFusionGCV v2.0 -- GStreamer pipeline manager\n"
        << "\n"
        << "Camera-to-GUI workflow (two processes):\n"
        << "  1. create camera app mycam       -- allocates pipeline, prints id (e.g. id=0)\n"
        << "  2. devices 0                      -- lists Device [N] and Cap [M] indices\n"
        << "  3. set-device 0 <N> <M>           -- selects the device and cap to stream\n"
        << "  4. algos 0 grayscale,canny        -- (optional) real-time OpenCV chain\n"
        << "  5. start 0                        -- begins streaming; prints the video socket\n"
        << "                                       path the GUI connects unixfdsrc to\n"
        << "  6. stop 0                         -- stops streaming\n"
        << "\n"
        << "Object detection (needs a model -- see scripts/fetch-models.sh):\n"
        << "  models                            -- lists the installed ONNX models\n"
        << "  model 0 yolov5n                   -- loads one into pipeline 0\n"
        << "  algos 0 detect                    -- puts the inference stage in the chain\n"
        << "  stats 0                           -- inference latency and last detections\n"
        << "\n"
        << "Commands:\n"
        << "  create <src> <snk> <name>        Create a pipeline\n"
        << "    src: none(0) file(1) camera(2) network(3) screen(4) test(5) custom(6)\n"
        << "    snk: none(0) screen(1) file(2) network(3) hardware(4) app(5) test(6) media(7)\n"
        << "  devices <id>                     List source devices with selectable caps\n"
        << "  set-device <id> <dev> <cap>      Select Device [dev] Cap [cap] from 'devices' output\n"
        << "  algos <id> <csv>                 Set OpenCV algorithm chain (empty csv disables)\n"
        << "  algos-list                       List available algorithm names\n"
        << "  accelerators                     List detected accel backends (cpu/vulkan/cuda)\n"
        << "  models                           List installed detector models\n"
        << "  model <id> [name]                Load a detector model (no name unloads it)\n"
        << "  detect-params <id> <conf> <nms> [draw]\n"
        << "                                   Detector thresholds (0..1) and box overlay (0/1)\n"
        << "  accel <id> <auto|cpu|vulkan|cuda>  Select accel backend (applied at next start)\n"
        << "  stats <id>                       Inference latency and last detections\n"
        << "  start <id>                       Start streaming (app sink -> prints socket path)\n"
        << "  stop <id>                        Stop streaming\n"
        << "  delete <id>                      Delete pipeline\n"
        << "  list                             Show active pipelines\n"
        << "  help                             Show this help\n"
        << "  quit                             Close this connection (daemon keeps running)\n"
        << "\n"
        << "Response format: OK [data] | ERR <CODE>\n";
    return o.str();
}

// Handle one command line; return the full response text (identical in CLI and
// daemon modes). 'quit'/'shutdown' are acted on by the caller, not here.
static std::string handleCommand(const std::string& line)
{
    std::ostringstream out;
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    cmd = toLower(cmd);

    if (cmd.empty()) {
        // nothing
    }
    else if (cmd == "quit" || cmd == "exit") {
        out << "OK bye\n";
    }
    else if (cmd == "shutdown") {
        out << "OK shutting down\n";
    }
    else if (cmd == "help") {
        out << helpText();
    }
    else if (cmd == "create") {
        std::string srcToken, snkToken, name;
        if (!(iss >> srcToken >> snkToken >> name)) {
            out << "ERR INVALID_ARGS create <src> <snk> <name>\n";
        } else {
            SourceType src; SinkType snk;
            if (!parseSourceType(srcToken, src))
                out << "ERR INVALID_ARGS unknown source '" << srcToken
                    << "' -- use: none file camera network screen test custom\n";
            else if (!parseSinkType(snkToken, snk))
                out << "ERR INVALID_ARGS unknown sink '" << snkToken
                    << "' -- use: none screen file network hardware app test media\n";
            else {
                size_t id = mediaLib_create(src, snk, name.c_str());
                pipelineMetas.push_back({name, src, snk});
                out << "OK id=" << id << "\n";
            }
        }
    }
    else if (cmd == "init") {
        size_t id = 0;
        std::string srcElem, snkElem;
        if (!(iss >> id >> srcElem >> snkElem))
            out << "ERR INVALID_ARGS init <id> <src_elem> <snk_elem>\n";
        else {
            errorState err = mediaLib_init(id, srcElem.c_str(), snkElem.c_str());
            out << (err == errorState::NO_ERR ? "OK\n" : ("ERR " + errStr(err) + "\n"));
        }
    }
    else if (cmd == "devices") {
        size_t id = 0;
        if (!(iss >> id))
            out << "ERR INVALID_ARGS devices <id>\n";
        else {
            size_t count = 0;
            deviceProperties* devs = nullptr;
            errorState err = mediaLib_getDevices(id, count, &devs);
            if (err == errorState::NO_ERR) {
                out << "OK " << count << " device(s)\n";
                for (size_t i = 0; i < count; i++)
                    out << "Device [" << i << "]: " << devs[i].deviceName << "\n"
                        << devs[i].formattedDeviceCapabilities
                        << "  -> use: set-device " << id << " " << i << " <cap>\n\n";
                delete[] devs;
            } else if (err == errorState::NO_VIDEO_DEVICE_FOUND_ERR) {
                out << "ERR NO_VIDEO_DEVICE_FOUND_ERR (no camera detected)\n";
            } else {
                out << "ERR " << errStr(err) << "\n";
            }
        }
    }
    else if (cmd == "set-device") {
        size_t id = 0;
        int32_t devId = 0, capIdx = 0;
        if (!(iss >> id >> devId >> capIdx))
            out << "ERR INVALID_ARGS set-device <id> <device_id> <cap_index>\n";
        else {
            errorState err = mediaLib_setDevice(id, devId, capIdx);
            out << (err == errorState::NO_ERR ? "OK\n" : ("ERR " + errStr(err) + "\n"));
        }
    }
    else if (cmd == "algos") {
        size_t id = 0;
        if (!(iss >> id))
            out << "ERR INVALID_ARGS algos <id> <csv>  (empty csv disables)\n";
        else {
            std::string csv;
            std::getline(iss, csv);                       // rest of line (may be empty)
            size_t a = csv.find_first_not_of(" \t");
            csv = (a == std::string::npos) ? "" : csv.substr(a);
            errorState err = mediaLib_setAlgorithms(id, csv.c_str());
            out << (err == errorState::NO_ERR ? "OK\n" : ("ERR " + errStr(err) + "\n"));
        }
    }
    else if (cmd == "algos-list") {
        out << "OK " << mediaLib_availableAlgorithms() << "\n";
    }
    else if (cmd == "accelerators") {
        // One line per backend the host can run; the GUI shows only available=1.
        out << "OK\n" << mediaLib_detectAccelerators();
    }
    else if (cmd == "accel") {
        size_t id = 0;
        std::string sel;
        if (!(iss >> id >> sel))
            out << "ERR INVALID_ARGS accel <id> <auto|cpu|vulkan|cuda>\n";
        else {
            AccelSelection s;
            if (!parseAccelSelection(sel, s))
                out << "ERR INVALID_ARGS unknown backend '" << sel
                    << "' -- use: auto cpu vulkan cuda\n";
            else {
                errorState err = mediaLib_setAccel(id, static_cast<int32_t>(s));
                out << (err == errorState::NO_ERR
                        ? ("OK " + std::string(accelSelectionName(s)) + "\n")
                        : ("ERR " + errStr(err) + "\n"));
            }
        }
    }
    else if (cmd == "models") {
        const std::string listing = mediaLib_availableModels();
        size_t count = static_cast<size_t>(std::count(listing.begin(), listing.end(), '\n'));
        out << "OK " << count << " model(s)\n" << listing;
        if (count == 0)
            out << "  none installed -- run scripts/fetch-models.sh\n";
    }
    else if (cmd == "model") {
        size_t id = 0;
        if (!(iss >> id))
            out << "ERR INVALID_ARGS model <id> [name]  (no name unloads)\n";
        else {
            std::string name;
            iss >> name;                                  // absent -> unload
            errorState err = mediaLib_setDetectorModel(id, name.c_str());
            if (err == errorState::NO_ERR)
                out << "OK " << (name.empty() ? "unloaded" : name) << "\n";
            else {
                out << "ERR " << errStr(err) << "\n";
                if (err == errorState::LOAD_MODEL_ERR)
                    out << "  hint: run 'models' for installed names\n";
            }
        }
    }
    else if (cmd == "detect-params") {
        size_t id = 0;
        float  conf = 0.0f, nms = 0.0f;
        if (!(iss >> id >> conf >> nms))
            out << "ERR INVALID_ARGS detect-params <id> <confidence> <nms> [draw]\n";
        else if (conf < 0.0f || conf > 1.0f || nms < 0.0f || nms > 1.0f)
            out << "ERR INVALID_ARGS confidence and nms must be in 0..1\n";
        else {
            int draw = 1;
            iss >> draw;                                  // optional, defaults to on
            errorState err = mediaLib_setDetectorParams(id, conf, nms, draw != 0);
            out << (err == errorState::NO_ERR ? "OK\n" : ("ERR " + errStr(err) + "\n"));
        }
    }
    else if (cmd == "stats") {
        size_t id = 0;
        if (!(iss >> id))
            out << "ERR INVALID_ARGS stats <id>\n";
        else {
            InferenceStats st;
            errorState err = mediaLib_getInferenceStats(id, st);
            if (err == errorState::NOT_IMPLEMENTED_YET_ERR) {
                // No inference stage in this pipeline — a normal state, not a
                // failure: report it in the same shape so clients parse one form.
                out << "OK model= loaded=0 infer_ms=0 avg_ms=0 inferred=0 skipped=0"
                       " objects=0 avg_conf=0\n";
            } else if (err != errorState::NO_ERR) {
                out << "ERR " << errStr(err) << "\n";
            } else {
                out << "OK model=" << st.modelName
                    << " loaded="   << (st.modelLoaded ? 1 : 0)
                    << " infer_ms=" << st.inferenceMs
                    << " avg_ms="   << st.avgInferenceMs
                    << " inferred=" << st.framesInferred
                    << " skipped="  << st.framesSkipped
                    << " objects="  << st.objectCount
                    << " avg_conf=" << st.avgConfidence << "\n";
                for (const auto& d : st.detections)
                    out << "det conf=" << d.confidence
                        << " box="     << d.x << "," << d.y << "," << d.width << "," << d.height
                        << " label="   << d.label << "\n";   // label last: it may contain spaces
                if (!st.lastError.empty())
                    out << "error=" << st.lastError << "\n";
            }
        }
    }
    else if (cmd == "start") {
        size_t id = 0;
        if (!(iss >> id))
            out << "ERR INVALID_ARGS start <id>\n";
        else {
            errorState err = mediaLib_startStreaming(id);
            if (err == errorState::NO_ERR) {
                const char* ep = mediaLib_getStreamEndpoint(id);
                if (ep && ep[0]) out << "OK " << ep << "\n";   // app sink: video socket path
                else             out << "OK\n";
            } else {
                out << "ERR " << errStr(err) << "\n";
                if (err == errorState::BUILD_PIPELINE_FAILED)
                    out << "  hint: run 'devices " << id << "' then 'set-device " << id
                        << " <dev> <cap>' before start\n";
            }
        }
    }
    else if (cmd == "stop") {
        size_t id = 0;
        if (!(iss >> id))
            out << "ERR INVALID_ARGS stop <id>\n";
        else {
            errorState err = mediaLib_stopStreaming(id);
            out << (err == errorState::NO_ERR ? "OK\n" : ("ERR " + errStr(err) + "\n"));
        }
    }
    else if (cmd == "delete") {
        size_t id = 0;
        if (!(iss >> id))
            out << "ERR INVALID_ARGS delete <id>\n";
        else if (id >= pipelineMetas.size())
            out << "ERR INVALID_ARGS no pipeline with id " << id << "\n";
        else {
            pipelineMetas.erase(pipelineMetas.begin() + static_cast<std::ptrdiff_t>(id));
            size_t remaining = mediaLib_delete(id);
            out << "OK remaining=" << remaining << "\n";
        }
    }
    else if (cmd == "list") {
        out << "OK " << pipelineMetas.size() << "\n";
        for (size_t i = 0; i < pipelineMetas.size(); i++) {
            int srcIdx = static_cast<int>(pipelineMetas[i].src);
            int snkIdx = static_cast<int>(pipelineMetas[i].snk);
            const char* sn = (srcIdx >= 0 && srcIdx <= 6) ? kSourceNames[srcIdx] : "?";
            const char* sk = (snkIdx >= 0 && snkIdx <= 7) ? kSinkNames[snkIdx] : "?";
            out << "  [" << i << "] " << pipelineMetas[i].name
                << "  src=" << sn << "  snk=" << sk << "\n";
        }
    }
    else {
        out << "ERR UNKNOWN_CMD '" << cmd << "' -- type 'help' for commands\n";
    }
    return out.str();
}

// Returns the lowercased first token of a line (the command verb).
static std::string commandVerb(const std::string& line)
{
    std::istringstream iss(line);
    std::string c;
    iss >> c;
    return toLower(c);
}

static int runStdinRepl()
{
    bool interactive = isatty(STDIN_FILENO);
    if (interactive)
        std::cout << "MediaFusionGCV ready. Type 'help' for commands.\n" << std::flush;

    std::string line;
    while (true) {
        if (interactive)
            std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::cout << handleCommand(line) << std::flush;
        std::string verb = commandVerb(line);
        if (verb == "quit" || verb == "exit") break;
    }
    return EXIT_SUCCESS;
}

// Serve the text protocol over a Unix-domain socket. Each request is one '\n'
// terminated line; each response is the reply text followed by a NUL byte so the
// client knows where a (possibly multi-line) response ends.
static int runServer(const std::string& socketPath)
{
    int listenFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd < 0) { std::perror("socket"); return 1; }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socketPath.size() >= sizeof(addr.sun_path)) {
        std::cerr << "ERR socket path too long\n";
        close(listenFd);
        return 1;
    }
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    unlink(socketPath.c_str());                       // clear any stale socket
    if (bind(listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind"); close(listenFd); return 1;
    }
    if (listen(listenFd, 4) < 0) {
        std::perror("listen"); close(listenFd); return 1;
    }

    std::cout << "MediaFusionGCV control daemon listening on " << socketPath << "\n" << std::flush;

    bool running = true;
    while (running) {
        int clientFd = accept(listenFd, nullptr, nullptr);
        if (clientFd < 0) {
            if (errno == EINTR) continue;
            std::perror("accept");
            break;
        }

        std::string inbuf;
        char buf[4096];
        bool clientOpen = true;
        while (clientOpen) {
            ssize_t n = read(clientFd, buf, sizeof(buf));
            if (n <= 0) break;                        // client closed / error
            inbuf.append(buf, static_cast<size_t>(n));

            size_t nl;
            while ((nl = inbuf.find('\n')) != std::string::npos) {
                std::string line = inbuf.substr(0, nl);
                inbuf.erase(0, nl + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();

                std::string resp = handleCommand(line);
                resp.push_back('\0');                 // frame terminator
                if (write(clientFd, resp.data(), resp.size()) < 0) { clientOpen = false; break; }

                std::string verb = commandVerb(line);
                if (verb == "quit" || verb == "exit") { clientOpen = false; break; }
                if (verb == "shutdown") { clientOpen = false; running = false; break; }
            }
        }
        close(clientFd);
    }

    close(listenFd);
    unlink(socketPath.c_str());
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
    errorState gstErr = mediaLib_GStreamerInit(argc, argv);
    if (gstErr != errorState::NO_ERR) {
        std::cerr << "ERR " << errStr(gstErr) << " GStreamer init failed\n" << std::flush;
        return 1;
    }

    // --serve <path> / --serve=<path> -> control-daemon mode; otherwise stdin CLI.
    std::string serveSocket;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--serve" || a == "-s") && i + 1 < argc) serveSocket = argv[++i];
        else if (a.rfind("--serve=", 0) == 0)              serveSocket = a.substr(8);
    }

    int rc = serveSocket.empty() ? runStdinRepl() : runServer(serveSocket);

    mediaLib_destroyAll();
    pipelineMetas.clear();
    return rc;
}
