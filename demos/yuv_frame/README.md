<a id="english"></a>

# YOLOv5 I420 YUV Frame Application

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This application runs one I420 YUV frame through the reusable YOLOv5 RPP pipeline per measured loop. It is designed for camera or video workflows where the input frame is already available as planar YUV data.

## Application Scope

- Load a validated classic YOLOv5n/s/m/l/x ONNX model.
- Read one I420 frame from a raw `.yuv` file.
- Use RPP preprocessing to convert and letterbox the YUV input for YOLOv5.
- Run one explicit warmup pass.
- Run measured inference loops and print stage-level latency.
- Convert the same YUV frame to BGR on the host only for drawing the final output image.
- Automatically save an `rpp_perf` trace when the project is built with `YOLO_ENABLE_RPP_PERF=ON`.

## Build

Build from the repository root with the standard script:

```shell
scripts/build.sh
```

The executable is generated at:

```text
build/bin/yolov5_yuv_frame_demo
```

## Run

Run the YUV application from the repository root with the standard script:

```shell
scripts/run.sh -m /path/to/yolov5s.onnx --demo yuv
```

Run a measured benchmark for the YUV workload:

```shell
scripts/benchmark.sh -m /path/to/yolov5s.onnx --demo yuv --loop 20
```

The release package does not include an ONNX model, so provide one with `-m/--model`. If the option is omitted, the script uses `weights/yolov5s.onnx` only when the user has placed that file there; otherwise it reports the missing model and exits. The default input, frame size, and output are `build/assets/test_0.yuv`, `1280x720`, and `build/output/output_yuv.jpg`.

To use another I420 frame through the script:

```shell
scripts/run.sh --demo yuv \
  -m /path/to/yolov5s.onnx \
  --input /path/to/frame.i420 \
  --yuv-width 1280 \
  --yuv-height 720
```

After building, the binary can also be executed directly:

```shell
build/bin/yolov5_yuv_frame_demo \
  -m /path/to/yolov5s.onnx \
  -i build/assets/test_0.yuv \
  --yuv-width 1280 \
  --yuv-height 720 \
  -o build/output/output_yuv.jpg \
  -l 1
```

## Arguments

```text
-m, --model      ONNX model path. Required.
-i, --input      One-frame I420 YUV file. Default: ../assets/test_0.yuv
--yuv-width      I420 frame width. Default: 1280
--yuv-height     I420 frame height. Default: 720
-o, --output     Rendered output image path. Default: ../output/output_yuv.jpg
-l, --loop       Measured loop count after one warmup pass. Default: 1
-v, --verbose    Enable verbose runtime logging.
```

## Execution Flow

```text
Read CLI and paths
        |
        v
Read one I420 frame from disk
        |
        v
Create BGR visualization frame on host
        |
        v
Create perf trace session in perf-enabled builds
        |
        v
Initialize YoloV5Pipeline and RppInferEngine
        |
        v
Warmup: run full pipeline once
        |
        v
Printed stages: H2D, YUV preprocess, inference, postprocess, D2H
        |
        v
Draw boxes and save rendered image
```

## Output

The application prints model and input paths first, then a timing summary:

```text
Model path: /path/to/yolov5s.onnx
Inference YUV path: ../assets/test_0.yuv

Summary:
Loop count: <loop>
Loop time: average per measured loop after warmup
Input H2D: <ms> ms, <bytes> bytes, <KiB> KiB
Preprocess: <ms> ms
Inference: <ms> ms
Postprocess: <ms> ms
Output D2H: <ms> ms, <bytes> bytes, <KiB> KiB
All time end to end: <ms> ms (preprocess+inference+postprocess)
End-to-end FPS: <fps>
OpenRT inference FPS: <fps>
Output path: ../output/output_yuv.jpg
```

`End-to-end FPS` is calculated from the reported preprocess + inference + postprocess latency. `OpenRT inference FPS` is calculated from the standalone OpenRT engine execution latency. Both are emphasized at the top of the performance report. `Input H2D` and `Output D2H` remain available as separate transfer fields.

The rendered image is saved to `build/output/output_yuv.jpg` by default.

## Related Documents

Click a document path below to jump to the corresponding guide:

- [Project API documentation](../../docs/api/README.md): reusable API index.
- [RPP acceleration modules](../../rpp/README.md): RPP execution layer guide.

## Reuse Notes

Use this workload when the integrating application receives raw I420 frames. The inference path calls `YoloV5Pipeline::runI420()` directly with the Y, U, and V planes packed in standard I420 order.

---

<a id="chinese"></a>

# YOLOv5 I420 YUV 帧应用

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本应用在每次测量 loop 中，将一帧 I420 YUV 数据送入可复用的 YOLOv5 RPP pipeline，适用于输入帧已经是 planar YUV 数据的摄像头或视频流场景。

## 应用范围

- 加载本版本验证的经典 YOLOv5n/s/m/l/x ONNX 模型。
- 从原始 `.yuv` 文件读取一帧 I420 数据。
- 使用 RPP 前处理完成 YUV 输入转换和 YOLOv5 letterbox。
- 显式执行一次完整 warmup。
- 执行指定次数的测量 loop，并打印阶段级耗时。
- 仅为了最终画框，在 host 上将同一帧 YUV 转为 BGR 可视化图片。
- 在开启 `YOLO_ENABLE_RPP_PERF=ON` 构建时，自动保存 `rpp_perf` trace。

## 构建

从仓库根目录使用标准脚本构建：

```shell
scripts/build.sh
```

可执行文件生成路径：

```text
build/bin/yolov5_yuv_frame_demo
```

## 运行

从仓库根目录使用标准脚本运行 YUV 应用：

```shell
scripts/run.sh -m /path/to/yolov5s.onnx --demo yuv
```

执行 YUV 工作负载 benchmark：

```shell
scripts/benchmark.sh -m /path/to/yolov5s.onnx --demo yuv --loop 20
```

发布包不包含 ONNX 模型，请通过 `-m/--model` 指定模型。如果省略该参数，脚本仅在用户已放置 `weights/yolov5s.onnx` 时使用该文件，否则会提示模型缺失并退出。默认输入、帧尺寸和输出路径分别为 `build/assets/test_0.yuv`、`1280x720` 和 `build/output/output_yuv.jpg`。

通过脚本使用其他 I420 输入帧：

```shell
scripts/run.sh --demo yuv \
  -m /path/to/yolov5s.onnx \
  --input /path/to/frame.i420 \
  --yuv-width 1280 \
  --yuv-height 720
```

构建完成后，也可以直接执行二进制：

```shell
build/bin/yolov5_yuv_frame_demo \
  -m /path/to/yolov5s.onnx \
  -i build/assets/test_0.yuv \
  --yuv-width 1280 \
  --yuv-height 720 \
  -o build/output/output_yuv.jpg \
  -l 1
```

## 参数

```text
-m, --model      ONNX 模型路径。必需。
-i, --input      单帧 I420 YUV 文件。默认：../assets/test_0.yuv
--yuv-width      I420 帧宽。默认：1280
--yuv-height     I420 帧高。默认：720
-o, --output     绘制检测框后的输出图片路径。默认：../output/output_yuv.jpg
-l, --loop       warmup 之后的测量 loop 次数。默认：1
-v, --verbose    开启更详细的运行时日志。
```

## 执行流程

```text
读取命令行参数和路径
        |
        v
从磁盘读取一帧 I420 数据
        |
        v
在 host 上生成用于画框的 BGR 可视化帧
        |
        v
perf 构建中创建 trace 会话
        |
        v
初始化 YoloV5Pipeline 和 RppInferEngine
        |
        v
Warmup：完整执行一次 pipeline
        |
        v
打印阶段：H2D、YUV 前处理、推理、后处理、D2H
        |
        v
绘制检测框并保存输出图片
```

## 输出

应用会优先打印模型路径和输入路径，然后打印汇总耗时：

```text
Model path: /path/to/yolov5s.onnx
Inference YUV path: ../assets/test_0.yuv

Summary:
Loop count: <loop>
Loop time: average per measured loop after warmup
Input H2D: <ms> ms, <bytes> bytes, <KiB> KiB
Preprocess: <ms> ms
Inference: <ms> ms
Postprocess: <ms> ms
Output D2H: <ms> ms, <bytes> bytes, <KiB> KiB
All time end to end: <ms> ms (preprocess+inference+postprocess)
End-to-end FPS: <fps>
OpenRT inference FPS: <fps>
Output path: ../output/output_yuv.jpg
```

`End-to-end FPS` 根据前处理 + 推理 + 后处理耗时计算，`OpenRT inference FPS` 根据单独的 OpenRT engine 执行耗时计算。两项指标会在性能报告顶部重点展示；`Input H2D` 和 `Output D2H` 仍作为独立传输指标打印。

默认输出图片保存到 `build/output/output_yuv.jpg`。

## 相关文档

点击下方文档路径可跳转到对应说明：

- [项目 API 文档](../../docs/api/README.md)：可复用 API 索引。
- [RPP 加速模块](../../rpp/README.md)：RPP 执行层说明。

## 复用说明

当集成方接收原始 I420 帧时，可使用该工作负载。推理路径会按照标准 I420 排布，将打包后的 Y、U、V plane 直接传给 `YoloV5Pipeline::runI420()`。
