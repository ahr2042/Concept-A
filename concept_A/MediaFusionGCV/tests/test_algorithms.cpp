// Standard and OpenCV headers first, deliberately: gstcheck.h defines a `fail`
// macro that collides with std::basic_ios::fail() once <iostream> is pulled in
// behind it (OpenCV does exactly that).
#include <opencv2/imgproc.hpp>

#include <string>
#include <vector>

#include <gst/check/gstcheck.h>
#include "MediaFusionGCV_API.h"
#include "Algorithms.h"

// Coverage for the algorithm registry and the parameter seam. The first two
// cases walk the whole registry, so an algorithm added later is checked against
// the Algorithm contract without anyone remembering to write a test for it.

// Every declared schema has to be usable by a client that only knows the
// descriptor: a range it can build a control from, and a default inside it.
GST_START_TEST(test_every_schema_is_well_formed)
{
    for (const auto& info : algorithmRegistry()) {
        fail_unless(info.name != nullptr && *info.name != '\0', "an algorithm has no name");
        fail_unless(info.summary != nullptr && *info.summary != '\0',
            "algorithm '%s' has no summary", info.name);

        // The console round-trips the name through upper case and back, so a
        // name that is not already lowercase would not survive the trip.
        const std::string name = info.name;
        for (char c : name)
            fail_unless(!(c >= 'A' && c <= 'Z'),
                "algorithm name '%s' must be lowercase to survive the GUI round trip",
                info.name);
        fail_unless(name.find(' ') == std::string::npos,
            "algorithm name '%s' must not contain spaces", info.name);

        for (const auto& p : info.schema()) {
            fail_unless(!p.key.empty(), "%s: a parameter has no key", info.name);
            fail_unless(!p.label.empty(), "%s: parameter '%s' has no label",
                info.name, p.key.c_str());
            fail_unless(p.min < p.max, "%s: parameter '%s' has an empty range [%f, %f]",
                info.name, p.key.c_str(), p.min, p.max);
            fail_unless(p.def >= p.min && p.def <= p.max,
                "%s: parameter '%s' default %f is outside [%f, %f]",
                info.name, p.key.c_str(), p.def, p.min, p.max);
            fail_unless(p.step > 0.0, "%s: parameter '%s' has a non-positive step",
                info.name, p.key.c_str());

            if (p.type == AlgorithmParam::Type::Enum)
                fail_unless(!p.choices.empty(),
                    "%s: enum parameter '%s' offers no choices", info.name, p.key.c_str());
            else
                fail_unless(p.choices.empty(),
                    "%s: non-enum parameter '%s' must not carry choices",
                    info.name, p.key.c_str());
        }
    }
}
GST_END_TEST

// The contract in Algorithm.h: apply() may not change the frame's size or type,
// because the pipeline caps are fixed for the buffer's lifetime. Breaking this
// takes the stream down, so it is worth enforcing mechanically.
GST_START_TEST(test_every_algorithm_preserves_frame_geometry)
{
    for (const auto& info : algorithmRegistry()) {
        auto algo = info.make();
        fail_unless(algo != nullptr, "registry entry '%s' produced nothing", info.name);
        fail_unless(std::string(algo->name()) == info.name,
            "registry name '%s' does not match the object's name '%s'",
            info.name, algo->name());

        // Something with structure in it, so a stage that keys off content has
        // work to do rather than seeing flat colour.
        cv::Mat frame(120, 160, CV_8UC3, cv::Scalar(30, 60, 90));
        cv::rectangle(frame, cv::Rect(40, 30, 50, 40), cv::Scalar(220, 220, 220), -1);

        // Defaults first, then the extremes of every knob: a stage must not be
        // able to crash or resize the frame at either end of its own range.
        algo->setParams(defaultParams(info.schema()));
        algo->apply(frame);
        for (const auto& p : info.schema()) {
            for (double v : { p.min, p.max }) {
                algo->setParams({ { p.key, v } });
                algo->apply(frame);
            }
        }

        fail_unless(frame.rows == 120 && frame.cols == 160,
            "%s resized the frame to %dx%d", info.name, frame.cols, frame.rows);
        fail_unless(frame.type() == CV_8UC3,
            "%s changed the frame type to %d", info.name, frame.type());
        fail_unless(frame.isContinuous() || frame.step >= 160 * 3,
            "%s replaced the frame with an incompatible buffer", info.name);
    }
}
GST_END_TEST

// The registry is the single source of truth: the name list and the factory
// must agree, which is exactly what the two hand-synced lists could not
// guarantee before.
GST_START_TEST(test_registry_is_the_only_source_of_names)
{
    const std::vector<std::string> names = availableAlgorithms();
    fail_unless(names.size() == algorithmRegistry().size(),
        "availableAlgorithms() lists %zu names for %zu registry entries",
        names.size(), algorithmRegistry().size());

    for (const auto& n : names) {
        fail_unless(findAlgorithm(n) != nullptr, "'%s' is listed but not in the registry",
            n.c_str());
        auto algo = makeAlgorithm(n);
        fail_unless(algo != nullptr, "'%s' is listed but the factory cannot build it",
            n.c_str());
    }

    fail_unless(makeAlgorithm("no-such-algorithm") == nullptr,
        "the factory invented an algorithm that is not registered");
    fail_unless(findAlgorithm("no-such-algorithm") == nullptr,
        "lookup found an algorithm that is not registered");
}
GST_END_TEST

// A typo used to be skipped silently, leaving an empty chain that still
// reported success. The whole request is rejected instead.
GST_START_TEST(test_unknown_algorithm_name_is_rejected)
{
    const size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::APPLICATION_SINK, "t");

    fail_unless(mediaLib_setAlgorithms(id, "grayscale") == errorState::NO_ERR,
        "a known algorithm must be accepted");
    fail_unless(mediaLib_setAlgorithms(id, "grayscale,typo") == errorState::INVALID_ARGS_ERR,
        "a chain containing an unknown name must be rejected");
    fail_unless(mediaLib_setAlgorithms(id, "") == errorState::NO_ERR,
        "an empty chain must stay valid -- it is how processing is disabled");

    mediaLib_delete(id);
}
GST_END_TEST

// Canny's thresholds were hardcoded at 80/160; they are the proof that the
// generic seam reaches a stage with no special-casing anywhere.
GST_START_TEST(test_canny_thresholds_are_tunable)
{
    const std::string schema = mediaLib_algorithmParams("canny");
    fail_unless(schema.find("key=low") != std::string::npos,
        "canny must expose a 'low' threshold, got '%s'", schema.c_str());
    fail_unless(schema.find("key=high") != std::string::npos,
        "canny must expose a 'high' threshold, got '%s'", schema.c_str());

    const size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::APPLICATION_SINK, "t");

    fail_unless(mediaLib_setAlgorithmParams(id, "canny", "low=90,high=200") == errorState::NO_ERR,
        "valid canny parameters must be accepted");

    // Out of range is clamped, not refused: a slider should never fail.
    fail_unless(mediaLib_setAlgorithmParams(id, "canny", "low=9999") == errorState::NO_ERR,
        "an out-of-range value must be clamped rather than rejected");
    const std::string values = mediaLib_getAlgorithmParams(id, "canny");
    fail_unless(values.find("low=255") != std::string::npos,
        "low should have been clamped to 255, got '%s'", values.c_str());
    fail_unless(values.find("high=200") != std::string::npos,
        "high should have kept its set value, got '%s'", values.c_str());

    // Unknown keys, unparsable numbers and unknown algorithms are all errors.
    fail_unless(mediaLib_setAlgorithmParams(id, "canny", "nosuch=1") == errorState::INVALID_ARGS_ERR,
        "an unknown parameter key must be rejected");
    fail_unless(mediaLib_setAlgorithmParams(id, "canny", "low=abc") == errorState::INVALID_ARGS_ERR,
        "an unparsable value must be rejected");
    fail_unless(mediaLib_setAlgorithmParams(id, "nosuch", "low=1") == errorState::INVALID_ARGS_ERR,
        "an unknown algorithm must be rejected");

    // A parameterless stage has an empty schema but is still a valid target.
    fail_unless(std::string(mediaLib_algorithmParams("grayscale")).empty(),
        "grayscale declares no parameters");

    mediaLib_delete(id);
}
GST_END_TEST

// Tuning is remembered per pipeline and replayed onto a stage that joins later,
// so the console can set a control before deploying, in either order.
GST_START_TEST(test_params_survive_a_chain_rebuild)
{
    const size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::APPLICATION_SINK, "t");

    // Set before "canny" is in the chain at all.
    fail_unless(mediaLib_setAlgorithmParams(id, "canny", "low=42") == errorState::NO_ERR,
        "parameters must be accepted before the stage is selected");
    fail_unless(mediaLib_setAlgorithms(id, "canny") == errorState::NO_ERR,
        "canny must be selectable");

    std::string values = mediaLib_getAlgorithmParams(id, "canny");
    fail_unless(values.find("low=42") != std::string::npos,
        "a value set before the stage joined was lost, got '%s'", values.c_str());

    // Rebuilding the chain must not reset it either.
    fail_unless(mediaLib_setAlgorithms(id, "grayscale,canny") == errorState::NO_ERR,
        "the chain must be replaceable");
    values = mediaLib_getAlgorithmParams(id, "canny");
    fail_unless(values.find("low=42") != std::string::npos,
        "rebuilding the chain reset the tuning, got '%s'", values.c_str());

    mediaLib_delete(id);
}
GST_END_TEST

// Bad ids must be bounced the way every other id-taking entry point does.
GST_START_TEST(test_param_api_bounds_checks)
{
    fail_unless(mediaLib_setAlgorithmParams(9999, "canny", "low=1") == errorState::NULLPTR_ERR,
        "a bad pipeline id must return NULLPTR_ERR");
    fail_unless(std::string(mediaLib_getAlgorithmParams(9999, "canny")).empty(),
        "a bad pipeline id must yield no values");
    fail_unless(std::string(mediaLib_algorithmParams("no-such-algorithm")).empty(),
        "an unknown algorithm has no schema");
    fail_unless(std::string(mediaLib_algorithmParams(nullptr)).empty(),
        "a null name must not crash");
}
GST_END_TEST

Suite* algorithms_suite()
{
    Suite*   s  = suite_create("algorithms");
    TCase*   tc = tcase_create("general");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_every_schema_is_well_formed);
    tcase_add_test(tc, test_every_algorithm_preserves_frame_geometry);
    tcase_add_test(tc, test_registry_is_the_only_source_of_names);
    tcase_add_test(tc, test_unknown_algorithm_name_is_rejected);
    tcase_add_test(tc, test_canny_thresholds_are_tunable);
    tcase_add_test(tc, test_params_survive_a_chain_rebuild);
    tcase_add_test(tc, test_param_api_bounds_checks);
    return s;
}
