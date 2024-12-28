#include "pch.h"
#include "MediaFusionGCV_API.h"
#include "PipelineManager.h"



std::vector<PipelineManager*> pipelines;


 int32_t mediaLib_GStreamerInit(int argc, char* argv[])
{
	gst_init(&argc, &argv);
	return (int)errorState::NO_ERR;
}

int32_t mediaLib_create(SourceType chosenSourceType, SinkType chosenSinkType, const char* pipelineName)
{
	pipelines.push_back(new PipelineManager(chosenSourceType, chosenSinkType, pipelineName));
	return pipelines.size() -1;
}

int32_t mediaLib_delete(int32_t pipelineId)
{
	if (pipelines[pipelineId] != nullptr)
	{
		delete pipelines[pipelineId];
		pipelines[pipelineId] = nullptr;		
	}	
	pipelines.erase(pipelines.begin() + pipelineId);
	return pipelines.size() - 1;
}


int32_t mediaLib_init(int32_t pipelineId, const char* sourceName, const char* sinkName)
{
	if (pipelines[pipelineId] != nullptr)
	{		
		int32_t result = pipelines[pipelineId]->setSourceElement(sourceName);
		if (result == (int32_t)errorState::NO_ERR)
		{
			//result = pipelines[pipelineId]->setSinkElement(sinkName);			
		}
		return result;
	}
	return (int32_t)errorState::NULLPTR_ERR;
}
// ############################################
// This funciton retrieve all detected deviecs as a char*
// @Para sourceType:
//		enum class SourceType { File, Camera, Network, Screen, Test, Custom };
// @Para names: 
// ############################################
int32_t mediaLib_getDevices(int32_t pipelineId, int32_t& numberOfDevices, deviceProperties** sourceDevices)
{
	if (sourceDevices == nullptr)
	{
		return (int32_t)errorState::NULLPTR_ERR;
	}
	int32_t result = (int32_t)errorState::NO_ERR;
	if (*sourceDevices != nullptr)
	{
		delete[] *sourceDevices;
		*sourceDevices = nullptr;
	}
	
	std::list<std::pair<std::string, std::string>> devicesList;
	result = pipelines[pipelineId]->getSourceInformation(devicesList);	
	if (result == (int32_t)errorState::NO_ERR)
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

int32_t mediaLib_setDevice(int32_t pipelineId, int32_t deviceID, int32_t capIndex)
{
	if (pipelines[pipelineId] != nullptr)
	{
		return pipelines[pipelineId]->setSourceCaps(deviceID, capIndex);
	}
	return (int32_t)errorState::NULLPTR_ERR;

}

int32_t mediaLib_startStreaming(int32_t pipelineId)
{

	return pipelines[pipelineId]->startStreaming();
}

int32_t mediaLib_stopStreaming(int32_t pipelineId)
{
	return pipelines[pipelineId]->stopStreaming();
}

