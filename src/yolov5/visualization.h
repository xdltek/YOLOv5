/**
 * @file visualization.h
 * @brief YOLOv5 visualization helpers.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_VISUALIZATION_H
#define XDLTEK_SAMPLES_YOLOV5_VISUALIZATION_H

#include "detection_types.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

/**
 * @brief Draw retained detections on a host BGR image.
 * @param image Image modified in place.
 * @param detections Detection boxes restored to source coordinates.
 * @param class_names Class-name table used for overlay labels.
 */
void draw_detections(cv::Mat& image, const std::vector<Detection>& detections, const std::vector<std::string>& class_names);

/**
 * @brief Save the rendered detection image to disk.
 * @param output_path Destination image path.
 * @param image Host image to encode.
 */
bool save_detection_image(const std::string& output_path, const cv::Mat& image);

#endif // XDLTEK_SAMPLES_YOLOV5_VISUALIZATION_H
