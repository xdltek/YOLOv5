<a id="english"></a>

# I420 YUV Frame YOLOv5 Demo

Language: [English](#english) | [中文](#chinese)

This demo runs one I420 YUV frame through the reusable YOLOv5 RPP pipeline per measured loop. It is intended for camera or video workflows where the input frame is already available as planar YUV data.

## What This Demo Covers

- Load a YOLOv5-compatible ONNX model.
- Read one I420 frame from a raw `.yuv` file.
- Use RPP preprocessing to convert and letterbox the YUV input for YOLOv5.
- Run one explicit warmup pass.
- Run measured inference loops and print stage-level latency.
- Convert the same YUV frame to BGR on the host only for drawing the final output image.
- Automatically save an `rpp_perf` trace when the project is built with `YOLO_ENABLE_RPP_PERF=ON`.

## Build

Build from the repository root:

```shell
rm -rf build
mkdir build
cd build
cmake ..
make -j8
```

The executable is generated at:

```text
build/bin/yolov5_yuv_frame_demo
```

## Run

Run from `build/bin`. The `--onnx` argument is required.

```shell
cd build/bin
./yolov5_yuv_frame_demo -o ../../onnx/yolov5s.onnx
```

The default input is a copied sample frame:

```text
../assets/test_0.yuv
```

The default frame size is `1280x720`.

To use another I420 frame:

```shell
./yolov5_yuv_frame_demo \
  -o /path/to/yolov5.onnx \
  --yuv /path/to/frame.i420 \
  --yuv-width 1280 \
  --yuv-height 720
```

## Arguments

```text
-o, --onnx       Required ONNX model path.
--yuv            One-frame I420 YUV file. Default: ../assets/test_0.yuv
--yuv-width      I420 frame width. Default: 1280
--yuv-height     I420 frame height. Default: 720
--output         Rendered output image path. Default: ../output/output_yuv.jpg
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
Measured loop: H2D -> YUV preprocess -> inference -> postprocess -> D2H
        |
        v
Draw boxes and save rendered image
```

## Output

The demo prints model and input paths first, then a timing summary:

```text
Model path: ../../onnx/yolov5s.onnx
Inference YUV path: ../assets/test_0.yuv

Summary:
Loop count: <loop>
Loop time: average per measured loop after warmup
Input H2D: <ms> ms, <bytes> bytes, <KiB> KiB
Preprocess: <ms> ms
Inference: <ms> ms
Postprocess: <ms> ms
Output D2H: <ms> ms, <bytes> bytes, <KiB> KiB
All time end to end: <ms> ms
FPS: <fps>
Output path: ../output/output_yuv.jpg
```

The rendered image is saved to `build/output/output_yuv.jpg` by default.

## Reuse Notes

Use this demo when your application receives raw I420 frames. The inference path calls `YoloV5Pipeline::runI420()` directly with the Y, U, and V planes packed in standard I420 order.

---

<a id="chinese"></a>

# I420 YUV 帧 YOLOv5 Demo

语言：[English](#english) | [中文](#chinese)

本 demo 在每次测量 loop 中，将一帧 I420 YUV 数据送入可复用的 YOLOv5 RPP pipeline。它适用于摄像头或视频流场景，此类场景中输入帧通常已经是 planar YUV 格式。

## 这个 Demo 包含什么

- 加载 YOLOv5 兼容 ONNX 模型。
- 从原始 `.yuv` 文件读取一帧 I420 数据。
- 使用 RPP 前处理完成 YUV 输入转换和 YOLOv5 letterbox。
- 显式执行一次完整 warmup。
- 执行指定次数的测量 loop，并打印阶段级耗时。
- 仅为了最终画框，在 host 上将同一帧 YUV 转为 BGR 可视化图片。
- 在开启 `YOLO_ENABLE_RPP_PERF=ON` 构建时，自动保存 `rpp_perf` trace。

## 构建

从仓库根目录执行：

```shell
rm -rf build
mkdir build
cd build
cmake ..
make -j8
```

可执行文件生成路径：

```text
build/bin/yolov5_yuv_frame_demo
```

## 运行

从 `build/bin` 目录运行。`--onnx` 参数是必需的。

```shell
cd build/bin
./yolov5_yuv_frame_demo -o ../../onnx/yolov5s.onnx
```

默认输入是构建时复制的示例帧：

```text
../assets/test_0.yuv
```

默认帧尺寸为 `1280x720`。

使用其他 I420 输入帧：

```shell
./yolov5_yuv_frame_demo \
  -o /path/to/yolov5.onnx \
  --yuv /path/to/frame.i420 \
  --yuv-width 1280 \
  --yuv-height 720
```

## 参数

```text
-o, --onnx       必需，ONNX 模型路径。
--yuv            单帧 I420 YUV 文件。默认：../assets/test_0.yuv
--yuv-width      I420 帧宽。默认：1280
--yuv-height     I420 帧高。默认：720
--output         绘制检测框后的输出图片路径。默认：../output/output_yuv.jpg
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
测量 loop：H2D -> YUV 前处理 -> 推理 -> 后处理 -> D2H
        |
        v
绘制检测框并保存输出图片
```

## 输出

demo 会优先打印模型路径和输入路径，然后打印汇总耗时：

```text
Model path: ../../onnx/yolov5s.onnx
Inference YUV path: ../assets/test_0.yuv

Summary:
Loop count: <loop>
Loop time: average per measured loop after warmup
Input H2D: <ms> ms, <bytes> bytes, <KiB> KiB
Preprocess: <ms> ms
Inference: <ms> ms
Postprocess: <ms> ms
Output D2H: <ms> ms, <bytes> bytes, <KiB> KiB
All time end to end: <ms> ms
FPS: <fps>
Output path: ../output/output_yuv.jpg
```

默认输出图片保存到 `build/output/output_yuv.jpg`。

## 复用说明

当应用侧接收原始 I420 帧时，可以参考这个 demo。推理路径会按照标准 I420 排布，将打包后的 Y、U、V plane 直接传给 `YoloV5Pipeline::runI420()`。
