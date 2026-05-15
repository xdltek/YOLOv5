# YOLOv5 Inference Demo on RPP

![XDL Logo](doc/logo/logo_color_horizontal.png)

This repository is a customer-facing C++ demo for running a YOLOv5 object detection model on RPP accelerator hardware.
It is intended to help unfamiliar users quickly build the project, run inference, and use the source code as a reference for their own edge inference applications.

**Table Of Contents**
- [Quick Start](#quick-start)
- [Repository Layout](#repository-layout)
- [Demo Guides](#demo-guides)
- [Prerequisites](#prerequisites)
- [How to Build](#how-to-build)
- [Code Execution Workflow](#code-execution-workflow)
- [Key Source Files](#key-source-files)
- [What Customers Can Reuse](#what-customers-can-reuse)
- [How to Adapt This Demo](#how-to-adapt-this-demo)

## Quick Start

Provide a YOLOv5-compatible ONNX file from your own export workflow, then run from the repository root:
> Note: You can use this repository (https://github.com/xdltek/onnx_model_tool) to quickly export YOLOv5s ONNX models.

```shell
rm -rf build
mkdir build
cd build
cmake ..
make -j8
cd bin
./yolov5_rgb_image_demo -o ../../onnx/yolov5s.onnx
```


Successful execution should print warmup-stable stage timing and FPS, and generate `build/output/output_rgb.jpg`.

## Repository Layout

- `CMakeLists.txt`
  Root build entry. Configures shared dependencies and builds the reusable libraries and demo targets.
- `src/perf/`
  Optional `rpp_perf` trace session wrapper used by demos through `-p` or `--perf`; it is active only when CMake is configured with `-DYOLO_ENABLE_RPP_PERF=ON`. If `--perf` is used in a non-perf build, the demo prints a warning and continues without trace capture.
- `src/utils/`
  Logging, ONNX parse helper, runtime buffer helpers, and CLI/path utilities.
- `src/yolov5/`
  YOLOv5-specific labels, detection types, visualization, and pipeline composition.
- `rpp/`
  RPP/RppRT hardware-facing modules, including runtime inference, preprocessing kernels/modules, and postprocessing.
- `demos/`
  Customer-facing executable entries, including RGB image and I420 YUV image demos.
- `assets/`
  Source sample image and I420 frame copied into `build/assets/` for quick verification.
- `3rd_party/`
  `argparse` header-only library used by demo main files for CLI parsing.

This repository does **not** bundle ONNX models. Class names for overlays are the embedded MS COCO 80-class list in `src/yolov5/labels.cpp` (`coco80_class_labels()`), matching standard YOLOv5 COCO pretrained ordering.

## Demo Guides

The root README explains the shared build and architecture. Each executable has its own customer-facing guide with its input format, command-line options, workflow, and expected output:

- [RGB image single-process demo](demos/rgb_image_single_process/README.md)
- [I420 YUV image single-process demo](demos/yuv_image_single_process/README.md)

## Prerequisites

Before building this demo, make sure the following dependencies are available:

- Linux development environment
- CMake `>= 3.10`
- A compiler with C++17 support
- OpenCV with the components required by this project
- RppRT installed under `/usr/local/rpp`
- Runtime libraries required by the target RPP accelerator card environment

If your RppRT installation path is different, update the corresponding path settings in `CMakeLists.txt`.

## How to Build

Run the following commands from the repository root:

```shell
rm -rf build
mkdir build
cd build
cmake ..
make -j8
```

After the build completes successfully, the executable should be available at:

```shell
build/bin/yolov5_rgb_image_demo
build/bin/yolov5_yuv_image_demo
```

Build outputs are grouped as:

```text
build/bin/      demo executables
build/lib/      libyolov5_core.so, librpp_runtime.so, librpp_yolo_preprocess.so, librpp_yolo_postprocess.so, and helper libraries
build/assets/   copied sample inputs
build/output/   rendered detection images
build/trace/    optional rpp_perf traces
```

## How to Run

Run demos from `build/bin` after building the project. **`--onnx` is required** because the repository does not include a model file.

```shell
cd build/bin
./yolov5_rgb_image_demo -o ../../onnx/yolov5s.onnx
./yolov5_yuv_image_demo -o ../../onnx/yolov5s.onnx
```

For full command-line options and expected output, see the per-demo guides:

- [RGB image single-process demo](demos/rgb_image_single_process/README.md)
- [I420 YUV image single-process demo](demos/yuv_image_single_process/README.md)

## Code Execution Workflow

```text
+------------------------+
| Parse CLI arguments    |
| and resolve input paths|
+------------------------+
            |
            v
+------------------------+
| Init optional --perf   |
| trace session          |
+------------------------+
            |
            v
+------------------------+
| Init YOLOv5 pipeline   |
| and RppRT engine       |
+------------------------+
            |
            v
+------------------------+
| Run full pipeline once |
| as explicit warmup     |
+------------------------+
            |
            v
+------------------------+
| Run measured pipeline: |
| preprocess + infer +   |
| postprocess            |
+------------------------+
            |
            v
+------------------------+
| Draw boxes, save image,|
| print latency and FPS  |
+------------------------+
```

## Key Source Files

- `rpp/runtime/rpp_infer_engine.*`
  Generic RPP/RppRT runtime wrapper. Parses the ONNX model, builds the execution engine, allocates buffers, and runs inference.
- `rpp/preprocess/rpp_yolo_preprocessor.*`
  RPP preprocessing module and kernels for RGB/BGR/I420 inputs.
- `rpp/postprocess/rpp_yolo_postprocessor.*`
  RPP postprocessing module for YOLO output slicing and NMS.
- `src/yolov5/yolo_pipeline.*`
  YOLOv5 pipeline composition. Runs RPP preprocessing, inference, and RPP postprocessing once per call without hidden warmup.
- `src/perf/perf_trace_session.*`
  Optional rpp_perf trace wrapper used by demos through `-p` or `--perf`.
- `demos/rgb_image_single_process/main.cpp`
  RGB/BGR image demo entry point.
- `demos/yuv_image_single_process/main.cpp`
  I420 YUV image demo entry point.

## What Customers Can Reuse

- Command-line argument flow for model and image selection
- Input preprocessing logic for YOLOv5-style models
- Runtime initialization and engine creation flow
- Host/device buffer usage through shared helpers in the RPP execution path
- Detection decoding, NMS, and image rendering logic

## How to Adapt This Demo

- Point -o to any YOLOv5-compatible ONNX model produced by your pipeline.
- If your model uses a different class ordering or dataset than COCO 80, edit `coco80_class_labels()` in `src/yolov5/labels.cpp` (or replace it with your own table).
- Replace the sample image path with your own image source.
- Reuse `rpp/runtime/RppInferEngine`, `src/yolov5/YoloV5Pipeline`, and the demo argument/timing pattern as the starting point for your own RPP-based inference application.
