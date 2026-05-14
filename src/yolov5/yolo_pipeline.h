/**
 * @file yolo_pipeline.h
 * @brief Reusable YOLOv5 pipeline built from preprocessing, inference, and postprocessing modules.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_PIPELINE_H
#define XDLTEK_SAMPLES_YOLOV5_PIPELINE_H

#include "detection_types.h"

#include <opencv2/core.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class RppInferEngine;
class RppYoloPostprocessor;
class RppYoloPreprocessor;

struct YoloV5PipelineOptions
{
    std::string model_path;
    int inference_count = 1;
    YoloV5Config config;
};

struct YoloV5StageTimes
{
    double input_h2d_ms = 0.0;
    size_t input_h2d_bytes = 0;
    double output_d2h_ms = 0.0;
    size_t output_d2h_bytes = 0;
    double preprocess_ms = 0.0;
    double inference_ms = 0.0;
    double postprocess_ms = 0.0;
    double end_to_end_ms = 0.0;
};

class YoloV5Pipeline
{
public:
    YoloV5Pipeline();
    ~YoloV5Pipeline();

    YoloV5Pipeline(const YoloV5Pipeline&) = delete;
    YoloV5Pipeline& operator=(const YoloV5Pipeline&) = delete;

    bool init(const YoloV5PipelineOptions& options, const std::vector<std::string>& class_names);

    bool runRgb(const cv::Mat& image,
                std::vector<Detection>& detections,
                YoloV5StageTimes* times = nullptr);

    bool runI420(const void* yuv_data,
                 size_t yuv_bytes,
                 int width,
                 int height,
                 std::vector<Detection>& detections,
                 YoloV5StageTimes* times = nullptr);

    const YoloV5Config& runtimeConfig() const { return config_; }

private:
    bool runInput(const void* data,
                  size_t bytes,
                  int width,
                  int height,
                  int row_stride_bytes,
                  int channels,
                  bool continuous,
                  int format,
                  std::vector<Detection>& detections,
                  YoloV5StageTimes* times);
    bool ensurePreprocessor(int width, int height);
    bool initPostprocessor(const std::vector<std::string>& class_names);

    std::unique_ptr<RppInferEngine> infer_engine_;
    std::unique_ptr<RppYoloPreprocessor> preprocessor_;
    std::unique_ptr<RppYoloPostprocessor> postprocessor_;

    YoloV5PipelineOptions options_;
    YoloV5Config config_;
    int preprocessor_width_ = 0;
    int preprocessor_height_ = 0;
    bool initialized_ = false;
};

#endif // XDLTEK_SAMPLES_YOLOV5_PIPELINE_H
