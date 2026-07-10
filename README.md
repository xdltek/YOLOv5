<a id="english"></a>

# YOLOv5 Inference Demo on RPP

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

![XDL Logo](docs/logo/logo_color_horizontal.png)

This repository is a C++ demo for running a YOLOv5 object detection model on RPP accelerator hardware.
It is intended to help users quickly build the project, run inference, and use the source code as a reference for edge inference applications.

**Table Of Contents**

Click a section link below to jump within this README:

- [Quick Start](#quick-start)
- [Repository Layout](#repository-layout)
- [Demo Guides](#demo-guides)
- [Documentation](#documentation)
- [Prerequisites](#prerequisites)
- [How to Build](#how-to-build)
- [How to Run](#how-to-run)
- [Direct Binary Usage](#direct-binary-usage)
- [Code Execution Workflow](#code-execution-workflow)
- [Key Source Files](#key-source-files)
- [Reusable Components](#reusable-components)
- [How to Adapt This Demo](#how-to-adapt-this-demo)

## Quick Start

Run from the repository root. `scripts/build.sh` downloads and verifies the default YOLOv5s ONNX model under `weights/` when it is missing.

```shell
scripts/build.sh
scripts/run.sh
```

Successful execution should print warmup-stable stage timing and FPS for the RGB and YUV demos, and generate rendered images under `build/output`.

The standard scripts are:

| Script | Purpose |
|---|---|
| `scripts/build.sh` | Downloads the default model when needed, cleans previous build outputs, configures CMake, and builds the project. |
| `scripts/run.sh` | Runs all demos by default, or one selected demo with configurable model, input, output, and loop count. |
| `scripts/benchmark.sh` | Runs all demos by default with repeated measured loops and saves a full log automatically. |

Common first checks:

```shell
scripts/run.sh --demo rgb
scripts/run.sh --demo yuv
scripts/benchmark.sh
```

`scripts/run.sh` resolves paths from the repository root. By default it runs `rgb` and `yuv` serially, uses `weights/yolov5s.onnx`, reads inputs from `build/assets`, and writes rendered images to `build/output`. If `/usr/local/rpp/bin/rpp_server` exists, the script ensures it is running before launching the demo and writes the server log to `build/logs/rpp_server.log`.

`scripts/benchmark.sh` saves the full console log to `build/logs/benchmark_<demo>_<timestamp>.log`.

## Repository Layout

Linked paths in this section can be clicked to jump to their detailed README.

- `CMakeLists.txt`
  Root build entry. Configures shared dependencies and builds the reusable libraries and demo targets.
- `src/perf/`
  Optional `rpp_perf` trace session wrapper. It is active only when CMake is configured with `-DYOLO_ENABLE_RPP_PERF=ON`; perf-enabled demo builds automatically save trace files under `build/trace`.
- `src/utils/`
  Logging, ONNX parse helper, buffer helpers, and CLI/path utilities.
- `src/yolov5/`
  YOLOv5-specific labels, detection types, visualization, and pipeline composition.
- [`rpp/`](rpp/README.md)
  RPP hardware-facing modules, including OpenRT inference, preprocessing kernels/modules, and postprocessing.
- `demos/`
  Executable demo entries, including RGB image and I420 YUV frame demos.
- [`docs/`](docs/README.md)
  Public project documentation, logo assets, and API documentation links.
- `scripts/`
  Standard build, run, and benchmark command entry points.
- `assets/`
  Source sample image and I420 frame copied into `build/assets/` for quick verification.
- `weights/`
  Local model drop-in directory. The directory is kept in Git, but model files are ignored.
- `3rd_party/`
  `argparse` header-only library used by demo main files for CLI parsing.

This repository does not track ONNX model files in Git. `scripts/build.sh` keeps `weights/yolov5s.onnx` available by downloading it from `xdltek/onnx_model_tool` and checking its MD5 when needed. Class names for overlays are the embedded MS COCO 80-class list in `src/yolov5/labels.cpp` (`coco80_class_labels()`), matching standard YOLOv5 COCO pretrained ordering.

## Demo Guides

The root README explains the shared build and architecture. Each executable has its own guide with its input format, command-line options, workflow, and expected output:

Click the guide path to jump to the corresponding demo document:

- [RGB image demo](demos/rgb_image/README.md): RGB/BGR image demo guide.
- [I420 YUV frame demo](demos/yuv_frame/README.md): I420 YUV frame demo guide.

## Documentation

Click the document path to jump to the corresponding documentation:

- [Project documentation](docs/README.md): public documentation entry.
- [API documentation](docs/api/README.md): reusable API index.
- [RPP acceleration modules](rpp/README.md): RPP module guide.
- [Performance trace helper](src/perf/README.md): perf trace guide.

## Prerequisites

Before building this demo, make sure the following dependencies are available:

- Linux development environment
- CMake `>= 3.16.3`
- A compiler with C++17 support
- OpenCV with the components required by this project
- openRT installed under `/usr/local/rpp`
- Runtime libraries required by the target RPP accelerator card environment
- `wget` and `md5sum` when the default model must be downloaded by `scripts/build.sh`

If your openRT installation path is different, update the corresponding path settings in `CMakeLists.txt`.

## How to Build

Run the following commands from the repository root:

```shell
scripts/build.sh
```

If `weights/yolov5s.onnx` is missing, the build script downloads the default model and verifies:

```shell
12f4516124a90056f01692bae254ec6a  weights/yolov5s.onnx
```

After the build completes successfully, the executable should be available at:

```shell
build/bin/yolov5_rgb_image_demo
build/bin/yolov5_yuv_frame_demo
```

Build outputs are grouped as:

```text
build/bin/      demo executables
build/lib/      libyolov5_core.so, librpp_inference.so, librpp_yolo_preprocess.so, librpp_yolo_postprocess.so, and helper libraries
build/assets/   copied sample inputs
build/output/   rendered detection images
build/trace/    optional rpp_perf traces
```

## How to Run

Run demos from the repository root after building the project. The default model path is `weights/yolov5s.onnx`; pass `--model` only when using a different model.

```shell
scripts/run.sh
```

By default, `scripts/run.sh` runs the RGB and YUV demos serially. Use `--demo rgb` or `--demo yuv` when running a single demo or passing a custom input/output path.

Benchmark runs use the same default model and inputs:

```shell
scripts/benchmark.sh
scripts/benchmark.sh --demo rgb --loop 20
scripts/benchmark.sh --demo yuv --loop 20
```

Each script supports `-h` and `--help`.

## Direct Binary Usage

The scripts are the recommended first-use entry point because they resolve paths and protect the `rpp_server` startup flow. After building, users can also run the demo binaries directly from the repository root by passing explicit paths:

```shell
build/bin/yolov5_rgb_image_demo \
  -o weights/yolov5s.onnx \
  -i build/assets/test_1.png \
  --output build/output/output_rgb.jpg \
  -l 1

build/bin/yolov5_yuv_frame_demo \
  -o weights/yolov5s.onnx \
  --yuv build/assets/test_0.yuv \
  --yuv-width 1280 \
  --yuv-height 720 \
  --output build/output/output_yuv.jpg \
  -l 1
```

When running binaries directly, make sure `rpp_server` is already running if the installed RPP environment requires it.

For full command-line options and expected output, see the per-demo guides:

Click the guide path to jump to the corresponding demo document:

- [RGB image demo](demos/rgb_image/README.md): RGB/BGR image demo guide.
- [I420 YUV frame demo](demos/yuv_frame/README.md): I420 YUV frame demo guide.

## Code Execution Workflow

```text
+------------------------+
| Parse CLI arguments    |
| and resolve input paths|
+------------------------+
            |
            v
+------------------------+
| Init optional perf     |
| trace session in perf  |
| builds                 |
+------------------------+
            |
            v
+------------------------+
| Init YOLOv5 pipeline   |
| and openRT engine      |
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

Linked source paths in this section can be clicked to jump to their module README.

- [`rpp/inference/rpp_infer_engine.*`](rpp/inference/README.md)
  Generic OpenRT on RPP inference wrapper. Parses the ONNX model, builds the execution engine, allocates buffers, and runs inference.
- [`rpp/preprocess/rpp_yolo_preprocessor.*`](rpp/preprocess/README.md)
  RPP preprocessing module and kernels for RGB/BGR/I420 inputs.
- [`rpp/postprocess/rpp_yolo_postprocessor.*`](rpp/postprocess/README.md)
  RPP postprocessing module for YOLO output slicing and NMS.
- [`src/yolov5/yolo_pipeline.*`](src/yolov5/README.md)
  YOLOv5 pipeline composition. Runs RPP preprocessing, inference, and RPP postprocessing once per call without hidden warmup.
- `src/perf/perf_trace_session.*`
  Optional rpp_perf trace wrapper used automatically when the project is built with `-DYOLO_ENABLE_RPP_PERF=ON`.
- `demos/rgb_image/main.cpp`
  RGB/BGR image demo entry point.
- `demos/yuv_frame/main.cpp`
  I420 YUV frame demo entry point.

## Reusable Components

- Command-line argument flow for model and image selection
- Input preprocessing logic for YOLOv5-style models
- Inference engine initialization and creation flow
- Host/device buffer usage through shared helpers in the RPP execution path
- Detection decoding, NMS, and image rendering logic

## How to Adapt This Demo

- Point -o to any YOLOv5-compatible ONNX model produced by your pipeline.
- If your model uses a different class ordering or dataset than COCO 80, edit `coco80_class_labels()` in `src/yolov5/labels.cpp` (or replace it with your own table).
- Replace the sample image path with your own image source.
- Reuse `rpp/inference/RppInferEngine`, `src/yolov5/YoloV5Pipeline`, and the demo argument/timing pattern as the starting point for your own RPP-based inference application.

---

<a id="chinese"></a>

# YOLOv5 RPP 推理 Demo

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

![XDL Logo](docs/logo/logo_color_horizontal.png)

本仓库是一个 C++ 示例工程，用于在 RPP 加速硬件上运行 YOLOv5 目标检测模型。
它的目标是帮助用户快速完成编译、运行推理，并理解如何复用项目中的 RPP 推理、前处理、后处理和 demo 组织方式。

**中文目录**

点击下方章节链接可在本文档内跳转：

- [快速开始](#快速开始)
- [仓库结构](#仓库结构)
- [Demo 说明](#demo-说明)
- [项目文档](#项目文档)
- [环境依赖](#环境依赖)
- [构建方式](#构建方式)
- [运行方式](#运行方式)
- [直接运行二进制](#直接运行二进制)
- [代码执行流程](#代码执行流程)
- [关键源码](#关键源码)
- [可复用组件](#可复用组件)
- [如何适配自己的应用](#如何适配自己的应用)

## 快速开始

在仓库根目录执行。`scripts/build.sh` 会在默认模型缺失时自动下载并校验 `weights/yolov5s.onnx`。

```shell
scripts/build.sh
scripts/run.sh
```

执行成功后，程序会打印 RGB 和 YUV demo 在 warmup 之后稳定的阶段耗时和 FPS，并在 `build/output` 下生成绘制结果。

标准脚本如下：

| 脚本 | 用途 |
|---|---|
| `scripts/build.sh` | 在需要时下载默认模型，清理上一次构建产物，执行 CMake 配置并编译工程。 |
| `scripts/run.sh` | 默认运行全部 demo，也支持选择单个 demo 并配置模型、输入、输出和 loop 次数。 |
| `scripts/benchmark.sh` | 默认对全部 demo 执行重复测量 loop，并自动保存完整日志。 |

常用首次检查命令：

```shell
scripts/run.sh --demo rgb
scripts/run.sh --demo yuv
scripts/benchmark.sh
```

`scripts/run.sh` 会从仓库根目录解析路径。默认会串行运行 `rgb` 和 `yuv`，使用 `weights/yolov5s.onnx`，从 `build/assets` 读取输入，并将绘制结果写入 `build/output`。如果 `/usr/local/rpp/bin/rpp_server` 存在，脚本会在启动 demo 前保证该服务已运行，并将 server 日志写入 `build/logs/rpp_server.log`。

`scripts/benchmark.sh` 会将完整控制台日志保存到 `build/logs/benchmark_<demo>_<timestamp>.log`。

## 仓库结构

本节中带链接的路径可点击跳转到对应 README。

- `CMakeLists.txt`
  根构建入口。负责配置公共依赖，并构建可复用库和 demo 可执行文件。
- `src/perf/`
  可选的 `rpp_perf` trace 会话封装。该功能只有在 CMake 配置 `-DYOLO_ENABLE_RPP_PERF=ON` 时生效；启用 perf 的 demo 构建会自动在 `build/trace` 下保存 trace 文件。
- `src/utils/`
  日志、ONNX 解析辅助、buffer 辅助、命令行和路径工具。
- `src/yolov5/`
  YOLOv5 业务相关代码，包括标签、检测结果类型、可视化和 pipeline 组装。
- [`rpp/`](rpp/README.md)
  面向 RPP 硬件执行路径的模块，包括 OpenRT 推理、前处理 kernel/模块和后处理模块。
- `demos/`
  demo 可执行入口，包括 RGB 图片和 I420 YUV 帧 demo。
- [`docs/`](docs/README.md)
  公开项目文档、logo 资源和 API 文档入口。
- `scripts/`
  标准构建、运行和 benchmark 命令入口。
- `assets/`
  示例输入资源，构建时会复制到 `build/assets/`，便于快速验证。
- `weights/`
  本地模型放置目录。目录本身保留在 Git 中，但模型文件不会提交。
- `3rd_party/`
  demo 命令行解析使用的 `argparse` header-only 三方库。

本仓库不在 Git 中跟踪 ONNX 模型文件。`scripts/build.sh` 会在需要时从 `xdltek/onnx_model_tool` 下载 `weights/yolov5s.onnx` 并进行 MD5 校验。可视化使用的类别名称来自 `src/yolov5/labels.cpp` 中内置的 MS COCO 80 类列表 `coco80_class_labels()`，顺序与标准 YOLOv5 COCO 预训练模型一致。

## Demo 说明

根 README 说明公共构建方式和整体架构。每个 demo 目录下还有独立说明文档，用于描述对应输入格式、命令行参数、执行流程和输出结果：

点击文档路径可跳转到对应 demo 说明：

- [RGB 图片 demo](demos/rgb_image/README.md)：RGB/BGR 图片 demo 说明。
- [I420 YUV 帧 demo](demos/yuv_frame/README.md)：I420 YUV 帧 demo 说明。

## 项目文档

点击文档路径可跳转到对应说明：

- [项目文档入口](docs/README.md)：公开文档入口。
- [API 文档](docs/api/README.md)：可复用 API 索引。
- [RPP 加速模块](rpp/README.md)：RPP 模块说明。
- [性能 trace 辅助模块](src/perf/README.md)：perf trace 说明。

## 环境依赖

构建前请确认环境中已具备：

- Linux 开发环境
- CMake `>= 3.16.3`
- 支持 C++17 的编译器
- OpenCV 以及本项目需要的组件
- 安装在 `/usr/local/rpp` 下的 openRT
- 目标 RPP 加速卡运行所需的运行时库
- 当 `scripts/build.sh` 需要下载默认模型时，需要 `wget` 和 `md5sum`

如果 openRT 安装路径不同，请同步修改 `CMakeLists.txt` 中的相关路径配置。

## 构建方式

在仓库根目录执行：

```shell
scripts/build.sh
```

如果 `weights/yolov5s.onnx` 不存在，构建脚本会下载默认模型并校验：

```shell
12f4516124a90056f01692bae254ec6a  weights/yolov5s.onnx
```

构建完成后，可执行文件默认生成在：

```shell
build/bin/yolov5_rgb_image_demo
build/bin/yolov5_yuv_frame_demo
```

构建产物按类型归类：

```text
build/bin/      demo 可执行文件
build/lib/      libyolov5_core.so、librpp_inference.so、librpp_yolo_preprocess.so、librpp_yolo_postprocess.so 以及辅助库
build/assets/   复制后的示例输入
build/output/   绘制检测框后的输出图片
build/trace/    可选的 rpp_perf trace 文件
```

## 运行方式

构建完成后，在仓库根目录运行 demo。默认模型路径为 `weights/yolov5s.onnx`；只有使用其他模型时才需要通过 `--model` 指定。

```shell
scripts/run.sh
```

默认情况下，`scripts/run.sh` 会串行运行 RGB 和 YUV demo。需要运行单个 demo 或传入自定义输入/输出路径时，使用 `--demo rgb` 或 `--demo yuv`。

Benchmark 使用相同的默认模型和输入：

```shell
scripts/benchmark.sh
scripts/benchmark.sh --demo rgb --loop 20
scripts/benchmark.sh --demo yuv --loop 20
```

每个脚本都支持 `-h` 和 `--help`。

## 直接运行二进制

推荐第一次使用时通过脚本运行，因为脚本会统一解析路径并保护 `rpp_server` 启动流程。构建完成后，也可以从仓库根目录直接执行 demo 二进制，并显式传入路径：

```shell
build/bin/yolov5_rgb_image_demo \
  -o weights/yolov5s.onnx \
  -i build/assets/test_1.png \
  --output build/output/output_rgb.jpg \
  -l 1

build/bin/yolov5_yuv_frame_demo \
  -o weights/yolov5s.onnx \
  --yuv build/assets/test_0.yuv \
  --yuv-width 1280 \
  --yuv-height 720 \
  --output build/output/output_yuv.jpg \
  -l 1
```

直接运行二进制时，如果当前 RPP 环境需要 `rpp_server`，请先确认该服务已在后台运行。

完整参数和预期输出请参考各 demo 文档：

点击文档路径可跳转到对应 demo 说明：

- [RGB 图片 demo](demos/rgb_image/README.md)：RGB/BGR 图片 demo 说明。
- [I420 YUV 帧 demo](demos/yuv_frame/README.md)：I420 YUV 帧 demo 说明。

## 代码执行流程

```text
+------------------------+
| 解析命令行参数和路径   |
+------------------------+
            |
            v
+------------------------+
| perf 构建中初始化      |
| 可选 trace 会话        |
+------------------------+
            |
            v
+------------------------+
| 初始化 YOLOv5 pipeline |
| 和 openRT engine       |
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

本节中带链接的源码路径可点击跳转到对应模块 README。

- [`rpp/inference/rpp_infer_engine.*`](rpp/inference/README.md)
  通用 OpenRT on RPP 推理封装。负责解析 ONNX、构建执行 engine、分配 buffer 并执行推理。
- [`rpp/preprocess/rpp_yolo_preprocessor.*`](rpp/preprocess/README.md)
  RGB/BGR/I420 输入的 RPP 前处理模块和 kernel。
- [`rpp/postprocess/rpp_yolo_postprocessor.*`](rpp/postprocess/README.md)
  YOLO 输出切分和 NMS 的 RPP 后处理模块。
- [`src/yolov5/yolo_pipeline.*`](src/yolov5/README.md)
  YOLOv5 pipeline 组装层。每次调用执行一次 RPP 前处理、推理和 RPP 后处理，不隐藏 warmup。
- `src/perf/perf_trace_session.*`
  使用 `-DYOLO_ENABLE_RPP_PERF=ON` 构建时自动启用的可选 rpp_perf trace 封装。
- `demos/rgb_image/main.cpp`
  RGB/BGR 图片 demo 入口。
- `demos/yuv_frame/main.cpp`
  I420 YUV 帧 demo 入口。

## 可复用组件

- 模型和图片选择的命令行参数组织方式
- YOLOv5 风格模型的输入前处理逻辑
- 推理 engine 初始化和创建流程
- RPP 执行路径中的 host/device buffer 使用方式
- 检测结果解码、NMS 和图片可视化逻辑

## 如何适配自己的应用

- 使用 `-o` 指向自己的 YOLOv5 兼容 ONNX 模型。
- 如果模型类别顺序或数据集不同于 COCO 80，请修改 `src/yolov5/labels.cpp` 中的 `coco80_class_labels()`，或替换为自己的类别表。
- 将示例图片路径替换为自己的输入源。
- 复用 `rpp/inference/RppInferEngine`、`src/yolov5/YoloV5Pipeline` 和 demo 中的参数/计时模式，作为自定义 RPP 推理应用的起点。
