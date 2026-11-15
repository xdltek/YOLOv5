<a id="english"></a>

# RPP Postprocess Module

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This module converts YOLOv5 model output into compact detection records.

## Responsibilities

- Fuse FP32-to-BF16 conversion, box/score slicing, and NMS in one OpenRT/RppRT engine.
- Run NMS on RPP without separate per-frame cast and pre-slice submissions.
- Copy NMS indices and the BF16 box tensor back to host for selected-box restoration.
- Restore boxes from letterboxed model coordinates to source coordinates.

## Output Contract

`RppYoloPostprocessor` assumes the standard YOLOv5 output layout:

```text
[x, y, w, h, objectness, class_scores...]
```

Models with a different output layout are outside this release guarantee. Supporting one requires coordinated updates to `YoloV5Config`, postprocess configuration, detection parsing, and product validation.

At 640×640, validated P5 models produce `[1,25200,85]` and P6 models produce `[1,25500,85]`. At 1280×1024, validated P5 models produce `[1,80640,85]` and P6 models produce `[1,81600,85]`. All use pixel-space `xywh`, objectness, and 80 COCO class scores.

## CPU Boundary

YOLO decode and NMS stay on RPP in the application path. The CPU uses returned NMS indices and BF16 box data only to restore selected boxes for result reporting and OpenCV visualization.

## Main API

- `RppYoloPostprocessor::init()`
  Builds the fused postprocess engine and allocates persistent buffers.
- `RppYoloPostprocessor::run()`
  Executes the fused device stage, D2H copy, and host detection parsing.

---

<a id="chinese"></a>

# RPP 后处理模块

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本模块将 YOLOv5 模型输出转换为 compact 检测记录。

## 模块职责

- 在一个 OpenRT/RppRT engine 中融合 FP32-to-BF16 转换、box/score 切分和 NMS。
- 在 RPP 上执行 NMS，避免每帧分别提交 cast 和 pre-slice。
- 将 NMS 索引和 BF16 box tensor 复制回 host，用于恢复选中的检测框。
- 将检测框从 letterbox 后的模型坐标恢复到源图坐标。

## 输出约束

`RppYoloPostprocessor` 默认使用标准 YOLOv5 输出布局：

```text
[x, y, w, h, objectness, class_scores...]
```

不同输出布局的模型不属于本次发布保证范围。如需扩展，必须同步更新 `YoloV5Config`、后处理配置、检测结果解析逻辑并重新完成产品验证。

在 640×640 输入下，已验证的 P5 模型输出 `[1,25200,85]`，P6 模型输出 `[1,25500,85]`；在 1280×1024 输入下，P5 模型输出 `[1,80640,85]`，P6 模型输出 `[1,81600,85]`。所有模型均使用像素坐标 `xywh`、objectness 和 80 个 COCO 类别分数。

## CPU 边界

应用执行路径中，YOLO decode 和 NMS 保持在 RPP 上执行。CPU 仅使用返回的 NMS indices 和 BF16 box 数据恢复选中的检测框，用于结果报告和 OpenCV 可视化。

## 主要 API

- `RppYoloPostprocessor::init()`
  构建融合后处理 engine 并分配持久 buffer。
- `RppYoloPostprocessor::run()`
  执行融合 device 阶段、D2H copy 和 host 检测解析。
