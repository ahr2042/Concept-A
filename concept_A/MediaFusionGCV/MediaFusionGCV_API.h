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

#ifdef __cplusplus
	extern "C" {
#endif

	MEDIAFUSIONGCV_API int32_t mediaLib_create();
	MEDIAFUSIONGCV_API int32_t mediaLib_init();
	MEDIAFUSIONGCV_API int32_t mediaLib_getDevicesNames(char* );
	MEDIAFUSIONGCV_API int32_t mediaLib_setDevice(int32_t );
	MEDIAFUSIONGCV_API int32_t mediaLib_startStreaming();
	MEDIAFUSIONGCV_API int32_t mediaLib_stopStreaming();


#ifdef __cplusplus
	}
#endif