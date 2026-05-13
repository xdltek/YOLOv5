# Changelog

All notable changes from this point forward should be recorded in this file.

## [Unreleased]

### Added
- Added reusable `Yolo` runtime APIs for host/device input and output buffers, execution context access, device bindings access, explicit warmup, and direct execution.
- Added one-time warmup tracking so `execute()` performs warmup automatically on the first call while still allowing callers to invoke `warmup()` explicitly.
- Added dedicated YOLOv5 modules for shared detection types, labels, preprocessing, postprocessing, visualization, and the demo pipeline.
- Added modular YOLOv5 preprocessing and postprocessing boundaries so future RPP device-buffer implementations can replace the current demo steps without changing `main.cpp`.
- Added an `rpp_preprocess` dynamic library module for I420 YUV and RGB/BGR device preprocessing.
- Added RPP kernels for I420-to-RGB CHW conversion and resize/normalize into NCHW float model input layout.
- Added CLI support for I420 input through `--yuv`, `--yuv-width`, and `--yuv-height`.
- Added letterbox metadata for postprocess coordinate restoration.
- Added `YOLO_PROFILE=1` performance profiling that runs one preprocess + inference pass as warmup, then reports the second pass as complete preprocessing time, inference time, and combined preprocessing + inference time.
- Added a `Yolo::enqueue()` wrapper so profile inference can be submitted through the runtime's asynchronous execution API.
- Added an `rpp_postprocess` dynamic library module that uses RPP `nms_pre_slice` for YOLOv5 device-output box/score slicing before host-side NMS.
- Added reserved RPP postprocess accessors for the sliced score DDR buffer shape and address.

### Changed
- Updated the demo detection path to use a local inference helper built on the new `Yolo` host-buffer, copy, warmup, execute, and output-buffer APIs.
- Reduced `main.cpp` to CLI parsing, path validation, image loading, pipeline invocation, visualization, and output saving.
- Removed preprocessing/postprocessing selection fields from the demo configuration.
- Updated the demo pipeline to run RPP preprocessing through SRAM staging and copy the normalized result into the model input DDR buffer before inference.
- Updated YOLO output decoding to restore boxes from letterboxed model coordinates back to source image coordinates.
- Updated build configuration to compile RPP preprocessing as a shared library linked by `yolov5_demo`.
- Updated the demo pipeline to require RPP postprocessing from the YOLO output DDR buffer.
- Updated RPP postprocessing to require the reference-style RPP `addNMS` path without host NMS.
- Updated RPP `addNMS` construction to use `input_max_out_per_class` as a runtime input binding, matching the provided reference implementation.
- Updated RPP NMS threshold bindings to BF16 scalars, matching rpprt NMS tests and avoiding extra FLOAT-to-BF16 identity layers during engine build.

### Fixed
- Fixed YOLO input spatial dimension parsing for NCHW bindings so `1x3x640x640` resolves to `640x640` instead of treating the channel dimension as height.
- Fixed RPP preprocessing buffer flow to use DDR-to-SRAM input staging, SRAM kernel execution, and SRAM-to-DDR model input copyback, which restores nonzero model input and detections for both RGB and I420 YUV demo paths.

### Removed
- Removed the old `Yolo::infer(cv::Mat&, int, std::vector<float>&)` convenience interface after validating the new API path.
