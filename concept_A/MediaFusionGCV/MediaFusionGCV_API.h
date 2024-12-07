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

#include <stdint.h>
#include "SourceType.h"
struct deviceProperties
{
	std::string deviceName = "";
	std::string formattedDeviceCapabilities = "";
};

enum sourceType
{
	File, Camera, Network, Screen, Test, Custom
};

#ifdef __cplusplus
	extern "C" {
#endif
	MEDIAFUSIONGCV_API int32_t mediaLib_GStreamerInit(int argc, char* argv[]);
	MEDIAFUSIONGCV_API int32_t mediaLib_create(SourceType);	
	MEDIAFUSIONGCV_API int32_t mediaLib_init(int32_t );
	MEDIAFUSIONGCV_API int32_t mediaLib_getDevices(int32_t, deviceProperties*);
	MEDIAFUSIONGCV_API int32_t mediaLib_setDevice(int32_t , int32_t );
	MEDIAFUSIONGCV_API int32_t mediaLib_startStreaming(int32_t );
	MEDIAFUSIONGCV_API int32_t mediaLib_stopStreaming(int32_t );

	MEDIAFUSIONGCV_API int32_t mediaLib_delete(int32_t);


#ifdef __cplusplus
	}
#endif