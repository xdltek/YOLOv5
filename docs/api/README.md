<a id="english"></a>

# API Documentation

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This page links to the main reusable API surfaces in the RPP-accelerated YOLOv5 application.

## Modules

Click a module path below to jump to its guide:

- [RPP acceleration modules](../../rpp/README.md): RPP execution layer guide.
- [RPP inference module](../../rpp/inference/README.md): OpenRT inference API guide.
- [RPP preprocess module](../../rpp/preprocess/README.md): preprocessing API guide.
- [RPP postprocess module](../../rpp/postprocess/README.md): postprocessing API guide.
- [YOLOv5 application module](../../src/yolov5/README.md): pipeline API guide.
- [Utility module](../../src/utils/README.md): shared utility guide.
- [Performance trace helper](../../src/perf/README.md): perf trace guide.
- [Script usage](../../README.md#quick-start): root README quick start.
- [RGB image application](../../demos/rgb_image/README.md): RGB workload guide.
- [I420 YUV frame application](../../demos/yuv_frame/README.md): YUV workload guide.

## Primary Interfaces

- `YoloV5Pipeline`
  Reusable application-level pipeline for RGB/BGR image input and packed I420 frame input.
- `RppInferEngine`
  OpenRT on RPP inference wrapper for ONNX model loading, engine creation, binding buffers, and execution.
- `RppYoloPreprocessor`
  RPP preprocessing module for letterbox resize, format conversion, and normalization.
- `RppYoloPostprocessor`
  RPP postprocessing module for YOLO output processing and compact detection output.

Use the module README files for migration notes, input/output contracts, and API boundaries.

---

<a id="chinese"></a>

# API 文档

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本页链接 RPP 加速 YOLOv5 应用中的主要可复用 API。

## 模块

点击下方模块路径可跳转到对应说明：

- [RPP 加速模块](../../rpp/README.md)：RPP 执行层说明。
- [RPP 推理模块](../../rpp/inference/README.md)：OpenRT 推理 API 说明。
- [RPP 前处理模块](../../rpp/preprocess/README.md)：前处理 API 说明。
- [RPP 后处理模块](../../rpp/postprocess/README.md)：后处理 API 说明。
- [YOLOv5 应用模块](../../src/yolov5/README.md)：pipeline API 说明。
- [工具模块](../../src/utils/README.md)：公共工具说明。
- [性能 trace 辅助模块](../../src/perf/README.md)：perf trace 说明。
- [脚本使用](../../README.md#快速开始)：根 README 快速开始。
- [RGB 图片应用](../../demos/rgb_image/README.md)：RGB 工作负载说明。
- [I420 YUV 帧应用](../../demos/yuv_frame/README.md)：YUV 工作负载说明。

## 主要接口

- `YoloV5Pipeline`
  面向 RGB/BGR 图片输入和 packed I420 帧输入的可复用应用层 pipeline。
- `RppInferEngine`
  基于 OpenRT on RPP 的 ONNX 模型加载、engine 创建、binding buffer 和执行封装。
- `RppYoloPreprocessor`
  用于 letterbox resize、格式转换和归一化的 RPP 前处理模块。
- `RppYoloPostprocessor`
  用于 YOLO 输出处理和 compact detection 输出的 RPP 后处理模块。

迁移说明、输入输出约束和 API 边界请参考各模块 README。
