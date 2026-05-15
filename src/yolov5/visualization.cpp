/**
 * @file visualization.cpp
 * @brief YOLOv5 visualization helpers.
 */
#include "visualization.h"
#include "labels.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
/**
 * @brief Return the fixed palette used when drawing detection boxes.
 */
const std::vector<cv::Scalar>& detection_colors()
{
    // Cycle a small high-contrast palette across class ids.
    static const std::vector<cv::Scalar> kColors = {
        cv::Scalar(255, 255, 0),
        cv::Scalar(0, 255, 0),
        cv::Scalar(0, 255, 255),
        cv::Scalar(255, 0, 0),
    };
    return kColors;
}
}

/**
 * @brief Draw detection boxes and labels onto an image.
 */
void draw_detections(cv::Mat& image, const std::vector<Detection>& detections, const std::vector<std::string>& class_names)
{
    // Draw boxes first, then overlay a filled label strip for readability.
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

/**
 * @brief Save a visualization image to disk using OpenCV image codecs.
 */
bool save_detection_image(const std::string& output_path, const cv::Mat& image)
{
    // cv::imwrite selects the encoder from the output file extension.
    return cv::imwrite(output_path, image);
}
