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
		mediaSources.push_back(new GStreamerSourceCamera);
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
}
