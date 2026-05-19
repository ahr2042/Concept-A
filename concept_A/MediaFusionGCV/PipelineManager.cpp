#include "PipelineManager.h"
#include "GStreamerSourceCamera.h"
#include "GStreamerSinkScreen.h"

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
    default:
        break;
    }

    if (source) {
        source->capsFilter = gst_element_factory_make("capsfilter",   "capsfilter");
        source->converter  = gst_element_factory_make("videoconvert", "converter");
    }

    if (!pipeline
        || (source && (!source->sourceElement || !source->capsFilter || !source->converter))
        || (sink   && !sink->sinkElement))
    {
        g_error("Failed to create pipeline elements");
    }
}

PipelineManager::~PipelineManager()
{
    if (mainLoop)
        g_main_loop_quit(mainLoop);
    if (pipelineThread) {
        g_thread_join(pipelineThread);
        pipelineThread = nullptr;
    }
    if (mainLoop) {
        g_main_loop_unref(mainLoop);
        mainLoop = nullptr;
    }
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
    delete source; source = nullptr;
    delete sink;   sink   = nullptr;
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

    mainLoop       = g_main_loop_new(NULL, FALSE);
    pipelineThread = g_thread_new("pipelineThread", startLoop, this);
    return errorState::NO_ERR;
}

errorState PipelineManager::stopStreaming()
{
    if (mainLoop)
        g_main_loop_quit(mainLoop);
    if (pipelineThread) {
        g_thread_join(pipelineThread);
        pipelineThread = nullptr;
    }
    if (mainLoop) {
        g_main_loop_unref(mainLoop);
        mainLoop = nullptr;
    }
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to set pipeline to NULL state.\n";
        return errorState::STOP_STREAMING_FAILED;
    }
    return errorState::NO_ERR;
}

errorState PipelineManager::buildPipeline()
{
    if (!source || !sink) return errorState::NULLPTR_ERR;

    gst_bin_add_many(GST_BIN(pipeline),
        source->sourceElement,
        source->capsFilter,
        source->converter,
        sink->sinkElement,
        NULL);

    struct LinkStep { GstElement* from; GstElement* to; const char* desc; };
    LinkStep steps[] = {
        { source->sourceElement, source->capsFilter,  "source → capsfilter"  },
        { source->capsFilter,    source->converter,   "capsfilter → convert" },
        { source->converter,     sink->sinkElement,   "convert → sink"       },
    };
    for (auto& s : steps) {
        if (!gst_element_link(s.from, s.to)) {
            std::cerr << "Failed to link: " << s.desc << "\n";
            return errorState::BUILD_PIPELINE_FAILED;
        }
    }
    return errorState::NO_ERR;
}

gpointer PipelineManager::startLoop(gpointer data)
{
    PipelineManager* self = static_cast<PipelineManager*>(data);
    GstStateChangeReturn ret = gst_element_set_state(self->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to set pipeline to PLAYING state.\n";
        return data;
    }
    g_main_loop_run(self->mainLoop);
    return data;
}
