#include <gst/check/gstcheck.h>

extern Suite* api_null_safety_suite();
extern Suite* pipeline_lifecycle_suite();
extern Suite* device_enumeration_suite();
extern Suite* inference_suite();

int main(int argc, char** argv)
{
    gst_check_init(&argc, &argv);

    SRunner* sr = srunner_create(api_null_safety_suite());
    srunner_add_suite(sr, pipeline_lifecycle_suite());
    srunner_add_suite(sr, device_enumeration_suite());
    srunner_add_suite(sr, inference_suite());

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return failed ? 1 : 0;
}
