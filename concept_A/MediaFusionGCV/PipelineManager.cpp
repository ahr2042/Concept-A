#include "pch.h"
#include "PipelineManager.h"



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

int32_t PipelineManager::getSourceInformation(int deviceId, std::string& cap)
{
	int32_t result = errorState::NO_ERR;
	((GStreamerSourceCamera*)mediaSources[pipelineManagerInfo.numberOfSources])->devicesContainer.size()
	cap = mediaSources[pipelineManagerInfo.numberOfSources]->getDeviceInfoReadable(deviceId);
	return result;
}