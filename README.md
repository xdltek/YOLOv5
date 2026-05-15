# YOLOv5 Inference Demo on RPP

![XDL Logo](doc/logo/logo_color_horizontal.png)

This repository is a C++ demo for running a YOLOv5 object detection model on RPP accelerator hardware.
It is intended to help users quickly build the project, run inference, and use the source code as a reference for edge inference applications.

**Table Of Contents**
- [Quick Start](#quick-start)
- [Repository Layout](#repository-layout)
- [Demo Guides](#demo-guides)
- [Prerequisites](#prerequisites)
- [How to Build](#how-to-build)
- [How to Run](#how-to-run)
- [Code Execution Workflow](#code-execution-workflow)
- [Key Source Files](#key-source-files)
- [Reusable Components](#reusable-components)
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
- [`rpp/`](rpp/README.md)
  RPP/RppRT hardware-facing modules, including runtime inference, preprocessing kernels/modules, and postprocessing.
- `demos/`
  Executable demo entries, including RGB image and I420 YUV image demos.
- `assets/`
  Source sample image and I420 frame copied into `build/assets/` for quick verification.
- `3rd_party/`
  `argparse` header-only library used by demo main files for CLI parsing.

This repository does **not** bundle ONNX models. Class names for overlays are the embedded MS COCO 80-class list in `src/yolov5/labels.cpp` (`coco80_class_labels()`), matching standard YOLOv5 COCO pretrained ordering.

## Demo Guides

The root README explains the shared build and architecture. Each executable has its own guide with its input format, command-line options, workflow, and expected output:

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

## Reusable Components

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

---

# YOLOv5 RPP 推理 Demo

![XDL Logo](doc/logo/logo_color_horizontal.png)

本仓库是一个 C++ 示例工程，用于在 RPP 加速硬件上运行 YOLOv5 目标检测模型。
它的目标是帮助用户快速完成编译、运行推理，并理解如何复用项目中的 RPP 推理、前处理、后处理和 demo 组织方式。

**中文目录**
- [快速开始](#快速开始)
- [仓库结构](#仓库结构)
- [Demo 说明](#demo-说明)
- [环境依赖](#环境依赖)
- [构建方式](#构建方式)
- [运行方式](#运行方式)
- [代码执行流程](#代码执行流程)
- [关键源码](#关键源码)
- [可复用组件](#可复用组件)
- [如何适配自己的应用](#如何适配自己的应用)

## 快速开始

请先准备一个与 YOLOv5 兼容的 ONNX 模型文件，然后在仓库根目录执行：
> 说明：可以使用 https://github.com/xdltek/onnx_model_tool 快速导出 YOLOv5s ONNX 模型。

```shell
rm -rf build
mkdir build
cd build
cmake ..
make -j8
cd bin
./yolov5_rgb_image_demo -o ../../onnx/yolov5s.onnx
```

执行成功后，程序会打印 warmup 之后稳定的阶段耗时和 FPS，并默认生成 `build/output/output_rgb.jpg`。

## 仓库结构

- `CMakeLists.txt`
  根构建入口。负责配置公共依赖，并构建可复用库和 demo 可执行文件。
- `src/perf/`
  可选的 `rpp_perf` trace 会话封装。demo 可通过 `-p` 或 `--perf` 启用；该功能只有在 CMake 配置 `-DYOLO_ENABLE_RPP_PERF=ON` 时生效。非 perf 构建中使用 `--perf` 时，demo 会打印提示并继续执行。
- `src/utils/`
  日志、ONNX 解析辅助、运行时 buffer 辅助、命令行和路径工具。
- `src/yolov5/`
  YOLOv5 业务相关代码，包括标签、检测结果类型、可视化和 pipeline 组装。
- [`rpp/`](rpp/README.md)
  面向 RPP/RppRT 硬件执行路径的模块，包括运行时推理、前处理 kernel/模块和后处理模块。
- `demos/`
  demo 可执行入口，包括 RGB 图片和 I420 YUV 图片单进程 demo。
- `assets/`
  示例输入资源，构建时会复制到 `build/assets/`，便于快速验证。
- `3rd_party/`
  demo 命令行解析使用的 `argparse` header-only 三方库。

本仓库**不包含** ONNX 模型文件。可视化使用的类别名称来自 `src/yolov5/labels.cpp` 中内置的 MS COCO 80 类列表 `coco80_class_labels()`，顺序与标准 YOLOv5 COCO 预训练模型一致。

## Demo 说明

根 README 说明公共构建方式和整体架构。每个 demo 目录下还有独立说明文档，用于描述对应输入格式、命令行参数、执行流程和输出结果：

- [RGB 图片单进程 demo](demos/rgb_image_single_process/README.md)
- [I420 YUV 图片单进程 demo](demos/yuv_image_single_process/README.md)

## 环境依赖

构建前请确认环境中已具备：

- Linux 开发环境
- CMake `>= 3.10`
- 支持 C++17 的编译器
- OpenCV 以及本项目需要的组件
- 安装在 `/usr/local/rpp` 下的 RppRT
- 目标 RPP 加速卡运行所需的运行时库

如果 RppRT 安装路径不同，请同步修改 `CMakeLists.txt` 中的相关路径配置。

## 构建方式

在仓库根目录执行：

```shell
rm -rf build
mkdir build
cd build
cmake ..
make -j8
```

构建完成后，可执行文件默认生成在：

```shell
build/bin/yolov5_rgb_image_demo
build/bin/yolov5_yuv_image_demo
```

构建产物按类型归类：

```text
build/bin/      demo 可执行文件
build/lib/      libyolov5_core.so、librpp_runtime.so、librpp_yolo_preprocess.so、librpp_yolo_postprocess.so 以及辅助库
build/assets/   复制后的示例输入
build/output/   绘制检测框后的输出图片
build/trace/    可选的 rpp_perf trace 文件
```

## 运行方式

构建完成后，从 `build/bin` 目录运行 demo。由于仓库不包含模型文件，`--onnx` 参数是必需的。

```shell
cd build/bin
./yolov5_rgb_image_demo -o ../../onnx/yolov5s.onnx
./yolov5_yuv_image_demo -o ../../onnx/yolov5s.onnx
```

完整参数和预期输出请参考各 demo 文档：

- [RGB 图片单进程 demo](demos/rgb_image_single_process/README.md)
- [I420 YUV 图片单进程 demo](demos/yuv_image_single_process/README.md)

## 代码执行流程

```text
+------------------------+
| 解析命令行参数和路径   |
+------------------------+
            |
            v
+------------------------+
| 初始化可选 --perf      |
| trace 会话             |
+------------------------+
            |
            v
+------------------------+
| 初始化 YOLOv5 pipeline |
| 和 RppRT engine        |
+------------------------+
            |
            v
+------------------------+
| 完整执行一次 pipeline  |
| 作为显式 warmup        |
+------------------------+
            |
            v
+------------------------+
| 执行测量流程：         |
| 前处理 + 推理 + 后处理 |
+------------------------+
            |
            v
+------------------------+
| 绘制检测框、保存图片、 |
| 打印延迟和 FPS         |
+------------------------+
```

## 关键源码

- `rpp/runtime/rpp_infer_engine.*`
  通用 RPP/RppRT 运行时封装。负责解析 ONNX、构建执行 engine、分配 buffer 并执行推理。
- `rpp/preprocess/rpp_yolo_preprocessor.*`
  RGB/BGR/I420 输入的 RPP 前处理模块和 kernel。
- `rpp/postprocess/rpp_yolo_postprocessor.*`
  YOLO 输出切分和 NMS 的 RPP 后处理模块。
- `src/yolov5/yolo_pipeline.*`
  YOLOv5 pipeline 组装层。每次调用执行一次 RPP 前处理、推理和 RPP 后处理，不隐藏 warmup。
- `src/perf/perf_trace_session.*`
  demo 通过 `-p` 或 `--perf` 使用的可选 rpp_perf trace 封装。
- `demos/rgb_image_single_process/main.cpp`
  RGB/BGR 图片 demo 入口。
- `demos/yuv_image_single_process/main.cpp`
  I420 YUV 图片 demo 入口。

## 可复用组件

- 模型和图片选择的命令行参数组织方式
- YOLOv5 风格模型的输入前处理逻辑
- 运行时初始化和 engine 创建流程
- RPP 执行路径中的 host/device buffer 使用方式
- 检测结果解码、NMS 和图片可视化逻辑

## 如何适配自己的应用

- 使用 `-o` 指向自己的 YOLOv5 兼容 ONNX 模型。
- 如果模型类别顺序或数据集不同于 COCO 80，请修改 `src/yolov5/labels.cpp` 中的 `coco80_class_labels()`，或替换为自己的类别表。
- 将示例图片路径替换为自己的输入源。
- 复用 `rpp/runtime/RppInferEngine`、`src/yolov5/YoloV5Pipeline` 和 demo 中的参数/计时模式，作为自定义 RPP 推理应用的起点。
