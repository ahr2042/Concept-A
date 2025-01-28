#pragma once

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "SourceType.h"
#include "SinkType.h"
#include "errorState.h"

#include "GStreamerSource.h"
#include "GStreamerSink.h"
#include "PipelineConfig.h"

#include <string>
#include <sstream>
#include <list>
#include <memory>
#include <stdexcept>
#include <map>
#include <vector>
#include <iostream>


class PipelineManager {
public:
    PipelineManager(SourceType , SinkType, const char*);

    errorState getSourceInformation(std::list<std::pair<std::string, std::string>>&);
    //int32_t getSinkDevices(int deviceId, std::list<std::pair<std::string, std::string>>);
    
    errorState setSourceElement(std::string);
    errorState setSourceCaps(int32_t, int32_t);

    errorState setSinkElement(std::string);
    errorState setSinkCaps(int32_t, int32_t);

    errorState startStreaming();
    errorState stopStreaming();


    

private:
    GstElement* pipeline = nullptr;
    GThread* pipleineThread = nullptr;
    std::vector<GStreamerSource*> mediaSources;
    std::vector<GStreamerSink*> mediaSinks;
    errorState buildPipeline();
    static gpointer startLoop(gpointer data);
    struct piplineInfo
    {
        SourceType typeOfSource = SourceType::NONE_SOURCE;
        int32_t numberOfSources = -1;
        std::string sourceName = "";
        std::string sourceCap = "";

        int32_t numberOfSinks = -1;
        std::string sinkName = "";
        std::string sinkCap = "";


    };

    struct _CustomData {
        GstElement* source;
        GstElement* convert;
        GstElement* resample;
        GstElement* sink;
        GstElement* pipeline;
    };

    piplineInfo pipelineManagerInfo;



};