/**
 * @file postprocess.cpp
 * @brief YOLOv5 output decoding and NMS implementations.
 */
#include "postprocess.h"

#include <algorithm>

void nms_boxes(const std::vector<cv::Rect>& bboxes,
               const std::vector<float>& scores,
               float score_threshold,
               float nms_threshold,
               std::vector<int>& indices,
               float eta,
               int top_k)
{
    indices.clear();

    if (bboxes.empty() || scores.empty() || bboxes.size() != scores.size()) {
        return;
    }

    std::vector<int> candidate_indices;
    for (int i = 0; i < static_cast<int>(scores.size()); i++) {
        if (scores[i] >= score_threshold) {
            candidate_indices.push_back(i);
        }
    }

    if (candidate_indices.empty()) {
        return;
    }

    std::sort(candidate_indices.begin(), candidate_indices.end(),
              [&scores](int a, int b) {
                  return scores[a] > scores[b];
              });

    if (top_k > 0 && top_k < static_cast<int>(candidate_indices.size())) {
        candidate_indices.resize(top_k);
    }

    std::vector<float> areas;
    areas.reserve(bboxes.size());
    for (const auto& rect : bboxes) {
        areas.push_back(static_cast<float>(rect.width * rect.height));
    }

    float adaptive_threshold = nms_threshold;

    while (!candidate_indices.empty()) {
        int best_idx = candidate_indices[0];
        indices.push_back(best_idx);

        if (candidate_indices.size() == 1) {
            break;
        }

        std::vector<int> remaining_indices;
        remaining_indices.reserve(candidate_indices.size() - 1);

        const cv::Rect& best_rect = bboxes[best_idx];

        for (int i = 1; i < static_cast<int>(candidate_indices.size()); i++) {
            int idx = candidate_indices[i];
            const cv::Rect& current_rect = bboxes[idx];

            int x1 = std::max(best_rect.x, current_rect.x);
            int y1 = std::max(best_rect.y, current_rect.y);
            int x2 = std::min(best_rect.x + best_rect.width, current_rect.x + current_rect.width);
            int y2 = std::min(best_rect.y + best_rect.height, current_rect.y + current_rect.height);

            float intersection = 0.0f;
            if (x2 > x1 && y2 > y1) {
                intersection = static_cast<float>((x2 - x1) * (y2 - y1));
            }

            float union_area = areas[best_idx] + areas[idx] - intersection;
            float iou = intersection / union_area;

            if (iou <= adaptive_threshold) {
                remaining_indices.push_back(idx);
            }
        }

        candidate_indices = std::move(remaining_indices);

        if (eta != 1.0f) {
            adaptive_threshold *= eta;
        }
    }
}

bool decode_yolov5_output(const std::vector<float>& output_data,
                          const cv::Size& padded_image_size,
                          const std::vector<std::string>& class_names,
                          const YoloV5Config& config,
                          std::vector<Detection>& detections,
                          std::string* error)
{
    if (class_names.empty() || config.output_dimensions < static_cast<int>(class_names.size()) + 5) {
        if (error != nullptr) {
            *error = "YOLO output dimensions do not match the configured class list.";
        }
        return false;
    }

    size_t expected_size = static_cast<size_t>(config.output_rows) * static_cast<size_t>(config.output_dimensions);
    if (output_data.size() < expected_size) {
        if (error != nullptr) {
            *error = "YOLO output tensor is smaller than expected by the demo decoder.";
        }
        return false;
    }

    float x_factor = static_cast<float>(padded_image_size.width) / static_cast<float>(config.input_width);
    float y_factor = static_cast<float>(padded_image_size.height) / static_cast<float>(config.input_height);

    const float* data = output_data.data();

    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (int i = 0; i < config.output_rows; ++i) {
        float confidence = data[4];
        if (confidence >= config.confidence_threshold) {
            const float* classes_scores = data + 5;
            int class_id = 0;
            float max_class_score = classes_scores[0];

            for (int c = 1; c < static_cast<int>(class_names.size()); c++) {
                if (classes_scores[c] > max_class_score) {
                    class_id = c;
                    max_class_score = classes_scores[c];
                }
            }

            if (max_class_score > config.score_threshold) {
                confidences.push_back(confidence);
                class_ids.push_back(class_id);

                float x = data[0];
                float y = data[1];
                float w = data[2];
                float h = data[3];
                int left = int((x - 0.5f * w) * x_factor);
                int top = int((y - 0.5f * h) * y_factor);
                int width = int(w * x_factor);
                int height = int(h * y_factor);
                boxes.push_back(cv::Rect(left, top, width, height));
            }
        }

        data += config.output_dimensions;
    }

    std::vector<int> nms_result;
    nms_boxes(boxes, confidences, config.score_threshold, config.nms_threshold, nms_result);

    for (int idx : nms_result) {
        Detection result;
        result.class_id = class_ids[idx];
        result.confidence = confidences[idx];
        result.box = boxes[idx];
        detections.push_back(result);
    }
    return true;
}
