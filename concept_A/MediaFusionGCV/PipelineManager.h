#pragma once
#include "GStreamerSource.h"
#include "GStreamerSink.h"


class PipelineManager {
public:
    PipelineManager() {};

    errorState getDevices();


private:

    std::vector<GStreamerSource*> mediaSources;
    std::vector<GStreamerSink*> mediaSinks;

    

};