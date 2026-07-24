#include "PipelineManager.h"
#include "GStreamerSourceCamera.h"
#include "GStreamerSinkScreen.h"
#include "GStreamerSinkApplication.h"
#include "FrameProcessor.h"
#include "ModelRegistry.h"
#include "AcceleratorRegistry.h"

PipelineManager::PipelineManager(SourceType srcType, SinkType snkType, const char* pipelineName)
{
    pipeline = gst_pipeline_new(pipelineName);

    switch (srcType) {
    case SourceType::CAMERA_SOURCE:
        source = new GStreamerSourceCamera();
        break;
    default:
        break;
    }

    switch (snkType) {
    case SinkType::SCREEN_SINK:
        sink = new GStreamerSinkScreen(ScreenSinks::AUTOVIDEOSINK);
        break;
    case SinkType::APPLICATION_SINK:
        // IPC sink: hands frames to the GUI process over a Unix socket. The
        // socket path can be overridden later via setSinkElement() (the sink
        // string from mediaLib_init); default is GStreamerSinkApplication's.
        sink = new GStreamerSinkApplication();
        break;
    default:
        break;
    }

    if (source) {
        source->capsFilter = gst_element_factory_make("capsfilter",   "src-capsfilter");
        source->converter  = gst_element_factory_make("videoconvert", "src-converter");
    }

    if (sink) {
        sink->converter = gst_element_factory_make("queue", "sink-queue");
    }

    if (!pipeline
        || (source && (!source->sourceElement || !source->capsFilter || !source->converter))
        || (sink   && (!sink->sinkElement || !sink->converter)))
    {
        g_error("Failed to create pipeline elements");
    }
}

PipelineManager::~PipelineManager()
{
    stopStreaming();

    // Delete source/sink before unreffing the pipeline so that each element's
    // ref count drops from 2→1 here (source/sink unref) and then 1→0 when the
    // pipeline bin is freed below — correct ordering avoids double-free.
    delete source;    source    = nullptr;
    delete sink;      sink      = nullptr;
    delete processor; processor = nullptr;

    // Drop our explicit refs on the optional GPU segment (2→1) before the bin
    // frees its own (1→0), mirroring the source/sink ordering above.
    for (GstElement* e : m_gpuElements)
        if (e) gst_object_unref(e);
    m_gpuElements.clear();

    if (pipeline) {
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
}

errorState PipelineManager::getSourceInformation(std::vector<std::pair<std::string, std::string>>& devicesList)
{
    if (!source) return errorState::NULLPTR_ERR;
    devicesList = source->getDeviceInfoReadable();
    return devicesList.empty() ? errorState::NO_VIDEO_DEVICE_FOUND_ERR : errorState::NO_ERR;
}

errorState PipelineManager::setSourceElement(const std::string& name)
{
    if (!source) return errorState::NULLPTR_ERR;
    if (source->setSourceElement(name) != errorState::NO_ERR)
        return errorState::SET_SOURCE_ELEMENT_ERR;
    return errorState::NO_ERR;
}

errorState PipelineManager::setSourceCaps(int32_t deviceID, int32_t capsIndex)
{
    if (!source) return errorState::NULLPTR_ERR;
    if (source->setCapsFilterElement(deviceID, capsIndex) != errorState::NO_ERR)
        return errorState::SET_SOURCE_CAPS_ERR;
    return errorState::NO_ERR;
}

errorState PipelineManager::setSinkElement(const std::string& name)
{
    if (!sink) return errorState::NULLPTR_ERR;
    if (sink->setSinkElement(name) != errorState::NO_ERR)
        return errorState::SET_SINK_ELEMENT_ERR;
    return errorState::NO_ERR;
}

errorState PipelineManager::setSinkCaps(int32_t deviceID, int32_t capsIndex)
{
    if (!sink) return errorState::NULLPTR_ERR;
    if (sink->setCapsFilterElement(deviceID, capsIndex) != errorState::NO_ERR)
        return errorState::SET_SINK_CAPS_ERR;
    return errorState::NO_ERR;
}

errorState PipelineManager::startStreaming()
{
    errorState result = buildPipeline();
    if (result != errorState::NO_ERR) return result;

    stopRequested.store(false);
    pipelineThread = g_thread_new("pipelineThread", startLoop, this);
    return errorState::NO_ERR;
}

errorState PipelineManager::stopStreaming()
{
    if (!pipelineThread) return errorState::NO_ERR;

    // Signal the bus-polling loop in startLoop to exit.
    stopRequested.store(true);

    g_thread_join(pipelineThread);
    pipelineThread = nullptr;

    if (pipeline)
        gst_element_set_state(pipeline, GST_STATE_NULL);

    stopRequested.store(false);
    return errorState::NO_ERR;
}

std::string PipelineManager::getStreamEndpoint() const
{
    return sink ? sink->endpoint() : std::string();
}

void PipelineManager::ensureProcessor()
{
    if (processor)
        return;
    processor = new FrameProcessor();
    // Adopt the chosen backend immediately. Without this, selecting GPU before
    // the processor exists (as the GUI does: accel, then model/algos) would build
    // a detector that configures on cv::dnn and never switches to the GPU engine.
    processor->setAccel(m_backend);
}

void PipelineManager::setProcessingEnabled(bool enabled)
{
    if (enabled)
        ensureProcessor();
    else if (!enabled && processor) {
        delete processor;
        processor = nullptr;
    }
}

errorState PipelineManager::setAccel(AccelSelection selection)
{
    m_accelSel = selection;
    // Resolve now for an immediate, honest echo (AUTO -> a concrete backend) and
    // apply to a processor that already exists. buildPipeline() re-resolves at
    // the next start, so hardware that appears/disappears in between is still
    // honoured and the resolved backend selects the decode topology there.
    m_backend = resolveBackend(selection);
    if (processor) {
        processor->setAccel(m_backend);
        // Reload an already-built detector on the new engine, so changing the
        // selection after a model was chosen switches backends too — not only
        // when accel is set before the model (the GUI's order).
        processor->setDetectorConfig(processor->detectorConfig());
    }
    return errorState::NO_ERR;
}

errorState PipelineManager::setAlgorithms(const std::vector<std::string>& names)
{
    ensureProcessor();                      // selecting algorithms enables the stage
    if (!processor->valid())
        return errorState::OBJECT_CREATION_ERR;
    processor->setAlgorithms(names);
    return errorState::NO_ERR;
}

errorState PipelineManager::setDetectorModel(const std::string& modelNameOrPath)
{
    // Resolve the name up front. The chain usually has no detector in it yet
    // (the console picks a model before deploying), and without this check a
    // typo would be accepted here and only surface later as an idle stage.
    ModelInfo info;
    if (!modelNameOrPath.empty() && !findModel(modelNameOrPath, info))
        return errorState::LOAD_MODEL_ERR;

    ensureProcessor();                      // model choice survives until "detect" is selected
    if (!processor->valid())
        return errorState::OBJECT_CREATION_ERR;

    DetectorConfig cfg = processor->detectorConfig();
    cfg.model = modelNameOrPath;
    if (!processor->setDetectorConfig(cfg))
        return errorState::LOAD_MODEL_ERR;
    return errorState::NO_ERR;
}

errorState PipelineManager::setDetectorParams(float confidence, float nms, bool drawBoxes)
{
    ensureProcessor();
    if (!processor->valid())
        return errorState::OBJECT_CREATION_ERR;

    DetectorConfig cfg = processor->detectorConfig();
    cfg.confidence = confidence;
    cfg.nms        = nms;
    cfg.drawBoxes  = drawBoxes;
    processor->setDetectorConfig(cfg);      // thresholds alone cannot fail to load
    return errorState::NO_ERR;
}

bool PipelineManager::inferenceStats(InferenceStats& out) const
{
    return processor && processor->valid() && processor->inferenceStats(out);
}

// The GStreamer element chain that does the colorspace convert on the GPU for a
// given backend, or empty if that backend has no offload path here. For a raw
// v4l2 camera there is nothing to "decode"; the win is moving the convert off the
// CPU. We keep the source side in system memory (glupload uploads it) rather than
// importing DMABuf, so device enumeration is unchanged and the only added cost is
// one upload — symmetric with the gldownload that returns the frame to the
// in-place BGR pad probe.
static std::vector<const char*> accelSegmentFactories(AccelBackend /*b*/)
{
    // GPU colorspace-convert segment, DISABLED. The glupload!glcolorconvert!
    // gldownload chain negotiated and streamed but delivered all-black frames to
    // the in-place BGR pad probe on this RADV/GL stack (framemean=0), which
    // starved the detector. Colorspace convert is ~1 ms on the CPU anyway, so the
    // high-value GPU work — the ncnn-Vulkan detector forward pass — is where the
    // win is (≈2.4x). Left as a seam: return the element chain here once the GL
    // download path is validated (or swap in a VA/`va` postproc that keeps data
    // in system memory correctly).
    return {};
}

bool PipelineManager::makeAccelSegment(std::vector<GstElement*>& out) const
{
    out.clear();
    const std::vector<const char*> factories = accelSegmentFactories(m_backend);
    if (factories.empty())
        return false;

    // All-or-nothing: if any element is missing (plugin/driver absent) drop to
    // the CPU topology rather than build a half GPU chain that cannot link.
    for (const char* f : factories) {
        GstElementFactory* fac = gst_element_factory_find(f);
        if (!fac) {
            std::cerr << "accel: element '" << f << "' unavailable; using CPU pipeline\n";
            return false;
        }
        gst_object_unref(fac);
    }

    for (const char* f : factories) {
        GstElement* e = gst_element_factory_make(f, nullptr);
        if (!e) {
            for (GstElement* made : out) gst_object_unref(made);
            out.clear();
            return false;
        }
        out.push_back(e);
    }
    return true;
}

errorState PipelineManager::buildPipeline()
{
    if (!source || !sink) return errorState::NULLPTR_ERR;

    const bool useProcessor = processor && processor->valid();

    // Resolve the operator's selection against detected hardware at start time:
    // AUTO tracks whatever is present now. On a GPU backend, try to build a GPU
    // convert segment; a missing plugin leaves gpuSeg empty and we stay on the
    // CPU topology, so start never fails for lack of a GPU element.
    m_backend = resolveBackend(m_accelSel);
    if (useProcessor)
        processor->setAccel(m_backend);

    std::vector<GstElement*> gpuSeg;
    if (m_backend != AccelBackend::CPU && makeAccelSegment(gpuSeg))
        std::cerr << "accel: GPU convert segment active (" << accelBackendName(m_backend) << ")\n";

    gst_bin_add_many(GST_BIN(pipeline),
        source->sourceElement,
        source->capsFilter,
        source->converter,
        sink->converter,
        sink->sinkElement,
        NULL);

    // gst_bin_add sinks the floating reference of each element so the pipeline
    // now holds the only reference.  Add an explicit ref so that source/sink
    // destructors can safely unref their own pointers without a double-free.
    gst_object_ref(source->sourceElement);
    gst_object_ref(source->capsFilter);
    gst_object_ref(source->converter);
    gst_object_ref(sink->converter);
    gst_object_ref(sink->sinkElement);

    if (useProcessor) {
        gst_bin_add_many(GST_BIN(pipeline), processor->filterElement, NULL);
        gst_object_ref(processor->filterElement);
    }

    // Own the GPU segment the same way: bin sinks the floating ref, we keep an
    // explicit one that the destructor drops (see m_gpuElements).
    for (GstElement* e : gpuSeg) {
        gst_bin_add(GST_BIN(pipeline), e);
        gst_object_ref(e);
        m_gpuElements.push_back(e);
    }

    struct LinkStep { GstElement* from; GstElement* to; const char* desc; };
    std::vector<LinkStep> steps = {
        { source->sourceElement, source->capsFilter, "source → src-capsfilter" },
    };

    // src-capsfilter → [GPU convert segment] → src-converter. The trailing
    // videoconvert still guarantees exact BGR/system-memory for the pad probe,
    // so the unixfdsink memfd contract is untouched whether or not the GPU
    // segment is present.
    if (!gpuSeg.empty()) {
        steps.push_back({ source->capsFilter, gpuSeg.front(), "src-capsfilter → gpu-in" });
        for (size_t i = 1; i < gpuSeg.size(); ++i)
            steps.push_back({ gpuSeg[i - 1], gpuSeg[i], "gpu → gpu" });
        steps.push_back({ gpuSeg.back(), source->converter, "gpu-out → src-converter" });
    } else {
        steps.push_back({ source->capsFilter, source->converter, "src-capsfilter → src-converter" });
    }

    if (useProcessor) {
        // Splice the BGR capsfilter in; its src-pad probe processes each buffer
        // in place (see FrameProcessor), so downstream allocation stays intact.
        steps.push_back({ source->converter,       processor->filterElement, "src-converter → fp-bgr" });
        steps.push_back({ processor->filterElement, sink->converter,         "fp-bgr → sink-queue"    });
    } else {
        steps.push_back({ source->converter,    sink->converter,        "src-converter → sink-queue" });
    }
    steps.push_back({ sink->converter, sink->sinkElement, "sink-queue → sink" });

    for (auto& s : steps) {
        if (!gst_element_link(s.from, s.to)) {
            std::cerr << "Failed to link: " << s.desc << "\n";
            GstCaps* setCaps = nullptr;
            g_object_get(source->capsFilter, "caps", &setCaps, NULL);
            gchar* capsStr = setCaps ? gst_caps_to_string(setCaps) : nullptr;
            std::cerr << "  capsfilter caps: "
                      << (capsStr ? capsStr : "(not set — ANY)") << "\n";
            g_free(capsStr);
            if (setCaps) gst_caps_unref(setCaps);
            return errorState::BUILD_PIPELINE_FAILED;
        }
    }
    return errorState::NO_ERR;
}

gpointer PipelineManager::startLoop(gpointer data)
{
    auto* self = static_cast<PipelineManager*>(data);

    GstStateChangeReturn ret = gst_element_set_state(self->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Pipeline failed to reach PLAYING state.\n";
        return data;
    }

    // Poll the bus for errors/EOS, checking stopRequested every 100 ms.
    // This avoids g_main_loop_run which is unreliable in forked child processes.
    GstBus* bus = gst_element_get_bus(self->pipeline);
    while (!self->stopRequested.load()) {
        GstMessage* msg = gst_bus_timed_pop_filtered(bus,
            100 * GST_MSECOND,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (!msg) continue;

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* err = nullptr;
            gst_message_parse_error(msg, &err, nullptr);
            std::cerr << "Pipeline error: " << (err ? err->message : "unknown") << "\n";
            if (err) g_error_free(err);
        }
        gst_message_unref(msg);
        break;
    }
    gst_object_unref(bus);
    return data;
}
