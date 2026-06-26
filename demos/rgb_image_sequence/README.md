<a id="english"></a>

# RGB Image Sequence YOLOv5 Demo

Language: [English](#english) | [中文](#chinese)

This demo runs multiple RGB/BGR images sequentially in one process through the reusable YOLOv5 RPP pipeline. Each pipeline call still processes one image at a time; this demo does not require or imply a multi-batch YOLO model.

## What This Demo Covers

- Load a YOLOv5-compatible ONNX model once.
- Collect images from one directory or from a text list.
- Run one explicit warmup pass with the first image.
- Reuse the same `YoloV5Pipeline` for all measured images.
- Draw detection boxes and save one rendered output image per input image.
- Print average stage-level latency across all measured image runs.
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
build/bin/yolov5_rgb_image_sequence_demo
```

## Run

Run from `build/bin`. The `--onnx` argument is required.

```shell
cd build/bin
./yolov5_rgb_image_sequence_demo \
  -o ../../onnx/yolov5s.onnx \
  --image-dir ../assets \
  --output-dir ../output/rgb_image_sequence
```

To use a text file:

```shell
./yolov5_rgb_image_sequence_demo \
  -o /path/to/yolov5.onnx \
  --image-list /path/to/images.txt \
  --output-dir ../output/rgb_image_sequence
```

## Arguments

```text
-o, --onnx        Required ONNX model path.
--image-dir       Directory containing RGB/BGR images. Default: ../assets
--image-list      Text file containing one image path per line.
--output-dir      Directory for rendered output images. Default: ../output/rgb_image_sequence
-l, --loop        Measured loop count per image after one warmup pass. Default: 1
-v, --verbose     Enable verbose runtime logging.
```

If `--image-list` is provided, it takes precedence over `--image-dir`. Directory scanning is non-recursive and accepts `.jpg`, `.jpeg`, `.png`, and `.bmp`.

## Execution Flow

```text
Read CLI and paths
        |
        v
Collect image sequence from directory or list
        |
        v
Create perf trace session in perf-enabled builds
        |
        v
Initialize YoloV5Pipeline and RppInferEngine once
        |
        v
Warmup: run full pipeline once on the first image
        |
        v
For each image:
  load image -> H2D -> preprocess -> inference -> postprocess -> D2H
        |
        v
Draw boxes, save rendered images, and print average latency
```

## Output

The demo prints model and input source paths first, then a timing summary:

```text
Model path: ../../onnx/yolov5s.onnx
Inference image source: ../assets
Image count: <images>
Output dir: ../output/rgb_image_sequence

Summary:
Image count: <images>
Loop count per image: <loop>
Measured frames: <images * loop>
Loop time: average per measured image run after warmup
Input H2D: <ms> ms, <bytes> bytes, <KiB> KiB
Preprocess: <ms> ms
Inference: <ms> ms
Postprocess: <ms> ms
Output D2H: <ms> ms, <bytes> bytes, <KiB> KiB
All time end to end: <ms> ms
FPS: <fps>
Output dir: ../output/rgb_image_sequence
```

Each rendered output image is saved as:

```text
<sequence_index>_<input_stem>_detect.jpg
```

## Reuse Notes

Use this demo when one process needs to reuse the same YOLOv5 pipeline for a sequence of host-side image files. Camera or live-stream inputs can reuse `YoloV5Pipeline`, but their frame acquisition, queueing, and buffer lifetime should be handled in a separate input adapter.

---

<a id="chinese"></a>

# RGB 图片序列 YOLOv5 Demo

语言：[English](#english) | [中文](#chinese)

本 demo 在一个进程内按顺序处理多张 RGB/BGR 图片，并复用同一个 YOLOv5 RPP pipeline。每次 pipeline 调用仍然只处理一张图片；本 demo 不表示也不要求 YOLO 模型使用多 batch 推理。

## 这个 Demo 包含什么

- 加载一次 YOLOv5 兼容 ONNX 模型。
- 从目录或文本列表收集输入图片。
- 使用第一张图片显式执行一次完整 warmup。
- 对所有测量图片复用同一个 `YoloV5Pipeline`。
- 为每张输入图片绘制检测框并保存一张输出图片。
- 打印所有测量图片执行过程的阶段级平均耗时。
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
build/bin/yolov5_rgb_image_sequence_demo
```

## 运行

从 `build/bin` 目录运行。`--onnx` 参数是必需的。

```shell
cd build/bin
./yolov5_rgb_image_sequence_demo \
  -o ../../onnx/yolov5s.onnx \
  --image-dir ../assets \
  --output-dir ../output/rgb_image_sequence
```

使用文本列表：

```shell
./yolov5_rgb_image_sequence_demo \
  -o /path/to/yolov5.onnx \
  --image-list /path/to/images.txt \
  --output-dir ../output/rgb_image_sequence
```

## 参数

```text
-o, --onnx        必需，ONNX 模型路径。
--image-dir       RGB/BGR 图片目录。默认：../assets
--image-list      文本文件路径，每行一个图片路径。
--output-dir      绘制检测框后的输出目录。默认：../output/rgb_image_sequence
-l, --loop        warmup 之后每张图片的测量 loop 次数。默认：1
-v, --verbose     开启更详细的运行时日志。
```

如果提供 `--image-list`，会优先使用列表输入而不是 `--image-dir`。目录扫描不递归，支持 `.jpg`、`.jpeg`、`.png` 和 `.bmp`。

## 执行流程

```text
读取命令行参数和路径
        |
        v
从目录或列表收集图片序列
        |
        v
perf 构建中创建 trace 会话
        |
        v
初始化一次 YoloV5Pipeline 和 RppInferEngine
        |
        v
Warmup：使用第一张图片完整执行一次 pipeline
        |
        v
逐张图片执行：
  加载图片 -> H2D -> 前处理 -> 推理 -> 后处理 -> D2H
        |
        v
绘制检测框、保存输出图片并打印平均耗时
```

## 输出

demo 会优先打印模型路径和输入源路径，然后打印汇总耗时：

```text
Model path: ../../onnx/yolov5s.onnx
Inference image source: ../assets
Image count: <images>
Output dir: ../output/rgb_image_sequence

Summary:
Image count: <images>
Loop count per image: <loop>
Measured frames: <images * loop>
Loop time: average per measured image run after warmup
Input H2D: <ms> ms, <bytes> bytes, <KiB> KiB
Preprocess: <ms> ms
Inference: <ms> ms
Postprocess: <ms> ms
Output D2H: <ms> ms, <bytes> bytes, <KiB> KiB
All time end to end: <ms> ms
FPS: <fps>
Output dir: ../output/rgb_image_sequence
```

每张输出图片的命名方式为：

```text
<序号>_<输入文件名>_detect.jpg
```

## 复用说明

当一个进程需要对一组 host 侧图片文件连续推理时，可以参考这个 demo。摄像头或实时流输入也可以复用 `YoloV5Pipeline`，但帧采集、队列策略和 buffer 生命周期建议由独立的输入适配层处理。
