#include "pch.h"
#include "MediaFusionGCV_API.h"
#include "PipelineManager.h"



std::vector<PipelineManager*> pipelines;


errorState mediaLib_GStreamerInit(int argc, char* argv[])
{
	gst_init(&argc, &argv);
	return errorState::NO_ERR;
}

size_t mediaLib_create(SourceType chosenSourceType, SinkType chosenSinkType, const char* pipelineName)
{
	pipelines.push_back(new PipelineManager(chosenSourceType, chosenSinkType, pipelineName));
	return pipelines.size() -1;
}

size_t mediaLib_delete(int32_t pipelineId)
{
	if (pipelines[pipelineId] != nullptr)
	{
		delete pipelines[pipelineId];
		pipelines[pipelineId] = nullptr;		
	}	
	pipelines.erase(pipelines.begin() + pipelineId);
	return pipelines.size() - 1;
}


errorState mediaLib_init(int32_t pipelineId, const char* sourceName, const char* sinkName)
{
	if (pipelines[pipelineId] != nullptr)
	{		
		errorState result = pipelines[pipelineId]->setSourceElement(sourceName);
		if (result == errorState::NO_ERR)
		{
			//result = pipelines[pipelineId]->setSinkElement(sinkName);			
		}
		return result;
	}
	return errorState::NULLPTR_ERR;
}
// ############################################
// This funciton retrieve all detected deviecs as a char*
// @Para sourceType:
//		enum class SourceType { File, Camera, Network, Screen, Test, Custom };
// @Para names: 
// ############################################
errorState mediaLib_getDevices(int32_t pipelineId, size_t& numberOfDevices, deviceProperties** sourceDevices)
{
	if (sourceDevices == nullptr)
	{
		return errorState::NULLPTR_ERR;
	}
	errorState result = errorState::NO_ERR;
	if (*sourceDevices != nullptr)
	{
		delete[] *sourceDevices;
		*sourceDevices = nullptr;
	}
	
	std::list<std::pair<std::string, std::string>> devicesList;
	result = pipelines[pipelineId]->getSourceInformation(devicesList);	
	if (result == errorState::NO_ERR)
	{
		numberOfDevices = devicesList.size();
		*sourceDevices = new deviceProperties[numberOfDevices];
		for (int i = 0; i < numberOfDevices; i++)
		{
			std::pair<std::string, std::string> device = devicesList.front();
			(*sourceDevices)[i].deviceName = device.first;
			(*sourceDevices)[i].formattedDeviceCapabilities = device.second;
			devicesList.pop_front();
		}
	}
	return result;

}

errorState mediaLib_setDevice(int32_t pipelineId, int32_t deviceID, int32_t capIndex)
{
	if (pipelines[pipelineId] != nullptr)
	{
		return pipelines[pipelineId]->setSourceCaps(deviceID, capIndex);
	}
	return errorState::NULLPTR_ERR;

}

errorState mediaLib_startStreaming(int32_t pipelineId)
{

	return pipelines[pipelineId]->startStreaming();
}

errorState mediaLib_stopStreaming(int32_t pipelineId)
{
	return pipelines[pipelineId]->stopStreaming();
}

