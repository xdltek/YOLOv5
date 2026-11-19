<a id="english"></a>

# Release Notes

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

## RPP-Accelerated YOLOv5 Application V2.0

This release note records only the final differences between the current code and the baseline tag below.

| Item | Value |
|---|---|
| Release version | V2.0 |
| Baseline tag | `V1.0` |
| Target code | Current repository code |
| Supported models | COCO80 YOLOv5 P5 and classic P6 n/s/m/l/x detection models at static 640×640 and 1280×1024 input shapes |

### Validated Models

Twenty static FP32 COCO80 detection profiles were validated on 2026-11-19: P5 YOLOv5n/s/m/l/x and classic P6 YOLOv5n6/s6/m6/l6/x6 at 640×640 and 1280×1024. Every profile passed ONNX checker, RGB application validation with 20 measured frames, and I420 YUV functional validation with non-empty detections and rendered results.

Complete P5 and P6 performance tables for both input shapes are published in `README.md`. The table below summarizes the 1280×1024 RGB measurements.

| Model profile | End-to-end latency | FPS | Status |
|---|---:|---:|---|
| YOLOv5n P5 | 26.244 ms | 38.104 | PASS |
| YOLOv5s P5 | 31.695 ms | 31.551 | PASS |
| YOLOv5m P5 | 52.390 ms | 19.088 | PASS |
| YOLOv5l P5 | 73.469 ms | 13.611 | PASS |
| YOLOv5x P5 | 128.139 ms | 7.804 | PASS |
| YOLOv5n6 P6 | 27.240 ms | 36.711 | PASS |
| YOLOv5s6 P6 | 31.877 ms | 31.371 | PASS |
| YOLOv5m6 P6 | 52.962 ms | 18.882 | PASS |
| YOLOv5l6 P6 | 75.428 ms | 13.258 | PASS |
| YOLOv5x6 P6 | 130.264 ms | 7.677 | PASS |

Each reference result uses one complete-pipeline warmup followed by 20 measured RGB frames. Engine initialization and visualization are excluded. Performance may vary with the accelerator, SDK, driver, firmware, and operating conditions.

### Added

- Added modular RPP execution components:
  - `rpp/inference/RppInferEngine` for ONNX parsing, OpenRT on RPP engine creation, binding buffer ownership, and synchronous inference.
  - `rpp/preprocess/RppYoloPreprocessor` and RPP kernels for RGB/BGR image and I420 YUV frame preprocessing.
  - `rpp/postprocess/RppYoloPostprocessor` for YOLOv5 output slicing, RPP NMS, NMS/box result transfer, and host detection parsing.
- Added reusable YOLOv5 application components under `src/yolov5/`, including detection types, COCO labels, visualization, and `YoloV5Pipeline`.
- Added shared utility components under `src/utils/`, including logging, parser helpers, RPP buffer management, runtime error recording, and path/CLI helpers.
- Added optional `src/perf/PerfTraceSession` for `rpp_perf` trace capture when built with `YOLO_ENABLE_RPP_PERF=ON`.
- Added two input-specific workload executables:
  - `demos/rgb_image` builds `yolov5_rgb_image_demo`.
  - `demos/yuv_frame` builds `yolov5_yuv_frame_demo`.
- Added `assets/test_0.yuv` for the I420 YUV workload and moved the sample RGB image from `image/test_1.png` to `assets/test_1.png`.
- Added `weights/.gitkeep` so the local model directory exists while ONNX model files remain ignored by Git.
- Added public documentation entries under `docs/`, module and workload README files, API navigation, and bilingual root README content.
- Added standard scripts:
  - `scripts/build.sh` for clean build output handling, CMake configuration, and compilation; model availability is not a build prerequisite.
  - `scripts/run.sh` for RGB/YUV workload execution, default run-all behavior, repository-root path resolution, and `rpp_server` protection.
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
- Optimized the independent preprocessing stage with persistent stream/SRAM reuse and fewer runtime submissions; the application remains a conventional preprocess, OpenRT inference, and postprocess pipeline without graph mode.
- Updated postprocessing to run YOLOv5 output processing and NMS on RPP instead of the baseline CPU-side application path.
- Fused FP32-to-BF16 conversion, box/score slicing, and NMS into one local-SDK OpenRT/RppRT engine to reduce per-frame postprocess submissions without adding a shared-library dependency.
- Updated application execution to use explicit initialization, one warmup pass, measured loops, rendered output, and stage-level timing.
- Updated timing output to highlight end-to-end FPS and OpenRT inference FPS, while reporting H2D, D2H, and stage latency separately.
- Decoupled compilation from model preparation. `scripts/build.sh` no longer checks, downloads, or verifies ONNX files; scripts and C++ executables consistently use `-m/--model`, `-i/--input`, and `-o/--output`, and fall back to `weights/yolov5s.onnx` only when it exists.
- Added content-keyed OpenRT engine caching with explicit build/reuse runtime logs.
- Added cache-load error detection. A cache that reports OpenRT deserialization errors is rejected and rebuilt from ONNX instead of being used for inference.
- Updated `scripts/run.sh` so the default mode runs both RGB and YUV workloads serially. Single-workload execution remains available through `--demo rgb` or `--demo yuv`.
- Updated `scripts/benchmark.sh` so the default benchmark covers both workloads and saves logs under `build/logs/`.
- Validated 20 COCO80 model profiles: P5 YOLOv5n/s/m/l/x and classic P6 YOLOv5n6/s6/m6/l6/x6 at static 640×640 and 1280×1024 input shapes. Other model families and output contracts are outside the V2.0 release scope.
- Updated documentation style so README files are bilingual and module/document links use explicit clickable path descriptions.
- Moved logo assets from `doc/logo/` to `docs/logo/`.

### Removed

- Removed the baseline `yolov5/main.cpp`, `yolov5/yolo.cpp`, and `yolov5/yolo.h` monolithic application entry and convenience interface.
- Removed the baseline `common/` helper layout after moving reusable helpers into `src/utils/`.
- Removed baseline CPU-side YOLOv5 decode/NMS from the normal application execution path.
- Removed the old `image/` sample input directory after moving sample inputs into `assets/`.

---

<a id="chinese"></a>

# 发布说明

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

## RPP 加速 YOLOv5 应用 V2.0

本文只记录当前代码相对于下方基准 tag 的最终差异。

| 项目 | 内容 |
|---|---|
| 发布版本 | V2.0 |
| 基准 tag | `V1.0` |
| 目标代码 | 当前仓库代码 |
| 支持模型 | COCO80 YOLOv5 P5 和经典 P6 n/s/m/l/x 检测模型，支持静态 640×640 与 1280×1024 输入 shape |

### 已验证模型

2026-11-19，共 20 个静态 FP32 COCO80 检测规格通过验证：640×640 和 1280×1024 下的 P5 YOLOv5n/s/m/l/x，以及经典 P6 YOLOv5n6/s6/m6/l6/x6。全部规格均通过 ONNX checker、20 帧 RGB 应用正式测量和 I420 YUV 功能验证，均获得非空检测结果并成功输出检测图。

完整的两种输入尺寸 P5/P6 性能表见 `README.md`。下表汇总 1280×1024 RGB 测量结果。

| 模型规格 | 端到端耗时 | FPS | 状态 |
|---|---:|---:|---|
| YOLOv5n P5 | 26.244 ms | 38.104 | PASS |
| YOLOv5s P5 | 31.695 ms | 31.551 | PASS |
| YOLOv5m P5 | 52.390 ms | 19.088 | PASS |
| YOLOv5l P5 | 73.469 ms | 13.611 | PASS |
| YOLOv5x P5 | 128.139 ms | 7.804 | PASS |
| YOLOv5n6 P6 | 27.240 ms | 36.711 | PASS |
| YOLOv5s6 P6 | 31.877 ms | 31.371 | PASS |
| YOLOv5m6 P6 | 52.962 ms | 18.882 | PASS |
| YOLOv5l6 P6 | 75.428 ms | 13.258 | PASS |
| YOLOv5x6 P6 | 130.264 ms | 7.677 | PASS |

每项参考结果均为完整 pipeline warmup 1 次后正式测量 20 帧，不包含 engine 初始化和可视化。实际性能可能随加速卡、SDK、驱动、固件和运行条件变化。

### 新增

- 新增模块化 RPP 执行组件：
  - `rpp/inference/RppInferEngine`：负责 ONNX 解析、OpenRT on RPP engine 创建、binding buffer 管理和同步推理。
  - `rpp/preprocess/RppYoloPreprocessor` 及 RPP kernel：负责 RGB/BGR 图片和 I420 YUV 帧前处理。
  - `rpp/postprocess/RppYoloPostprocessor`：负责 YOLOv5 输出切分、RPP NMS、NMS/box 结果回传和 host 侧检测结果解析。
- 新增 `src/yolov5/` 可复用 YOLOv5 应用组件，包括检测结果类型、COCO 标签、可视化和 `YoloV5Pipeline`。
- 新增 `src/utils/` 公共工具组件，包括日志、parser 辅助、RPP buffer 管理、运行时错误记录、路径和命令行辅助。
- 新增可选 `src/perf/PerfTraceSession`，用于在 `YOLO_ENABLE_RPP_PERF=ON` 构建下采集 `rpp_perf` trace。
- 新增两个按输入类型拆分的工作负载可执行入口：
  - `demos/rgb_image` 构建 `yolov5_rgb_image_demo`。
  - `demos/yuv_frame` 构建 `yolov5_yuv_frame_demo`。
- 新增 `assets/test_0.yuv` 作为 I420 YUV 工作负载示例输入，并将 RGB 示例图片从 `image/test_1.png` 移动到 `assets/test_1.png`。
- 新增 `weights/.gitkeep`，保留本地模型目录，同时继续忽略 ONNX 模型文件。
- 新增 `docs/` 公开文档入口、模块与工作负载 README、API 导航和双语根 README。
- 新增标准脚本：
  - `scripts/build.sh`：负责清理构建产物、CMake 配置和编译；模型不是编译前置条件。
  - `scripts/run.sh`：负责 RGB/YUV 工作负载运行、默认 run-all、仓库根目录路径解析和 `rpp_server` 保护。
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
- 通过持久化复用 stream/SRAM 并减少 runtime 提交次数，优化独立前处理阶段；应用仍保持“前处理、OpenRT 推理、后处理”三阶段执行，不使用 Graph 模式。
- 后处理更新为在 RPP 上执行 YOLOv5 输出处理和 NMS，不再使用基准版本的 CPU 侧应用后处理路径。
- 将 FP32-to-BF16 转换、box/score 切分和 NMS 融合为一个基于本地 SDK 的 OpenRT/RppRT engine，减少每帧后处理提交次数，且不新增 shared library 依赖。
- 应用执行流程更新为显式初始化、一次 warmup、多轮测量、结果绘制和阶段级耗时输出。
- 计时输出重点展示端到端 FPS 和 OpenRT 单推理 FPS，同时单独打印 H2D、D2H 及各阶段延迟。
- 编译与模型准备解耦。`scripts/build.sh` 不再检查、下载或校验 ONNX；脚本与 C++ 可执行程序统一使用 `-m/--model`、`-i/--input` 和 `-o/--output`，且仅在 `weights/yolov5s.onnx` 存在时使用默认模型。
- 新增基于模型内容的 OpenRT engine 缓存，并在运行日志中明确打印新建或复用状态。
- 新增缓存加载错误检测。若缓存反序列化时报告 OpenRT 错误，程序会拒绝该缓存并从 ONNX 重建，不会继续用于推理。
- `scripts/run.sh` 默认串行运行 RGB 和 YUV 两个工作负载；仍可通过 `--demo rgb` 或 `--demo yuv` 运行单个工作负载。
- `scripts/benchmark.sh` 默认覆盖两个工作负载，并将日志保存到 `build/logs/`。
- 本次发布已验证 20 个 COCO80 模型规格：静态 640×640 和 1280×1024 下的 P5 YOLOv5n/s/m/l/x，以及经典 P6 YOLOv5n6/s6/m6/l6/x6。其他模型系列和输出契约不属于 V2.0 发布范围。
- 文档更新为双语 README，并统一模块和文档跳转链接的可点击路径描述。
- logo 资源从 `doc/logo/` 移动到 `docs/logo/`。

### 移除

- 移除基准版本中的 `yolov5/main.cpp`、`yolov5/yolo.cpp` 和 `yolov5/yolo.h` 单体应用入口和便捷接口。
- 移除基准版本中的 `common/` helper 布局，相关可复用 helper 已迁移到 `src/utils/`。
- 从常规应用执行路径中移除基准版本 CPU 侧 YOLOv5 decode/NMS。
- 移除旧的 `image/` 示例输入目录，示例输入统一移动到 `assets/`。
