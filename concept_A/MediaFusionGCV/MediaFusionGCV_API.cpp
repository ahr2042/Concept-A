#include "MediaFusionGCV_API.h"
#include "PipelineManager.h"
#include "Algorithms.h"
#include "ModelRegistry.h"
#include "AcceleratorRegistry.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>



std::vector<PipelineManager*> pipelines;


errorState mediaLib_GStreamerInit(int argc, char* argv[])
{
	gst_init(&argc, &argv);

	// Probe acceleration once, here, so the cost (creating a Vulkan instance to
	// enumerate devices) is paid at startup and the detected set is ready for the
	// first 'accelerators' query. Cached thereafter (see AcceleratorRegistry).
	std::string summary;
	for (const auto& a : detectAccelerators())
		if (a.available) {
			if (!summary.empty()) summary += ", ";
			summary += accelBackendName(a.backend);
			if (!a.device.empty()) summary += " (" + a.device + ")";
		}
	std::cerr << "accelerators detected: " << (summary.empty() ? "cpu" : summary) << "\n";
	return errorState::NO_ERR;
}

size_t mediaLib_create(SourceType chosenSourceType, SinkType chosenSinkType, const char* pipelineName)
{
	pipelines.push_back(new PipelineManager(chosenSourceType, chosenSinkType, pipelineName));
	return pipelines.size() -1;
}

size_t mediaLib_delete(size_t pipelineId)
{
	if (pipelineId < pipelines.size())
	{
		delete pipelines[pipelineId];
		pipelines.erase(pipelines.begin() + pipelineId);
	}
	return pipelines.size();
}


errorState mediaLib_init(size_t pipelineId, const char* sourceName, const char* sinkName)
{
	if (pipelineId < pipelines.size() && pipelines[pipelineId] != nullptr)
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
errorState mediaLib_getDevices(size_t pipelineId, size_t& numberOfDevices, deviceProperties** sourceDevices)
{
	if (sourceDevices == nullptr)
	{
		return errorState::NULLPTR_ERR;
	}
	if (pipelineId >= pipelines.size() || pipelines[pipelineId] == nullptr)
	{
		return errorState::NULLPTR_ERR;
	}
	errorState result = errorState::NO_ERR;
	if (*sourceDevices != nullptr)
	{
		delete[] *sourceDevices;
		*sourceDevices = nullptr;
	}
	
	std::vector<std::pair<std::string, std::string>> devicesList;
	result = pipelines[pipelineId]->getSourceInformation(devicesList);
	if (result == errorState::NO_ERR)
	{
		numberOfDevices = devicesList.size();
		*sourceDevices = new deviceProperties[numberOfDevices];
		for (size_t i = 0; i < numberOfDevices; i++)
		{
			(*sourceDevices)[i].deviceName                    = devicesList[i].first;
			(*sourceDevices)[i].formattedDeviceCapabilities   = devicesList[i].second;
		}
	}
	return result;

}

errorState mediaLib_setDevice(size_t pipelineId, int32_t deviceID, int32_t capIndex)
{
	if (pipelineId < pipelines.size() && pipelines[pipelineId] != nullptr)
	{
		return pipelines[pipelineId]->setSourceCaps(deviceID, capIndex);
	}
	return errorState::NULLPTR_ERR;

}

void mediaLib_destroyAll()
{
	for (auto* p : pipelines) delete p;
	pipelines.clear();
	gst_deinit();
}

errorState mediaLib_startStreaming(size_t pipelineId)
{
	if (pipelineId >= pipelines.size() || pipelines[pipelineId] == nullptr)
		return errorState::NULLPTR_ERR;
	return pipelines[pipelineId]->startStreaming();
}

errorState mediaLib_stopStreaming(size_t pipelineId)
{
	if (pipelineId >= pipelines.size() || pipelines[pipelineId] == nullptr)
		return errorState::NULLPTR_ERR;
	return pipelines[pipelineId]->stopStreaming();
}

const char* mediaLib_getStreamEndpoint(size_t pipelineId)
{
	static thread_local std::string endpoint;
	endpoint.clear();
	if (pipelineId < pipelines.size() && pipelines[pipelineId] != nullptr)
		endpoint = pipelines[pipelineId]->getStreamEndpoint();
	return endpoint.c_str();
}

errorState mediaLib_setAlgorithms(size_t pipelineId, const char* csvNames)
{
	if (pipelineId >= pipelines.size() || pipelines[pipelineId] == nullptr)
		return errorState::NULLPTR_ERR;

	std::vector<std::string> names;
	if (csvNames) {
		std::stringstream ss(csvNames);
		std::string item;
		while (std::getline(ss, item, ',')) {
			size_t a = item.find_first_not_of(" \t");
			size_t b = item.find_last_not_of(" \t");
			if (a != std::string::npos)
				names.push_back(item.substr(a, b - a + 1));
		}
	}
	return pipelines[pipelineId]->setAlgorithms(names);
}

const char* mediaLib_availableAlgorithms()
{
	static thread_local std::string csv;
	csv.clear();
	for (const auto& n : availableAlgorithms()) {
		if (!csv.empty()) csv += ',';
		csv += n;
	}
	return csv.c_str();
}

errorState mediaLib_setDetectorModel(size_t pipelineId, const char* modelNameOrPath)
{
	if (pipelineId >= pipelines.size() || pipelines[pipelineId] == nullptr)
		return errorState::NULLPTR_ERR;
	return pipelines[pipelineId]->setDetectorModel(modelNameOrPath ? modelNameOrPath : "");
}

errorState mediaLib_setDetectorParams(size_t pipelineId, float confidence, float nms, bool drawBoxes)
{
	if (pipelineId >= pipelines.size() || pipelines[pipelineId] == nullptr)
		return errorState::NULLPTR_ERR;
	return pipelines[pipelineId]->setDetectorParams(confidence, nms, drawBoxes);
}

const char* mediaLib_availableModels()
{
	static thread_local std::string listing;
	listing.clear();
	for (const auto& m : availableModels()) {
		listing += "name=" + m.name;
		listing += " classes=" + std::to_string(m.classCount);
		listing += " input="   + std::to_string(m.inputSize);
		listing += " path="    + m.path;
		listing += "\n";
	}
	return listing.c_str();
}

errorState mediaLib_getInferenceStats(size_t pipelineId, InferenceStats& stats)
{
	if (pipelineId >= pipelines.size() || pipelines[pipelineId] == nullptr)
		return errorState::NULLPTR_ERR;
	if (!pipelines[pipelineId]->inferenceStats(stats))
		return errorState::NOT_IMPLEMENTED_YET_ERR;
	return errorState::NO_ERR;
}

const char* mediaLib_detectAccelerators()
{
	static thread_local std::string listing;
	listing.clear();
	for (const auto& a : detectAccelerators()) {
		listing += "backend=";    listing += accelBackendName(a.backend);
		listing += " available="; listing += (a.available ? "1" : "0");
		listing += " device=";    listing += (a.device.empty() ? "-" : a.device);
		listing += "\n";
	}
	return listing.c_str();
}

errorState mediaLib_setAccel(size_t pipelineId, int32_t selection)
{
	if (pipelineId >= pipelines.size() || pipelines[pipelineId] == nullptr)
		return errorState::NULLPTR_ERR;
	return pipelines[pipelineId]->setAccel(static_cast<AccelSelection>(selection));
}

