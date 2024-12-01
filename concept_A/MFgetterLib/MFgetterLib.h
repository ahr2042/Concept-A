#pragma once
#ifdef GETTERLIB_EXPORTS
#define GETTERLIB_API __declspec(dllexport)
#else
#define GETTERLIB_API __declspec(dllimport)
#endif

#define MAX_PARAMETER_SIZE 4096

#ifdef __cplusplus
extern "C"
#endif
{
	GETTERLIB_API int32_t mediaLib_create();
	GETTERLIB_API int32_t mediaLib_init();
	GETTERLIB_API int32_t mediaLib_getDevicesNames(char* o_pc_names);
	GETTERLIB_API int32_t mediaLib_setDevice(int32_t i_i_deviceId);
	GETTERLIB_API int32_t mediaLib_startStreaming();
	GETTERLIB_API int32_t mediaLib_stopStreaming();
	GETTERLIB_API int32_t mediaLib_interpreteError(int32_t i_i_errorId, char* o_pc_interpretation);

#ifdef __cplusplus
}
#endif