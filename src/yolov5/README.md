<a id="english"></a>

# YOLOv5 Application Module

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This module contains the reusable YOLOv5 application layer.

## Responsibilities

- Define detection and letterbox data structures.
- Provide the COCO 80-class label table.
- Compose preprocessing, inference, and postprocessing through `YoloV5Pipeline`.
- Draw detections and save rendered images.

## Main API

- `YoloV5Pipeline::init()`
  Initializes OpenRT on RPP inference and RPP postprocessing.
- `YoloV5Pipeline::runRgb()`
  Runs one RGB/BGR image through the full pipeline.
- `YoloV5Pipeline::runI420()`
  Runs one packed I420 frame through the full pipeline.
- `draw_detections()`
  Draws detection boxes on an OpenCV image.

## Timing Contract

`YoloV5StageTimes` reports H2D and D2H separately. `end_to_end_ms` is the preprocess + inference + postprocess latency used for `End-to-end FPS`; `inference_ms` is the standalone engine execution latency used for `OpenRT inference FPS`.

---

<a id="chinese"></a>

# YOLOv5 应用模块

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本模块包含可复用的 YOLOv5 应用层。

## 模块职责

- 定义检测结果和 letterbox 数据结构。
- 提供 COCO 80 类 label 表。
- 通过 `YoloV5Pipeline` 组合前处理、推理和后处理。
- 绘制检测框并保存结果图片。

## 主要 API

- `YoloV5Pipeline::init()`
  初始化 OpenRT on RPP 推理和 RPP 后处理。
- `YoloV5Pipeline::runRgb()`
  执行一张 RGB/BGR 图片的完整 pipeline。
- `YoloV5Pipeline::runI420()`
  执行一帧 packed I420 数据的完整 pipeline。
- `draw_detections()`
  在 OpenCV 图片上绘制检测框。

## 计时约束

`YoloV5StageTimes` 单独上报 H2D 和 D2H。`end_to_end_ms` 是用于计算 `End-to-end FPS` 的前处理 + 推理 + 后处理耗时；`inference_ms` 是用于计算 `OpenRT inference FPS` 的单独 engine 执行耗时。
