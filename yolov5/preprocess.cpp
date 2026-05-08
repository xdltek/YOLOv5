/**
 * @file preprocess.cpp
 * @brief YOLOv5 preprocessing implementations.
 */
#include "preprocess.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <vector>

cv::Mat format_yolov5(const cv::Mat& source)
{
    int col = source.cols;
    int row = source.rows;
    int max_size = std::max(col, row);
    cv::Mat result = cv::Mat::zeros(max_size, max_size, CV_8UC3);
    source.copyTo(result(cv::Rect(0, 0, col, row)));
    return result;
}

cv::Mat blob_from_image(cv::InputArray image,
                        double scalefactor,
                        const cv::Size& size,
                        const cv::Scalar& mean,
                        bool swapRB,
                        bool crop,
                        int ddepth)
{
    cv::Mat input = image.getMat();
    CV_Assert(!input.empty());

    cv::Mat resized;
    if (size.width > 0 && size.height > 0) {
        if (crop) {
            double input_aspect = static_cast<double>(input.cols) / input.rows;
            double target_aspect = static_cast<double>(size.width) / size.height;

            if (input_aspect > target_aspect) {
                int crop_width = static_cast<int>(input.rows * target_aspect);
                int x_offset = (input.cols - crop_width) / 2;
                cv::Rect roi(x_offset, 0, crop_width, input.rows);
                cv::resize(input(roi), resized, size);
            }
            else {
                int crop_height = static_cast<int>(input.cols / target_aspect);
                int y_offset = (input.rows - crop_height) / 2;
                cv::Rect roi(0, y_offset, input.cols, crop_height);
                cv::resize(input(roi), resized, size);
            }
        }
        else {
            cv::resize(input, resized, size);
        }
    }
    else {
        resized = input.clone();
    }

    if (swapRB) {
        cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
    }

    cv::Mat floatImage;
    resized.convertTo(floatImage, ddepth, scalefactor);

    if (mean != cv::Scalar()) {
        cv::Mat meanMat(floatImage.size(), floatImage.type(), mean);
        meanMat *= scalefactor;
        cv::subtract(floatImage, meanMat, floatImage);
    }

    std::vector<cv::Mat> channels;
    cv::split(floatImage, channels);

    int dims[] = {1, static_cast<int>(channels.size()), floatImage.rows, floatImage.cols};
    cv::Mat blob(4, dims, ddepth);

    for (int c = 0; c < static_cast<int>(channels.size()); c++) {
        cv::Mat channelBlob(blob.size[2], blob.size[3], ddepth, blob.ptr(0, c));
        channels[c].copyTo(channelBlob);
    }

    return blob;
}

bool preprocess_yolov5(const cv::Mat& image, const YoloV5Config& config, YoloPreprocessResult& result, std::string* error)
{
    if (image.empty()) {
        if (error != nullptr) {
            *error = "Input image is empty.";
        }
        return false;
    }

    result.padded_image = format_yolov5(image);
    result.input_blob = blob_from_image(result.padded_image,
                                        1.0 / 255.0,
                                        cv::Size(config.input_width, config.input_height),
                                        cv::Scalar(),
                                        true,
                                        false);
    return true;
}
