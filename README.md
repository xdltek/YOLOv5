<a id="english"></a>

# RPP-Accelerated YOLOv5 Application

**Language: English (default) | [切换到中文](#chinese)**

![XDL Logo](docs/logo/logo_color_horizontal.png)

This application brings supported YOLOv5 object detection to RPP accelerator hardware. It provides streamlined build, inference, and performance measurement workflows for RGB images and I420 YUV frames, together with C++ interfaces for solution integration.

Application highlights:

- RPP-accelerated preprocessing, OpenRT inference, and postprocessing in one integrated pipeline.
- RGB/BGR image and I420 YUV frame input workflows.
- Automatic engine caching for faster startup on subsequent runs with the same model.
- Annotated image output plus latency, FPS, and stage-level performance reporting.
- Script-based operation and C++ interfaces for system integration.

**Table Of Contents**

Click a section link below to jump within this README:

- [Quick Start](#quick-start)
- [Prerequisites](#prerequisites)
- [How to Build](#how-to-build)
- [How to Run](#how-to-run)
- [Package Contents](#package-contents)
- [Workload Guides](#workload-guides)
- [Documentation](#documentation)
- [Advanced: Direct Executable Usage](#advanced-direct-executable-usage)
- [Processing Workflow](#processing-workflow)
- [Integration Reference](#integration-reference)
- [Integration Capabilities](#integration-capabilities)
- [Application Integration](#application-integration)
- [Appendix: Supported Models And ONNX Export](#appendix-supported-models-and-onnx-export)

## Quick Start

Run the following commands from the application root. Replace the model path if your ONNX file has a different name.

```shell
scripts/build.sh
scripts/run.sh -o weights/yolov5s_sim.onnx
```

Successful execution reports measured latency and FPS after warmup, then saves annotated result images under `build/output`.

The standard scripts are:

| Script | Purpose |
|---|---|
| `scripts/build.sh` | Cleans previous build outputs, configures CMake, and builds the project; it does not inspect or prepare a model. |
| `scripts/run.sh` | Runs both supported workloads or one selected workload with configurable model, input, output, and measurement count. |
| `scripts/benchmark.sh` | Measures one or both workloads over repeated runs and saves the complete performance log. |

Run a single input format or start a benchmark as follows:

```shell
scripts/run.sh -o weights/yolov5s_sim.onnx --demo rgb
scripts/run.sh -o weights/yolov5s_sim.onnx --demo yuv
scripts/benchmark.sh -o weights/yolov5s_sim.onnx --demo rgb --loop 20
```

`scripts/run.sh` resolves paths from the package root. By default it runs the `rgb` and `yuv` workloads serially, uses `weights/yolov5s.onnx`, reads inputs from `build/assets`, and writes annotated images to `build/output`. If `/usr/local/rpp/bin/rpp_server` is installed, the script ensures that the service is available before inference and writes its log to `build/logs/rpp_server.log`.

`scripts/benchmark.sh` saves the full console log to `build/logs/benchmark_<demo>_<timestamp>.log`.

## Package Contents

Linked paths in this section can be clicked to jump to their detailed README.

- `CMakeLists.txt`
  Main build configuration for application executables and shared runtime libraries.
- `src/perf/`
  Optional `rpp_perf` tracing support. It is active only when CMake is configured with `-DYOLO_ENABLE_RPP_PERF=ON`; enabled builds save trace files under `build/trace`.
- `src/utils/`
  Shared logging, ONNX parsing, buffer management, and command-line utilities.
- `src/yolov5/`
  YOLOv5-specific labels, detection types, visualization, and pipeline composition.
- [`rpp/`](rpp/README.md)
  RPP-accelerated inference, preprocessing, and postprocessing components.
- `demos/`
  Application executable sources for RGB image and I420 YUV frame workloads.
- [`docs/`](docs/README.md)
  Public project documentation, logo assets, and API documentation links.
- `scripts/`
  Customer-facing build, inference, and performance measurement commands.
- `assets/`
  Reference RGB image and I420 frame copied into `build/assets/` during configuration.
- `weights/`
  Local model drop-in directory. The directory is kept in Git, but model files are ignored.
- `3rd_party/`
  `argparse` header-only dependency used by the application executables.

ONNX model files are supplied separately and are not included in Git. Building is independent of model availability and never downloads or verifies an ONNX file. At runtime, pass a model with `-o/--onnx`; when that option is omitted, the scripts use `weights/yolov5s.onnx` if it exists. Detection overlays use the embedded MS COCO 80-class list in `src/yolov5/labels.cpp`, following the standard YOLOv5 COCO class order.

## Workload Guides

Each supported workload provides a dedicated guide covering its input format, command-line options, processing flow, and expected output:

Select the guide for the required input format:

- [RGB image workload](demos/rgb_image/README.md): RGB/BGR image inference.
- [I420 YUV frame workload](demos/yuv_frame/README.md): I420 YUV frame inference.

## Documentation

Click the document path to jump to the corresponding documentation:

- [Project documentation](docs/README.md): public documentation entry.
- [API documentation](docs/api/README.md): integration API index.
- [RPP acceleration components](rpp/README.md): accelerated processing reference.
- [Performance tracing](src/perf/README.md): optional trace configuration and output.

## Prerequisites

Before building the application, make sure the following dependencies are available:

- Linux development environment
- CMake `>= 3.16.3`
- A compiler with C++17 support
- OpenCV with the components required by this project
- openRT installed under `/usr/local/rpp`
- Runtime libraries required by the target RPP accelerator card environment

If your openRT installation path is different, update the corresponding path settings in `CMakeLists.txt`.

## How to Build

Run the following command from the application root:

```shell
scripts/build.sh
```

The build does not require an ONNX model. A supported model is required only for inference or performance measurement.

After the build completes successfully, the executable should be available at:

```shell
build/bin/yolov5_rgb_image_demo
build/bin/yolov5_yuv_frame_demo
```

Build outputs are grouped as:

```text
build/bin/      application executables
build/lib/      application runtime and RPP acceleration libraries
build/assets/   reference input files
build/output/   rendered detection images
build/trace/    optional rpp_perf traces
```

## How to Run

Run the application from the package root after building. Specifying the ONNX path explicitly is recommended. If `-o/--onnx` is omitted, the script uses `weights/yolov5s.onnx` only when that file exists.

| Option | Description | Default |
|---|---|---|
| `-o, --onnx PATH` | Static, simplified YOLOv5 ONNX model | `weights/yolov5s.onnx` when present |
| `-d, --demo NAME` | Workload: `all`, `rgb`, or `yuv` | `all` |
| `-i, --input PATH` | Input image or I420 YUV file | Reference input for the selected workload |
| `--output PATH` | Rendered result image | `build/output/output_<demo>.jpg` |
| `-l, --loop COUNT` | Number of measured frames after warmup | `1` |

```shell
scripts/run.sh -o weights/yolov5s_sim.onnx
scripts/run.sh -o weights/yolov5s_sim.onnx --demo rgb \
  -i /path/to/input.jpg \
  --output build/output/result.jpg
```

By default, `scripts/run.sh` runs the RGB and YUV workloads serially. Use `--demo rgb` or `--demo yuv` to select one workload or provide a custom input/output path. The option name `--demo` is retained for command-line compatibility.

Benchmark runs use the same default model and inputs:

```shell
scripts/benchmark.sh -o weights/yolov5s_sim.onnx --demo rgb --loop 20
scripts/benchmark.sh -o weights/yolov5s_sim.onnx --demo yuv --loop 20
```

Each script supports `-h` and `--help`.

On the first run for a model, the runtime builds an engine and stores it under `<model-directory>/.engine_cache/`. Later runs with identical ONNX content, OpenRT version, and engine build settings reuse that engine. Runtime logs always state whether the engine was built or reused. Replacing the ONNX content produces a different cache key and triggers a new build.

## Advanced: Direct Executable Usage

The scripts provide the recommended customer workflow because they resolve paths and manage `rpp_server` startup. For system integration, the application executables can also be launched directly with explicit paths:

The scripts and C++ executables use the same model option: `-o/--onnx`. Use `--output` for the annotated result image. For input data, the application script uses `-i/--input`; the RGB executable uses `-i/--image`, and the YUV executable uses `--yuv`.

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

For complete command-line options and expected output, see the workload guides:

Select the applicable workload guide:

- [RGB image workload](demos/rgb_image/README.md): RGB/BGR image inference.
- [I420 YUV frame workload](demos/yuv_frame/README.md): I420 YUV frame inference.

## Processing Workflow

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

## Integration Reference

The following components provide the primary C++ integration interfaces:

- [`rpp/inference/rpp_infer_engine.*`](rpp/inference/README.md)
  OpenRT on RPP inference component for model loading, engine initialization, buffer allocation, and execution.
- [`rpp/preprocess/rpp_yolo_preprocessor.*`](rpp/preprocess/README.md)
  RPP preprocessing for RGB, BGR, and I420 inputs.
- [`rpp/postprocess/rpp_yolo_postprocessor.*`](rpp/postprocess/README.md)
  RPP postprocessing for YOLO output decoding and NMS.
- [`src/yolov5/yolo_pipeline.*`](src/yolov5/README.md)
  YOLOv5 pipeline composition. Runs RPP preprocessing, inference, and RPP postprocessing once per call without hidden warmup.
- `src/perf/perf_trace_session.*`
  Optional `rpp_perf` tracing support enabled with `-DYOLO_ENABLE_RPP_PERF=ON`.
- `demos/rgb_image/main.cpp`
  RGB/BGR application executable.
- `demos/yuv_frame/main.cpp`
  I420 YUV application executable.

## Integration Capabilities

- Command-line argument flow for model and image selection
- Input preprocessing logic for YOLOv5-style models
- Inference engine initialization and creation flow
- Host/device buffer management across the RPP execution path
- Detection decoding, NMS, and image rendering logic

## Application Integration

- Use `scripts/run.sh -o/--onnx` to select a supported YOLOv5 ONNX model. Use `--output` for the rendered result image.
- If your model uses a different class ordering or dataset than COCO 80, edit `coco80_class_labels()` in `src/yolov5/labels.cpp` (or replace it with your own table).
- Replace the reference input path with the target image or frame source.
- Integrate `rpp/inference/RppInferEngine` and `src/yolov5/YoloV5Pipeline` into the target RPP-based inference workflow.

## Appendix: Supported Models And ONNX Export

### Supported model family

Models exported from the official [Ultralytics YOLOv5 repository](https://github.com/ultralytics/yolov5) are supported for the classic YOLOv5 variants below:

| Variant | Typical weight name | Status |
| :--- | :--- | :--- |
| YOLOv5n | `yolov5n.pt` | Supported |
| YOLOv5s | `yolov5s.pt` | Supported |
| YOLOv5m | `yolov5m.pt` | Supported |
| YOLOv5l | `yolov5l.pt` | Supported |
| YOLOv5x | `yolov5x.pt` | Supported |

The current release does not support the following Ultralytics variants:

- YOLOv5u models: `yolov5nu`, `yolov5su`, `yolov5mu`, `yolov5lu`, and `yolov5xu`.
- YOLOv5 P6-u models: `yolov5n6u`, `yolov5s6u`, `yolov5m6u`, `yolov5l6u`, and `yolov5x6u`.

### ONNX export requirements

This product supports ONNX models with static input dimensions only. Simplify the exported model with `onnxsim`, and do not use the YOLOv5 `--dynamic` export option.

```shell
git clone https://github.com/ultralytics/yolov5.git
cd yolov5
pip install -r requirements.txt
pip install onnx onnxruntime onnxsim

python export.py \
  --weights yolov5s.pt \
  --include onnx \
  --imgsz 640 640 \
  --batch-size 1 \
  --opset 12

python -m onnxsim yolov5s.onnx yolov5s_sim.onnx
```

Replace `yolov5s.pt` with `yolov5n.pt`, `yolov5m.pt`, `yolov5l.pt`, or `yolov5x.pt` as required, and update both filenames in the `onnxsim` command. Use the generated `*_sim.onnx` file when running this product.

---

<a id="chinese"></a>

# RPP 加速 YOLOv5 应用

**语言：[English](#english) | 中文**

![XDL Logo](docs/logo/logo_color_horizontal.png)

本应用面向 RPP 加速硬件提供 YOLOv5 目标检测能力，支持 RGB 图片和 I420 YUV 帧的标准化编译、推理与性能测量流程，并提供用于方案集成的 C++ 接口。

应用亮点：

- 在统一 pipeline 中完成 RPP 加速前处理、OpenRT 推理和后处理。
- 支持 RGB/BGR 图片和 I420 YUV 帧输入流程。
- 相同模型再次运行时自动复用 engine 缓存，减少重复构建等待。
- 输出带检测结果的图片，并报告延迟、FPS 和各阶段性能数据。
- 同时提供脚本化操作方式和面向系统集成的 C++ 接口。

**中文目录**

点击下方章节链接可在本文档内跳转：

- [快速开始](#快速开始)
- [环境依赖](#环境依赖)
- [构建方式](#构建方式)
- [运行方式](#运行方式)
- [交付内容](#交付内容)
- [工作负载指南](#工作负载指南)
- [项目文档](#项目文档)
- [高级用法：直接运行可执行程序](#高级用法直接运行可执行程序)
- [处理流程](#处理流程)
- [集成参考](#集成参考)
- [集成能力](#集成能力)
- [应用集成](#应用集成)
- [附录：支持的模型与 ONNX 导出](#附录支持的模型与-onnx-导出)

## 快速开始

在应用根目录执行以下命令。如果您的 ONNX 文件名不同，请替换模型路径。

```shell
scripts/build.sh
scripts/run.sh -o weights/yolov5s_sim.onnx
```

执行成功后，应用会报告 RGB 和 YUV 工作负载在 warmup 后的延迟与 FPS，并在 `build/output` 下保存带检测结果的图片。

标准脚本如下：

| 脚本 | 用途 |
|---|---|
| `scripts/build.sh` | 清理上一次构建产物，执行 CMake 配置并编译应用；不检查、下载或校验模型。 |
| `scripts/run.sh` | 运行全部工作负载，或选择单一工作负载并配置模型、输入、输出和测量次数。 |
| `scripts/benchmark.sh` | 对一个或全部工作负载执行重复测量，并保存完整性能日志。 |

运行单一输入格式或启动性能测试：

```shell
scripts/run.sh -o weights/yolov5s_sim.onnx --demo rgb
scripts/run.sh -o weights/yolov5s_sim.onnx --demo yuv
scripts/benchmark.sh -o weights/yolov5s_sim.onnx --demo rgb --loop 20
```

`scripts/run.sh` 会从应用根目录解析路径。默认串行运行 `rgb` 和 `yuv` 工作负载，使用 `weights/yolov5s.onnx`，从 `build/assets` 读取输入，并将标注结果写入 `build/output`。如果系统安装了 `/usr/local/rpp/bin/rpp_server`，脚本会在推理前确保服务可用，并将日志写入 `build/logs/rpp_server.log`。

`scripts/benchmark.sh` 会将完整控制台日志保存到 `build/logs/benchmark_<demo>_<timestamp>.log`。

## 交付内容

本节中带链接的路径可点击跳转到对应 README。

- `CMakeLists.txt`
  应用可执行程序和公共运行库的主构建配置。
- `src/perf/`
  可选的 `rpp_perf` 性能追踪能力。使用 `-DYOLO_ENABLE_RPP_PERF=ON` 构建时启用，并将 trace 文件保存到 `build/trace`。
- `src/utils/`
  公共日志、ONNX 解析、buffer 管理、命令行和路径工具。
- `src/yolov5/`
  YOLOv5 业务相关代码，包括标签、检测结果类型、可视化和 pipeline 组装。
- [`rpp/`](rpp/README.md)
  基于 RPP 加速的推理、前处理和后处理组件。
- `demos/`
  RGB 图片和 I420 YUV 帧工作负载的应用程序源码。
- [`docs/`](docs/README.md)
  公开项目文档、logo 资源和 API 文档入口。
- `scripts/`
  面向客户的构建、推理和性能测量命令。
- `assets/`
  参考 RGB 图片和 I420 帧，配置时复制到 `build/assets/`。
- `weights/`
  本地模型放置目录。目录本身保留在 Git 中，但模型文件不会提交。
- `3rd_party/`
  应用可执行程序使用的 `argparse` header-only 依赖。

ONNX 模型由客户单独提供，不随 Git 交付。编译过程与模型是否存在无关，也不会下载或校验 ONNX。运行时可通过 `-o/--onnx` 传入模型；未传入时，仅在 `weights/yolov5s.onnx` 存在的情况下使用该默认模型。检测结果使用 `src/yolov5/labels.cpp` 中内置的 MS COCO 80 类名称，顺序与标准 YOLOv5 COCO 预训练模型一致。

## 工作负载指南

每种受支持的工作负载都提供独立指南，说明输入格式、命令行参数、处理流程和预期输出：

请根据所需输入格式选择指南：

- [RGB 图片工作负载](demos/rgb_image/README.md)：RGB/BGR 图片推理。
- [I420 YUV 帧工作负载](demos/yuv_frame/README.md)：I420 YUV 帧推理。

## 项目文档

点击文档路径可跳转到对应说明：

- [项目文档入口](docs/README.md)：公开文档入口。
- [API 文档](docs/api/README.md)：集成 API 索引。
- [RPP 加速组件](rpp/README.md)：加速处理参考。
- [性能追踪](src/perf/README.md)：可选 trace 配置与输出说明。

## 环境依赖

构建应用前，请确认环境满足以下要求：

- Linux 开发环境
- CMake `>= 3.16.3`
- 支持 C++17 的编译器
- OpenCV 以及本项目需要的组件
- 安装在 `/usr/local/rpp` 下的 openRT
- 目标 RPP 加速卡运行所需的运行时库

如果 openRT 安装路径不同，请同步修改 `CMakeLists.txt` 中的相关路径配置。

## 构建方式

在应用根目录执行：

```shell
scripts/build.sh
```

编译不需要 ONNX 模型；执行推理或性能测量时才需要准备受支持的模型。

构建完成后，可执行文件默认生成在：

```shell
build/bin/yolov5_rgb_image_demo
build/bin/yolov5_yuv_frame_demo
```

构建产物按类型归类：

```text
build/bin/      应用可执行程序
build/lib/      应用运行库和 RPP 加速库
build/assets/   参考输入文件
build/output/   绘制检测框后的输出图片
build/trace/    可选的 rpp_perf trace 文件
```

## 运行方式

构建完成后，在应用根目录运行。建议显式指定 ONNX 模型路径；若省略 `-o/--onnx`，脚本仅在 `weights/yolov5s.onnx` 存在时使用该默认模型。

| 参数 | 说明 | 默认值 |
|---|---|---|
| `-o, --onnx PATH` | 静态维度、已简化的 YOLOv5 ONNX 模型 | 存在时使用 `weights/yolov5s.onnx` |
| `-d, --demo NAME` | 运行范围：`all`、`rgb` 或 `yuv` | `all` |
| `-i, --input PATH` | 输入图片或 I420 YUV 文件 | 对应工作负载的参考输入 |
| `--output PATH` | 绘制检测结果的输出图片 | `build/output/output_<demo>.jpg` |
| `-l, --loop COUNT` | warmup 后的测量帧数 | `1` |

```shell
scripts/run.sh -o weights/yolov5s_sim.onnx
scripts/run.sh -o weights/yolov5s_sim.onnx --demo rgb \
  -i /path/to/input.jpg \
  --output build/output/result.jpg
```

默认情况下，`scripts/run.sh` 串行运行 RGB 和 YUV 工作负载。使用 `--demo rgb` 或 `--demo yuv` 可选择单一工作负载或传入自定义输入/输出路径。参数名 `--demo` 为保持命令行兼容性而保留。

Benchmark 使用相同的默认模型和输入：

```shell
scripts/benchmark.sh -o weights/yolov5s_sim.onnx --demo rgb --loop 20
scripts/benchmark.sh -o weights/yolov5s_sim.onnx --demo yuv --loop 20
```

每个脚本都支持 `-h` 和 `--help`。

## 高级用法：直接运行可执行程序

脚本是推荐的客户使用方式，可统一解析路径并管理 `rpp_server` 启动。系统集成场景也可以直接运行应用可执行程序，并显式传入路径：

脚本和 C++ 可执行程序统一使用 `-o/--onnx` 指定模型，使用 `--output` 指定标注结果图片。输入数据方面，应用脚本使用 `-i/--input`，RGB 可执行程序使用 `-i/--image`，YUV 可执行程序使用 `--yuv`。

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

完整参数和预期输出请参考工作负载指南：

请选择对应的工作负载指南：

- [RGB 图片工作负载](demos/rgb_image/README.md)：RGB/BGR 图片推理。
- [I420 YUV 帧工作负载](demos/yuv_frame/README.md)：I420 YUV 帧推理。

## 处理流程

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

## 集成参考

以下组件提供主要的 C++ 集成接口：

- [`rpp/inference/rpp_infer_engine.*`](rpp/inference/README.md)
  OpenRT on RPP 推理组件，负责模型加载、engine 初始化、buffer 分配和推理执行。
- [`rpp/preprocess/rpp_yolo_preprocessor.*`](rpp/preprocess/README.md)
  RGB、BGR 和 I420 输入的 RPP 前处理。
- [`rpp/postprocess/rpp_yolo_postprocessor.*`](rpp/postprocess/README.md)
  YOLO 输出解码和 NMS 的 RPP 后处理。
- [`src/yolov5/yolo_pipeline.*`](src/yolov5/README.md)
  YOLOv5 pipeline 组装层。每次调用执行一次 RPP 前处理、推理和 RPP 后处理，不隐藏 warmup。
- `src/perf/perf_trace_session.*`
  使用 `-DYOLO_ENABLE_RPP_PERF=ON` 启用的可选 `rpp_perf` 性能追踪能力。
- `demos/rgb_image/main.cpp`
  RGB/BGR 图片应用可执行程序。
- `demos/yuv_frame/main.cpp`
  I420 YUV 帧应用可执行程序。

## 集成能力

- 模型和图片选择的命令行参数组织方式
- YOLOv5 风格模型的输入前处理逻辑
- 推理 engine 初始化和创建流程
- RPP 执行路径中的 host/device buffer 使用方式
- 检测结果解码、NMS 和图片可视化逻辑

## 应用集成

- 使用 `scripts/run.sh -o/--onnx` 指定受支持的 YOLOv5 ONNX 模型，使用 `--output` 指定绘制结果的输出图片路径。
- 如果模型类别顺序或数据集不同于 COCO 80，请修改 `src/yolov5/labels.cpp` 中的 `coco80_class_labels()`，或替换为自己的类别表。
- 将参考图片路径替换为目标输入源。
- 将 `rpp/inference/RppInferEngine` 和 `src/yolov5/YoloV5Pipeline` 集成到目标 RPP 推理流程。

## 附录：支持的模型与 ONNX 导出

### 支持的模型系列

当前支持通过 Ultralytics 官方 [YOLOv5 仓库](https://github.com/ultralytics/yolov5) 导出的以下经典 YOLOv5 模型：

| 模型 | 常用权重文件名 | 支持状态 |
| :--- | :--- | :--- |
| YOLOv5n | `yolov5n.pt` | 支持 |
| YOLOv5s | `yolov5s.pt` | 支持 |
| YOLOv5m | `yolov5m.pt` | 支持 |
| YOLOv5l | `yolov5l.pt` | 支持 |
| YOLOv5x | `yolov5x.pt` | 支持 |

当前版本不支持以下 Ultralytics 模型：

- YOLOv5u：`yolov5nu`、`yolov5su`、`yolov5mu`、`yolov5lu`、`yolov5xu`。
- YOLOv5 P6-u：`yolov5n6u`、`yolov5s6u`、`yolov5m6u`、`yolov5l6u`、`yolov5x6u`。

### ONNX 导出要求

本产品仅支持静态输入维度的 ONNX 模型。导出后必须使用 `onnxsim` 简化模型，并且不要使用 YOLOv5 的 `--dynamic` 导出参数。

```shell
git clone https://github.com/ultralytics/yolov5.git
cd yolov5
pip install -r requirements.txt
pip install onnx onnxruntime onnxsim

python export.py \
  --weights yolov5s.pt \
  --include onnx \
  --imgsz 640 640 \
  --batch-size 1 \
  --opset 12

python -m onnxsim yolov5s.onnx yolov5s_sim.onnx
```

可根据需要将 `yolov5s.pt` 替换为 `yolov5n.pt`、`yolov5m.pt`、`yolov5l.pt` 或 `yolov5x.pt`，并同步修改 `onnxsim` 命令中的两个文件名。运行本产品时，请使用生成的 `*_sim.onnx` 文件。
