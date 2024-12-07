#pragma once
#include "GStreamerSource.h"
#include "GStreamerSink.h"
#include "PipelineConfig.h"
#include "SourceType.h"


class PipelineManager {
public:
    PipelineManager();
    PipelineManager(SourceType );

    int32_t getSourceInformation(int deviceId, std::string&);
    int32_t getSinkDevices(int deviceId, std::string);
    


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