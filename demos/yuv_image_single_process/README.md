# I420 YUV Image Single-Process YOLOv5 Demo

This demo runs one I420 YUV frame through the reusable YOLOv5 RPP pipeline in a single host process. It is intended for camera or video workflows where the input frame is already available as planar YUV data.

## What This Demo Shows

- Load a YOLOv5-compatible ONNX model.
- Read one I420 frame from a raw `.yuv` file.
- Use RPP preprocessing to convert and letterbox the YUV input for YOLOv5.
- Run one explicit warmup pass.
- Run measured inference loops and print customer-facing latency.
- Convert the same YUV frame to BGR on the host only for drawing the final output image.
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
build/bin/yolov5_yuv_image_demo
```

## Run

Run from `build/bin`. The `--onnx` argument is required.

```shell
cd build/bin
./yolov5_yuv_image_demo -o ../../onnx/yolov5s.onnx
```

The default input is a copied sample frame:

```text
../assets/test_0.yuv
```

The default frame size is `1280x720`.

To use another I420 frame:

```shell
./yolov5_yuv_image_demo \
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
-p, --perf       Save an rpp_perf trace JSON when the project is built with YOLO_ENABLE_RPP_PERF=ON.
                 In a non-perf build, the demo prints a warning and continues without trace capture.
--perf-dir       Trace output directory. Default: ../trace
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
Create optional perf trace session
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
