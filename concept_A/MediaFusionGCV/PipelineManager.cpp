#include "pch.h"
#include "PipelineManager.h"


#include "GStreamerSourceCamera.h";
#include "GStreamerSinkScreen.h"


PipelineManager::PipelineManager(SourceType chosenSourceType, SinkType chosenSinkType, const char* pipelineName = "pipeline")
{
	pipeline = gst_pipeline_new(pipelineName);

	switch (chosenSourceType)
	{
	case SourceType::FILE_SOURCE:
		break;
	case SourceType::CAMERA_SOURCE:
		mediaSources.push_back(new GStreamerSourceCamera());
		break;
	case SourceType::NETWORK_SOURCE:
		break;
	case SourceType::SCREEN_SOURCE:
		break;
	case SourceType::TEST_SOURCE:
		break;
	case SourceType::CUSTOM_SOURCE:
		break;
	default:
		break;
	}	

	pipelineManagerInfo.typeOfSource = chosenSourceType;
	pipelineManagerInfo.numberOfSources++;
	pipelineManagerInfo.numberOfSinks++;
	mediaSources[pipelineManagerInfo.numberOfSources]->capsFilter = gst_element_factory_make("capsfilter", "capsfilter");
	mediaSources[pipelineManagerInfo.numberOfSources]->converter = gst_element_factory_make("videoconvert", "converter");	

	switch (chosenSinkType)
	{
	case SinkType::SCREEN_SINK:
		mediaSinks.push_back(new GStreamerSinkScreen(ScreenSinks::AUTOVIDEOSINK));
		break;
	case SinkType::FILE_SINK:
		break;
	case SinkType::NETWORK_SINK:
		break;
	case SinkType::HARDWARE_SINK:
		break;
	case SinkType::APPLICATION_SINK:
		break;
	case SinkType::DEBUGGING_AND_TESTING_SINK:
		break;
	case SinkType::MEDIA_AND_STREAMING_SINK:
		break;
	default:
		break;
	}

	if (!pipeline 
		|| !mediaSinks[pipelineManagerInfo.numberOfSinks]->sinkElement
		|| !mediaSources[pipelineManagerInfo.numberOfSources]->capsFilter 	
		|| !mediaSources[pipelineManagerInfo.numberOfSources]->converter
		|| !mediaSources[pipelineManagerInfo.numberOfSources]->sourceElement
		) 
	{
		g_error("Failed to create elements");		
	}

}

errorState PipelineManager::getSourceInformation(std::list<std::pair<std::string, std::string>>& devicesList)
{
	if (mediaSources[pipelineManagerInfo.numberOfSources] != nullptr)
	{
		devicesList = mediaSources[pipelineManagerInfo.numberOfSources]->getDeviceInfoReadable();
		if (devicesList.empty())
		{
			return errorState::NO_VIDEO_DEVICE_FOUND_ERR;
		}
		return errorState::NO_ERR;
	}
	return errorState::NULLPTR_ERR;
}

errorState PipelineManager::setSourceElement(std::string deviceName)
{
	if (mediaSources[pipelineManagerInfo.numberOfSources] != nullptr)
	{
		if (mediaSources[pipelineManagerInfo.numberOfSources]->setSourceElement(deviceName) == errorState::NO_ERR)
		{
			pipelineManagerInfo.sourceName = deviceName;			
			return errorState::NO_ERR;
		}
		return errorState::SET_SOURCE_ELEMENT_ERR;
	}
	return errorState::NULLPTR_ERR;
}


errorState PipelineManager::setSourceCaps(int32_t deviceID, int32_t capsIndex)
{
	if (mediaSources[pipelineManagerInfo.numberOfSources] == nullptr)
	{
		return errorState::NULLPTR_ERR;
	}
	
	if (mediaSources[pipelineManagerInfo.numberOfSources]->setCapsFilterElement(deviceID, capsIndex) != errorState::NO_ERR)
	{
		return errorState::SET_SOURCE_CAPS_ERR;
	}	
	return errorState::NO_ERR;
}

errorState PipelineManager::setSinkElement(std::string deviceName)
{
	if (mediaSinks[pipelineManagerInfo.numberOfSinks] != nullptr)
	{
		if (mediaSinks[pipelineManagerInfo.numberOfSinks]->setSinkElement(deviceName) == errorState::NO_ERR)
		{
			pipelineManagerInfo.sinkName = deviceName;
			return errorState::NO_ERR;
		}
		return errorState::SET_SINK_ELEMENT_ERR;
	}
	return errorState::NULLPTR_ERR;
}

errorState PipelineManager::setSinkCaps(int32_t deviceID, int32_t capsIndex)
{
	if (mediaSinks[pipelineManagerInfo.numberOfSinks] == nullptr)
	{
		return errorState::NULLPTR_ERR;
	}

	if (mediaSinks[pipelineManagerInfo.numberOfSinks]->setCapsFilterElement(deviceID, capsIndex) != errorState::NO_ERR)
	{
		return errorState::SET_SINK_CAPS_ERR;
	}
	return errorState::NO_ERR;
}

errorState PipelineManager::startStreaming()
{
	errorState result = buildPipeline();
	if (result == errorState::NO_ERR)
	{
		pipleineThread = g_thread_new("pipleineThread", startLoop, this);
		//GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
		//if (ret == GST_STATE_CHANGE_FAILURE) {
		//	std::cerr << "Failed to set pipeline to PLAYING state." << std::endl;
		//	gst_object_unref(pipeline);
		//	return (int32_t)errorState::START_STREAMING_FAILED;
		//}
		return errorState::NO_ERR;
	}
	return result;
}

errorState PipelineManager::stopStreaming()
{
	g_thread_exit(pipleineThread);	
	GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_NULL);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		std::cerr << "Failed to set pipeline to PLAYING state." << std::endl;
		gst_object_unref(pipeline);
		return errorState::STOP_STREAMING_FAILED;
	}
	return errorState::NO_ERR;
}

errorState PipelineManager::buildPipeline()
{
	gst_bin_add_many(GST_BIN(pipeline), mediaSources[pipelineManagerInfo.numberOfSources]->sourceElement, mediaSources[pipelineManagerInfo.numberOfSources]->converter, mediaSinks[pipelineManagerInfo.numberOfSources]->sinkElement, NULL);
    if (!gst_element_link_many(mediaSources[pipelineManagerInfo.numberOfSources]->sourceElement, mediaSources[pipelineManagerInfo.numberOfSources]->converter, mediaSinks[pipelineManagerInfo.numberOfSources]->sinkElement, NULL)) {
        g_error("Failed to link elements");
        gst_object_unref(pipeline);
        return errorState::BUILD_PIPELINE_FAILED;
    }
	return errorState::NO_ERR;
}

gpointer PipelineManager::startLoop(gpointer data)
{
	GstStateChangeReturn ret = gst_element_set_state(((PipelineManager*)data)->pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		std::cerr << "Failed to set pipeline to PLAYING state." << std::endl;
		gst_object_unref(((PipelineManager*)data)->pipeline);
		return data;
	}	
	GMainLoop* loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);	
}
