#include "stdafx.h"
#include "GuiMediaFusionModel.h"
#include "GuiMediaFusionController.h"
#include <iostream>

GuiMediaFusionModel::GuiMediaFusionModel(int mainArgc, char** mainArgv)
{
	(mediaLib_GStreamerInit(mainArgc, mainArgv));
	MF_Controller = new GuiMediaFusionController;
}

GuiMediaFusionModel::~GuiMediaFusionModel()
{
}

size_t GuiMediaFusionModel::createPipeline(SourceType selectedSource, SinkType selectedSink, std::string pipelineName)
{	
	pipelineStash.push_back(new pipeLine);

	size_t stashSize = pipelineStash.size() - 1;
	size_t piplineID = mediaLib_create(selectedSource, selectedSink, pipelineName.c_str());
	pipelineStash[stashSize]->pipelineId = piplineID;
	pipelineStash[stashSize]->source = selectedSource;
	pipelineStash[stashSize]->sink = selectedSink;	
	return piplineID;
}

errorState GuiMediaFusionModel::init()
{
	return errorState();
}

errorStateGui GuiMediaFusionModel::getAvailableDevices(size_t piplineID)
{
	size_t devicesCount = 0;
	deviceProperties* devices = nullptr;
	if (mediaLib_getDevices(piplineID, devicesCount, &devices) == errorState::NO_ERR)
	{
		if (devices != nullptr)
		{
			for (int i = 0; i < devicesCount; i++)
			{ 
				std::cout << "Device number " << i + 1 << ":" << std::endl;
				//std::cout << foundDevices[i].deviceName << std::endl << foundDevices[i].formattedDeviceCapabilities << std::endl;
			}
		}
	}
	return errorStateGui::GET_AVAILABLE_DEVICES_ERR;
}
errorStateGui GuiMediaFusionModel::startStream(size_t piplineID)
{
	if (piplineID >= 0)
	{
		mediaLib_startStreaming(piplineID);
	}
	return errorStateGui::NO_ERR;
}
errorStateGui GuiMediaFusionModel::stopStream(size_t piplineID)
{
	if (piplineID >= 0)
	{
		mediaLib_stopStreaming(piplineID);
	}
	return errorStateGui::NO_ERR;
}
