# Changelog

All notable changes from this point forward should be recorded in this file.

## [Unreleased]

### Changed
- Optimized the RGB/BGR preprocessing path by clearing the model tensor with a zero kernel and launching resize/normalize only over the letterbox content region, including vertical and horizontal padding cases.
- Optimized the I420 YUV preprocessing path with the same zero-kernel plus content-region launch strategy used by RGB/BGR preprocessing.
- Optimized RPP postprocessing scheduling by removing intermediate stream synchronizations between cast, pre-slice, and NMS.
- Clarified postprocess output D2H timing by synchronizing RPP device postprocess work before measuring result copies back to host.
- Updated preprocessing SRAM workspace management to use `rtMallocVirtSram`/`rtFreeVirtSram`, allowing preprocessing workspace to remain allocated through postprocessing.

## [v2.0]

### Added
- Added reusable RPP-facing modules under `rpp/`:
  - `rpp/runtime/RppInferEngine` for ONNX parsing, RppRT engine creation, buffer ownership, and inference execution.
  - `rpp/preprocess/RppYoloPreprocessor` plus RPP kernels for RGB/BGR image and I420 YUV frame preprocessing.
  - `rpp/postprocess/RppYoloPostprocessor` for YOLOv5 output decoding and RPP NMS.
- Added reusable YOLOv5 application modules under `src/yolov5/`, including detection types, labels, visualization, and `YoloV5Pipeline`.
- Added `src/perf/PerfTraceSession` for optional `rpp_perf` trace capture in builds configured with `-DYOLO_ENABLE_RPP_PERF=ON`.
- Added input-oriented demo entries:
  - `demos/rgb_image` builds `yolov5_rgb_image_demo`.
  - `demos/yuv_frame` builds `yolov5_yuv_frame_demo`.
- Added sample assets under `assets/`, including the RGB image and I420 YUV frame used by the default demo paths.
- Added bilingual README documentation for the root project, RPP modules, RGB image demo, and I420 YUV frame demo, with English/Chinese navigation links.

### Changed
- Reorganized the project from the initial monolithic `yolov5/` demo layout into reusable `rpp/`, `src/yolov5/`, `src/utils/`, `src/perf/`, and `demos/` modules.
- Updated CMake to build reusable shared libraries and grouped build outputs under:
  - `build/bin`
  - `build/lib`
  - `build/assets`
  - `build/output`
  - `build/trace`
- Updated the demo execution flow to run explicit `init -> warmup -> measured loop` stages.
- Updated demo timing output to report average per-loop input H2D, preprocess, inference, postprocess, output D2H, end-to-end latency, FPS, and transferred byte counts.
- Updated perf behavior so demo CLI no longer exposes `-p/--perf` or `--perf-dir`; perf traces are controlled only by `YOLO_ENABLE_RPP_PERF`.
- Updated RGB/BGR and I420 preprocessing to run on RPP, including fused letterbox resize, format conversion, and normalization into NCHW model input layout.
- Updated YOLOv5 postprocessing to use the RPP postprocess path instead of CPU-side decode/NMS.
- Updated host/device transfers used by preprocessing and postprocessing to use pinned host staging and asynchronous runtime copies where applicable.
- Updated YOLOv5 box restoration to map detections from letterboxed model coordinates back to source image coordinates.
- Updated project documentation to use neutral module descriptions and input-oriented demo naming so future batch demos can be added without renaming the current demos.

### Removed
- Removed the initial mixed-input `yolov5/main.cpp` demo entry and the old `Yolo` convenience interface.
- Removed the old `common/` layout after moving reusable helpers into `src/utils/`.
- Removed CPU-side YOLOv5 postprocessing fallback from the normal demo path.
- Removed unused debug helpers, debug macros, obsolete intermediate RGB conversion utilities, and the preprocessing model-input clear step.
- Removed device-internal SRAM/DDR transfer timing from the demo timing summary.

## [v1.0] - Init

### Added
- Added the initial YOLOv5 C++ demo project.
- Added the original mixed-input demo under `yolov5/`.
- Added common runtime, logging, parser, buffer, and utility helpers under `common/`.
- Added the initial CMake build entry, README, license, logo, third-party argparse header, and sample RGB image.
