#include <gst/check/gstcheck.h>
#include "MediaFusionGCV_API.h"

// Null pointer passed as output buffer → NULLPTR_ERR
GST_START_TEST(test_getdevices_null_output_ptr)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_null");
    size_t n = 0;
    errorState r = mediaLib_getDevices(id, n, nullptr);
    fail_unless(r == errorState::NULLPTR_ERR,
        "expected NULLPTR_ERR but got %d", (int)r);
    mediaLib_delete(id);
}
GST_END_TEST

// First pipeline always gets id 0
GST_START_TEST(test_create_first_id_is_zero)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_first");
    fail_unless(id == 0, "first pipeline id must be 0, got %zu", id);
    mediaLib_delete(id);
}
GST_END_TEST

// Creating N pipelines yields sequential ids 0..N-1
GST_START_TEST(test_create_sequential_ids)
{
    size_t a = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "ta");
    size_t b = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "tb");
    size_t c = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "tc");
    fail_unless(a == 0 && b == 1 && c == 2,
        "expected ids 0,1,2 got %zu,%zu,%zu", a, b, c);
    // delete in reverse to keep indices stable
    mediaLib_delete(c);
    mediaLib_delete(b);
    mediaLib_delete(a);
}
GST_END_TEST

// getDevices with a pre-allocated buffer: the API must free and reallocate it
GST_START_TEST(test_getdevices_frees_existing_buffer)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_buf");
    size_t n = 0;
    deviceProperties* devs = nullptr;

    // first call (allocates or reports no devices)
    mediaLib_getDevices(id, n, &devs);
    // call again with the existing pointer — API must not double-free or leak
    errorState r = mediaLib_getDevices(id, n, &devs);
    fail_unless(r == errorState::NO_ERR || r == errorState::NO_VIDEO_DEVICE_FOUND_ERR,
        "unexpected error on second getDevices call: %d", (int)r);

    delete[] devs;
    mediaLib_delete(id);
}
GST_END_TEST

Suite* api_null_safety_suite()
{
    Suite*  s  = suite_create("api_null_safety");
    TCase*  tc = tcase_create("null_safety");
    tcase_set_timeout(tc, 15);
    tcase_add_test(tc, test_getdevices_null_output_ptr);
    tcase_add_test(tc, test_create_first_id_is_zero);
    tcase_add_test(tc, test_create_sequential_ids);
    tcase_add_test(tc, test_getdevices_frees_existing_buffer);
    suite_add_tcase(s, tc);
    return s;
}
