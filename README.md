# YOLOv5 Inference Demo on RPP

![XDL Logo](doc/logo/logo_color_horizontal.png)

This repository is a customer-facing C++ demo for running a YOLOv5 object detection model on RPP accelerator hardware.
It is intended to help unfamiliar users quickly build the project, run inference, and use the source code as a reference for their own edge inference applications.

**Table Of Contents**
- [Quick Start](#quick-start)
- [Repository Layout](#repository-layout)
- [Prerequisites](#prerequisites)
- [How to Build](#how-to-build)
- [How to Run](#how-to-run)
- [Program Arguments](#program-arguments)
- [Expected Output](#expected-output)
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
./yolov5_rgb_image_demo -o /path/to/your/yolov5.onnx -i ../image/test_1.png
```


Successful execution should print warmup-stable stage timing and FPS, and generate `build/output_rgb.jpg`.

## Repository Layout

- `CMakeLists.txt`
  Root build entry. Configures shared dependencies and builds the reusable libraries and demo targets.
- `src/core/`
  Generic RPP/RppRT inference wrapper code, currently `RppInferEngine`.
- `src/perf/`
  Optional `rpp_perf` trace session wrapper used by demos through `--perf`.
- `src/utils/`
  Logging, ONNX parse helper, runtime buffer helpers, and CLI/path utilities.
- `src/yolov5/`
  YOLOv5-specific labels, detection types, visualization, and pipeline composition.
- `demos/`
  Customer-facing executable entries, including RGB image and I420 YUV image demos.
- `rpp_preprocess/` and `rpp_postprocess/`
  RPP preprocessing kernels/modules and RPP postprocessing implementation.
- `image/`
  Sample image copied into `build/` for quick verification (`test_1.png`).
- `3rd_party/`
  `argparse` header-only library used by demo main files for CLI parsing.

This repository does **not** bundle ONNX models. Class names for overlays are the embedded MS COCO 80-class list in `src/yolov5/labels.cpp` (`coco80_class_labels()`), matching standard YOLOv5 COCO pretrained ordering.

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
build/yolov5_rgb_image_demo
build/yolov5_yuv_image_demo
```

## How to Run

Run the demo from the `build` directory. **`--onnx` is required.**

```shell
cd build
./yolov5_rgb_image_demo \
  -o /path/to/your/yolov5.onnx \
  -i ../image/test_1.png
```

You can also pass any other image path supported by OpenCV imread.

```shell
cd build
./yolov5_rgb_image_demo \
  -o /path/to/your/yolov5.onnx \
  -i /path/to/your/picture.jpg
```

For one-frame I420 YUV input:

```shell
cd build
./yolov5_yuv_image_demo \
  -o /path/to/your/yolov5.onnx \
  --yuv /path/to/frame.i420 \
  --yuv-width 1280 \
  --yuv-height 720
```

## Program Arguments

The demo supports the following command-line arguments:

- `-o` or `--onnx` (**required**)
  Path to the ONNX model file. The model is not included in this repository; use your ONNX export toolchain.
- `-i` or `--image`
  Path to the input image file for `yolov5_rgb_image_demo` (default: `test_1.png`).
- `--yuv`, `--yuv-width`, `--yuv-height`
  I420 input file and frame dimensions for `yolov5_yuv_image_demo`.
- `--output`
  Path to the rendered detection image.
- `--perf`
  Save a demo-specific `rpp_perf` trace JSON.
- `--perf-dir`
  Output directory for `--perf` traces.
- `-v` or `--verbose`
  Enable verbose runtime logging.
- `--loop`
  Repeat inference multiple times for timing measurement.

Example:

```shell
cd build
./yolov5_rgb_image_demo \
  -o /path/to/your/yolov5.onnx \
  -i ../image/test_1.png \
  --loop 10 \
  --perf
```

## Expected Output

When the demo runs successfully, the terminal output should include information similar to:

```text
Model path: /path/to/your/yolov5.onnx
Inference image path: ../image/test_1.png

Summary:
Input H2D: <milliseconds> ms, <bytes> bytes, <KiB> KiB
Preprocess: <milliseconds> ms
Inference: <milliseconds> ms
Postprocess: <milliseconds> ms
Output D2H: <milliseconds> ms, <bytes> bytes, <KiB> KiB
All time end to end: <milliseconds> ms
FPS: <fps>
Output path: output_rgb.jpg
```

The final rendered detection result is written to:

```text
build/output_rgb.jpg
```

Overlay text uses the embedded COCO 80 names when `class_id` is in range; otherwise labels fall back to `class_<id>`.

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

- `src/core/rpp_infer_engine.*`
  Generic RPP/RppRT runtime wrapper. Parses the ONNX model, builds the execution engine, allocates buffers, and runs inference.
- `src/yolov5/yolo_pipeline.*`
  YOLOv5 pipeline composition. Runs RPP preprocessing, inference, and RPP postprocessing once per call without hidden warmup.
- `src/perf/perf_trace_session.*`
  Optional rpp_perf trace wrapper used by demos through `--perf`.
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
- Reuse `src/core/RppInferEngine`, `src/yolov5/YoloV5Pipeline`, and the demo argument/timing pattern as the starting point for your own RPP-based inference application.
