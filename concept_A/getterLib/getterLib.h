#pragma once
#ifdef GETTERLIB_EXPORTS
#define GETTERLIB_API __declspec(dllexport)
#else
#define GETTERLIB_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C"
#endif
{
	GETTERLIB_API void API_f_v_create();
	GETTERLIB_API int API_f_i_init();
	GETTERLIB_API int API_f_i_setDevice(int i_i_deviceId);
	GETTERLIB_API int API_f_i_startStreaming();
	GETTERLIB_API int API_f_i_stopStreaming();
	GETTERLIB_API void API_f_v_interpreteError(int i_i_errorId, char* o_pc_interpretation);

#ifdef __cplusplus
}
#endif