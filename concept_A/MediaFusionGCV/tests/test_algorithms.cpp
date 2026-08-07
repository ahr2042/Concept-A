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

    // A parameterless stage still describes itself — it reports a summary and no
    // knobs, which is not the same as an unknown algorithm reporting nothing.
    const std::string grayscale = mediaLib_algorithmParams("grayscale");
    fail_unless(grayscale.find("key=") == std::string::npos,
        "grayscale declares no parameters, got '%s'", grayscale.c_str());
    fail_unless(grayscale.find("summary=") != std::string::npos,
        "every registered algorithm describes itself, got '%s'", grayscale.c_str());

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

// framediff is the first stage that carries state between frames: a still scene
// must read as still, and something that moves must light up where it moved.
GST_START_TEST(test_framediff_reports_motion_only_where_it_happened)
{
    auto motion = makeAlgorithm("framediff");
    fail_unless(motion != nullptr, "framediff must be registered");

    // Mask mode, so the result is exactly "what moved" with nothing blended in.
    motion->setParams({ { "mode", 1.0 }, { "sensitivity", 75.0 } });

    const cv::Scalar background(30, 60, 90);
    const cv::Rect   startsAt(20, 20, 30, 30);
    const cv::Rect   movesTo(100, 60, 30, 30);

    cv::Mat frame(120, 160, CV_8UC3, background);
    cv::rectangle(frame, startsAt, cv::Scalar(240, 240, 240), -1);

    // First frame only seeds the background model — nothing has moved yet.
    motion->apply(frame);

    // A still scene: feed the same frame again and expect an empty mask.
    cv::Mat still(120, 160, CV_8UC3, background);
    cv::rectangle(still, startsAt, cv::Scalar(240, 240, 240), -1);
    motion->apply(still);
    cv::Mat stillGray;
    cv::cvtColor(still, stillGray, cv::COLOR_BGR2GRAY);
    fail_unless(cv::countNonZero(stillGray) == 0,
        "a still scene reported %d moving pixels", cv::countNonZero(stillGray));

    // Now move the block. Both the vacated and the newly covered area differ
    // from the model, so both should register.
    cv::Mat moved(120, 160, CV_8UC3, background);
    cv::rectangle(moved, movesTo, cv::Scalar(240, 240, 240), -1);
    motion->apply(moved);

    cv::Mat movedGray;
    cv::cvtColor(moved, movedGray, cv::COLOR_BGR2GRAY);
    fail_unless(cv::countNonZero(movedGray(movesTo)) > 0,
        "no motion reported where the block moved to");
    fail_unless(cv::countNonZero(movedGray(startsAt)) > 0,
        "no motion reported where the block moved from");

    // ... and nowhere else: a corner the block never touched must stay clear.
    const cv::Rect untouched(130, 95, 25, 20);
    fail_unless(cv::countNonZero(movedGray(untouched)) == 0,
        "motion reported in a region nothing moved through");
}
GST_END_TEST

// --- motion (background subtraction) ------------------------------------
//
// These stages are statistical rather than pointwise: nothing they say about
// the first frame means anything, so each case feeds a scene until the model
// has settled and only then asks what moved.

namespace {

// A scene with structure in it, so "nothing moved" is a claim about an image
// and not about flat colour. `block` is the thing that will move.
cv::Mat scene(cv::Size size, const cv::Rect& block)
{
    cv::Mat m(size, CV_8UC3, cv::Scalar(30, 60, 90));
    cv::rectangle(m, cv::Rect(0, 0, size.width, size.height / 4),
                  cv::Scalar(70, 70, 70), -1);
    cv::rectangle(m, block, cv::Scalar(240, 240, 240), -1);
    return m;
}

// Hold a scene still until the subtractor accepts it as background. A fresh
// model reports everything as foreground, and KNN in particular needs enough
// frames to fill its sample buffers -- the tests shorten `history` to keep
// that count small.
void settle(Algorithm& algo, const cv::Mat& still, int frames)
{
    for (int i = 0; i < frames; ++i) {
        cv::Mat f = still.clone();
        algo.apply(f);
    }
}

// Moving pixels inside a region of a mask-mode result.
int flagged(const cv::Mat& rendered, const cv::Rect& region)
{
    cv::Mat gray;
    cv::cvtColor(rendered(region), gray, cv::COLOR_BGR2GRAY);
    return cv::countNonZero(gray);
}

// Tuning shared by the cases below: a short history so both models converge in
// a test-sized number of frames, and mask mode so the result is exactly the
// stage's verdict with nothing blended into it.
AlgorithmParams motionTuning(double method, double shadows)
{
    return { { "method", method }, { "history", 100.0 },
             { "mode", 1.0 }, { "shadows", shadows } };
}

constexpr int kSettleFrames = 150;

} // namespace

// The core claim of the stage, asked of both subtractors behind the one knob:
// a scene it has learned is quiet, and a block that moves lights up where it
// went and where it left -- and nowhere else.
GST_START_TEST(test_motion_learns_the_scene_before_flagging_it)
{
    const cv::Size size(160, 120);
    const cv::Rect startsAt(20, 20, 30, 30);
    const cv::Rect movesTo(100, 60, 30, 30);
    const cv::Rect untouched(130, 95, 25, 20);

    for (double method : { 0.0 /* mog2 */, 1.0 /* knn */ }) {
        auto motion = makeAlgorithm("motion");
        fail_unless(motion != nullptr, "motion must be registered");

        // Shadows off here: the vacated area is a dark patch where the model
        // expects a bright block, which is the very thing shadow suppression
        // exists to discard. test_motion_shadow_suppression covers that.
        motion->setParams(motionTuning(method, 0.0));

        const cv::Mat still = scene(size, startsAt);
        settle(*motion, still, kSettleFrames);

        cv::Mat quiet = still.clone();
        motion->apply(quiet);
        const int noise = flagged(quiet, cv::Rect(0, 0, size.width, size.height));
        fail_unless(noise == 0,
            "method %d: a settled, still scene reported %d moving pixels",
            static_cast<int>(method), noise);

        cv::Mat moved = scene(size, movesTo);
        motion->apply(moved);

        fail_unless(flagged(moved, movesTo) > movesTo.area() / 2,
            "method %d: only %d of %d pixels flagged where the block moved to",
            static_cast<int>(method), flagged(moved, movesTo), movesTo.area());
        fail_unless(flagged(moved, startsAt) > startsAt.area() / 2,
            "method %d: only %d of %d pixels flagged where the block moved from",
            static_cast<int>(method), flagged(moved, startsAt), startsAt.area());
        fail_unless(flagged(moved, untouched) == 0,
            "method %d: %d pixels flagged in a region nothing moved through",
            static_cast<int>(method), flagged(moved, untouched));
    }
}
GST_END_TEST

// The analysis runs on a downscaled copy and the mask is scaled back to the
// frame, so a coordinate slip would report motion in the wrong place entirely.
// This is the case that would catch it: a frame big enough to trigger the
// downscale, and a block that crosses to the opposite corner.
GST_START_TEST(test_motion_maps_a_downscaled_mask_back_onto_the_frame)
{
    const cv::Size size(960, 720);            // 2x the 360-row analysis height
    const cv::Rect startsAt(60, 60, 120, 120);
    const cv::Rect movesTo(700, 500, 120, 120);
    const cv::Rect neverTouched(700, 60, 120, 120);

    auto motion = makeAlgorithm("motion");
    motion->setParams(motionTuning(0.0, 0.0));

    const cv::Mat still = scene(size, startsAt);
    settle(*motion, still, kSettleFrames);

    cv::Mat moved = scene(size, movesTo);
    motion->apply(moved);

    fail_unless(moved.rows == size.height && moved.cols == size.width
                && moved.type() == CV_8UC3,
        "the frame came back as %dx%d type %d", moved.cols, moved.rows, moved.type());

    fail_unless(flagged(moved, movesTo) > movesTo.area() / 2,
        "only %d of %d pixels flagged where the block moved to",
        flagged(moved, movesTo), movesTo.area());
    fail_unless(flagged(moved, startsAt) > startsAt.area() / 2,
        "only %d of %d pixels flagged where the block moved from",
        flagged(moved, startsAt), startsAt.area());
    fail_unless(flagged(moved, neverTouched) == 0,
        "%d pixels flagged in the one corner the block never occupied -- the "
        "upscaled mask is not landing where the analysis found motion",
        flagged(moved, neverTouched));
}
GST_END_TEST

// A shadow is a uniformly dimmed version of the background, and reporting one
// as an object is the classic false positive. The flag has to make the
// difference, so the same input is run twice with only that knob moved.
GST_START_TEST(test_motion_shadow_suppression)
{
    const cv::Size size(160, 120);
    const cv::Rect shaded(40, 40, 60, 40);

    // Grey scene, and a patch at 0.7x of it: same chromaticity, lower
    // intensity, which is what the shadow test looks for.
    cv::Mat still(size, CV_8UC3, cv::Scalar(200, 200, 200));
    cv::Mat shadowed = still.clone();
    cv::rectangle(shadowed, shaded, cv::Scalar(140, 140, 140), -1);

    int detected[2] = { 0, 0 };
    for (int shadows = 0; shadows <= 1; ++shadows) {
        auto motion = makeAlgorithm("motion");
        motion->setParams(motionTuning(0.0, static_cast<double>(shadows)));

        settle(*motion, still, kSettleFrames);

        cv::Mat frame = shadowed.clone();
        motion->apply(frame);
        detected[shadows] = flagged(frame, shaded);
    }

    fail_unless(detected[0] > shaded.area() / 2,
        "with shadow detection off a dimmed patch is just foreground, but only "
        "%d of %d pixels were flagged", detected[0], shaded.area());
    fail_unless(detected[1] < shaded.area() / 10,
        "with shadow detection on the dimmed patch should be discarded, but "
        "%d of %d pixels were still flagged", detected[1], shaded.area());
}
GST_END_TEST

// The `background` render mode shows what the model believes the empty scene
// looks like. It is the diagnostic for "why is it flagging everything", so it
// has to show the learned scene and not the frame that was just handed in.
GST_START_TEST(test_motion_can_render_the_learned_background)
{
    const cv::Size size(160, 120);
    const cv::Rect startsAt(20, 20, 30, 30);
    const cv::Rect movesTo(100, 60, 30, 30);

    auto motion = makeAlgorithm("motion");
    motion->setParams({ { "method", 0.0 }, { "history", 100.0 }, { "mode", 2.0 } });

    const cv::Mat still = scene(size, startsAt);
    settle(*motion, still, kSettleFrames);

    // Hand it a frame with the block somewhere else. One frame cannot shift a
    // settled model, so the answer should still be the scene it learned.
    cv::Mat moved = scene(size, movesTo);
    motion->apply(moved);

    fail_unless(moved.rows == size.height && moved.cols == size.width
                && moved.type() == CV_8UC3,
        "background mode returned a %dx%d type %d frame",
        moved.cols, moved.rows, moved.type());

    const double atOldPosition = cv::mean(moved(startsAt))[0];
    const double atNewPosition = cv::mean(moved(movesTo))[0];
    fail_unless(atOldPosition > 200.0,
        "the learned background lost the block that was always there (mean %.1f)",
        atOldPosition);
    fail_unless(atNewPosition < 100.0,
        "the learned background already contains a block seen in one frame "
        "(mean %.1f) -- this is the input, not the model", atNewPosition);
}
GST_END_TEST

// The console builds a combo box from `choices`, so the enum knobs have to
// arrive over the wire with their options intact -- a schema that is
// well-formed in memory but loses its choices in serialization renders as an
// empty dropdown.
GST_START_TEST(test_motion_schema_reaches_the_wire)
{
    const std::string schema = mediaLib_algorithmParams("motion");

    for (const char* key : { "key=method", "key=history", "key=threshold",
                             "key=shadows", "key=learn-rate", "key=mode" })
        fail_unless(schema.find(key) != std::string::npos,
            "motion must expose '%s', got '%s'", key, schema.c_str());

    fail_unless(schema.find("choices=mog2|knn") != std::string::npos,
        "the subtractor choice must reach the client, got '%s'", schema.c_str());
    fail_unless(schema.find("choices=overlay|mask|background") != std::string::npos,
        "the render modes must reach the client, got '%s'", schema.c_str());
    fail_unless(schema.find("type=bool") != std::string::npos,
        "the shadow toggle must be typed as a bool, got '%s'", schema.c_str());

    const size_t id = mediaLib_create(SourceType::CAMERA_SOURCE, SinkType::APPLICATION_SINK, "t");
    fail_unless(mediaLib_setAlgorithms(id, "motion") == errorState::NO_ERR,
        "motion must be selectable as a chain");
    fail_unless(mediaLib_setAlgorithmParams(id, "motion", "method=1,threshold=40")
                    == errorState::NO_ERR,
        "valid motion parameters must be accepted");

    const std::string values = mediaLib_getAlgorithmParams(id, "motion");
    fail_unless(values.find("method=1") != std::string::npos,
        "the subtractor choice did not stick, got '%s'", values.c_str());
    fail_unless(values.find("threshold=40") != std::string::npos,
        "the threshold did not stick, got '%s'", values.c_str());

    mediaLib_delete(id);
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
    tcase_add_test(tc, test_framediff_reports_motion_only_where_it_happened);
    tcase_add_test(tc, test_motion_learns_the_scene_before_flagging_it);
    tcase_add_test(tc, test_motion_maps_a_downscaled_mask_back_onto_the_frame);
    tcase_add_test(tc, test_motion_shadow_suppression);
    tcase_add_test(tc, test_motion_can_render_the_learned_background);
    tcase_add_test(tc, test_motion_schema_reaches_the_wire);
    return s;
}
