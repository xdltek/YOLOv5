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
| Supported models | Classic YOLOv5n/s/m/l/x and YOLOv5n6/s6/m6/l6/x6 detection models at static 640×640 and 1280×1024 input shapes |
| INT8 status | Experimental OpenRT parse/engine execution; accuracy is outside this release gate |

### Release Verification

The release candidate was rebuilt from clean generated outputs. All 20 supported detection artifacts completed one warmup pass and one measured RGB inference pass with non-empty detections and a rendered result on 2026-11-15. The 1280×1024 set also passed first-run OpenRT engine construction. Performance was measured separately with the 640×640 `yolov5s.onnx`, one warmup frame, and 50 measured frames per workload.

| Validated family | ONNX artifacts | Input | Output | Functional status |
| :--- | :--- | :--- | :--- | :--- |
| YOLOv5 P5 | `yolov5{n,s,m,l,x}_640x640_sim.onnx` | FP32 `[1,3,640,640]` | FP32 `[1,25200,85]` | 5/5 passed |
| YOLOv5 P6 | `yolov5{n6,s6,m6,l6,x6}_640x640_sim.onnx` | FP32 `[1,3,640,640]` | FP32 `[1,25500,85]` | 5/5 passed |
| YOLOv5 P5 | `yolov5{n,s,m,l,x}_1280x1024_sim.onnx` | FP32 `[1,3,1280,1024]` | FP32 `[1,80640,85]` | 5/5 passed |
| YOLOv5 P6 | `yolov5{n6,s6,m6,l6,x6}_1280x1024_sim.onnx` | FP32 `[1,3,1280,1024]` | FP32 `[1,81600,85]` | 5/5 passed |

The following table reports YOLOv5s performance only:

| Workload | Throughput | End-to-end latency | Preprocess | OpenRT inference | Postprocess | Last-frame detections |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| RGB image | 105.816 FPS | 9.450 ms | 1.588 ms | 4.362 ms | 3.501 ms | 20 |
| I420 YUV frame | 95.763 FPS | 10.442 ms | 2.295 ms | 4.471 ms | 3.676 ms | 2 |

These figures describe this specific local release run and are not a cross-platform performance guarantee. The complete benchmark log is generated under `build/logs/` by `scripts/benchmark.sh`.

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
- Updated timing output to report H2D and D2H transfer time separately; FPS is calculated from the printed preprocess + inference + postprocess latency.
- Decoupled compilation from model preparation. `scripts/build.sh` no longer checks, downloads, or verifies ONNX files; scripts and C++ executables consistently use `-m/--model`, `-i/--input`, and `-o/--output`, and fall back to `weights/yolov5s.onnx` only when it exists.
- Added content-keyed OpenRT engine caching with explicit build/reuse runtime logs.
- Updated `scripts/run.sh` so the default mode runs both RGB and YUV workloads serially. Single-workload execution remains available through `--demo rgb` or `--demo yuv`.
- Updated `scripts/benchmark.sh` so the default benchmark covers both workloads and saves logs under `build/logs/`.
- Validated the unquantized classic YOLOv5 P5 and P6 COCO80 detection families at static 640×640 and 1280×1024 input shapes. Classification, segmentation, YOLOv5u, P6-u, YOLOv8, custom-class outputs, and other output contracts are not part of the V2.0 release guarantee.
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
| 支持模型 | 经典 YOLOv5n/s/m/l/x 和 YOLOv5n6/s6/m6/l6/x6 检测模型，支持静态 640×640 与 1280×1024 输入 shape |
| INT8 状态 | 实验性 OpenRT 解析和 engine 执行能力；精度不作为本次发布门槛 |

### 发布验证

发布候选版本已从干净的生成目录重新构建。2026-11-15，全部 20 个受支持检测模型均完成 1 次 warmup 和 1 次 RGB 正式推理，获得非空检测结果并成功输出检测图；1280×1024 模型还完成了首次 OpenRT engine 构建验证。性能测试另使用 640×640 `yolov5s.onnx`，每个工作负载执行 1 帧 warmup 和 50 帧正式测量。

| 已验证系列 | ONNX 文件 | 输入 | 输出 | 功能状态 |
| :--- | :--- | :--- | :--- | :--- |
| YOLOv5 P5 | `yolov5{n,s,m,l,x}_640x640_sim.onnx` | FP32 `[1,3,640,640]` | FP32 `[1,25200,85]` | 5/5 通过 |
| YOLOv5 P6 | `yolov5{n6,s6,m6,l6,x6}_640x640_sim.onnx` | FP32 `[1,3,640,640]` | FP32 `[1,25500,85]` | 5/5 通过 |
| YOLOv5 P5 | `yolov5{n,s,m,l,x}_1280x1024_sim.onnx` | FP32 `[1,3,1280,1024]` | FP32 `[1,80640,85]` | 5/5 通过 |
| YOLOv5 P6 | `yolov5{n6,s6,m6,l6,x6}_1280x1024_sim.onnx` | FP32 `[1,3,1280,1024]` | FP32 `[1,81600,85]` | 5/5 通过 |

下表仅报告 YOLOv5s 的性能数据：

| 工作负载 | 吞吐率 | 端到端延迟 | 前处理 | OpenRT 推理 | 后处理 | 最后一帧检测数量 |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: |
| RGB 图片 | 105.816 FPS | 9.450 ms | 1.588 ms | 4.362 ms | 3.501 ms | 20 |
| I420 YUV 帧 | 95.763 FPS | 10.442 ms | 2.295 ms | 4.471 ms | 3.676 ms | 2 |

以上结果仅代表本地本次发布测量，不构成跨平台性能保证。完整 benchmark 日志由 `scripts/benchmark.sh` 生成并保存在 `build/logs/`。

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
- 计时输出更新为单独打印 H2D 和 D2H 传输耗时；FPS 使用打印的前处理 + 推理 + 后处理耗时计算。
- 编译与模型准备解耦。`scripts/build.sh` 不再检查、下载或校验 ONNX；脚本与 C++ 可执行程序统一使用 `-m/--model`、`-i/--input` 和 `-o/--output`，且仅在 `weights/yolov5s.onnx` 存在时使用默认模型。
- 新增基于模型内容的 OpenRT engine 缓存，并在运行日志中明确打印新建或复用状态。
- `scripts/run.sh` 默认串行运行 RGB 和 YUV 两个工作负载；仍可通过 `--demo rgb` 或 `--demo yuv` 运行单个工作负载。
- `scripts/benchmark.sh` 默认覆盖两个工作负载，并将日志保存到 `build/logs/`。
- 本次发布已验证未量化的经典 YOLOv5 P5 与 P6 COCO80 检测系列，覆盖静态 640×640 和 1280×1024 输入 shape。分类、分割、YOLOv5u、P6-u、YOLOv8、自定义类别输出及其他输出契约不属于 V2.0 发布保证范围。
- 文档更新为双语 README，并统一模块和文档跳转链接的可点击路径描述。
- logo 资源从 `doc/logo/` 移动到 `docs/logo/`。

### 移除

- 移除基准版本中的 `yolov5/main.cpp`、`yolov5/yolo.cpp` 和 `yolov5/yolo.h` 单体应用入口和便捷接口。
- 移除基准版本中的 `common/` helper 布局，相关可复用 helper 已迁移到 `src/utils/`。
- 从常规应用执行路径中移除基准版本 CPU 侧 YOLOv5 decode/NMS。
- 移除旧的 `image/` 示例输入目录，示例输入统一移动到 `assets/`。
