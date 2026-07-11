#pragma once
#ifdef __cplusplus
extern "C" {
#endif

	enum struct SinkType : int32_t
	{
		NONE_SINK,
		SCREEN_SINK,
		FILE_SINK,
		NETWORK_SINK,
		HARDWARE_SINK,
		APPLICATION_SINK,
		DEBUGGING_AND_TESTING_SINK,
		MEDIA_AND_STREAMING_SINK
	};
#ifdef __cplusplus
}
#endif