#include "pch.h"
#include "PipelineManager.h"


#include "GStreamerSourceCamera.h";

PipelineManager::PipelineManager()
{

}

PipelineManager::PipelineManager(SourceType chosenType)
{
	switch (chosenType)
	{
	case File:
		break;
	case Camera:
		mediaSources.push_back(new GStreamerSourceCamera());
		break;
	case Network:
		break;
	case Screen:
		break;
	case Test:
		break;
	case Custom:
		break;
	default:
		break;
	}	

	pipelineManagerInfo.typeOfSource = chosenType;
	pipelineManagerInfo.numberOfSources++;
}

int32_t PipelineManager::getSourceInformation(std::list<std::pair<std::string, std::string>>& devicesList)
{
	int32_t result = errorState::NO_ERR;	
	devicesList = mediaSources[pipelineManagerInfo.numberOfSources]->getDeviceInfoReadable();
	if (devicesList.empty())
	{
		return NO_VIDEO_DEVICE_FOUND_ERR;
	}

	return result;
}