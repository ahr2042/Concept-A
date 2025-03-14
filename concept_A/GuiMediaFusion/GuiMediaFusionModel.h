#pragma once
#include "MediaFusionGCV_API.h"
#include "errorStateGui.h"
#include "guiElements.h"
#include <vector>

class GuiMediaFusionController;

class GuiMediaFusionModel
{	
public:
	GuiMediaFusionModel(int, char**);
	~GuiMediaFusionModel();

	size_t createPipeline(SourceType, SinkType, std::string);
	errorState init();
	errorStateGui getAvailableDevices(size_t);
	errorStateGui startStream(size_t);
	errorStateGui stopStream(size_t);



private:
	std::vector<pipeLine*> pipelineStash;
	
	GuiMediaFusionController* MF_Controller = nullptr;
	
};