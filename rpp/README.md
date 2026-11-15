<a id="english"></a>

# RPP Acceleration Modules

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This directory contains the hardware-facing acceleration layer used by the YOLOv5 applications. It wraps OpenRT on RPP inference, RPP preprocessing kernels, and RPP postprocessing kernels behind reusable C++ interfaces.

The code in this directory is intentionally kept separate from application entry code and YOLOv5 business logic. Applications should usually call `src/yolov5/YoloV5Pipeline` instead of invoking these low-level modules directly.

Related documentation: [API documentation](../docs/api/README.md): public API index.

## Directory Layout

- `inference/`
  Generic OpenRT on RPP inference engine wrapper. It parses the ONNX model, builds the execution engine, owns binding buffers, and measures the inference execution interval.
- `preprocess/`
  YOLOv5 input preprocessing on RPP. It provides host-side wrapper code plus RPP kernels for RGB/BGR image input and I420 YUV frame input.
- `postprocess/`
  YOLOv5 output postprocessing on RPP. It runs detection decoding, filtering, and NMS on the accelerator, then transfers the NMS indices and BF16 box tensor needed for host-side restoration.

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
| D2H NMS/box data     |
+-----------------------+
```

## Data Movement

The RPP layer keeps intermediate tensors on the device whenever possible:

- Input H2D happens before preprocessing. The RGB/BGR application copies decoded image bytes; the YUV application copies I420 frame bytes.
- Preprocessing output stays on the device and is consumed directly by `RppInferEngine`.
- Inference output stays on the device and is consumed directly by `RppYoloPostprocessor`.
- Output D2H copies NMS indices and the BF16 box tensor required by the CPU to restore and draw selected boxes.

The CPU does not run YOLOv5 decode or NMS in the normal application path. It parses the returned NMS indices and selected box data for visualization.

## Postprocess Contract

`postprocess/RppYoloPostprocessor` assumes a standard YOLOv5 output tensor layout:

```text
[x, y, w, h, objectness, class_scores...]
```

Models with a different output layout are outside the validated product scope and require coordinated configuration, parsing, and validation changes.

The normal application path keeps YOLO decode and NMS on RPP. The CPU restores selected boxes for result reporting and OpenCV visualization.

## Timing Ownership

Applications print stage-level timing:

```text
Input H2D -> Preprocess -> Inference -> Postprocess -> Output D2H
```

The report highlights two throughput metrics: `End-to-end FPS`, calculated from preprocess + inference + postprocess latency, and `OpenRT inference FPS`, calculated from standalone engine execution latency. `Input H2D` and `Output D2H` remain separate transfer fields.

The RPP modules expose these timings through their result structures. More detailed device-side events are captured by `rpp_perf` when the project is built with `YOLO_ENABLE_RPP_PERF=ON`.

## Reuse Guidance

For a new YOLOv5-style application:

1. Use `src/yolov5/YoloV5Pipeline` as the main integration point.
2. Select the input path that matches the source format, such as `runRgb()` or `runI420()`.
3. Keep warmup and measured loop policy in the application executable, not in the RPP module.
4. Use the RPP classes directly only for focused inference, preprocessing, or postprocessing experiments.

For a non-YOLO model:

1. Reuse `inference/RppInferEngine` if the model can be loaded through the same OpenRT on RPP ONNX flow.
2. Add model-specific preprocessing and postprocessing modules beside the current YOLOv5 modules or under a new model-specific directory.
3. Keep model labels, visualization, and application-specific reporting outside this `rpp/` directory.

## Module Responsibilities

`inference/RppInferEngine` should remain model-agnostic. It owns inference initialization, engine construction, input/output buffers, and OpenRT on RPP inference execution.

`preprocess/RppYoloPreprocessor` should own only YOLOv5 input tensor preparation. It should not know about labels, output boxes, visualization, or application command-line options.

`postprocess/RppYoloPostprocessor` should own only YOLOv5 output tensor decoding and NMS. It should return compact detection records for host-side output handling or drawing.

---

<a id="chinese"></a>

# RPP 加速模块

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本目录包含 YOLOv5 应用使用的硬件侧加速层。它通过可复用的 C++ 接口封装了 OpenRT on RPP 推理、RPP 前处理 kernel 和 RPP 后处理 kernel。

这里的代码有意与应用入口和 YOLOv5 业务逻辑分离。正常情况下，应用应优先调用 `src/yolov5/YoloV5Pipeline`，而不是直接调用这些底层 RPP 模块。

相关文档：[API 文档](../docs/api/README.md)：公开 API 索引。

## 目录结构

- `inference/`
  通用 OpenRT on RPP 推理 engine 封装。它负责解析 ONNX 模型、构建执行 engine、管理 binding buffer，并测量推理执行区间。
- `preprocess/`
  YOLOv5 输入前处理的 RPP 实现。该目录包含 host 侧封装代码，以及面向 RGB/BGR 图片输入和 I420 YUV 帧输入的 RPP kernel。
- `postprocess/`
  YOLOv5 输出后处理的 RPP 实现。它在加速器上执行检测结果解码、过滤和 NMS，然后回传 host 恢复检测框所需的 NMS 索引与 BF16 box tensor。

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
| D2H NMS/box 数据     |
+-----------------------+
```

## 数据搬运

RPP 层会尽可能让中间 tensor 保留在 device 侧：

- 输入 H2D 发生在前处理之前。RGB/BGR 应用复制解码后的图片字节；YUV 应用复制 I420 帧字节。
- 前处理输出保留在 device 侧，并直接作为 `RppInferEngine` 的输入。
- 推理输出保留在 device 侧，并直接作为 `RppYoloPostprocessor` 的输入。
- 输出 D2H 复制 NMS 索引和 BF16 box tensor，CPU 使用这些数据恢复并绘制选中的检测框。

常规应用路径中，CPU 不执行 YOLOv5 decode 或 NMS，仅解析返回的 NMS 索引和选中 box 数据用于可视化。

## 后处理约束

`postprocess/RppYoloPostprocessor` 默认使用标准 YOLOv5 输出 tensor 布局：

```text
[x, y, w, h, objectness, class_scores...]
```

不同输出布局的模型不属于已验证产品范围，需要同步更新配置、解析逻辑并重新完成验证。

常规应用路径中，YOLO decode 和 NMS 保持在 RPP 上执行。CPU 负责恢复选中的检测框，用于结果输出和 OpenCV 可视化。

## 计时归属

应用打印阶段级耗时：

```text
Input H2D -> Preprocess -> Inference -> Postprocess -> Output D2H
```

报告重点展示两项吞吐率指标：`End-to-end FPS` 根据前处理 + 推理 + 后处理耗时计算，`OpenRT inference FPS` 根据单独的 engine 执行耗时计算。`Input H2D` 和 `Output D2H` 仍作为独立传输指标打印。

RPP 模块通过各自的结果结构暴露这些耗时。更细粒度的 device 侧事件由 `rpp_perf` 捕获：项目需要使用 `YOLO_ENABLE_RPP_PERF=ON` 构建。

## 复用建议

对于新的 YOLOv5 风格应用：

1. 使用 `src/yolov5/YoloV5Pipeline` 作为主要集成入口。
2. 根据输入格式选择对应接口，例如 `runRgb()` 或 `runI420()`。
3. 将 warmup 和测量 loop 策略放在应用可执行文件中，不放在 RPP 模块内部。
4. 只有在做推理、前处理或后处理专项实验时，才直接调用 RPP 类。

对于非 YOLO 模型：

1. 如果模型可以通过相同的 OpenRT on RPP ONNX 流程加载，可复用 `inference/RppInferEngine`。
2. 在当前 YOLOv5 模块旁边，或新的模型专属目录下，新增模型对应的前处理和后处理模块。
3. 模型标签、可视化和应用特定的输出格式应放在 `rpp/` 目录之外。

## 模块职责

`inference/RppInferEngine` 应保持模型无关。它负责推理初始化、engine 构建、输入输出 buffer 和 OpenRT on RPP 推理执行。

`preprocess/RppYoloPreprocessor` 只负责 YOLOv5 输入 tensor 准备。它不应关心标签、输出框、可视化或应用命令行参数。

`postprocess/RppYoloPostprocessor` 只负责 YOLOv5 输出 tensor 解码和 NMS。它应返回 compact 检测记录，供 host 侧输出处理或画框。
