#include "pch.h"
#include "MediaFusionGCV_API.h"
#include "PipelineManager.h"


#include "errorState.h"

std::vector<PipelineManager*> pipelines;


MEDIAFUSIONGCV_API int32_t mediaLib_GStreamerInit(int argc, char* argv[])
{
	gst_init(&argc, &argv);
}


int32_t mediaLib_create(SourceType chosenType)
{
	pipelines.push_back(new PipelineManager(chosenType));
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
int32_t mediaLib_getDevices(int32_t pipelineId, deviceProperties* sourceDevices)
{
	int32_t result = (int32_t)errorState::NO_ERR;
	std::string tmp = "";
	result = pipelines[pipelineId]->getSourceInformation(-1, tmp);
	sourceDevices = new deviceProperties[result];
	for (int i = 0; i < result; i++)
	{
		sourceDevices[i].deviceName
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

