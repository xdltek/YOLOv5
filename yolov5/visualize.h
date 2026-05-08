/**
 * @file visualize.h
 * @brief YOLOv5 visualization helpers.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_VISUALIZE_H
#define XDLTEK_SAMPLES_YOLOV5_VISUALIZE_H

#include "detection_types.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

void draw_detections(cv::Mat& image, const std::vector<Detection>& detections, const std::vector<std::string>& class_names);
bool save_detection_image(const std::string& output_path, const cv::Mat& image);

#endif // XDLTEK_SAMPLES_YOLOV5_VISUALIZE_H
