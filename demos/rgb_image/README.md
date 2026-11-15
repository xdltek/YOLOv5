<a id="english"></a>

# YOLOv5 RGB Image Application

Language switch: click [English](#english) or [中文](#chinese) to jump to the corresponding section.

This application runs one RGB/BGR image through the reusable YOLOv5 RPP pipeline per measured loop. It provides a ready-to-use entry point for model loading, host-to-device input transfer, RPP preprocessing, OpenRT inference, RPP postprocessing, device-to-host result transfer, and rendered detection output.

## Application Scope

- Load a validated classic YOLOv5n/s/m/l/x ONNX model.
- Read an image with OpenCV.
- Run one explicit warmup pass.
- Run measured inference loops and print stage-level latency.
- Draw detection boxes and save the output image.
- Automatically save an `rpp_perf` trace when the project is built with `YOLO_ENABLE_RPP_PERF=ON`.

## Build

Build from the repository root with the standard script:

```shell
scripts/build.sh
```

The executable is generated at:

```text
build/bin/yolov5_rgb_image_demo
```

## Run

Run the RGB application from the repository root with the standard script:

```shell
scripts/run.sh -m /path/to/yolov5s.onnx --demo rgb
```

Run a measured benchmark for the RGB workload:

```shell
scripts/benchmark.sh -m /path/to/yolov5s.onnx --demo rgb --loop 20
```

The release package does not include an ONNX model, so provide one with `-m/--model`. If the option is omitted, the script uses `weights/yolov5s.onnx` only when the user has placed that file there; otherwise it reports the missing model and exits. The default input and output paths are `build/assets/test_1.png` and `build/output/output_rgb.jpg`.

To use another image through the script:

```shell
scripts/run.sh -m /path/to/yolov5s.onnx --demo rgb --input /path/to/image.jpg
```

After building, the binary can also be executed directly:

```shell
build/bin/yolov5_rgb_image_demo \
  -m /path/to/yolov5s.onnx \
  -i build/assets/test_1.png \
  -o build/output/output_rgb.jpg \
  -l 1
```

## Arguments

```text
-m, --model     ONNX model path. Required.
-i, --input     RGB/BGR image path. Default: ../assets/test_1.png
-o, --output    Rendered output image path. Default: ../output/output_rgb.jpg
-l, --loop      Measured loop count after one warmup pass. Default: 1
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
Create perf trace session in perf-enabled builds
        |
        v
Initialize YoloV5Pipeline and RppInferEngine
        |
        v
Warmup: run full pipeline once
        |
        v
Printed stages: H2D, preprocess, inference, postprocess, D2H
        |
        v
Draw boxes and save rendered image
```

## Output

The application prints model and input paths first, then a timing summary:

```text
Model path: /path/to/yolov5s.onnx
Inference image path: ../assets/test_1.png

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
Output path: ../output/output_rgb.jpg
```

`End-to-end FPS` is calculated from the reported preprocess + inference + postprocess latency. `OpenRT inference FPS` is calculated from the standalone OpenRT engine execution latency. Both are emphasized at the top of the performance report. `Input H2D` and `Output D2H` remain available as separate transfer fields.

The rendered image is saved to `build/output/output_rgb.jpg` by default.

## Related Documents

Click a document path below to jump to the corresponding guide:

- [Project API documentation](../../docs/api/README.md): reusable API index.
- [RPP acceleration modules](../../rpp/README.md): RPP execution layer guide.

## Reuse Notes

Use this workload when the integrating application already receives compressed or decoded image files on the host. Call `YoloV5Pipeline::runRgb()` with the target `cv::Mat` when integrating the pipeline.

---

<a id="chinese"></a>

# YOLOv5 RGB 图片应用

语言切换：点击 [English](#english) 或 [中文](#chinese) 跳转到对应语言章节。

本应用在每次测量 loop 中，将一张 RGB/BGR 图片送入可复用的 YOLOv5 RPP pipeline，提供模型加载、输入 H2D、RPP 前处理、OpenRT 推理、RPP 后处理、结果 D2H 以及检测框绘制输出的完整执行入口。

## 应用范围

- 加载本版本验证的经典 YOLOv5n/s/m/l/x ONNX 模型。
- 使用 OpenCV 读取图片。
- 显式执行一次完整 warmup。
- 执行指定次数的测量 loop，并打印阶段级耗时。
- 绘制检测框并保存输出图片。
- 在开启 `YOLO_ENABLE_RPP_PERF=ON` 构建时，自动保存 `rpp_perf` trace。

## 构建

从仓库根目录使用标准脚本构建：

```shell
scripts/build.sh
```

可执行文件生成路径：

```text
build/bin/yolov5_rgb_image_demo
```

## 运行

从仓库根目录使用标准脚本运行 RGB 应用：

```shell
scripts/run.sh -m /path/to/yolov5s.onnx --demo rgb
```

执行 RGB 工作负载 benchmark：

```shell
scripts/benchmark.sh -m /path/to/yolov5s.onnx --demo rgb --loop 20
```

发布包不包含 ONNX 模型，请通过 `-m/--model` 指定模型。如果省略该参数，脚本仅在用户已放置 `weights/yolov5s.onnx` 时使用该文件，否则会提示模型缺失并退出。默认输入和输出路径分别为 `build/assets/test_1.png` 和 `build/output/output_rgb.jpg`。

通过脚本使用其他图片：

```shell
scripts/run.sh -m /path/to/yolov5s.onnx --demo rgb --input /path/to/image.jpg
```

构建完成后，也可以直接执行二进制：

```shell
build/bin/yolov5_rgb_image_demo \
  -m /path/to/yolov5s.onnx \
  -i build/assets/test_1.png \
  -o build/output/output_rgb.jpg \
  -l 1
```

## 参数

```text
-m, --model     ONNX 模型路径。必需。
-i, --input     RGB/BGR 图片路径。默认：../assets/test_1.png
-o, --output    绘制检测框后的输出图片路径。默认：../output/output_rgb.jpg
-l, --loop      warmup 之后的测量 loop 次数。默认：1
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
perf 构建中创建 trace 会话
        |
        v
初始化 YoloV5Pipeline 和 RppInferEngine
        |
        v
Warmup：完整执行一次 pipeline
        |
        v
打印阶段：H2D、前处理、推理、后处理、D2H
        |
        v
绘制检测框并保存输出图片
```

## 输出

应用会优先打印模型路径和输入路径，然后打印汇总耗时：

```text
Model path: /path/to/yolov5s.onnx
Inference image path: ../assets/test_1.png

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
Output path: ../output/output_rgb.jpg
```

`End-to-end FPS` 根据前处理 + 推理 + 后处理耗时计算，`OpenRT inference FPS` 根据单独的 OpenRT engine 执行耗时计算。两项指标会在性能报告顶部重点展示；`Input H2D` 和 `Output D2H` 仍作为独立传输指标打印。

默认输出图片保存到 `build/output/output_rgb.jpg`。

## 相关文档

点击下方文档路径可跳转到对应说明：

- [项目 API 文档](../../docs/api/README.md)：可复用 API 索引。
- [RPP 加速模块](../../rpp/README.md)：RPP 执行层说明。

## 复用说明

当集成方已经在 host 上获得压缩图片或解码后的图片时，可使用该工作负载。集成 pipeline 时，使用目标 `cv::Mat` 调用 `YoloV5Pipeline::runRgb()`。
