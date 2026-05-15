# RGB Image Single-Process YOLOv5 Demo

This demo runs one RGB/BGR image through the reusable YOLOv5 RPP pipeline in a single host process. It is the simplest customer-facing entry point for validating model loading, host-to-device input transfer, RPP preprocessing, RppRT inference, RPP postprocessing, device-to-host result transfer, and rendered detection output.

## What This Demo Shows

- Load a YOLOv5-compatible ONNX model.
- Read an image with OpenCV.
- Run one explicit warmup pass.
- Run measured inference loops and print customer-facing latency.
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
