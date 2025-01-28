#pragma once
#ifdef __cplusplus
extern "C" {
#endif

    enum struct SourceType : int32_t
    {
        NONE_SOURCE,
        FILE_SOURCE,
        CAMERA_SOURCE,
        NETWORK_SOURCE,
        SCREEN_SOURCE,
        TEST_SOURCE,
        CUSTOM_SOURCE
    };
#ifdef __cplusplus
}
#endif