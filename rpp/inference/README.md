<a id="english"></a>

# RPP Inference Module

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This module wraps OpenRT on RPP inference for the validated classic YOLOv5n/s/m/l/x detection models at static 640×640 and 1280×1024 input shapes.

## Responsibilities

- Parse the ONNX model.
- Build the OpenRT on RPP execution engine on the first run, then safely validate and reuse its cache when supported by the installed SDK.
- Own model input and output binding buffers.
- Execute synchronous inference.
- Expose model input shape, output shape, and tensor data types to the pipeline.

## Main API

- `RppInferEngine::init()`
  Builds the inference engine and allocates bindings.
- `RppInferEngine::execute()`
  Runs one inference pass using the current input binding.
- `RppInferEngine::getInputDeviceBuffer()`
  Returns the device buffer consumed by preprocessing.
- `RppInferEngine::getOutputDeviceBuffer()`
  Returns the device buffer consumed by postprocessing.

## Input And Output

- Input: supported static, simplified YOLOv5 ONNX model path.
- Input binding: FP32 model input tensor.
- Output binding: FP32 YOLOv5 output tensor.

## Reuse Notes

Reuse this module when the model can be loaded through the same OpenRT on RPP ONNX flow. Keep model-specific preprocessing, postprocessing, labels, and visualization outside this module.

The cache is stored under `<model-directory>/.engine_cache/`. Its key includes ONNX content, OpenRT version, and engine build settings. Initialization prints whether it built, reused, or rejected a cached engine. Any OpenRT error reported during deserialization makes the cache invalid and triggers a safe ONNX rebuild.

---

<a id="chinese"></a>

# RPP 推理模块

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本模块封装已验证的经典 YOLOv5n/s/m/l/x 检测模型在 OpenRT on RPP 上的推理流程，支持静态 640×640 和 1280×1024 输入 shape。

## 模块职责

- 解析 ONNX 模型。
- 首次运行时构建 OpenRT on RPP 执行 engine；后续在本机 SDK 支持时安全验证并复用缓存。
- 管理模型输入和输出 binding buffer。
- 执行同步推理。
- 向 pipeline 暴露模型输入尺寸、输出尺寸和 tensor 数据类型。

## 主要 API

- `RppInferEngine::init()`
  构建推理 engine 并分配 binding。
- `RppInferEngine::execute()`
  使用当前输入 binding 执行一次推理。
- `RppInferEngine::getInputDeviceBuffer()`
  返回前处理写入的 device buffer。
- `RppInferEngine::getOutputDeviceBuffer()`
  返回后处理读取的 device buffer。

## 输入输出

- 输入：受支持的静态维度、已简化 YOLOv5 ONNX 模型路径。
- 输入 binding：FP32 模型输入 tensor。
- 输出 binding：FP32 YOLOv5 输出 tensor。

## 复用说明

当模型可以通过相同的 OpenRT on RPP ONNX 流程加载时，可复用本模块。模型相关的前处理、后处理、标签和可视化逻辑应放在本模块之外。

缓存保存在 `<模型目录>/.engine_cache/` 下，缓存键包含 ONNX 内容、OpenRT 版本和 engine 构建配置。初始化日志会明确打印本次是新建、复用还是拒绝缓存。反序列化期间只要 OpenRT 报告错误，该缓存即视为无效，并安全回退到 ONNX 重建。
