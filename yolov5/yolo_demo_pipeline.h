/**
 * @file yolo_demo_pipeline.h
 * @brief High-level YOLOv5 demo pipeline.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_DEMO_PIPELINE_H
#define XDLTEK_SAMPLES_YOLOV5_DEMO_PIPELINE_H

#include "detection_types.h"

#include <opencv2/core.hpp>

#include <cstddef>
#include <string>
#include <vector>

struct YoloV5PipelineOptions
{
    std::string model_path;
    int inference_count = 1;
    YoloV5Config config;
};

bool detect_yolov5(const cv::Mat& image,
                   const YoloV5PipelineOptions& options,
                   const std::vector<std::string>& class_names,
                   std::vector<Detection>& detections);

bool detect_yolov5_i420(const void* yuv_data,
                        size_t yuv_bytes,
                        int width,
                        int height,
                        const YoloV5PipelineOptions& options,
                        const std::vector<std::string>& class_names,
                        std::vector<Detection>& detections);

#endif // XDLTEK_SAMPLES_YOLOV5_DEMO_PIPELINE_H
