#include "pch.h"
#include "MediaFusionGCV_API.h"
#include "PipelineManager.h"



std::vector<PipelineManager*> pipelines;


MEDIAFUSIONGCV_API int32_t mediaLib_GStreamerInit(int argc, char* argv[])
{
	gst_init(&argc, &argv);
	return (int)errorState::NO_ERR;
}


int32_t mediaLib_create(SourceType chosenType)
{
	pipelines.push_back(new PipelineManager(chosenType));
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


int32_t mediaLib_init(int32_t pipelineId, const char* deviceName)
{
	if (pipelines[pipelineId] != nullptr)
	{		
		return pipelines[pipelineId]->setSourceElement(deviceName);
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
			//memcpy(&(sourceDevices[i].deviceName), device.first.c_str(), device.first.size());
			//memcpy(&(sourceDevices[i].formattedDeviceCapabilities), device.second.c_str(), device.second.size());
			(*sourceDevices)[i].deviceName = device.first;
			(*sourceDevices)[i].formattedDeviceCapabilities = device.second;
			devicesList.pop_front();
		}
	}
	return result;

}

int32_t mediaLib_setDevice(int32_t pipelineId, int32_t deviceId)
{
	return (int32_t)errorState::NO_ERR;

}

int32_t mediaLib_startStreaming(int32_t pipelineId)
{
	return (int32_t)errorState::NO_ERR;

}

int32_t mediaLib_stopStreaming(int32_t pipelineId)
{
	return (int32_t)errorState::NO_ERR;

}

