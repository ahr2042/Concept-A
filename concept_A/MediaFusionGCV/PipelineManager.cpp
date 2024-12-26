#include "pch.h"
#include "PipelineManager.h"


#include "GStreamerSourceCamera.h";
#include "GStreamerSinkScreen.h"

PipelineManager::PipelineManager()
{

}

PipelineManager::PipelineManager(SourceType chosenSourceType, SinkType chosenSinkType)
{
	switch (chosenSourceType)
	{
	case SourceType::FILE:
		break;
	case SourceType::CAMERA:
		mediaSources.push_back(new GStreamerSourceCamera());
		break;
	case SourceType::NETWORK:
		break;
	case SourceType::SCREEN:
		break;
	case SourceType::TEST:
		break;
	case SourceType::CUSTOM:
		break;
	default:
		break;
	}	

	pipelineManagerInfo.typeOfSource = chosenSourceType;
	pipelineManagerInfo.numberOfSources++;
	mediaSources[pipelineManagerInfo.numberOfSources]->capsFilter = gst_element_factory_make("capsfilter", "capsfilter");
	mediaSources[pipelineManagerInfo.numberOfSources]->converter = gst_element_factory_make("videoconvert", "converter");	

	switch (chosenSinkType)
	{
	case SinkType::SCREEN:
		mediaSinks.push_back(new GStreamerSinkScreen);
		break;
	case SinkType::FILE:
		break;
	case SinkType::NETWORK:
		break;
	case SinkType::HARDWARE:
		break;
	case SinkType::APPLICATION:
		break;
	case SinkType::DEBUGGING_AND_TESTING:
		break;
	case SinkType::MEDIA_AND_STREAMING:
		break;
	default:
		break;
	}
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
