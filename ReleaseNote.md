<a id="english"></a>

# Release Notes

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

## YOLOv5 RPP Inference Demo V2.0

This release note records only the final differences between the current code and the baseline tag below.

| Item | Value |
|---|---|
| Release version | V2.0 |
| Baseline tag | `V1.0` |
| Target code | Current repository code |

### Added

- Added modular RPP execution components:
  - `rpp/inference/RppInferEngine` for ONNX parsing, OpenRT on RPP engine creation, binding buffer ownership, and synchronous inference.
  - `rpp/preprocess/RppYoloPreprocessor` and RPP kernels for RGB/BGR image and I420 YUV frame preprocessing.
  - `rpp/postprocess/RppYoloPostprocessor` for YOLOv5 output slicing, RPP NMS, compact result copy, and host detection parsing.
- Added reusable YOLOv5 application components under `src/yolov5/`, including detection types, COCO labels, visualization, and `YoloV5Pipeline`.
- Added shared utility components under `src/utils/`, including logging, parser helpers, RPP buffer management, runtime error recording, and path/CLI helpers.
- Added optional `src/perf/PerfTraceSession` for `rpp_perf` trace capture when built with `YOLO_ENABLE_RPP_PERF=ON`.
- Added two input-specific demo executables:
  - `demos/rgb_image` builds `yolov5_rgb_image_demo`.
  - `demos/yuv_frame` builds `yolov5_yuv_frame_demo`.
- Added `assets/test_0.yuv` for the I420 YUV demo and moved the sample RGB image from `image/test_1.png` to `assets/test_1.png`.
- Added `weights/.gitkeep` so the local model directory exists while ONNX model files remain ignored by Git.
- Added public documentation entries under `docs/`, module README files, demo README files, API navigation, and bilingual root README content.
- Added standard scripts:
  - `scripts/build.sh` for model preparation, clean build output handling, CMake configuration, and build.
  - `scripts/run.sh` for RGB/YUV demo execution, default run-all behavior, repository-root path resolution, and `rpp_server` protection.
  - `scripts/benchmark.sh` for repeated-loop benchmark runs and automatic log capture.

### Changed

- Reorganized the project from the baseline `common/` and `yolov5/` layout into the current `rpp/`, `src/`, `demos/`, `scripts/`, `docs/`, `assets/`, and `weights/` layout.
- Updated CMake to use CMake 3.16.3, C++17, reusable shared-library targets, RPP CUDA compilation, and grouped build outputs:
  - `build/bin`
  - `build/lib`
  - `build/assets`
  - `build/output`
  - `build/trace`
- Updated the inference path to run through OpenRT on RPP with model input/output binding buffers managed by `RppInferEngine`.
- Updated preprocessing to run on RPP for both RGB/BGR images and I420 YUV frames, including letterbox resize, format conversion, normalization, and model tensor writing.
- Updated postprocessing to run YOLOv5 output processing and NMS on RPP instead of the baseline CPU-side demo path.
- Updated demo execution to use explicit initialization, one warmup pass, measured loops, rendered output, and stage-level timing.
- Updated timing output to report H2D and D2H transfer time separately; FPS is calculated from the printed preprocess + inference + postprocess latency.
- Decoupled compilation from model preparation. `scripts/build.sh` no longer checks, downloads, or verifies ONNX files; run and benchmark scripts use the C++-consistent `-o/--onnx` option and fall back to `weights/yolov5s.onnx` only when it exists.
- Added content-keyed OpenRT engine caching with explicit build/reuse runtime logs.
- Updated `scripts/run.sh` so the default mode runs both RGB and YUV demos serially. Single-demo execution remains available through `--demo rgb` or `--demo yuv`.
- Updated `scripts/benchmark.sh` so the default benchmark covers all demos and saves logs under `build/logs/`.
- Updated documentation style so README files are bilingual and module/document links use explicit clickable path descriptions.
- Moved logo assets from `doc/logo/` to `docs/logo/`.

### Removed

- Removed the baseline `yolov5/main.cpp`, `yolov5/yolo.cpp`, and `yolov5/yolo.h` monolithic demo entry and convenience interface.
- Removed the baseline `common/` helper layout after moving reusable helpers into `src/utils/`.
- Removed baseline CPU-side YOLOv5 decode/NMS from the normal demo execution path.
- Removed the old `image/` sample input directory after moving sample inputs into `assets/`.

---

<a id="chinese"></a>

# Release Notes

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

## YOLOv5 RPP 推理 Demo V2.0

本文只记录当前代码相对于下方基准 tag 的最终差异。

| 项目 | 内容 |
|---|---|
| 发布版本 | V2.0 |
| 基准 tag | `V1.0` |
| 目标代码 | 当前仓库代码 |

### 新增

- 新增模块化 RPP 执行组件：
  - `rpp/inference/RppInferEngine`：负责 ONNX 解析、OpenRT on RPP engine 创建、binding buffer 管理和同步推理。
  - `rpp/preprocess/RppYoloPreprocessor` 及 RPP kernel：负责 RGB/BGR 图片和 I420 YUV 帧前处理。
  - `rpp/postprocess/RppYoloPostprocessor`：负责 YOLOv5 输出切分、RPP NMS、compact 结果回传和 host 侧检测结果解析。
- 新增 `src/yolov5/` 可复用 YOLOv5 应用组件，包括检测结果类型、COCO 标签、可视化和 `YoloV5Pipeline`。
- 新增 `src/utils/` 公共工具组件，包括日志、parser 辅助、RPP buffer 管理、运行时错误记录、路径和命令行辅助。
- 新增可选 `src/perf/PerfTraceSession`，用于在 `YOLO_ENABLE_RPP_PERF=ON` 构建下采集 `rpp_perf` trace。
- 新增两个按输入类型拆分的 demo 可执行入口：
  - `demos/rgb_image` 构建 `yolov5_rgb_image_demo`。
  - `demos/yuv_frame` 构建 `yolov5_yuv_frame_demo`。
- 新增 `assets/test_0.yuv` 作为 I420 YUV demo 示例输入，并将 RGB 示例图片从 `image/test_1.png` 移动到 `assets/test_1.png`。
- 新增 `weights/.gitkeep`，保留本地模型目录，同时继续忽略 ONNX 模型文件。
- 新增 `docs/` 公开文档入口、模块 README、demo README、API 导航和双语根 README。
- 新增标准脚本：
  - `scripts/build.sh`：负责模型准备、清理构建产物、CMake 配置和编译。
  - `scripts/run.sh`：负责 RGB/YUV demo 运行、默认 run-all、仓库根目录路径解析和 `rpp_server` 保护。
  - `scripts/benchmark.sh`：负责重复 loop benchmark 和自动日志保存。

### 变更

- 将项目从基准版本的 `common/` 和 `yolov5/` 布局重构为当前 `rpp/`、`src/`、`demos/`、`scripts/`、`docs/`、`assets/` 和 `weights/` 布局。
- 更新 CMake：使用 CMake 3.16.3、C++17、可复用 shared library targets、RPP CUDA 编译，并按类型组织构建输出：
  - `build/bin`
  - `build/lib`
  - `build/assets`
  - `build/output`
  - `build/trace`
- 推理路径更新为通过 OpenRT on RPP 执行，并由 `RppInferEngine` 管理模型 input/output binding buffer。
- 前处理更新为在 RPP 上执行 RGB/BGR 图片和 I420 YUV 帧处理，包括 letterbox resize、格式转换、归一化和模型输入 tensor 写入。
- 后处理更新为在 RPP 上执行 YOLOv5 输出处理和 NMS，不再使用基准版本的 CPU 侧 demo 后处理路径。
- demo 执行流程更新为显式初始化、一次 warmup、多轮测量、结果绘制和阶段级耗时输出。
- 计时输出更新为单独打印 H2D 和 D2H 传输耗时；FPS 使用打印的前处理 + 推理 + 后处理耗时计算。
- 编译与模型准备解耦。`scripts/build.sh` 不再检查、下载或校验 ONNX；run 和 benchmark 脚本与 C++ 统一使用 `-o/--onnx`，且仅在 `weights/yolov5s.onnx` 存在时使用默认模型。
- 新增基于模型内容的 OpenRT engine 缓存，并在运行日志中明确打印新建或复用状态。
- `scripts/run.sh` 默认串行运行 RGB 和 YUV 两个 demo；仍可通过 `--demo rgb` 或 `--demo yuv` 运行单个 demo。
- `scripts/benchmark.sh` 默认覆盖全部 demo，并将日志保存到 `build/logs/`。
- 文档更新为双语 README，并统一模块和文档跳转链接的可点击路径描述。
- logo 资源从 `doc/logo/` 移动到 `docs/logo/`。

### 移除

- 移除基准版本中的 `yolov5/main.cpp`、`yolov5/yolo.cpp` 和 `yolov5/yolo.h` 单体 demo 入口和便捷接口。
- 移除基准版本中的 `common/` helper 布局，相关可复用 helper 已迁移到 `src/utils/`。
- 从常规 demo 执行路径中移除基准版本 CPU 侧 YOLOv5 decode/NMS。
- 移除旧的 `image/` 示例输入目录，示例输入统一移动到 `assets/`。
