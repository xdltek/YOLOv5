/**
 * @file detection_types.h
 * @brief Shared YOLOv5 detection data structures and pipeline configuration.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_DETECTION_TYPES_H
#define XDLTEK_SAMPLES_YOLOV5_DETECTION_TYPES_H

#include <opencv2/core.hpp>

struct Detection
{
    // Index into class-name list (`coco80_class_labels()` for standard COCO models).
    int class_id = -1;
    // Confidence score of the retained detection.
    float confidence = 0.0f;
    // Bounding rectangle in original image coordinates.
    cv::Rect box;
};

struct LetterboxInfo
{
    int source_width = 0;
    int source_height = 0;
    int model_width = 0;
    int model_height = 0;
    int resized_width = 0;
    int resized_height = 0;
    float scale = 1.0f;
    float pad_x = 0.0f;
    float pad_y = 0.0f;
};

struct YoloV5Config
{
    int input_width = 640;
    int input_height = 640;
    // Candidate must pass class-score filtering before NMS.
    float score_threshold = 0.2f;
    // Overlap threshold used to suppress duplicate boxes.
    float nms_threshold = 0.4f;
    // Objectness threshold used before class-score evaluation.
    float confidence_threshold = 0.4f;
    // YOLOv5 default output row layout: [x, y, w, h, obj_conf, class_scores...].
    int output_dimensions = 85;
    int output_rows = 25200;
};

#endif // XDLTEK_SAMPLES_YOLOV5_DETECTION_TYPES_H
