/**
 * @file preprocess.h
 * @brief YOLOv5 preprocessing interfaces.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_PREPROCESS_H
#define XDLTEK_SAMPLES_YOLOV5_PREPROCESS_H

#include "detection_types.h"

#include <opencv2/core.hpp>

#include <string>

struct YoloPreprocessResult
{
    cv::Mat padded_image;
    cv::Mat input_blob;
};

cv::Mat format_yolov5(const cv::Mat& source);

cv::Mat blob_from_image(cv::InputArray image,
                        double scalefactor = 1.0,
                        const cv::Size& size = cv::Size(),
                        const cv::Scalar& mean = cv::Scalar(),
                        bool swapRB = false,
                        bool crop = false,
                        int ddepth = CV_32F);

bool preprocess_yolov5(const cv::Mat& image, const YoloV5Config& config, YoloPreprocessResult& result, std::string* error);

#endif // XDLTEK_SAMPLES_YOLOV5_PREPROCESS_H
