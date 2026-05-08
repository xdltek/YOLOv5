/**
 * @file labels.h
 * @brief YOLOv5 class label helpers.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_LABELS_H
#define XDLTEK_SAMPLES_YOLOV5_LABELS_H

#include <string>
#include <vector>

const std::vector<std::string>& coco80_class_labels();
std::string detection_class_label(int class_id, const std::vector<std::string>& names);

#endif // XDLTEK_SAMPLES_YOLOV5_LABELS_H
