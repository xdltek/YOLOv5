<a id="english"></a>

# RPP Postprocess Module

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This module converts YOLOv5 model output into compact detection records.

## Responsibilities

- Convert model output from FP32 to BF16 for RPP postprocessing.
- Slice boxes and class scores for RPP NMS.
- Run NMS on RPP.
- Copy compact NMS results back to host.
- Restore boxes from letterboxed model coordinates to source coordinates.

## Output Contract

`RppYoloPostprocessor` assumes the standard YOLOv5 output layout:

```text
[x, y, w, h, objectness, class_scores...]
```

If a model uses a different output layout, update `YoloV5Config`, postprocess configuration, and detection parsing together.

## CPU Boundary

YOLO decode and NMS stay on RPP in the normal demo path. The CPU receives compact detection records only for result printing and OpenCV visualization.

## Main API

- `RppYoloPostprocessor::init()`
  Builds postprocess engines and allocates buffers.
- `RppYoloPostprocessor::run()`
  Executes cast, pre-slice, NMS, D2H copy, and host detection parsing.

---

<a id="chinese"></a>

# RPP 后处理模块

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本模块将 YOLOv5 模型输出转换为 compact 检测记录。

## 模块职责

- 将模型 FP32 输出转换为 RPP 后处理使用的 BF16。
- 为 RPP NMS 切分 boxes 和 class scores。
- 在 RPP 上执行 NMS。
- 将 compact NMS 结果复制回 host。
- 将检测框从 letterbox 后的模型坐标恢复到源图坐标。

## 输出约束

`RppYoloPostprocessor` 默认使用标准 YOLOv5 输出布局：

```text
[x, y, w, h, objectness, class_scores...]
```

如果模型使用不同输出布局，需要同步更新 `YoloV5Config`、后处理配置和检测结果解析逻辑。

## CPU 边界

常规 demo 路径中，YOLO decode 和 NMS 保持在 RPP 上执行。CPU 只接收 compact 检测记录，用于结果打印和 OpenCV 可视化。

## 主要 API

- `RppYoloPostprocessor::init()`
  构建后处理 engine 并分配 buffer。
- `RppYoloPostprocessor::run()`
  执行 cast、pre-slice、NMS、D2H copy 和 host 检测解析。
