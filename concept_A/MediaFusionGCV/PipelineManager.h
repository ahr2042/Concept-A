#pragma once
#include "GStreamerSource.h"
#include "GStreamerSink.h"


class PipelineManager {
public:
    errorState getDevices();

protected:
    std::vector<GStreamerSource*> mediaSources;
    std::vector<GStreamerSink*> mediaSinks;

    

private:

    
    

};