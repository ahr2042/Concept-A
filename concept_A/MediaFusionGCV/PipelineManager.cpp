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
	mediaSources[pipelineManagerInfo.numberOfSources]->capsFilter = gst_element_factory_make("capsfilter", "capsfilter");
	mediaSources[pipelineManagerInfo.numberOfSources]->converter = gst_element_factory_make("videoconvert", "converter");
}

int32_t PipelineManager::getSourceInformation(std::list<std::pair<std::string, std::string>>& devicesList)
{
	if (mediaSources[pipelineManagerInfo.numberOfSources] != nullptr)
	{
		devicesList = mediaSources[pipelineManagerInfo.numberOfSources]->getDeviceInfoReadable();
		if (devicesList.empty())
		{
			return (int32_t)errorState::NO_VIDEO_DEVICE_FOUND_ERR;
		}
		return (int32_t)errorState::NO_ERR;
	}
	return (int32_t)errorState::NULLPTR_ERR;
}

int32_t PipelineManager::setSourceElement(std::string deviceName)
{
	if (mediaSources[pipelineManagerInfo.numberOfSources] != nullptr)
	{
		if (mediaSources[pipelineManagerInfo.numberOfSources]->setSourceElement(deviceName) == (int32_t)errorState::NO_ERR)
		{
			pipelineManagerInfo.sourceName = deviceName;
			return (int32_t)errorState::NO_ERR;
		}
		return (int32_t)errorState::SET_SOURCE_ELEMENT_ERR;
	}
	return (int32_t)errorState::NULLPTR_ERR;
}


int32_t PipelineManager::setSourceCaps(int32_t deviceID, int32_t capsIndex)
{
	if (mediaSources[pipelineManagerInfo.numberOfSources] == nullptr)
	{
		return (int32_t)errorState::NULLPTR_ERR;
	}
	
	if (mediaSources[pipelineManagerInfo.numberOfSources]->setCapsFilterElement(deviceID, capsIndex) != (int32_t)errorState::NO_ERR)
	{
		return (int32_t)errorState::SET_SOURCE_CAPS_ERR;
	}	
	return (int32_t)errorState::NO_ERR;
}
