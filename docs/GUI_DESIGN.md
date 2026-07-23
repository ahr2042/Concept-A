# GuiMediaFusion — VISION_OS GUI Design

Software design for the Qt6 GUI frontend, built from the Stitch design
**"VisionStream AI Dashboard"** (design system: *Industrial AI Vision Platform*,
Stitch project `7522035134849163288`). The Stitch screens show the **final
product vision** including planned AI features; this document maps every design
element to what the current MediaFusionGCV backend actually supports. Elements
the backend cannot serve yet are built as clearly-labelled placeholders
(`PLANNED` badges / disabled controls) so later iterations only fill in logic,
not layout.

## 1. Process model (unchanged, two processes)

```
┌──────────────────────────┐  control: UDS, text protocol   ┌──────────────────────────┐
│  MediaFusionGCV (daemon) │◄──────────────────────────────►│  GUIMediaFusion (Qt6)    │
│  --serve <ctl.sock>      │   one '\n' line per request,   │                          │
│                          │   NUL-terminated reply         │  BackendService          │
│  PipelineManager stash   │                                │   ├─ QProcess (daemon)   │
│  camera → [OpenCV] →     │  video: unixfdsink→unixfdsrc   │   └─ ControlClient       │
│  unixfdsink (per-stream  │───────────────────────────────►│      (worker thread)     │
│  unique socket path)     │      zero-copy memfd + SCM     │  StreamReceiver (per     │
└──────────────────────────┘                                │  tile, GstVideoOverlay)  │
                                                            └──────────────────────────┘
```

* The GUI never links the pipeline library; it drives the daemon over the
  control socket and renders frames received over the per-stream unixfd socket.
* The GUI **owns the daemon lifecycle** (professional single-click UX): on
  startup `BackendService` tries to connect to `MEDIAFUSION_CTL`
  (default `/tmp/mediafusiongcv-control.sock`); if unreachable and *autostart*
  is enabled it spawns `MediaFusionGCV --serve <path>` via `QProcess` and
  retries with backoff. `REBOOT_CORE` / `TERMINATE_PID` on the Dashboard map to
  daemon restart / shutdown — real controls, exactly as the design's buttons.

## 2. GUI architecture (replaces the old Model/View/Controller triad)

Layered, signal-driven; strict threading rule: **sockets on the worker thread,
GStreamer bus polling via timers, widgets only on the GUI thread.**

```
main.cpp                 bootstrap: fonts, Theme, MainWindow
MainWindow               shell: TopBar (logo/nav/actions), SideRail (sources),
                         QStackedWidget of pages, status footer
core/
  AppLog                 app-wide log bus (level, tag, message) → Logs page,
                         Dashboard event feed, CSV export
  BackendService         public async API of the backend; owns BackendWorker
                         (QThread + ControlClient) and the daemon QProcess;
                         session table maps GUI stream sessions → daemon
                         pipeline ids (create→configure→start; stop→delete —
                         fresh pipeline per session, ids re-based after erase)
  DeviceParser           parses the daemon's `devices` listing into
                         DeviceInfo{name, caps[{index,label,raw}]}
  InferenceTypes         parses `models` and `stats <id>` into DetectorModel /
                         InferenceSnapshot; BackendService polls stats at 1 Hz
                         (quietly — the wire log would drown otherwise) and
                         fans the result out to Dashboard and Analytics
  SystemMonitor          1 Hz sampler of real host telemetry: amdgpu hwmon
                         (edge temp, fan, VRAM, gpu_busy_percent), coretemp
theme/Theme              design tokens from the Stitch DESIGN.md (colors,
                         type ramp, spacing, radii) → generated QSS; accent
                         hue switching (cyan/magenta/lilac/peach)
widgets/
  Components             SectionHeader, LedDot, Badge, ToggleSwitch, StatTile,
                         MiniBars, LineChart, PlannedOverlay (placeholder veil)
  Shell                  TopBar, NavTabBar, SideRail
  VideoTile              StreamReceiver + chrome (REC dot, source chip, FPS
                         badge) with offline "NO_SIGNAL" placeholder state
pages/                   Dashboard, Pipeline, MultiGrid, Analytics, Compare,
                         Settings, Logs
dialogs/                 DeviceManagerDialog (modal, from design screen 3)
StreamReceiver           unixfdsrc → glimagesink into native QWidget (existing),
                         extended with a buffer-count/byte pad probe (fps &
                         throughput) and timer-polled bus errors
ControlClient            unchanged (POSIX, Qt-free)
```

Old `GuiMediaFusion{,Model,Controller}`, `PreLaunchSettings.ui`, `guiElements.h`,
`errorStateGui.h`, `stdafx.h` are retired (superseded by the above; history in git).

## 3. Design→backend feature matrix

| Design element (screen) | Status | Wiring |
|---|---|---|
| Live viewport w/ source chip, REC timer (Dashboard) | **REAL** | VideoTile + StreamReceiver on `start` socket |
| START/STOP stream, REBOOT_CORE, TERMINATE_PID | **REAL** | deploy/stop session; daemon restart/shutdown |
| Device list & caps selection (Device Manager) | **REAL** | `devices` → DeviceParser; CONNECT = `set-device` |
| Protocol filter chips USB / RTSP / GigE / CoaXPress | PARTIAL | USB (V4L2) real; others disabled `PLANNED` |
| Processing chain (Pipeline node PROCESS) | **REAL** | `algos-list` → grayscale/canny/detect checkboxes → `algos` |
| Pipeline editor canvas SOURCE→PROCESS→SINK | PARTIAL | fixed linear chain (matches backend); node drag `PLANNED` |
| DEPLOY PIPELINE | **REAL** | create → set-device → algos → start |
| Multi-grid 2×2 tiles, per-tile source | **REAL**¹ | one session per tile (¹ limited by #cameras) |
| GLOBAL SYNC / MASTER CLOCK / REC ALL / FREEZE | PLANNED | disabled chrome |
| FPS / THROUGHPUT telemetry | **REAL** | StreamReceiver pad-probe (frames & bytes / s) |
| GPU_UTILIZATION, thermal tiles, fan (Analytics) | **REAL** | SystemMonitor (amdgpu + coretemp hwmon) |
| Inference latency chart | **REAL** | `stats <id>` @ 1 Hz → Analytics chart + Dashboard block |
| FRAME_INTEGRITY, MEM BW | PLANNED | placeholder charts, honest zeros + badges |
| Detection model combo, confidence slider, boxes | **REAL** | `models` → combo; `model`/`detect-params`; boxes drawn in-frame by the engine |
| GPU_ACCELERATION "NVIDIA/CUDA/TensorRT" labels | ADAPTED | this rig is AMD and inference is CPU-only → `PLANNED`² |
| Detection summary + detections log | **REAL** | TOTAL_OBJECTS / AVG_CONFIDENCE from `stats`; label changes go to AppLog |
| Stream Comparison RAW vs AI | PLANNED | needs backend tee; page ships as chrome + note |
| System event log w/ filter + CSV export (Logs) | **REAL** | AppLog (includes raw control-protocol transcript) |
| Settings: accent hue, log verbosity, overlays | **REAL** | Theme regeneration; AppLog threshold; QSettings |
| Settings: connection (socket path, autostart) | **REAL** | BackendService config |
| Settings: NETWORK / STORAGE / API ACCESS tabs | PLANNED | empty-state panels |

² the detector runs on the CPU through OpenCV DNN. There is no CUDA on this rig
and OpenCV's OpenCL target needs an ICD that is not guaranteed present, so the
acceleration toggle stays disabled until an ONNX Runtime / ncnn Vulkan backend
lands. `DetectorAlgorithm::runInference()` is the seam that would change.

## 4. Control-protocol session policy

`mediaLib_delete` erases from the pipeline vector, so daemon ids **shift** after
delete. BackendService hides this: it keeps `sessionId → daemonId` and, after
deleting daemon id *k*, decrements every mapped id > *k*. Restart-after-stop of
the same pipeline would re-add elements to the bin (unsupported by backend), so
each GUI "start" creates a fresh daemon pipeline and each "stop" is
`stop <id>` + `delete <id>`. One control connection, commands serialized on the
worker thread.

## 5. Theme

Tokens lifted from the Stitch design system: surface `#131315`, container ramp
`#0e0e10…#353437`, on-surface `#e5e1e4`, outline-variant `#3a494b`, primary
accent `#00f2ff` (dim `#00dbe7`), secondary `#b600f8`, tertiary `#7318ff`,
error `#ffb4ab` on `#93000a`. Type ramp: Space Grotesk (display), Geist (body,
Inter fallback), JetBrains Mono (data/labels, letter-spaced ALL_CAPS). Fonts are
bundled as OFL TTFs when present in `resources/fonts/` and fall back to system
faces otherwise. Accent switching regenerates the QSS at runtime (Settings →
ACCENT HUE, real).

## 6. Error handling & observability

* Every control command logs `→ cmd` / `← reply` to AppLog (DEBUG level).
* Daemon crash/exit → `daemonStateChanged(Offline)` → status footer LED turns
  error-red, active tiles fall back to NO_SIGNAL placeholders.
* GStreamer bus errors on the receive side surface as WARN log entries and
  tile badges, never dialogs (command-center UX: nothing modal during ops).
* All placeholder controls carry tooltips: "PLANNED — not yet implemented in
  MediaFusionGCV backend".

## 7. Build

`concept_A/GuiMediaFusion/CMakeLists.txt` builds `GUIMediaFusion` from the new
sources (Qt6 Widgets + gstreamer-1.0 + gstreamer-video-1.0 via pkg-config, C++20).
No dependency on the backend library — shared enum headers only.
