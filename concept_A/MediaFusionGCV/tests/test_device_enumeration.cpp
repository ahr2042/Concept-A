#include <gst/check/gstcheck.h>
#include "MediaFusionGCV_API.h"

// getDevices must succeed or report no camera — never return an unexpected code
GST_START_TEST(test_getdevices_graceful_no_camera)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_gd");
    size_t n  = 0;
    deviceProperties* devs = nullptr;
    errorState r = mediaLib_getDevices(id, n, &devs);
    fail_unless(r == errorState::NO_ERR || r == errorState::NO_VIDEO_DEVICE_FOUND_ERR,
        "getDevices returned unexpected code %d", (int)r);
    if (r == errorState::NO_ERR)
        fail_unless(n > 0, "NO_ERR but numberOfDevices is 0");
    else
        fail_unless(n == 0, "NO_VIDEO_DEVICE_FOUND_ERR but numberOfDevices is non-zero");
    delete[] devs;
    mediaLib_delete(id);
}
GST_END_TEST

// Negative device id must be rejected with SET_SOURCE_CAPS_ERR
GST_START_TEST(test_setdevice_negative_device_id)
{
    size_t id  = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_ndi");
    errorState r = mediaLib_setDevice(id, -1, 0);
    fail_unless(r == errorState::SET_SOURCE_CAPS_ERR,
        "expected SET_SOURCE_CAPS_ERR for negative deviceId, got %d", (int)r);
    mediaLib_delete(id);
}
GST_END_TEST

// Negative cap index must be rejected with SET_SOURCE_CAPS_ERR
GST_START_TEST(test_setdevice_negative_cap_index)
{
    size_t id  = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_nci");
    errorState r = mediaLib_setDevice(id, 0, -1);
    fail_unless(r == errorState::SET_SOURCE_CAPS_ERR,
        "expected SET_SOURCE_CAPS_ERR for negative capIndex, got %d", (int)r);
    mediaLib_delete(id);
}
GST_END_TEST

// Out-of-range device id must be rejected with SET_SOURCE_CAPS_ERR
GST_START_TEST(test_setdevice_oob_device_id)
{
    size_t id  = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_obd");
    errorState r = mediaLib_setDevice(id, 9999, 0);
    fail_unless(r == errorState::SET_SOURCE_CAPS_ERR,
        "expected SET_SOURCE_CAPS_ERR for oob deviceId, got %d", (int)r);
    mediaLib_delete(id);
}
GST_END_TEST

// Out-of-range cap index must be rejected with SET_SOURCE_CAPS_ERR
GST_START_TEST(test_setdevice_oob_cap_index)
{
    size_t id  = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_obc");
    size_t n   = 0;
    deviceProperties* devs = nullptr;
    errorState found = mediaLib_getDevices(id, n, &devs);
    delete[] devs;

    if (found == errorState::NO_ERR && n > 0) {
        // device 0 exists — ask for an impossible cap index
        errorState r = mediaLib_setDevice(id, 0, 99999);
        fail_unless(r == errorState::SET_SOURCE_CAPS_ERR,
            "expected SET_SOURCE_CAPS_ERR for oob capIndex, got %d", (int)r);
    }
    // if no camera: skip (setDevice with oob deviceId is already covered above)
    mediaLib_delete(id);
}
GST_END_TEST

Suite* device_enumeration_suite()
{
    Suite*  s  = suite_create("device_enumeration");
    TCase*  tc = tcase_create("enumeration");
    tcase_set_timeout(tc, 15);
    tcase_add_test(tc, test_getdevices_graceful_no_camera);
    tcase_add_test(tc, test_setdevice_negative_device_id);
    tcase_add_test(tc, test_setdevice_negative_cap_index);
    tcase_add_test(tc, test_setdevice_oob_device_id);
    tcase_add_test(tc, test_setdevice_oob_cap_index);
    suite_add_tcase(s, tc);
    return s;
}
