# RPP Acceleration Modules

This directory contains the hardware-facing acceleration layer used by the YOLOv5 demos. It wraps RppRT inference, RPP preprocessing kernels, and RPP postprocessing kernels behind reusable C++ interfaces.

The code in this directory is intentionally kept separate from demo entry code and YOLOv5 business logic. Demo programs should usually call `src/yolov5/YoloV5Pipeline` instead of invoking these low-level modules directly.

## Directory Layout

- `runtime/`
  Generic RppRT inference engine wrapper. It parses the ONNX model, builds the execution engine, owns runtime buffers, and measures the OpenRT engine execution interval.
- `preprocess/`
  YOLOv5 input preprocessing on RPP. It provides host-side wrapper code plus RPP kernels for RGB/BGR image input and I420 YUV frame input.
- `postprocess/`
  YOLOv5 output postprocessing on RPP. It runs detection decoding, filtering, and NMS on the accelerator, then copies the compact detection results back to the host.

## Call Flow

The normal single-image pipeline calls the RPP modules in this order:

```text
+-----------------------+
| Host input image     |
| RGB/BGR or I420 YUV  |
+-----------------------+
           |
           v
+-----------------------+
| RppYoloPreprocessor  |
| H2D + resize/format  |
+-----------------------+
           |
           v
+-----------------------+
| RppInferEngine       |
| OpenRT ONNX infer    |
+-----------------------+
           |
           v
+-----------------------+
| RppYoloPostprocessor |
| decode + NMS on RPP  |
+-----------------------+
           |
           v
+-----------------------+
| Host detections      |
| D2H compact results  |
+-----------------------+
```

## Data Movement

The RPP layer keeps intermediate tensors on the device whenever possible:

- Input H2D happens before preprocessing. RGB/BGR demos copy decoded image bytes; YUV demos copy the I420 frame bytes.
- Preprocessing output stays on the device and is consumed directly by `RppInferEngine`.
- Inference output stays on the device and is consumed directly by `RppYoloPostprocessor`.
- Output D2H copies only the compact postprocessed detection buffer required by the CPU to draw boxes or print results.

The CPU does not run YOLOv5 decode or NMS in the normal demo path. It only receives the final detection records and uses them for visualization.

## Timing Ownership

Demos print stage-level timing:

```text
Input H2D -> Preprocess -> Inference -> Postprocess -> Output D2H
```

The RPP modules expose these timings through their result structures. More detailed device-side events are captured by `rpp_perf` when the project is built with `YOLO_ENABLE_RPP_PERF=ON` and the demo is launched with `-p` or `--perf`.

## Reuse Guidance

For a new YOLOv5-style demo:

1. Use `src/yolov5/YoloV5Pipeline` as the main integration point.
2. Select the input path that matches the source format, such as `runRgb()` or `runI420()`.
3. Keep warmup and measured loop policy in the demo executable, not in the RPP module.
4. Use the RPP classes directly only for focused runtime, preprocessing, or postprocessing experiments.

For a non-YOLO model:

1. Reuse `runtime/RppInferEngine` if the model can be loaded through the same RppRT ONNX flow.
2. Add model-specific preprocessing and postprocessing modules beside the current YOLOv5 modules or under a new model-specific directory.
3. Keep model labels, visualization, and demo-specific reporting outside this `rpp/` directory.

## Module Responsibilities

`runtime/RppInferEngine` should remain model-agnostic. It owns runtime initialization, engine construction, input/output buffers, and OpenRT inference execution.

`preprocess/RppYoloPreprocessor` should own only YOLOv5 input tensor preparation. It should not know about labels, output boxes, visualization, or demo command-line options.

`postprocess/RppYoloPostprocessor` should own only YOLOv5 output tensor decoding and NMS. It should return compact detection records for host-side output handling or drawing.

---

# RPP 加速模块

本目录包含 YOLOv5 demo 使用的硬件侧加速层。它通过可复用的 C++ 接口封装了 RppRT 推理、RPP 前处理 kernel 和 RPP 后处理 kernel。

这里的代码有意与 demo 入口和 YOLOv5 业务逻辑分离。正常情况下，demo 程序应优先调用 `src/yolov5/YoloV5Pipeline`，而不是直接调用这些底层 RPP 模块。

## 目录结构

- `runtime/`
  通用 RppRT 推理 engine 封装。它负责解析 ONNX 模型、构建执行 engine、管理运行时 buffer，并测量 OpenRT engine 执行区间。
- `preprocess/`
  YOLOv5 输入前处理的 RPP 实现。该目录包含 host 侧封装代码，以及面向 RGB/BGR 图片输入和 I420 YUV 帧输入的 RPP kernel。
- `postprocess/`
  YOLOv5 输出后处理的 RPP 实现。它在加速器上执行检测结果解码、过滤和 NMS，然后将 compact 后的检测结果复制回 host。

## 调用流程

常规单图 pipeline 按如下顺序调用 RPP 模块：

```text
+-----------------------+
| Host 输入图片         |
| RGB/BGR 或 I420 YUV   |
+-----------------------+
           |
           v
+-----------------------+
| RppYoloPreprocessor  |
| H2D + resize/format  |
+-----------------------+
           |
           v
+-----------------------+
| RppInferEngine       |
| OpenRT ONNX 推理     |
+-----------------------+
           |
           v
+-----------------------+
| RppYoloPostprocessor |
| RPP 上 decode + NMS  |
+-----------------------+
           |
           v
+-----------------------+
| Host 检测结果        |
| D2H compact results  |
+-----------------------+
```

## 数据搬运

RPP 层会尽可能让中间 tensor 保留在 device 侧：

- 输入 H2D 发生在前处理之前。RGB/BGR demo 复制解码后的图片字节；YUV demo 复制 I420 帧字节。
- 前处理输出保留在 device 侧，并直接作为 `RppInferEngine` 的输入。
- 推理输出保留在 device 侧，并直接作为 `RppYoloPostprocessor` 的输入。
- 输出 D2H 只复制 compact 后的检测结果 buffer，CPU 使用该结果进行画框或打印。

常规 demo 路径中，CPU 不执行 YOLOv5 decode 或 NMS。CPU 只接收最终检测记录，并用于可视化。

## 计时归属

demo 打印阶段级耗时：

```text
Input H2D -> Preprocess -> Inference -> Postprocess -> Output D2H
```

RPP 模块通过各自的结果结构暴露这些耗时。更细粒度的 device 侧事件由 `rpp_perf` 捕获：项目需要使用 `YOLO_ENABLE_RPP_PERF=ON` 构建，并在运行 demo 时添加 `-p` 或 `--perf`。

## 复用建议

对于新的 YOLOv5 风格 demo：

1. 使用 `src/yolov5/YoloV5Pipeline` 作为主要集成入口。
2. 根据输入格式选择对应接口，例如 `runRgb()` 或 `runI420()`。
3. 将 warmup 和测量 loop 策略放在 demo 可执行文件中，不放在 RPP 模块内部。
4. 只有在做 runtime、前处理或后处理专项实验时，才直接调用 RPP 类。

对于非 YOLO 模型：

1. 如果模型可以通过相同的 RppRT ONNX 流程加载，可复用 `runtime/RppInferEngine`。
2. 在当前 YOLOv5 模块旁边，或新的模型专属目录下，新增模型对应的前处理和后处理模块。
3. 模型标签、可视化和 demo 特定的输出格式应放在 `rpp/` 目录之外。

## 模块职责

`runtime/RppInferEngine` 应保持模型无关。它负责运行时初始化、engine 构建、输入输出 buffer 和 OpenRT 推理执行。

`preprocess/RppYoloPreprocessor` 只负责 YOLOv5 输入 tensor 准备。它不应关心标签、输出框、可视化或 demo 命令行参数。

`postprocess/RppYoloPostprocessor` 只负责 YOLOv5 输出 tensor 解码和 NMS。它应返回 compact 检测记录，供 host 侧输出处理或画框。
