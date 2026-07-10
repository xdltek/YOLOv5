/**
 * @file detection_types.h
 * @brief Shared YOLOv5 detection data structures and pipeline configuration.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_DETECTION_TYPES_H
#define XDLTEK_SAMPLES_YOLOV5_DETECTION_TYPES_H

#include <opencv2/core.hpp>

/**
 * @brief One YOLOv5 detection restored to original image coordinates.
 */
struct Detection
{
    // Index into class-name list (`coco80_class_labels()` for standard COCO models).
    int class_id = -1;
    // Confidence score of the retained detection.
    float confidence = 0.0f;
    // Bounding rectangle in original image coordinates.
    cv::Rect box;
};

/**
 * @brief Geometry metadata shared by preprocessing and postprocessing.
 */
struct LetterboxInfo
{
    // Width of the original source image or frame.
    int source_width = 0;
    // Height of the original source image or frame.
    int source_height = 0;
    // Width of the model input tensor.
    int model_width = 0;
    // Height of the model input tensor.
    int model_height = 0;
    // Width after aspect-ratio-preserving resize.
    int resized_width = 0;
    // Height after aspect-ratio-preserving resize.
    int resized_height = 0;
    // Scale applied from source coordinates to resized coordinates.
    float scale = 1.0f;
    // Horizontal padding inserted by letterbox.
    float pad_x = 0.0f;
    // Vertical padding inserted by letterbox.
    float pad_y = 0.0f;
};

/**
 * @brief Runtime parameters for a standard YOLOv5s-style detection model.
 */
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
