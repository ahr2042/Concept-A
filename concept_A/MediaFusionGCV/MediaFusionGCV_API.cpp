#include "pch.h"
#include "MediaFusionGCV_API.h"
#include "PipelineManager.h"

#include "errorState.h"


int32_t mediaLib_create()
{
	return (int32_t)errorState::NO_ERR;
}

int32_t mediaLib_init()
{
	return (int32_t)errorState::NO_ERR;

}

int32_t mediaLib_getDevicesNames(char* names)
{
	return (int32_t)errorState::NO_ERR;

}

int32_t mediaLib_setDevice(int32_t deviceId)
{
	return (int32_t)errorState::NO_ERR;

}

int32_t mediaLib_startStreaming()
{
	return (int32_t)errorState::NO_ERR;

}

int32_t mediaLib_stopStreaming()
{
	return (int32_t)errorState::NO_ERR;

}