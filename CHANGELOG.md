# Changelog

All notable changes from this point forward should be recorded in this file.

## [Unreleased]

### Added
- Added a reusable `rpp/runtime/RppInferEngine` model inference wrapper that can be shared by future demos.
- Added a reusable `src/yolov5/YoloV5Pipeline` that composes RPP preprocessing, RppRT inference, and RPP postprocessing without hidden warmup.
- Added `src/perf/PerfTraceSession` so demos can enable rpp_perf trace capture with a `--perf` CLI option.
- Added separate single-process RGB image and I420 YUV image demos under `demos/`.
- Added dedicated YOLOv5 modules for shared detection types, labels, preprocessing, postprocessing, visualization, and the demo pipeline.
- Added modular YOLOv5 preprocessing and postprocessing boundaries so future RPP device-buffer implementations can replace the current demo steps without changing `main.cpp`.
- Added an `rpp/preprocess` dynamic library module for I420 YUV and RGB/BGR device preprocessing.
- Added RPP kernels for fused I420/RGB/BGR letterbox resize and normalize into NCHW float model input layout.
- Added CLI support for I420 input through `--yuv`, `--yuv-width`, and `--yuv-height`.
- Added letterbox metadata for postprocess coordinate restoration.
- Added an `rpp/postprocess` dynamic library module that uses RPP `nms_pre_slice` and RPP addNMS for YOLOv5 device-output box/score processing.
- Added postprocess warmup and detailed profile timing for RPP cast, NMS slice, NMS, D2H, IO, and end-to-end stages.
- Added optional `rpp_perf` Perfetto trace integration through `YOLO_ENABLE_RPP_PERF` and demo `--perf` options.
- Added RPP driver trace forwarding so `rppLogsDumpToTraceWindows` records driver/API and device-side operations into the demo trace window.

### Changed
- Reorganized hardware-facing RPP code under `rpp/runtime`, `rpp/preprocess`, and `rpp/postprocess`, with YOLOv5 application code remaining under `src/yolov5`.
- Renamed the RPP shared library targets and outputs to `librpp_runtime.so`, `librpp_yolo_preprocess.so`, and `librpp_yolo_postprocess.so`.
- Renamed YOLOv5 visualization source files to `visualization.*` to match `libyolov5_visualization.so`.
- Restricted CMake include directories to target-level public/private dependencies instead of sharing one project-wide include list.
- Moved RPP driver trace enabling from the reusable YOLOv5 pipeline into the demo-owned `PerfTraceSession` flow.
- Split customer-facing demo usage into dedicated RGB and I420 YUV README files, with the root README serving as the project overview.
- Updated the build layout to group executables under `build/bin`, shared libraries under `build/lib`, sample inputs under `build/assets`, rendered images under `build/output`, and optional traces under `build/trace`.
- Updated demo default paths to run from `build/bin` with copied sample assets and output directories.
- Updated demo timing policy so each demo performs `init -> warmup run -> measured run` explicitly and prints customer-facing preprocess, inference, postprocess, end-to-end, and FPS metrics.
- Updated demo logs to print model/input paths first, then a final execution-flow summary with input H2D, preprocess, inference, postprocess, output D2H, end-to-end time, FPS, and output path.
- Updated demo H2D/D2H timing lines to include transferred byte counts.
- Updated demo summaries to print measured loop count and state that reported timings are average per measured loop after warmup.
- Added `-l` as the short form of `--loop` and `-p` as the short form of `--perf` for both demos.
- Clarified that demo `--perf` trace capture requires configuring CMake with `-DYOLO_ENABLE_RPP_PERF=ON`.
- Added a warning when `--perf` is requested from a build that was not configured with `YOLO_ENABLE_RPP_PERF`.
- Renamed rpp_perf scopes to unique full-stage names and separated warmup runs from measured loops in traces.
- Updated rpp_perf setup to resolve trace directories to absolute paths and initialize the RPP driver before attaching driver/device trace records.
- Expanded Doxygen function comments and inline implementation-step comments across project-owned demo, RPP, YOLOv5, perf, and utility code.
- Updated the build to produce `yolov5_rgb_image_demo` and `yolov5_yuv_image_demo` instead of the previous mixed-input `yolov5_demo` target.
- Updated the preprocessor lifecycle to release SRAM buffers after each preprocessing stage so postprocessing can allocate its RPP NMS SRAM workspace.
- Updated the demo detection path to use the reusable `YoloV5Pipeline` and `RppInferEngine` modules.
- Reduced `main.cpp` to CLI parsing, path validation, image loading, pipeline invocation, visualization, and output saving.
- Removed preprocessing/postprocessing selection fields from the demo configuration.
- Updated the demo pipeline to run RPP preprocessing through SRAM staging and copy the normalized result into the model input DDR buffer before inference.
- Updated YOLO output decoding to restore boxes from letterboxed model coordinates back to source image coordinates.
- Updated build configuration to compile RPP preprocessing as a shared library linked by the RGB and YUV demo targets.
- Updated the demo pipeline to require RPP postprocessing from the YOLO output DDR buffer.
- Updated RPP postprocessing to require the reference-style RPP `addNMS` path without host NMS.
- Updated RPP `addNMS` construction to use `input_max_out_per_class` as a runtime input binding, matching the provided reference implementation.
- Updated RPP NMS threshold bindings to BF16 scalars, matching rpprt NMS tests and avoiding extra FLOAT-to-BF16 identity layers during engine build.
- Updated RGB and I420 preprocessing to use pinned host staging with `rtHostAlloc` and asynchronous H2D copies.
- Updated RPP postprocess D2H copies to use pinned host buffers and `rtMemcpyAsync` for NMS outputs and BF16 boxes.
- Simplified profile output to report only high-level preprocess, inference, postprocess, IO, and total timings.
- Updated the demo, RPP preprocess, and RPP postprocess paths to emit nested trace scopes for pipeline, inference, preprocess, postprocess, and IO stages when `rpp_perf` tracing is enabled.
- Updated `rpp_perf` instrumentation to leave device operation timing to drv_api automatic trace records instead of manually wrapping each device copy, kernel, or enqueue call.
- Updated RPP preprocessing kernels to write the full model-input tensor, including zero-valued letterbox padding, so the separate `rtMemset` clear step is no longer needed.

### Fixed
- Fixed YOLO input spatial dimension parsing for NCHW bindings so `1x3x640x640` resolves to `640x640` instead of treating the channel dimension as height.
- Fixed RPP preprocessing buffer flow to use DDR-to-SRAM input staging, SRAM kernel execution, and SRAM-to-DDR model input copyback, which restores nonzero model input and detections for both RGB and I420 YUV demo paths.
- Fixed I420 preprocessing so YUV input follows the fused RPP letterbox resize/normalize path and restores box marking on generated output images.

### Removed
- Removed the old mixed-input `yolov5/main.cpp` demo entry.
- Removed the old `Yolo::infer(cv::Mat&, int, std::vector<float>&)` convenience interface after validating the new API path.
- Removed unused debug helpers, debug macros, and obsolete intermediate RGB conversion utilities from project-owned code.
- Removed unused runtime/sample helper APIs that were no longer called by the modular demo pipeline.
- Removed CPU-side YOLOv5 postprocessing fallback and device-internal SRAM/DDR transfer timing from profile output.
- Removed the preprocessing `rtMemset` model-input clear from the demo execution flow.
