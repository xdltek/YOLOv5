/**
 * @file labels.h
 * @brief YOLOv5 class label helpers.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_LABELS_H
#define XDLTEK_SAMPLES_YOLOV5_LABELS_H

#include <string>
#include <vector>

/**
 * @brief Return the COCO 80-class label table used by standard YOLOv5 models.
 */
const std::vector<std::string>& coco80_class_labels();

/**
 * @brief Resolve a class id to display text for rendered detections.
 * @param class_id Detection class id.
 * @param names Class-name table supplied by the demo.
 */
std::string detection_class_label(int class_id, const std::vector<std::string>& names);

#endif // XDLTEK_SAMPLES_YOLOV5_LABELS_H
