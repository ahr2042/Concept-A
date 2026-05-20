#include "MediaFusionGCV_API.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <unistd.h>

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

static void printHelp()
{
    std::cout
        << "MediaFusionGCV v2.0 -- GStreamer pipeline manager\n"
        << "\n"
        << "Camera-to-screen workflow:\n"
        << "  1. create camera screen mycam    -- allocates pipeline, prints id (e.g. id=0)\n"
        << "  2. devices 0                     -- lists Device [N] and Cap [M] indices\n"
        << "  3. set-device 0 <N> <M>          -- selects the device and cap to stream\n"
        << "  4. start 0                       -- begins streaming\n"
        << "  5. stop 0                        -- stops streaming\n"
        << "\n"
        << "  The GStreamer element names (v4l2src, autovideosink) are chosen automatically\n"
        << "  from the source/sink type you give to 'create'. You do not need 'init'.\n"
        << "\n"
        << "Commands:\n"
        << "  create <src> <snk> <name>        Create a pipeline\n"
        << "    src: none(0) file(1) camera(2) network(3) screen(4) test(5) custom(6)\n"
        << "    snk: none(0) screen(1) file(2) network(3) hardware(4) app(5) test(6) media(7)\n"
        << "  devices <id>                     List source devices with selectable caps\n"
        << "  set-device <id> <dev> <cap>      Select Device [dev] Cap [cap] from 'devices' output\n"
        << "  start <id>                       Start streaming\n"
        << "  stop <id>                        Stop streaming\n"
        << "  delete <id>                      Delete pipeline\n"
        << "  list                             Show active pipelines\n"
        << "  help                             Show this help\n"
        << "  quit                             Exit\n"
        << "\n"
        << "Response format: OK [data] | ERR <CODE>\n"
        << std::flush;
}

int main(int argc, char* argv[])
{
    errorState gstErr = mediaLib_GStreamerInit(argc, argv);
    if (gstErr != errorState::NO_ERR) {
        std::cerr << "ERR " << errStr(gstErr) << " GStreamer init failed\n" << std::flush;
        return 1;
    }

    bool interactive = isatty(STDIN_FILENO);
    if (interactive)
        std::cout << "MediaFusionGCV ready. Type 'help' for commands.\n" << std::flush;

    std::string line;
    while (true) {
        if (interactive)
            std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        cmd = toLower(cmd);

        if (cmd == "quit" || cmd == "exit") {
            std::cout << "OK bye\n" << std::flush;
            break;
        }

        else if (cmd == "help") {
            printHelp();
        }
        else if (cmd == "create") {
            std::string srcToken, snkToken, name;
            if (!(iss >> srcToken >> snkToken >> name)) {
                std::cout << "ERR INVALID_ARGS create <src> <snk> <name>\n" << std::flush;
                continue;
            }
            SourceType src; SinkType snk;
            if (!parseSourceType(srcToken, src)) {
                std::cout << "ERR INVALID_ARGS unknown source '" << srcToken
                          << "' -- use: none file camera network screen test custom\n" << std::flush;
                continue;
            }
            if (!parseSinkType(snkToken, snk)) {
                std::cout << "ERR INVALID_ARGS unknown sink '" << snkToken
                          << "' -- use: none screen file network hardware app test media\n" << std::flush;
                continue;
            }
            size_t id = mediaLib_create(src, snk, name.c_str());
            pipelineMetas.push_back({name, src, snk});
            std::cout << "OK id=" << id << "\n" << std::flush;
        }
        else if (cmd == "init") {
            size_t id = 0;
            std::string srcElem, snkElem;
            if (!(iss >> id >> srcElem >> snkElem)) {
                std::cout << "ERR INVALID_ARGS init <id> <src_elem> <snk_elem>\n" << std::flush;
                continue;
            }
            errorState err = mediaLib_init(id, srcElem.c_str(), snkElem.c_str());
            if (err == errorState::NO_ERR)
                std::cout << "OK\n" << std::flush;
            else
                std::cout << "ERR " << errStr(err) << "\n" << std::flush;
        }
        else if (cmd == "devices") {
            size_t id = 0;
            if (!(iss >> id)) {
                std::cout << "ERR INVALID_ARGS devices <id>\n" << std::flush;
                continue;
            }
            size_t count = 0;
            deviceProperties* devs = nullptr;
            errorState err = mediaLib_getDevices(id, count, &devs);
            if (err == errorState::NO_ERR) {
                std::cout << "OK " << count << " device(s)\n";
                for (size_t i = 0; i < count; i++) {
                    std::cout << "Device [" << i << "]: " << devs[i].deviceName << "\n"
                              << devs[i].formattedDeviceCapabilities
                              << "  → use: set-device " << id << " " << i << " <cap>\n\n";
                }
                delete[] devs;
            } else if (err == errorState::NO_VIDEO_DEVICE_FOUND_ERR) {
                std::cout << "ERR NO_VIDEO_DEVICE_FOUND_ERR (no camera detected)\n";
            } else {
                std::cout << "ERR " << errStr(err) << "\n";
            }
            std::cout << std::flush;
        }
        else if (cmd == "set-device") {
            size_t id = 0;
            int32_t devId = 0, capIdx = 0;
            if (!(iss >> id >> devId >> capIdx)) {
                std::cout << "ERR INVALID_ARGS set-device <id> <device_id> <cap_index>\n" << std::flush;
                continue;
            }
            errorState err = mediaLib_setDevice(id, devId, capIdx);
            if (err == errorState::NO_ERR)
                std::cout << "OK\n" << std::flush;
            else
                std::cout << "ERR " << errStr(err) << "\n" << std::flush;
        }
        else if (cmd == "start") {
            size_t id = 0;
            if (!(iss >> id)) {
                std::cout << "ERR INVALID_ARGS start <id>\n" << std::flush;
                continue;
            }
            errorState err = mediaLib_startStreaming(id);
            if (err == errorState::NO_ERR)
                std::cout << "OK\n" << std::flush;
            else {
                std::cout << "ERR " << errStr(err) << "\n";
                if (err == errorState::BUILD_PIPELINE_FAILED)
                    std::cout << "  hint: run 'devices " << id
                              << "' then 'set-device " << id
                              << " <dev> <cap>' before start\n";
                std::cout << std::flush;
            }
        }
        else if (cmd == "stop") {
            size_t id = 0;
            if (!(iss >> id)) {
                std::cout << "ERR INVALID_ARGS stop <id>\n" << std::flush;
                continue;
            }
            errorState err = mediaLib_stopStreaming(id);
            if (err == errorState::NO_ERR)
                std::cout << "OK\n" << std::flush;
            else
                std::cout << "ERR " << errStr(err) << "\n" << std::flush;
        }
        else if (cmd == "delete") {
            size_t id = 0;
            if (!(iss >> id)) {
                std::cout << "ERR INVALID_ARGS delete <id>\n" << std::flush;
                continue;
            }
            if (id < pipelineMetas.size())
                pipelineMetas.erase(pipelineMetas.begin() + static_cast<std::ptrdiff_t>(id));
            size_t remaining = mediaLib_delete(id);
            std::cout << "OK remaining=" << remaining << "\n" << std::flush;
        }
        else if (cmd == "list") {
            std::cout << "OK " << pipelineMetas.size() << "\n";
            for (size_t i = 0; i < pipelineMetas.size(); i++) {
                int srcIdx = static_cast<int>(pipelineMetas[i].src);
                int snkIdx = static_cast<int>(pipelineMetas[i].snk);
                const char* sn = (srcIdx >= 0 && srcIdx <= 6) ? kSourceNames[srcIdx] : "?";
                const char* sk = (snkIdx >= 0 && snkIdx <= 7) ? kSinkNames[snkIdx] : "?";
                std::cout << "  [" << i << "] " << pipelineMetas[i].name
                          << "  src=" << sn << "  snk=" << sk << "\n";
            }
            std::cout << std::flush;
        }
        else {
            std::cout << "ERR UNKNOWN_CMD '" << cmd << "' -- type 'help' for commands\n" << std::flush;
        }
    }

    mediaLib_destroyAll();
    pipelineMetas.clear();
    return EXIT_SUCCESS;
}
