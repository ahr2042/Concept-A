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
    PipelineManager();
    PipelineManager(SourceType , SinkType);

    int32_t getSourceInformation(std::list<std::pair<std::string, std::string>>&);
    //int32_t getSinkDevices(int deviceId, std::list<std::pair<std::string, std::string>>);
    
    int32_t setSourceElement(std::string);
    int32_t setSourceCaps(int32_t, int32_t);

private:
    GstElement* pipeline;
    std::vector<GStreamerSource*> mediaSources;
    std::vector<GStreamerSink*> mediaSinks;

    struct piplineInfo
    {
        SourceType typeOfSource = SourceType::NONE;
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