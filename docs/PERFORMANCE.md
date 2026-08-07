# Performance backlog

Working record for the multi-camera performance effort. The list outlives any
single session: each item says where the cost is, what it should buy, and how to
tell whether it worked. **Update the status column in the same change that lands
an item**, the way `README.md`'s feature tables are kept.

Opened after a report that a second camera dropped heavily and that CPU, RAM and
GPU use were all high.

## How to measure

```bash
# which cap index is which
printf 'create camera app t\ndevices 0\nquit\n' | ./concept_A/x64_debug/MediaFusionGCV

# engine throughput for one capture mode (GUI not involved)
scripts/bench-capture.sh <cap-index> [label] [algos-csv]

# whole path including the console viewport
./concept_A/x64_debug/GUIMediaFusion --selftest-stream
```

Record with every number: cap index and geometry, algorithm chain, accel
selection, camera count, and build type. Frame rate alone is not a result —
`dropped:` from the bench script and `skipped=` from `stats <id>` say whether the
pipeline is shedding work to keep up.

Two traps on this hardware, both hit while taking the baseline below:

- **Auto-exposure sets the frame rate.** In office light this camera settles at
  20 fps at 720p and 15 fps at 1080p regardless of what the caps advertise, and
  it is stable to ±0.1 fps, so it reads exactly like a hard pipeline ceiling.
  Compare modes under the same lighting, in one sitting.
- **The camera arrives through PipeWire**, not `v4l2src` (`camera-source
  element: pipewiresrc` in the daemon log). PipeWire does its own negotiation,
  so v4l2-level reasoning about buffers and alt-settings does not transfer
  directly.

## Baseline

Ryzen-class 12-core, Radeon RX 6750 XT (RADV), 16 GB, GStreamer 1.24.2,
OpenCV 4.12, HP 320 FHD Webcam via PipeWire. One camera, no processing chain,
after P1–P5. Measured 2026-07-25.

| Capture mode | fps | dropped |
|---|---|---|
| MJPEG 1920×1080 | 15.1 | 0 |
| MJPEG 1280×720 | 20.1 | 0 |
| MJPEG 640×480 | 20.1 | 0 |
| **raw YUY2 1920×1080** | **4.5** | 0 |

The last row is the headline: this camera only advertises **5 fps** for raw
1080p, against **30 fps** for MJPEG at the same geometry. Before P5 the raw mode
was the only thing the engine could select, so 1080p was hard-capped at 5 fps for
a single camera — before any question of a second one.

**Not yet measured:** anything with two cameras attached, and anything with the
`detect` stage live. Both are gaps in this baseline; fill them when the hardware
is on the desk.

### Cost of a processing stage

One camera, MJPEG 1280×720, RelWithDebInfo, CPU accel, measured 2026-08-05 in one
sitting. `framediff` at its defaults and `canny` are both **free at the rate this
camera delivers**:

| Chain | fps | dropped |
|---|---|---|
| (none) | 15.06 | 0 |
| `framediff` | 15.06 | 0 |
| `canny` | 15.07 | 0 |

Re-measured 2026-08-07 in better light, where the same mode delivered 24 fps —
`motion` is free at that rate too:

| Chain | fps | dropped |
|---|---|---|
| (none) | 24.03 | 0 |
| `framediff` | 24.03 | 0 |
| `motion` | 24.02 | 0 |

Read both tables as budget statements, not as "these stages are free": each frame
had 40–66 ms of slack, so a stage costing a few ms cannot show up. The number
that would move first is `dropped:`, and it did not.

### Cost of a processing stage, measured

The camera cannot be driven fast enough to expose a per-frame cost, so these come
from timing `Algorithm::apply()` directly over 120 synthetic frames after a
60-frame warm-up, with the OpenCV pool capped at 3 as the engine caps it (P3).
Measured 2026-08-07, RelWithDebInfo, CPU.

| Stage | 1280×720 | 1920×1080 |
|---|---|---|
| `framediff` | 2.42 ms | 3.43 ms |
| `motion` (mog2, overlay) | 2.24 ms | 4.42 ms |
| `motion` (knn, overlay) | 2.20 ms | 4.35 ms |
| `motion` (mog2, mask) | 1.46 ms | 2.80 ms |
| *control:* MOG2 driven at full frame | 4.59 ms | 10.70 ms |

The last row is the reason the motion family downscales. Running the subtractor
at the full frame costs 10.7 ms at 1080p — squarely in the 10–15 ms the note
below predicted, and a quarter of a 24 fps frame budget spent on one stage.
Against a 360-row analysis copy the same stage is 4.4 ms, so the subtractor
itself has stopped scaling with capture resolution and what is left is the resize
and the full-res draw.

KNN and MOG2 cost the same here, so the choice between them is about scene
behaviour rather than budget.

Two things fall out of that. Choosing a bigger capture mode no longer makes the
motion analysis more expensive, only the resize and the blend. And `overlay` mode
costs ~1.6 ms more than `mask` at 1080p — a full-frame `clone` + `addWeighted`
that a later pass could narrow to the mask's bounding box.

## Landed

| # | Item | Where |
|---|---|---|
| P1 | Default `CMAKE_BUILD_TYPE` to `RelWithDebInfo`. With no build type CMake passes no `-O` flag at all, and the hottest code is this project's own — the per-frame pad probe and `parseYoloOutput`'s scan over ~25k anchors per inference | both `CMakeLists.txt` |
| P2 | `videoconvert n-threads=0`. It defaults to **1**, so the per-camera colourspace convert was single-threaded on both sides | `PipelineManager.cpp` ctor, `StreamReceiver.cpp` |
| P3 | Cap OpenCV's thread pool at ¼ of the cores (`MEDIAFUSION_CV_THREADS` overrides; `0` disables the cap). Left alone OpenCV takes every core for each `forward()`, so N cameras meant N×cores runnable threads evicting the capture threads | `MediaFusionGCV_API.cpp` |
| P4 | `queue max-size-buffers=3 leaky=downstream` directly after the source. The only queue used to be at the *end* of the chain, so capture, convert and every OpenCV stage ran in one thread and any hiccup stalled buffer dequeue — driver-level frame drops | `GStreamerSource.h`, `PipelineManager.cpp` |
| P5 | **MJPEG capture.** Enumeration filtered everything except `video/x-raw`, so only uncompressed modes were selectable; a decoder (`jpegdec`, else `avdec_mjpeg`) is now spliced in when an encoded mode is chosen. 720p YUYV is ~55 MB/s (~440 Mbit/s) — two cameras cannot both fit on one USB 2.0 bus (480 Mbit/s shared) and the UVC driver starves the second one | `GStreamerSourceCamera.cpp`, `PipelineManager.cpp`, `DeviceParser.cpp` |

## Backlog

Payoff is an estimate from reading the code unless it says *measured*. Effort is
S (a sitting), M (a session), L (multi-session or needs a design decision).

| # | Item | Payoff | Effort | Status |
|---|---|---|---|---|
| P6 | Skip `videoconvert` + the BGR capsfilter entirely when no algorithms are active | high | S | open |
| P7 | Letterbox straight to `inputSize` instead of `squarePadded()` | high | S | open |
| P8 | Copy the detector job at input size, not full resolution | med | S | open |
| P9 | Drop the OpenCL grayscale/canny path and the global `setUseOpenCL` | med | S | open |
| P10 | Pick a sensible default capture mode instead of cap 0 | med | S | open |
| P11 | Cache the device scan instead of re-running it per `PipelineManager` | med | M | open |
| P12 | Share one detector/model across pipelines | high | L | open |
| P13 | Carry YUYV/NV12 over the IPC socket, convert once at the sink | high | L | open |
| P14 | Share one GL context/display across tiles | med | L | open |
| P15 | Narrow `FrameProcessor::m_mutex` so `stats` does not block on a frame | low | S | open |
| P16 | Keep watching the bus after the first message | low | S | open |
| P17 | Revisit `sync=false` on both sinks — nothing paces or sheds on the clock | med | M | open |
| P18 | VA-API hardware JPEG decode (`vajpegdec`) | med | M | open |
| P19 | GPU colourspace segment — currently disabled, produced black frames | med | L | open |
| P20 | Daemon control loop is a single-threaded serial accept | low | M | open |
| P21 | Per-pipeline retained caps text dump | low | S | open |

### Notes

**The motion stages downscale for analysis.** `framediff` is single-channel and
cheap. Its successors are not — the measurement above puts full-frame MOG2 at
10.6 ms at 1080p, and dense optical flow will be far worse — and the whole chain
runs on the streaming thread holding `FrameProcessor::m_mutex` (see P15). So
`motion` runs its subtractor on a 360-row copy and scales the mask back up to
draw, which more than halves it; a motion mask does not need 1080p. **Every later
stage in the family should do the same**, and M3's boxes should be found at
analysis resolution and scaled up, not found at full resolution.

The constant lives in `MotionAlgorithm::kAnalysisRows`. It is deliberately not a
parameter: an operator who lowers it to buy frame rate silently loses small or
distant movers, and the measurement above says there is nothing left to buy —
the subtractor no longer scales with capture resolution.

**P6 — skip the convert when nothing processes.** `source->converter`
(`videoconvert`) is created in the `PipelineManager` constructor and always
linked, and `FrameProcessor` forces `format=BGR` on its capsfilter. With no
algorithms selected the whole convert is waste: the frame could travel to
`unixfdsink` in the capture format. Watch the `unixfdsink` memfd allocation
contract when changing what crosses the socket.

**P7 — `squarePadded()`.** `InferenceBackend.cpp` allocates and zero-fills a
`max(w,h)²` BGR Mat *per inference* — 1920×1920×3 = 11 MB — copies the frame in,
then `blobFromImage` resizes it down to 640. Both the cv::dnn and the ncnn path
go through it. Letterboxing directly at `inputSize` removes the allocation, the
memset and the large-format resize.

**P8 — detector job copy.** `DetectorAlgorithm::apply()` does
`frame.copyTo(m_job)` on the streaming thread for every submitted frame — a full
6.2 MB copy at 1080p. Combine with P7: downscale into the job buffer instead.

**P9 — OpenCL filters.** The GPU path in `Algorithms.cpp` uploads the frame, runs
two `cvtColor`s and downloads it — two PCIe round-trips of a 6 MB frame for work
the file's own comment puts at ~1 ms on CPU. `wantOpenCL()` also calls
`cv::ocl::setUseOpenCL(true)`, which is **process-global**: one camera choosing
GPU flips OpenCL on for every pipeline in the daemon.

**P10 — default capture mode.** `MultiGridPage::toggleSlot` hardcodes
`capIndex = 0`, and cap 0 is whatever the driver happens to list first. On this
camera that is now MJPEG 1080p30, which is a good default by luck rather than by
choice. A policy — prefer encoded, then the highest advertised rate, then a
resolution ceiling — should live in the engine so every client inherits it.
Worth doing together with P11.

**P11 — device scan per pipeline.** `GStreamerSourceCamera`'s constructor runs a
full `GstDeviceMonitor` scan, so every `create` re-opens every camera node and
re-enumerates every cap — including for the throwaway `__probe__` pipeline
`BackendWorker::queryDevices()` creates and immediately deletes. Note the scan is
also how a newly plugged camera is noticed, so a cache needs an invalidation
story (`GstDeviceMonitor`'s added/removed bus messages are the natural source).

**P12 — one detector across pipelines.** Today each pipeline owns a
`DetectorAlgorithm` with its own `cv::dnn::Net`/`ncnn::Net`: N cameras means N
copies of the weights in RAM/VRAM and N independent Vulkan nets. A shared engine
with a submission queue would cut memory near-linearly and stop N cameras from
each spawning a full-width forward pass. Design decision: per-camera fairness
policy when one queue serves several streams.

**P13 — stop shipping BGR over the socket.** BGR is forced for the whole IPC path
by the FrameProcessor capsfilter. 1080p BGR is 6.22 MB/frame — two cameras at
30 fps is ~373 MB/s of memfd traffic — and the console then does *another* CPU
convert to RGBA before every GL upload. Carrying the capture format and
converting once, in the GL sink, removes a full convert and most of the traffic.
Interacts with P6 and with the in-place pad-probe contract below.

**P14 — one GL context for all tiles.** Each `StreamReceiver` builds its own
`glimagesink`, so a 2×2 grid is four GL contexts, four context threads and four
texture pools. `gstglcontext` sharing via a `GstGLDisplay` on the pipeline bus is
the supported route.

**P15 — processor mutex.** `FrameProcessor::m_mutex` is held for the entire
algorithm chain on every frame, and `inferenceStats()` takes the same mutex, so
the 1 Hz stats poll blocks behind a frame's worth of OpenCV work. Jitter, not
throughput — but it makes the telemetry lie about its own sample time.

**P16 — bus loop.** `PipelineManager::startLoop` `break`s out of the poll loop
after the *first* message, so one mid-stream error is logged and then nothing is
watching the bus for the rest of the session.

**P17 — pacing.** `unixfdsink sync=false` and `glimagesink sync=false` mean
nothing in the chain paces or drops on the clock, and `sync=false` also neuters
QoS events travelling upstream. It buys latency, which was the intent; the cost
is that every frame is converted and uploaded no matter how far behind the
console is. Needs a measurement of latency-vs-load before changing.

**P18 — VA-API JPEG decode.** `vajpegdec` is present on this box and would move
MJPEG decode onto the GPU, but it hands back VA memory and the download path is
the one that produced all-black frames before (see `accelSegmentFactories`).
Needs validation against a real camera before it can be a default. `jpegdec`
(libjpeg-turbo, system memory) is the safe choice until then.

**P19 — GPU colourspace segment.** `accelSegmentFactories()` returns `{}` on
purpose: `glupload ! glcolorconvert ! gldownload` negotiated and streamed but
delivered all-black frames to the BGR pad probe on this RADV/GL stack. The seam
is in place. Largely subsumed by P13 if frames stop being converted to BGR.

**P20 — control loop.** `runServer` accepts one client, serves it to completion,
then accepts the next. Fine for one console; it is the blocker for a second
client or for a remote operator.

**P21 — retained caps text.** Every `deviceProperties` keeps a formatted text
dump of every cap of every device for the life of the pipeline, and P5 roughly
doubled the number of caps. Small per pipeline, but it scales with
cameras × caps × pipelines for no reason.

## Invariants

Things that look like optimizations and break the pipeline. Each cost a debugging
session already.

- **The OpenCV stage must stay an in-place pad probe** on a BGR capsfilter. An
  appsink→appsrc bridge allocates its own buffers, which breaks `unixfdsink`'s
  memfd allocation negotiation — `GST_FLOW_ERROR`, "Internal data stream error".
- **Capture modes carrying a non-system memory feature must stay filtered out**
  (`memory:DMABuf` / `format=DMA_DRM` from PipeWire). The annotation is lost on
  the way to the capsfilter and then matches no software element, so `start`
  fails with `BUILD_PIPELINE_FAILED`.
- **Pipeline ids shift after `delete`** — they are indexes into the engine's
  stash. Clients must re-base, as `BackendService` does.
- **A stopped pipeline cannot be restarted.** Create a fresh one per session.
