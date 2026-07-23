#pragma once

#ifdef _WIN32
	#ifdef MEDIAFUSIONGCV_EXPORTS
		#define MEDIAFUSIONGCV_API __declspec(dllexport)
	#else
		#define MEDIAFUSIONGCV_API __declspec(dllimport)
	#endif
#else
	#ifdef MEDIAFUSIONGCV_EXPORTS
		#define MEDIAFUSIONGCV_API __attribute__((visibility("default")))
	#else
		#define MEDIAFUSIONGCV_API
	#endif
#endif


#include <string>
#include <stdint.h>

// Outside the extern "C" block below: it declares std::string/std::vector
// members, and templates cannot have C linkage.
#include "InferenceStats.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "errorState.h"
#include "SourceType.h"
#include "SinkType.h"



	static const char* sourcesTypes[6] = {
		"File", "Camera", "Network", "Screen", "Test", "Custom"
	};

	static const char* sinksTypes[7] = {
		"Screen", "File", "Network", "Hardware", "Application", "Test", "Media"
	};


	struct deviceProperties
	{
		std::string deviceName = "";
		std::string formattedDeviceCapabilities = "";
	};

	struct pipeLine
	{
		size_t pipelineId = 0;
		SourceType source = SourceType::NONE_SOURCE;
		SinkType sink = SinkType::NONE_SINK;
	};




	MEDIAFUSIONGCV_API errorState mediaLib_GStreamerInit(int argc, char* argv[]);
	MEDIAFUSIONGCV_API size_t mediaLib_create(SourceType, SinkType, const char*);
	MEDIAFUSIONGCV_API errorState mediaLib_init(size_t, const char*, const char*);
	MEDIAFUSIONGCV_API errorState mediaLib_getDevices(size_t, size_t&, deviceProperties**);
	MEDIAFUSIONGCV_API errorState mediaLib_setDevice(size_t, int32_t, int32_t);
	MEDIAFUSIONGCV_API errorState mediaLib_startStreaming(size_t);
	MEDIAFUSIONGCV_API errorState mediaLib_stopStreaming(size_t);

	// Returns the socket path a receiver connects to for this pipeline's frames
	// (empty string for non-IPC sinks). Pointer valid until the next call.
	MEDIAFUSIONGCV_API const char* mediaLib_getStreamEndpoint(size_t);

	// Real-time OpenCV processing: choose the algorithm chain (comma-separated
	// names, e.g. "grayscale,canny"; empty string disables processing). Valid
	// names come from mediaLib_availableAlgorithms() (comma-separated; pointer
	// valid until the next call).
	MEDIAFUSIONGCV_API errorState  mediaLib_setAlgorithms(size_t, const char* csvNames);
	MEDIAFUSIONGCV_API const char* mediaLib_availableAlgorithms();

	// Inference stage — configures the "detect" algorithm. The model is loaded
	// on the spot, so LOAD_MODEL_ERR here means a missing or unreadable graph;
	// an empty name unloads it. Detector settings are remembered per pipeline
	// and survive changes to the algorithm chain.
	MEDIAFUSIONGCV_API errorState  mediaLib_setDetectorModel(size_t, const char* modelNameOrPath);
	MEDIAFUSIONGCV_API errorState  mediaLib_setDetectorParams(size_t, float confidence,
	                                                          float nms, bool drawBoxes);

	// One model per line: "name=<stem> classes=<n> input=<px> path=<file>".
	// Empty when no weights are installed. Pointer valid until the next call.
	MEDIAFUSIONGCV_API const char* mediaLib_availableModels();

	// Last completed inference for this pipeline. NULLPTR_ERR for a bad id,
	// NOT_IMPLEMENTED_YET_ERR when the chain has no inference stage.
	MEDIAFUSIONGCV_API errorState  mediaLib_getInferenceStats(size_t, InferenceStats&);

	MEDIAFUSIONGCV_API size_t mediaLib_delete(size_t);
	MEDIAFUSIONGCV_API void   mediaLib_destroyAll();


#ifdef __cplusplus
	}
#endif