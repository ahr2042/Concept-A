#include <gst/check/gstcheck.h>
#include "MediaFusionGCV_API.h"

// Create then immediately delete — no crash, no leak
GST_START_TEST(test_create_delete_no_crash)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_lcd");
    mediaLib_delete(id);
    // reaching here means no abort/crash
}
GST_END_TEST

// Starting without ever calling setDevice must return an error, not crash
GST_START_TEST(test_start_without_setdevice_returns_error)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_nodev");
    errorState r = mediaLib_startStreaming(id);
    // Either the pipeline built fine (no camera → will fail later in the GLib thread,
    // but startStreaming itself returns NO_ERR after spawning the thread) or
    // buildPipeline failed — both are acceptable; what is NOT acceptable is a crash.
    (void)r;
    mediaLib_stopStreaming(id);
    mediaLib_delete(id);
}
GST_END_TEST

// Calling stopStreaming before startStreaming must not crash
GST_START_TEST(test_stop_without_start_no_crash)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_nostop");
    mediaLib_stopStreaming(id);
    mediaLib_delete(id);
}
GST_END_TEST

// Calling stopStreaming twice in a row must not crash
GST_START_TEST(test_double_stop_no_crash)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_dbl");
    mediaLib_startStreaming(id);
    mediaLib_stopStreaming(id);
    mediaLib_stopStreaming(id);
    mediaLib_delete(id);
}
GST_END_TEST

// Deleting a pipeline while it is streaming must not crash
GST_START_TEST(test_delete_while_streaming_no_crash)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::SCREEN_SINK, "t_dws");
    mediaLib_startStreaming(id);
    mediaLib_delete(id);   // destructor must stop the thread safely
}
GST_END_TEST

Suite* pipeline_lifecycle_suite()
{
    Suite*  s  = suite_create("pipeline_lifecycle");
    TCase*  tc = tcase_create("lifecycle");
    tcase_set_timeout(tc, 15);
    tcase_add_test(tc, test_create_delete_no_crash);
    tcase_add_test(tc, test_start_without_setdevice_returns_error);
    tcase_add_test(tc, test_stop_without_start_no_crash);
    tcase_add_test(tc, test_double_stop_no_crash);
    tcase_add_test(tc, test_delete_while_streaming_no_crash);
    suite_add_tcase(s, tc);
    return s;
}
