<a id="english"></a>

# Utility Module

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This module contains shared helpers used by the applications, inference wrapper, and RPP modules.

## Responsibilities

- Logging wrappers.
- ONNX parser helper.
- RPP binding buffer management.
- Path and CLI argument helpers.
- Runtime error recording helpers used by OpenRT/RPP APIs.

## Reuse Notes

Keep utilities model-agnostic. Model-specific labels, preprocessing rules, and postprocessing rules should remain in the YOLOv5 or RPP model modules.

---

<a id="chinese"></a>

# 工具模块

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本模块包含应用、推理封装和 RPP 模块共用的辅助代码。

## 模块职责

- 日志封装。
- ONNX parser 辅助。
- RPP binding buffer 管理。
- 路径和命令行参数辅助。
- OpenRT/RPP API 使用的错误记录辅助。

## 复用说明

工具模块应保持模型无关。模型标签、前处理规则和后处理规则应保留在 YOLOv5 或 RPP 模型模块中。
