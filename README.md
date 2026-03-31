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
./yolov5_demo -o /path/to/your/yolov5.onnx -i ../image/test_1.png
```


Successful execution should print model/image paths and inference timing, and generate `build/output.jpg`.

## Repository Layout

- `CMakeLists.txt`
  Root build entry. Configures shared dependencies and builds the `yolov5_demo` target.
- `yolov5/`
  Demo-specific source files and module-level documentation.
- `common/`
  Logging (`logging.*`, `logger.*`), ONNX parse helper (`parser_api.h`), `sampleCommon.h`, `rpp_buffer_manager.h`, and CLI helpers (`utils.hpp`) used by the demo.
- `image/`
  Sample image copied into `build/` for quick verification (`test_1.png`).
- `3rd_party/`
  `argparse` header-only library used by `main.cpp` for CLI parsing.

This repository does **not** bundle ONNX models. Class names for overlays are the embedded MS COCO 80-class list in `yolov5/main.cpp` (`coco80_class_labels()`), matching standard YOLOv5 COCO pretrained ordering.

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
build/yolov5_demo
```

## How to Run

Run the demo from the `build` directory. **`--onnx` is required.**

```shell
cd build
./yolov5_demo \
  -o /path/to/your/yolov5.onnx \
  -i ../image/test_1.png
```

You can also pass any other image path supported by OpenCV imread.

```shell
cd build
./yolov5_demo \
  -o /path/to/your/yolov5.onnx \
  -i /path/to/your/picture.jpg
```

## Program Arguments

The demo supports the following command-line arguments:

- `-o` or `--onnx` (**required**)
  Path to the ONNX model file. The model is not included in this repository; use your ONNX export toolchain.
- `-i` or `--image`
  Path to the input image file (default: `test_1.png`).
- `-v` or `--verbose`
  Enable verbose runtime logging.
- `--loop`
  Repeat inference multiple times for timing measurement.

Example:

```shell
cd build
./yolov5_demo \
  -o /path/to/your/yolov5.onnx \
  -i ../image/test_1.png \
  --loop 10 \
  --verbose
```

## Expected Output

When the demo runs successfully, the terminal output should include information similar to:

```text
ONNX model path: /path/to/your/yolov5.onnx
Image file path: ../image/test_1.png
inference takes: <milliseconds>  milliseconds, frames per second: <fps>
Image saved successfully as output.jpg
```

The final rendered detection result is written to:

```text
build/output.jpg
```

Overlay text uses the embedded COCO 80 names when `class_id` is in range; otherwise labels fall back to `class_<id>`.

## Code Execution Workflow

```text
+------------------------+
| Parse CLI arguments    |
| and resolve model      |
| / image input paths    |
+------------------------+
            |
            v
+------------------------+
| Load embedded COCO-80  |
| labels; load input     |
| image from path        |
+------------------------+
            |
            v
+------------------------+
| Preprocess image       |
| pad to square and      |
| create NCHW blob       |
+------------------------+
            |
            v
+------------------------+
| Initialize RPP runtime |
| parse ONNX and build   |
| execution engine       |
+------------------------+
            |
            v
+------------------------+
| Copy input, run RPP    |
| accelerator inference, |
| and read output back   |
+------------------------+
            |
            v
+------------------------+
| Decode detections,     |
| apply NMS, draw boxes, |
| and save output.jpg    |
+------------------------+
```

## Key Source Files

- `yolov5/main.cpp`
  Demo entry point. Handles arguments, image preprocessing, output decoding, NMS, visualization, and embedded COCO class names.
- `yolov5/yolo.cpp`
  Runtime wrapper. Parses the ONNX model, builds the execution engine, allocates buffers, and runs inference.
- `yolov5/yolo.h`
  Public interface for the runtime wrapper class.
- `common/rpp_buffer_manager.h`
  Shared host/device buffer management used during inference on the RPP accelerator target.

## What Customers Can Reuse

- Command-line argument flow for model and image selection
- Input preprocessing logic for YOLOv5-style models
- Runtime initialization and engine creation flow
- Host/device buffer usage through shared helpers in the RPP execution path
- Detection decoding, NMS, and image rendering logic

## How to Adapt This Demo

- Point -o to any YOLOv5-compatible ONNX model produced by your pipeline.
- If your model uses a different class ordering or dataset than COCO 80, edit `coco80_class_labels()` in `yolov5/main.cpp` (or replace it with your own table).
- Replace the sample image path with your own image source.
- Reuse `yolov5/yolo.cpp` and the helpers in `common/` as the starting point for your own RPP-based inference application.
