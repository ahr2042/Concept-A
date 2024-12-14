#pragma once

#include <gst/gst.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include <string>
#include <sstream>
#include <list>
#include <memory>
#include <stdexcept>
#include <map>
#include <vector>
#include <iostream>


#include "SourceType.h"
#include "errorState.h"

#include "GStreamerSource.h"
#include "GStreamerSink.h"
#include "PipelineConfig.h"



class PipelineManager {
public:
    PipelineManager();
    PipelineManager(SourceType );

    int32_t getSourceInformation(std::list<std::pair<std::string, std::string>>&);
    int32_t getSinkDevices(int deviceId, std::list<std::pair<std::string, std::string>>);
    


private:

    std::vector<GStreamerSource*> mediaSources;
    std::vector<GStreamerSink*> mediaSinks;

    struct piplineInfo
    {
        SourceType typeOfSource = SourceType::None;
        int32_t numberOfSources = 0;
        std::string sourceName = "";
        std::string sourceCap = "";

        int32_t numberOfSinks = 0;
        std::string sinkName = "";
        std::string sinkCap = "";
    };

    piplineInfo pipelineManagerInfo;


};