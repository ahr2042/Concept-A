#include "pch.h"
#include "MediaFusionGCV_API.h"
#include "PipelineManager.h"

#include "errorState.h"

std::vector<PipelineManager*> pipelines;

int32_t mediaLib_create()
{
	pipelines.push_back(new PipelineManager);
	return pipelines.size() -1;
}

int32_t mediaLib_delete(int32_t pipelineId)
{
	delete pipelines[pipelineId];
	pipelines[pipelineId] = nullptr;
	pipelines.erase(pipelines.begin() + pipelineId);
	return pipelines.size() - 1;
}


int32_t mediaLib_init(int32_t pipelineId)
{

	return (int32_t)errorState::NO_ERR;

}
// ############################################
// This funciton retrieve all detected deviecs as a char*
// @Para sourceType:
//		enum class SourceType { File, Camera, Network, Screen, Test, Custom };
// @Para names: 
// ############################################
int32_t mediaLib_getDevices(int32_t pipelineId, sourceType typeId, deviceProperties* propertiesStructure)
{
	switch (typeId)
	{
	case File:
		return (int32_t)NOT_IMPLEMENTED_YET_ERR;
		break;
	case Camera:
		pipelines[pipelineId]->getDevices();
		break;
	case Network:
		return (int32_t)NOT_IMPLEMENTED_YET_ERR;
		break;
	case Screen:
		return (int32_t)NOT_IMPLEMENTED_YET_ERR;
		break;
	case Test:
		return (int32_t)NOT_IMPLEMENTED_YET_ERR;
		break;
	case Custom:
		return (int32_t)NOT_IMPLEMENTED_YET_ERR;
		break;
	default:
		return (int32_t)UNDEFINED_DEVICE_TYPE_ID_ERR;
		break;
	}
	
	return (int32_t)errorState::NO_ERR;

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

