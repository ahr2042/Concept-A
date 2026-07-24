// Standard and OpenCV headers first, deliberately: gstcheck.h defines a `fail`
// macro that collides with std::basic_ios::fail() once <iostream> is pulled in
// behind it (OpenCV does exactly that).
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <thread>

#include <gst/check/gstcheck.h>
#include "MediaFusionGCV_API.h"
#include "ModelRegistry.h"
#include "Detector.h"
#include "AcceleratorRegistry.h"

// These tests must pass on a machine with no model weights installed — that is
// the state of a fresh clone and of CI, since models/ is gitignored and fetched
// by scripts/fetch-models.sh. Anything that needs a real graph is guarded by
// availableModels() and skipped (not failed) when none is present.

// "detect" must be offered even with no weights on disk: the stage exists, it
// is simply idle until a model is selected.
GST_START_TEST(test_detect_is_a_registered_algorithm)
{
    const std::string algos = mediaLib_availableAlgorithms();
    fail_unless(algos.find("detect") != std::string::npos,
        "expected 'detect' in algorithm list, got '%s'", algos.c_str());
}
GST_END_TEST

// A detector with no model configured must leave frames untouched rather than
// crashing or blanking the stream.
GST_START_TEST(test_detector_without_model_is_a_noop)
{
    DetectorAlgorithm det;
    cv::Mat frame(120, 160, CV_8UC3, cv::Scalar(30, 60, 90));
    const cv::Mat before = frame.clone();

    det.apply(frame);

    fail_unless(cv::countNonZero(cv::Mat(frame != before).reshape(1)) == 0,
        "an unconfigured detector must not modify the frame");

    InferenceStats stats;
    fail_unless(det.snapshotStats(stats), "detector must report stats");
    fail_unless(!stats.modelLoaded, "no model may be loaded");
    fail_unless(stats.framesInferred == 0, "nothing may have been inferred");
}
GST_END_TEST

// A name that resolves to nothing is rejected up front, before any pipeline is
// streaming, so the console can report it at configure time.
GST_START_TEST(test_unknown_model_is_rejected)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::APPLICATION_SINK, "t_model");

    errorState r = mediaLib_setDetectorModel(id, "definitely-not-a-model");
    fail_unless(r == errorState::LOAD_MODEL_ERR,
        "expected LOAD_MODEL_ERR for an unknown model, got %d", (int)r);

    // The empty name is the documented "unload" and must succeed.
    r = mediaLib_setDetectorModel(id, "");
    fail_unless(r == errorState::NO_ERR,
        "unloading the model must succeed, got %d", (int)r);

    mediaLib_delete(id);
}
GST_END_TEST

// Bad pipeline ids follow the same convention as the rest of the API.
GST_START_TEST(test_inference_api_bounds_checks)
{
    const size_t bogus = 9999;
    InferenceStats stats;

    fail_unless(mediaLib_setDetectorModel(bogus, "x")     == errorState::NULLPTR_ERR, "model");
    fail_unless(mediaLib_setDetectorParams(bogus, 0.5f, 0.5f, true) == errorState::NULLPTR_ERR, "params");
    fail_unless(mediaLib_getInferenceStats(bogus, stats)  == errorState::NULLPTR_ERR, "stats");
}
GST_END_TEST

// Thresholds are accepted independently of whether a model is loaded, and a
// pipeline with no inference stage reports "no stats" rather than an error.
GST_START_TEST(test_params_without_model_and_stats_absent)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::APPLICATION_SINK, "t_params");

    fail_unless(mediaLib_setDetectorParams(id, 0.4f, 0.5f, true) == errorState::NO_ERR,
        "thresholds must be settable before a model is chosen");

    InferenceStats stats;
    errorState r = mediaLib_getInferenceStats(id, stats);
    fail_unless(r == errorState::NOT_IMPLEMENTED_YET_ERR,
        "a chain without 'detect' must report NOT_IMPLEMENTED_YET_ERR, got %d", (int)r);

    mediaLib_delete(id);
}
GST_END_TEST

// The registry only ever offers models it can actually point at.
GST_START_TEST(test_model_registry_entries_are_resolvable)
{
    for (const auto& m : availableModels()) {
        fail_unless(!m.name.empty(), "a model with no name was listed");
        fail_unless(!m.path.empty(), "model '%s' has no path", m.name.c_str());

        ModelInfo found;
        fail_unless(findModel(m.name, found),
            "listed model '%s' cannot be looked up by name", m.name.c_str());
        fail_unless(found.path == m.path, "lookup of '%s' returned a different path", m.name.c_str());
    }
}
GST_END_TEST

// With weights present, the full path must work: load, run, publish stats.
// Skipped (with a note) on a machine that has not fetched any.
GST_START_TEST(test_detector_runs_when_a_model_is_installed)
{
    const auto models = availableModels();
    if (models.empty()) {
        GST_INFO("no models installed — skipping (run scripts/fetch-models.sh)");
        return;
    }

    DetectorAlgorithm det;
    DetectorConfig     cfg;
    cfg.model = models.front().name;
    fail_unless(det.configure(cfg), "configure('%s') failed", cfg.model.c_str());

    InferenceStats stats;
    det.snapshotStats(stats);
    fail_unless(stats.modelLoaded, "model '%s' did not load", cfg.model.c_str());

    // Inference is asynchronous: submit frames until the worker publishes one.
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(64, 64, 64));
    bool inferred = false;
    for (int i = 0; i < 100 && !inferred; ++i) {
        cv::Mat copy = frame.clone();
        det.apply(copy);
        det.snapshotStats(stats);
        inferred = stats.framesInferred > 0;
        if (!inferred)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    fail_unless(inferred, "detector never completed an inference on '%s'", cfg.model.c_str());
    fail_unless(stats.lastError.empty(), "detector reported: %s", stats.lastError.c_str());
    fail_unless(stats.inferenceMs > 0.0, "inference time must be recorded");
}
GST_END_TEST

// Detection must always offer CPU, list each backend exactly once, and — on any
// box — never contradict itself. These invariants hold with or without a GPU:
// on a CPU-only build (no WITH_GPU) vulkan/cuda are simply reported unavailable.
GST_START_TEST(test_accelerators_detection_invariants)
{
    const auto& accels = detectAccelerators();
    fail_unless(!accels.empty(), "detection returned nothing");

    bool haveCpu = false;
    int  cpu = 0, vulkan = 0, cuda = 0;
    for (const auto& a : accels) {
        switch (a.backend) {
            case AccelBackend::CPU:    ++cpu;    haveCpu = haveCpu || a.available; break;
            case AccelBackend::VULKAN: ++vulkan; break;
            case AccelBackend::CUDA:   ++cuda;   break;
        }
    }
    fail_unless(haveCpu, "CPU must always be available");
    fail_unless(cpu == 1 && vulkan == 1 && cuda == 1,
        "each backend must appear exactly once (cpu=%d vulkan=%d cuda=%d)", cpu, vulkan, cuda);
}
GST_END_TEST

// resolveBackend() must return a backend that is actually available for every
// selection — so a stale or optimistic pick (e.g. VULKAN on a CPU-only box)
// degrades to CPU rather than failing a stream.
GST_START_TEST(test_resolve_backend_is_always_runnable)
{
    const AccelSelection sels[] = { AccelSelection::AUTO, AccelSelection::CPU,
                                    AccelSelection::VULKAN, AccelSelection::CUDA };
    for (AccelSelection sel : sels) {
        const AccelBackend b = resolveBackend(sel);
        bool available = false;
        for (const auto& a : detectAccelerators())
            if (a.backend == b) available = a.available;
        fail_unless(available, "resolveBackend(%d) chose an unavailable backend %d",
            (int)sel, (int)b);
    }
    fail_unless(resolveBackend(AccelSelection::CPU) == AccelBackend::CPU,
        "explicit CPU must resolve to CPU");
}
GST_END_TEST

// The accel control API: valid selections are accepted on a real pipeline, a
// bad id is rejected like the rest of the API, and detection is serialisable.
GST_START_TEST(test_accel_api)
{
    size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::APPLICATION_SINK, "t_accel");
    for (int sel = 0; sel <= 3; ++sel)          // AUTO, CPU, VULKAN, CUDA
        fail_unless(mediaLib_setAccel(id, sel) == errorState::NO_ERR,
            "selection %d must be accepted", sel);
    fail_unless(mediaLib_setAccel(9999, 0) == errorState::NULLPTR_ERR,
        "a bad pipeline id must be rejected");

    const std::string listing = mediaLib_detectAccelerators();
    fail_unless(listing.find("backend=cpu") != std::string::npos,
        "detection listing must include cpu, got '%s'", listing.c_str());

    mediaLib_delete(id);
}
GST_END_TEST

Suite* inference_suite()
{
    Suite* s = suite_create("inference");
    TCase* tc = tcase_create("general");

    // Loading and running a graph is far slower than the default 4 s limit.
    tcase_set_timeout(tc, 60);

    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_detect_is_a_registered_algorithm);
    tcase_add_test(tc, test_detector_without_model_is_a_noop);
    tcase_add_test(tc, test_unknown_model_is_rejected);
    tcase_add_test(tc, test_inference_api_bounds_checks);
    tcase_add_test(tc, test_params_without_model_and_stats_absent);
    tcase_add_test(tc, test_model_registry_entries_are_resolvable);
    tcase_add_test(tc, test_detector_runs_when_a_model_is_installed);
    tcase_add_test(tc, test_accelerators_detection_invariants);
    tcase_add_test(tc, test_resolve_backend_is_always_runnable);
    tcase_add_test(tc, test_accel_api);
    return s;
}
