<a id="english"></a>

# RPP Preprocess Module

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This module prepares YOLOv5 model input tensors on RPP.

## Responsibilities

- Validate host or device input metadata.
- Copy host input through a pinned host staging buffer when needed.
- Compute YOLO letterbox scale and padding.
- Convert input format, resize, and normalize into NCHW model layout.
- Report H2D and preprocessing timings.

## Supported Formats

- `BGR_HWC`
- `RGB_HWC`
- `RGB_CHW`
- `YUV_I420`
- `NV12`
- `NV21`

The current demos use `BGR_HWC` for RGB image input and `YUV_I420` for raw frame input.

## Main API

- `RppYoloPreprocessor::init()`
  Configures maximum source size and model input shape.
- `RppYoloPreprocessor::run()`
  Runs one preprocessing pass into the model input device buffer.
- `RppYoloPreprocessor::releaseSramBuffers()`
  Releases temporary SRAM workspaces after a pipeline pass.
- `make_letterbox_info()`
  Computes geometry metadata shared with postprocessing.

## Reuse Notes

Use this module when the target model expects YOLOv5-style letterbox and normalized NCHW input. Keep model labels, detection parsing, and visualization outside preprocessing.

---

<a id="chinese"></a>

# RPP 前处理模块

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本模块在 RPP 上准备 YOLOv5 模型输入 tensor。

## 模块职责

- 校验 host 或 device 输入元数据。
- 在需要时通过页锁定 Host 中转缓冲区复制输入。
- 计算 YOLO letterbox 缩放比例和 padding。
- 完成格式转换、resize、归一化，并写入 NCHW 模型输入布局。
- 上报 H2D 和前处理耗时。

## 支持格式

- `BGR_HWC`
- `RGB_HWC`
- `RGB_CHW`
- `YUV_I420`
- `NV12`
- `NV21`

当前 demo 使用 `BGR_HWC` 处理 RGB 图片输入，使用 `YUV_I420` 处理原始帧输入。

## 主要 API

- `RppYoloPreprocessor::init()`
  配置最大源图尺寸和模型输入尺寸。
- `RppYoloPreprocessor::run()`
  执行一次前处理，并写入模型输入 device buffer。
- `RppYoloPreprocessor::releaseSramBuffers()`
  在一次 pipeline 完成后释放临时 SRAM workspace。
- `make_letterbox_info()`
  计算与后处理共享的几何信息。

## 复用说明

当目标模型使用 YOLOv5 风格 letterbox 和归一化 NCHW 输入时，可复用本模块。模型标签、检测解析和可视化逻辑应放在前处理模块之外。
