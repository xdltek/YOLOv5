# Changelog

All notable changes from this point forward should be recorded in this file.

## [Unreleased]

### Added
- Added reusable `Yolo` runtime APIs for host/device input and output buffers, execution context access, device bindings access, explicit warmup, and direct execution.
- Added one-time warmup tracking so `execute()` performs warmup automatically on the first call while still allowing callers to invoke `warmup()` explicitly.
- Added dedicated YOLOv5 modules for shared detection types, labels, preprocessing, postprocessing, visualization, and the demo pipeline.
- Added modular YOLOv5 preprocessing and postprocessing boundaries so future RPP device-buffer implementations can replace the current demo steps without changing `main.cpp`.

### Changed
- Updated the demo detection path to use a local inference helper built on the new `Yolo` host-buffer, copy, warmup, execute, and output-buffer APIs.
- Reduced `main.cpp` to CLI parsing, path validation, image loading, pipeline invocation, visualization, and output saving.
- Removed preprocessing/postprocessing selection fields from the demo configuration.

### Removed
- Removed the old `Yolo::infer(cv::Mat&, int, std::vector<float>&)` convenience interface after validating the new API path.
