#pragma once
#include "GStreamerSource.h"
#include "GStreamerSink.h"
#include "PipelineConfig.h"
#include "SourceType.h"


class PipelineManager {
public:
    PipelineManager();
    PipelineManager(SourceType );

    


private:

    std::vector<GStreamerSource*> mediaSources;
    std::vector<GStreamerSink*> mediaSinks;

    

};