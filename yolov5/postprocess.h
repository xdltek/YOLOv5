/**
 * @file postprocess.h
 * @brief YOLOv5 output decoding and NMS interfaces.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_POSTPROCESS_H
#define XDLTEK_SAMPLES_YOLOV5_POSTPROCESS_H

#include "detection_types.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

void nms_boxes(const std::vector<cv::Rect>& bboxes,
               const std::vector<float>& scores,
               float score_threshold,
               float nms_threshold,
               std::vector<int>& indices,
               float eta = 1.0f,
               int top_k = 0);

bool decode_yolov5_output(const std::vector<float>& output_data,
                          const cv::Size& padded_image_size,
                          const std::vector<std::string>& class_names,
                          const YoloV5Config& config,
                          std::vector<Detection>& detections,
                          std::string* error);

#endif // XDLTEK_SAMPLES_YOLOV5_POSTPROCESS_H
