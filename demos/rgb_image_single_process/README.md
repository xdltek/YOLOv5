# RGB Image Single-Process YOLOv5 Demo

This demo runs one RGB/BGR image through the reusable YOLOv5 RPP pipeline in a single host process. It is a simple entry point for validating model loading, host-to-device input transfer, RPP preprocessing, RppRT inference, RPP postprocessing, device-to-host result transfer, and rendered detection output.

## What This Demo Covers

- Load a YOLOv5-compatible ONNX model.
- Read an image with OpenCV.
- Run one explicit warmup pass.
- Run measured inference loops and print stage-level latency.
- Draw detection boxes and save the output image.
- Optionally save an `rpp_perf` trace with `-p` or `--perf`.

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
build/bin/yolov5_rgb_image_demo
```

## Run

Run from `build/bin`. The `--onnx` argument is required.

```shell
cd build/bin
./yolov5_rgb_image_demo -o ../../onnx/yolov5s.onnx
```

The default input image is:

```text
../assets/test_1.png
```

To use another image:

```shell
./yolov5_rgb_image_demo \
  -o /path/to/yolov5.onnx \
  -i /path/to/image.jpg
```

## Arguments

```text
-o, --onnx      Required ONNX model path.
-i, --image     RGB/BGR image path. Default: ../assets/test_1.png
--output        Rendered output image path. Default: ../output/output_rgb.jpg
-l, --loop      Measured loop count after one warmup pass. Default: 1
-p, --perf      Save an rpp_perf trace JSON when the project is built with YOLO_ENABLE_RPP_PERF=ON.
                In a non-perf build, the demo prints a warning and continues without trace capture.
--perf-dir      Trace output directory. Default: ../trace
-v, --verbose   Enable verbose runtime logging.
```

## Execution Flow

```text
Read CLI and paths
        |
        v
Load RGB/BGR image with OpenCV
        |
        v
Create optional perf trace session
        |
        v
Initialize YoloV5Pipeline and RppInferEngine
        |
        v
Warmup: run full pipeline once
        |
        v
Measured loop: H2D -> preprocess -> inference -> postprocess -> D2H
        |
        v
Draw boxes and save rendered image
```

## Output

The demo prints model and input paths first, then a timing summary:

```text
Model path: ../../onnx/yolov5s.onnx
Inference image path: ../assets/test_1.png

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
Output path: ../output/output_rgb.jpg
```

The rendered image is saved to `build/output/output_rgb.jpg` by default.

## Reuse Notes

Use this demo when your application already receives compressed or decoded image files on the host. For new applications, copy the CLI pattern and call `YoloV5Pipeline::runRgb()` with your own `cv::Mat`.

---

# RGB 图片单进程 YOLOv5 Demo

本 demo 在单个 host 进程中，将一张 RGB/BGR 图片送入可复用的 YOLOv5 RPP pipeline。它是一个简单的执行入口，可用于验证模型加载、输入 H2D、RPP 前处理、RppRT 推理、RPP 后处理、结果 D2H 以及检测框绘制输出。

## 这个 Demo 包含什么

- 加载 YOLOv5 兼容 ONNX 模型。
- 使用 OpenCV 读取图片。
- 显式执行一次完整 warmup。
- 执行指定次数的测量 loop，并打印阶段级耗时。
- 绘制检测框并保存输出图片。
- 在开启 `YOLO_ENABLE_RPP_PERF=ON` 构建时，可通过 `-p` 或 `--perf` 保存 `rpp_perf` trace。

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
build/bin/yolov5_rgb_image_demo
```

## 运行

从 `build/bin` 目录运行。`--onnx` 参数是必需的。

```shell
cd build/bin
./yolov5_rgb_image_demo -o ../../onnx/yolov5s.onnx
```

默认输入图片为：

```text
../assets/test_1.png
```

使用其他图片：

```shell
./yolov5_rgb_image_demo \
  -o /path/to/yolov5.onnx \
  -i /path/to/image.jpg
```

## 参数

```text
-o, --onnx      必需，ONNX 模型路径。
-i, --image     RGB/BGR 图片路径。默认：../assets/test_1.png
--output        绘制检测框后的输出图片路径。默认：../output/output_rgb.jpg
-l, --loop      warmup 之后的测量 loop 次数。默认：1
-p, --perf      在项目使用 YOLO_ENABLE_RPP_PERF=ON 构建时保存 rpp_perf trace JSON。
                非 perf 构建中使用该参数时，demo 会打印提示并继续执行，不保存 trace。
--perf-dir      trace 输出目录。默认：../trace
-v, --verbose   开启更详细的运行时日志。
```

## 执行流程

```text
读取命令行参数和路径
        |
        v
使用 OpenCV 加载 RGB/BGR 图片
        |
        v
创建可选 perf trace 会话
        |
        v
初始化 YoloV5Pipeline 和 RppInferEngine
        |
        v
Warmup：完整执行一次 pipeline
        |
        v
测量 loop：H2D -> 前处理 -> 推理 -> 后处理 -> D2H
        |
        v
绘制检测框并保存输出图片
```

## 输出

demo 会优先打印模型路径和输入路径，然后打印汇总耗时：

```text
Model path: ../../onnx/yolov5s.onnx
Inference image path: ../assets/test_1.png

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
Output path: ../output/output_rgb.jpg
```

默认输出图片保存到 `build/output/output_rgb.jpg`。

## 复用说明

当应用侧已经在 host 上拿到压缩图片或解码后的图片时，可以参考这个 demo。新应用可以复用这里的命令行组织方式，并使用自己的 `cv::Mat` 调用 `YoloV5Pipeline::runRgb()`。
