/**
 * @file visualize.cpp
 * @brief YOLOv5 visualization helpers.
 */
#include "visualize.h"
#include "labels.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
const std::vector<cv::Scalar>& detection_colors()
{
    static const std::vector<cv::Scalar> kColors = {
        cv::Scalar(255, 255, 0),
        cv::Scalar(0, 255, 0),
        cv::Scalar(0, 255, 255),
        cv::Scalar(255, 0, 0),
    };
    return kColors;
}
}

void draw_detections(cv::Mat& image, const std::vector<Detection>& detections, const std::vector<std::string>& class_names)
{
    const auto& colors = detection_colors();

    for (const auto& detection : detections) {
        auto box = detection.box;
        auto class_id = detection.class_id;
        size_t color_index = class_id >= 0 ? static_cast<size_t>(class_id) % colors.size() : 0;
        const auto color = colors[color_index];
        cv::rectangle(image, box, color, 3);

        cv::rectangle(image, cv::Point(box.x, box.y - 20), cv::Point(box.x + box.width, box.y), color, cv::FILLED);
        std::string label = detection_class_label(class_id, class_names);
        cv::putText(image, label, cv::Point(box.x, box.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
    }
}

bool save_detection_image(const std::string& output_path, const cv::Mat& image)
{
    return cv::imwrite(output_path, image);
}
