<a id="english"></a>

# RPP Preprocess Module

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This module prepares YOLOv5 model input tensors on RPP.

## Responsibilities

- Validate host or device input metadata.
- Copy host input through a reusable pinned staging buffer.
- Compute YOLO letterbox scale and padding.
- Convert input format, resize, and normalize into NCHW model layout.
- Reuse the runtime stream and SRAM workspaces across frames.
- Report H2D and preprocessing timings.

## Supported Formats

- `BGR_HWC`
- `RGB_HWC`
- `RGB_CHW`
- `YUV_I420`
The current applications use `BGR_HWC` for RGB image input and `YUV_I420` for raw frame input.
`NV12` and `NV21` are reserved enum values and are rejected until format-specific kernels are provided.

## Main API

- `RppYoloPreprocessor::init()`
  Configures maximum source size and model input shape.
- `RppYoloPreprocessor::run()`
  Runs one preprocessing pass into the model input device buffer.
- `RppYoloPreprocessor::releaseSramBuffers()`
  Explicitly releases persistent SRAM workspaces when an integration needs to return the memory early.
- `make_letterbox_info()`
  Computes geometry metadata shared with postprocessing.

## Reuse Notes

Use this module when the model exposes a static FP32 NCHW input and expects YOLOv5-style letterbox plus `[0, 1]` normalization. The implementation is a conventional preprocessing stage: it uses local SDK runtime calls and source kernels, but does not use graph capture or graph execution.

---

<a id="chinese"></a>

# RPP 前处理模块

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本模块在 RPP 上准备 YOLOv5 模型输入 tensor。

## 模块职责

- 校验 host 或 device 输入元数据。
- 通过可复用的页锁定 Host 中转缓冲区复制输入。
- 计算 YOLO letterbox 缩放比例和 padding。
- 完成格式转换、resize、归一化，并写入 NCHW 模型输入布局。
- 跨帧复用 runtime stream 和 SRAM workspace。
- 上报 H2D 和前处理耗时。

## 支持格式

- `BGR_HWC`
- `RGB_HWC`
- `RGB_CHW`
- `YUV_I420`
当前应用使用 `BGR_HWC` 处理 RGB 图片输入，使用 `YUV_I420` 处理原始帧输入。
`NV12` 和 `NV21` 目前仅为预留枚举值，在提供对应 kernel 前会被明确拒绝。

## 主要 API

- `RppYoloPreprocessor::init()`
  配置最大源图尺寸和模型输入尺寸。
- `RppYoloPreprocessor::run()`
  执行一次前处理，并写入模型输入 device buffer。
- `RppYoloPreprocessor::releaseSramBuffers()`
  在集成方需要提前归还内存时，显式释放持久化 SRAM workspace。
- `make_letterbox_info()`
  计算与后处理共享的几何信息。

## 复用说明

当模型使用静态 FP32 NCHW 外部输入，并要求 YOLOv5 风格 letterbox 与 `[0, 1]` 归一化时，可复用本模块。该实现是常规独立前处理阶段，仅使用本地 SDK runtime API 与源码 kernel，不使用 Graph capture 或 Graph 执行。
