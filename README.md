<a id="english"></a>

# RPP-Accelerated YOLOv5 Application

**Language: English (default) | [切换到中文](#chinese)**

![XDL Logo](docs/logo/logo_color_horizontal.png)

This package runs validated classic YOLOv5 object-detection models on RPP accelerator hardware. It provides ready-to-use build, inference, and benchmark commands for RGB images and I420 YUV frames, together with reusable C++ integration examples.

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
- [Known Limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [Appendix: Supported Models And ONNX Export](#appendix-supported-models-and-onnx-export)

## Quick Start

Run the following commands from the repository root. Replace the example model path if your ONNX file has a different name.

```shell
scripts/build.sh
scripts/run.sh -m weights/yolov5s.onnx
```

Successful execution reports warmup-stabilized latency and FPS for the RGB and YUV workloads and saves annotated result images under `build/output`.

The standard scripts are:

| Script | Purpose |
|---|---|
| `scripts/build.sh` | Cleans previous build outputs, configures CMake, and builds the project; it does not inspect or prepare a model. |
| `scripts/run.sh` | Runs both supported workloads or one selected workload with configurable model, input, output, and measurement count. |
| `scripts/benchmark.sh` | Measures one or both workloads over repeated runs and saves the complete performance log. |

Run a single input format or start a benchmark as follows:

```shell
scripts/run.sh -m weights/yolov5s.onnx --demo rgb
scripts/run.sh -m weights/yolov5s.onnx --demo yuv
scripts/benchmark.sh -m weights/yolov5s.onnx --demo rgb --loop 20
```

`scripts/run.sh` resolves paths from the package root and runs the `rgb` and `yuv` workloads serially unless one is selected explicitly. This repository does not include an ONNX model, so pass the model path with `-m/--model`. If that option is omitted, the script checks the optional local fallback `weights/yolov5s.onnx`; when the file is absent, it prints the required action and exits without starting inference. Reference inputs are read from `build/assets`, and annotated images are written to `build/output`. If `/usr/local/rpp/bin/rpp_server` is installed, the script ensures that the service is available before inference and writes its log to `build/logs/rpp_server.log`.

`scripts/benchmark.sh` saves the full console log to `build/logs/benchmark_<workload>_<timestamp>.log`.

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

This repository does not include or track ONNX model files in Git. Building is independent of model availability and never downloads or verifies an ONNX file. At runtime, pass a model with `-m/--model`. When that option is omitted, the scripts use `weights/yolov5s.onnx` only if the user has placed it there; otherwise they print a clear model-path error and exit. Class names for overlays are the embedded MS COCO 80-class list in `src/yolov5/labels.cpp` (`coco80_class_labels()`), matching standard YOLOv5 COCO pretrained ordering.

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
- OpenRT installed under `/usr/local/rpp`
- Runtime libraries required by the target RPP accelerator card environment

If your OpenRT installation path is different, update the corresponding path settings in `CMakeLists.txt`.

## How to Build

Run the following commands from the repository root:

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

Run the application from the package root after building. The release package does not contain an ONNX model, so specify its path with `-m/--model`. For convenience, an omitted model option falls back to `weights/yolov5s.onnx` only when the user has placed that file there; otherwise the script reports the missing model and exits.

| Option | Description | Default |
|---|---|---|
| `-m, --model PATH` | Supported static, simplified YOLOv5 ONNX model | Required; optional fallback: `weights/yolov5s.onnx` |
| `-d, --demo NAME` | Workload: `all`, `rgb`, or `yuv` | `all` |
| `-i, --input PATH` | Input image or I420 YUV file | Reference input for the selected workload |
| `-o, --output PATH` | Rendered result image | `build/output/output_<workload>.jpg` |
| `-b, --build-dir DIR` | Build directory used by `run.sh` | `build` |
| `-l, --loop COUNT` | Number of measured frames after warmup | `1` |
| `--yuv-width N` | I420 source width for the YUV workload | `1280` |
| `--yuv-height N` | I420 source height for the YUV workload | `720` |
| `-h, --help` | Display command help | — |

```shell
scripts/run.sh -m weights/yolov5s.onnx
scripts/run.sh -m weights/yolov5s.onnx --demo rgb \
  -i /path/to/input.jpg \
  -o build/output/result.jpg
```

By default, `scripts/run.sh` runs the RGB and YUV workloads serially. Use `--demo rgb` or `--demo yuv` to select one workload or provide a custom input/output path. The option name `--demo` is retained for command-line compatibility.

Benchmark runs use the same model-path and input handling:

```shell
scripts/benchmark.sh -m weights/yolov5s.onnx --demo rgb --loop 20
scripts/benchmark.sh -m weights/yolov5s.onnx --demo yuv --loop 20
```

`scripts/benchmark.sh` accepts the same model, workload, input, output, build-directory, and YUV-size options as `scripts/run.sh`. Its measured-loop default is `20`; `run.sh` defaults to `1` for a quick functional run.

Each script supports `-h` and `--help`.

On the first run for a model, the runtime builds an engine and stores it under `<model-directory>/.engine_cache/`. Later runs with identical ONNX content, OpenRT version, and engine build settings reuse that engine. Runtime logs always state whether the engine was built or reused. Replacing the ONNX content produces a different cache key and triggers a new build.

## Advanced: Direct Executable Usage

The scripts provide the recommended customer workflow because they resolve paths and manage `rpp_server` startup. For system integration, the application executables can also be launched directly with explicit paths:

The scripts and C++ executables use the same path options: `-m/--model` for the ONNX model, `-i/--input` for the source image or frame, and `-o/--output` for the annotated result image.

```shell
build/bin/yolov5_rgb_image_demo \
  -m weights/yolov5s.onnx \
  -i build/assets/test_1.png \
  -o build/output/output_rgb.jpg \
  -l 1

build/bin/yolov5_yuv_frame_demo \
  -m weights/yolov5s.onnx \
  -i build/assets/test_0.yuv \
  --yuv-width 1280 \
  --yuv-height 720 \
  -o build/output/output_yuv.jpg \
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
| and OpenRT engine      |
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
| print pipeline and     |
| inference-only FPS     |
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
- Host/device buffer usage through shared helpers in the RPP execution path
- Detection decoding, NMS, and image rendering logic

## Application Integration

- Use `scripts/run.sh -m/--model` to select a supported YOLOv5 ONNX model and `-o/--output` for the rendered result image.
- This release uses the embedded COCO 80 class ordering from `src/yolov5/labels.cpp`.
- Replace the reference input path with the target image or frame source.
- Integrate `rpp/inference/RppInferEngine` and `src/yolov5/YoloV5Pipeline` into the target RPP-based inference workflow.

## Known Limitations

- This release supports the static, simplified, COCO80 P5/P6 contracts listed in the appendix. Dynamic shapes, `u` variants, custom-class outputs, and nonstandard coordinate layouts require separate adaptation and validation.
- INT8 model parsing and engine execution remain experimental; INT8 accuracy is not a V2.0 release criterion.
- The application uses independent preprocessing, OpenRT inference, and postprocessing stages. Graph mode is not used.

## Troubleshooting

- If `-m/--model` is omitted and the optional fallback model is not present, rerun with an explicit supported ONNX path.
- If an executable is missing, rebuild with `scripts/build.sh`.
- Some larger models can emit OpenRT SDK `KernelWeights` or `BiasWeights` messages while a cached engine is being deserialized. If the log then reports `Reusing cached OpenRT engine` and ends with `[SUCCESS]`, the engine was loaded and inference completed correctly. A first-time ONNX build was separately verified without these messages.
- If engine loading or inference does not reach `[SUCCESS]`, remove only that model's matching file under `<model-directory>/.engine_cache/` and rerun to rebuild it. Do not remove the ONNX model.

## Appendix: Supported Models And ONNX Export

### Validated model family

This release supports the classic COCO80 YOLOv5 P5 and P6 detection families listed below at two static input shapes. All 20 detection artifacts were validated end to end with FP32 input/output bindings, one warmup pass, one measured RGB inference pass, a non-empty detection result, and a rendered output image. The 1280×1024 set was additionally verified through first-run OpenRT engine construction.

| Model family | 640×640 artifact | 1280×1024 artifact | Status |
| :--- | :--- | :--- | :--- |
| YOLOv5n | `yolov5n_640x640_sim.onnx` | `yolov5n_1280x1024_sim.onnx` | Verified |
| YOLOv5s | `yolov5s_640x640_sim.onnx` | `yolov5s_1280x1024_sim.onnx` | Verified |
| YOLOv5m | `yolov5m_640x640_sim.onnx` | `yolov5m_1280x1024_sim.onnx` | Verified |
| YOLOv5l | `yolov5l_640x640_sim.onnx` | `yolov5l_1280x1024_sim.onnx` | Verified |
| YOLOv5x | `yolov5x_640x640_sim.onnx` | `yolov5x_1280x1024_sim.onnx` | Verified |
| YOLOv5n6 | `yolov5n6_640x640_sim.onnx` | `yolov5n6_1280x1024_sim.onnx` | Verified |
| YOLOv5s6 | `yolov5s6_640x640_sim.onnx` | `yolov5s6_1280x1024_sim.onnx` | Verified |
| YOLOv5m6 | `yolov5m6_640x640_sim.onnx` | `yolov5m6_1280x1024_sim.onnx` | Verified |
| YOLOv5l6 | `yolov5l6_640x640_sim.onnx` | `yolov5l6_1280x1024_sim.onnx` | Verified |
| YOLOv5x6 | `yolov5x6_640x640_sim.onnx` | `yolov5x6_1280x1024_sim.onnx` | Verified |

| Input contract | P5 decoded output | P6 decoded output | Validated opset |
| :--- | :--- | :--- | :--- |
| FP32 NCHW `[1,3,640,640]` | FP32 `[1,25200,85]` | FP32 `[1,25500,85]` | 12 |
| FP32 NCHW `[1,3,1280,1024]` | FP32 `[1,80640,85]` | FP32 `[1,81600,85]` | 19 |

The validated files are stored locally under `weights/yolov5/onnx_sim/`. They use pixel-space `xywh` and the standard COCO 80-class ordering. In the 1280×1024 filenames and export command, the dimensions follow the model tensor's height×width order. The 50-frame performance figures in the release notes were measured with YOLOv5s at 640×640 only and are not performance guarantees for other models or input shapes.

YOLOv5u variants (`yolov5nu` through `yolov5xu`), YOLOv5 P6-u variants (`yolov5n6u` through `yolov5x6u`), YOLOv8, custom-class outputs, normalized-coordinate outputs, and other model families are not supported by this release.

Q/DQ INT8 ONNX parsing and engine execution are retained as an experimental capability when OpenRT accepts the graph and its external bindings remain FP32. INT8 numerical accuracy and end-to-end detection results are not release acceptance criteria for this version.

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

For the validated high-resolution contract, use `--imgsz 1280 1024 --opset 19`; the dimensions are height followed by width. Replace `yolov5s.pt` with `yolov5n.pt`, `yolov5m.pt`, `yolov5l.pt`, `yolov5x.pt`, or the corresponding P6 weight (`yolov5n6.pt` through `yolov5x6.pt`) as required. Update both filenames in the `onnxsim` command, then pass the generated `*_sim.onnx` file with `-m/--model`. As an optional convenience, a YOLOv5s artifact placed at `weights/yolov5s.onnx` is used when the model option is omitted.

---

<a id="chinese"></a>

# RPP 加速 YOLOv5 应用

**语言：[English](#english) | 中文**

![XDL Logo](docs/logo/logo_color_horizontal.png)

本产品用于在 RPP 加速硬件上运行经过验证的经典 YOLOv5 目标检测模型，提供可直接使用的 RGB 图片和 I420 YUV 帧编译、推理及性能测试命令，并附带可复用的 C++ 集成示例。

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
- [已知限制](#已知限制)
- [故障排查](#故障排查)
- [附录：支持的模型与 ONNX 导出](#附录支持的模型与-onnx-导出)

## 快速开始

在仓库根目录执行以下命令。如果您的 ONNX 文件名不同，请替换示例模型路径。

```shell
scripts/build.sh
scripts/run.sh -m weights/yolov5s.onnx
```

执行成功后，应用会报告 RGB 和 YUV 工作负载在 warmup 后的延迟与 FPS，并在 `build/output` 下保存带检测结果的图片。

标准脚本如下：

| 脚本 | 用途 |
|---|---|
| `scripts/build.sh` | 清理上一次构建产物，执行 CMake 配置并编译工程；不检查、下载或校验模型。 |
| `scripts/run.sh` | 运行全部工作负载，或选择单一工作负载并配置模型、输入、输出和测量次数。 |
| `scripts/benchmark.sh` | 对一个或全部工作负载执行重复测量，并保存完整性能日志。 |

运行单一输入格式或启动性能测试：

```shell
scripts/run.sh -m weights/yolov5s.onnx --demo rgb
scripts/run.sh -m weights/yolov5s.onnx --demo yuv
scripts/benchmark.sh -m weights/yolov5s.onnx --demo rgb --loop 20
```

`scripts/run.sh` 会从应用根目录解析路径；未显式选择工作负载时，会串行运行 `rgb` 和 `yuv`。本仓库不包含 ONNX 模型，因此运行时应通过 `-m/--model` 指定模型路径。如果省略该参数，脚本会检查可选的本地回退路径 `weights/yolov5s.onnx`；文件不存在时，会输出操作提示并在启动推理前退出。参考输入从 `build/assets` 读取，标注结果写入 `build/output`。如果系统安装了 `/usr/local/rpp/bin/rpp_server`，脚本会在推理前确保服务可用，并将日志写入 `build/logs/rpp_server.log`。

`scripts/benchmark.sh` 会将完整控制台日志保存到 `build/logs/benchmark_<workload>_<timestamp>.log`。

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

本仓库不包含 ONNX 模型文件，也不在 Git 中跟踪此类文件。编译过程与模型是否存在无关，也不会下载或校验 ONNX。运行时应通过 `-m/--model` 传入模型；如果省略该参数，脚本仅在用户已将模型放置为 `weights/yolov5s.onnx` 时使用该文件，否则会输出明确的模型路径错误并退出。可视化使用的类别名称来自 `src/yolov5/labels.cpp` 中内置的 MS COCO 80 类列表 `coco80_class_labels()`，顺序与标准 YOLOv5 COCO 预训练模型一致。

## 工作负载指南

每种受支持的工作负载都提供独立指南，说明输入格式、命令行参数、处理流程和预期输出：

请根据所需输入格式选择指南：

- [RGB 图片工作负载](demos/rgb_image/README.md)：RGB/BGR 图片推理。
- [I420 YUV 帧工作负载](demos/yuv_frame/README.md)：I420 YUV 帧推理。

## 项目文档

点击文档路径可跳转到对应说明：

- [项目文档入口](docs/README.md)：公开文档入口。
- [API 文档](docs/api/README.md)：可复用 API 索引。
- [RPP 加速模块](rpp/README.md)：RPP 模块说明。
- [性能 trace 辅助模块](src/perf/README.md)：perf trace 说明。

## 环境依赖

构建应用前，请确认环境满足以下要求：

- Linux 开发环境
- CMake `>= 3.16.3`
- 支持 C++17 的编译器
- OpenCV 以及本项目需要的组件
- 安装在 `/usr/local/rpp` 下的 OpenRT
- 目标 RPP 加速卡运行所需的运行时库

如果 OpenRT 安装路径不同，请同步修改 `CMakeLists.txt` 中的相关路径配置。

## 构建方式

在仓库根目录执行：

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

构建完成后，在仓库根目录运行。发布包不包含 ONNX 模型，因此应通过 `-m/--model` 指定模型路径。为便于本地使用，省略该参数时仅在用户已放置 `weights/yolov5s.onnx` 的情况下使用该回退文件；否则脚本会报告模型缺失并退出。

| 参数 | 说明 | 默认值 |
|---|---|---|
| `-m, --model PATH` | 受支持的静态维度、已简化 YOLOv5 ONNX 模型 | 必需；可选回退路径为 `weights/yolov5s.onnx` |
| `-d, --demo NAME` | 运行范围：`all`、`rgb` 或 `yuv` | `all` |
| `-i, --input PATH` | 输入图片或 I420 YUV 文件 | 对应工作负载的参考输入 |
| `-o, --output PATH` | 绘制检测结果的输出图片 | `build/output/output_<workload>.jpg` |
| `-b, --build-dir DIR` | `run.sh` 使用的构建目录 | `build` |
| `-l, --loop COUNT` | warmup 后的测量帧数 | `1` |
| `--yuv-width N` | YUV 工作负载的 I420 源帧宽度 | `1280` |
| `--yuv-height N` | YUV 工作负载的 I420 源帧高度 | `720` |
| `-h, --help` | 显示命令帮助 | — |

```shell
scripts/run.sh -m weights/yolov5s.onnx
scripts/run.sh -m weights/yolov5s.onnx --demo rgb \
  -i /path/to/input.jpg \
  -o build/output/result.jpg
```

默认情况下，`scripts/run.sh` 串行运行 RGB 和 YUV 工作负载。使用 `--demo rgb` 或 `--demo yuv` 可选择单一工作负载或传入自定义输入/输出路径。参数名 `--demo` 为保持命令行兼容性而保留。

Benchmark 使用相同的模型路径和输入处理方式：

```shell
scripts/benchmark.sh -m weights/yolov5s.onnx --demo rgb --loop 20
scripts/benchmark.sh -m weights/yolov5s.onnx --demo yuv --loop 20
```

`scripts/benchmark.sh` 与 `scripts/run.sh` 支持相同的模型、工作负载、输入、输出、构建目录和 YUV 尺寸参数。性能测试脚本的测量 loop 默认值为 `20`，快速功能运行脚本的默认值为 `1`。

每个脚本都支持 `-h` 和 `--help`。

## 高级用法：直接运行可执行程序

脚本是推荐的客户使用方式，可统一解析路径并管理 `rpp_server` 启动。系统集成场景也可以直接运行应用可执行程序，并显式传入路径：

脚本和 C++ 可执行程序统一使用 `-m/--model` 指定 ONNX 模型、`-i/--input` 指定输入图片或帧、`-o/--output` 指定标注结果图片。

```shell
build/bin/yolov5_rgb_image_demo \
  -m weights/yolov5s.onnx \
  -i build/assets/test_1.png \
  -o build/output/output_rgb.jpg \
  -l 1

build/bin/yolov5_yuv_frame_demo \
  -m weights/yolov5s.onnx \
  -i build/assets/test_0.yuv \
  --yuv-width 1280 \
  --yuv-height 720 \
  -o build/output/output_yuv.jpg \
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
| 和 OpenRT engine       |
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
| 打印端到端与单推理 |
| FPS                    |
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

- 使用 `scripts/run.sh -m/--model` 指定受支持的 YOLOv5 ONNX 模型，使用 `-o/--output` 指定绘制结果的输出图片路径。
- 本次发布使用 `src/yolov5/labels.cpp` 中内置的 COCO 80 类顺序。
- 将示例图片路径替换为自己的输入源。
- 将 `rpp/inference/RppInferEngine` 和 `src/yolov5/YoloV5Pipeline` 集成到目标 RPP 推理流程。

## 已知限制

- 本版本支持附录中列出的静态维度、已简化 COCO80 P5/P6 模型契约。动态维度、`u` 系列、自定义类别输出和非标准坐标布局需要单独适配并重新验证。
- INT8 模型解析和 engine 执行仍为实验性能力；INT8 精度不属于 V2.0 发布验收项。
- 应用采用独立的前处理、OpenRT 推理和后处理阶段，不使用 Graph 模式。

## 故障排查

- 如果未传入 `-m/--model` 且可选回退模型不存在，请使用明确的 ONNX 模型路径重新运行。
- 可执行程序不存在时，请使用 `scripts/build.sh` 重新构建。
- 部分较大模型在反序列化缓存 engine 时，可能打印 OpenRT SDK 的 `KernelWeights` 或 `BiasWeights` 信息。如果后续日志显示 `Reusing cached OpenRT engine` 并最终输出 `[SUCCESS]`，则 engine 已成功加载且推理正常完成。无缓存的首次 ONNX 构建已单独验证，不会出现这些信息。
- 如果 engine 加载或推理未到达 `[SUCCESS]`，仅删除 `<模型目录>/.engine_cache/` 下与该模型对应的缓存文件后重新运行，以触发重建；请勿删除 ONNX 模型。

## 附录：支持的模型与 ONNX 导出

### 已验证模型系列

本版本支持下表所列的经典 COCO80 YOLOv5 P5 和 P6 检测模型，并覆盖两种静态输入 shape。全部 20 个检测模型均使用 FP32 输入/输出 binding 完成端到端验证，包括 1 次 warmup、1 次 RGB 正式推理、非空检测结果和检测图输出；1280×1024 模型还完成了首次 OpenRT engine 构建验证。

| 模型系列 | 640×640 文件 | 1280×1024 文件 | 状态 |
| :--- | :--- | :--- | :--- |
| YOLOv5n | `yolov5n_640x640_sim.onnx` | `yolov5n_1280x1024_sim.onnx` | 已验证 |
| YOLOv5s | `yolov5s_640x640_sim.onnx` | `yolov5s_1280x1024_sim.onnx` | 已验证 |
| YOLOv5m | `yolov5m_640x640_sim.onnx` | `yolov5m_1280x1024_sim.onnx` | 已验证 |
| YOLOv5l | `yolov5l_640x640_sim.onnx` | `yolov5l_1280x1024_sim.onnx` | 已验证 |
| YOLOv5x | `yolov5x_640x640_sim.onnx` | `yolov5x_1280x1024_sim.onnx` | 已验证 |
| YOLOv5n6 | `yolov5n6_640x640_sim.onnx` | `yolov5n6_1280x1024_sim.onnx` | 已验证 |
| YOLOv5s6 | `yolov5s6_640x640_sim.onnx` | `yolov5s6_1280x1024_sim.onnx` | 已验证 |
| YOLOv5m6 | `yolov5m6_640x640_sim.onnx` | `yolov5m6_1280x1024_sim.onnx` | 已验证 |
| YOLOv5l6 | `yolov5l6_640x640_sim.onnx` | `yolov5l6_1280x1024_sim.onnx` | 已验证 |
| YOLOv5x6 | `yolov5x6_640x640_sim.onnx` | `yolov5x6_1280x1024_sim.onnx` | 已验证 |

| 输入契约 | P5 解码输出 | P6 解码输出 | 已验证 opset |
| :--- | :--- | :--- | :--- |
| FP32 NCHW `[1,3,640,640]` | FP32 `[1,25200,85]` | FP32 `[1,25500,85]` | 12 |
| FP32 NCHW `[1,3,1280,1024]` | FP32 `[1,80640,85]` | FP32 `[1,81600,85]` | 19 |

已验证文件位于本地 `weights/yolov5/onnx_sim/`，均使用像素坐标 `xywh` 和标准 COCO 80 类顺序。1280×1024 文件名和导出命令中的尺寸顺序与模型 tensor 的高×宽一致。ReleaseNote 中的 50 帧性能数据仅使用 640×640 YOLOv5s 测得，不代表其他型号或输入 shape 的性能保证。

本版本不支持 YOLOv5u（`yolov5nu` 至 `yolov5xu`）、YOLOv5 P6-u（`yolov5n6u` 至 `yolov5x6u`）、YOLOv8、自定义类别输出、归一化坐标输出及其他模型系列。

对于外部 binding 仍为 FP32、且能够被 OpenRT 接受的 Q/DQ INT8 ONNX，本版本保留模型解析和 engine 执行能力，作为实验性功能。INT8 数值精度和端到端检测结果不作为本版本发布验收项。

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

如需使用已验证的高分辨率契约，请设置 `--imgsz 1280 1024 --opset 19`；尺寸顺序为高、宽。可按需将 `yolov5s.pt` 替换为 `yolov5n.pt`、`yolov5m.pt`、`yolov5l.pt`、`yolov5x.pt`，或对应的 P6 权重（`yolov5n6.pt` 至 `yolov5x6.pt`）。同时更新 `onnxsim` 命令中的输入和输出文件名，并通过 `-m/--model` 传入生成的 `*_sim.onnx`。为便于本地使用，也可将 YOLOv5s 模型放置为 `weights/yolov5s.onnx`，作为未传入模型参数时的可选回退文件。
