#ifndef XDLTEK_SAMPLES_RPP_YOLO_POSTPROCESSOR_H
#define XDLTEK_SAMPLES_RPP_YOLO_POSTPROCESSOR_H

#include "detection_types.h"

#include <Infer.h>
#include <rpp_runtime.h>

#include <cstddef>
#include <memory>
#include <vector>

struct RppYoloPostprocessConfig
{
    int output_rows = 0;
    int output_dimensions = 0;
    int class_count = 0;
    float confidence_threshold = 0.0f;
    float score_threshold = 0.0f;
    float nms_threshold = 0.0f;
    int max_output_boxes_per_class = 200;
    bool agnostic = true;
    bool has_box_confidence = true;
};

struct RppPostprocessProfile
{
    double cast_ms = 0.0;
    double nms_slice_ms = 0.0;
    double nms_ms = 0.0;
    double device_to_host_ms = 0.0;
    size_t device_to_host_bytes = 0;
    double total_ms = 0.0;
};

class RppYoloPostprocessor
{
public:
    RppYoloPostprocessor() = default;
    ~RppYoloPostprocessor();

    RppYoloPostprocessor(const RppYoloPostprocessor&) = delete;
    RppYoloPostprocessor& operator=(const RppYoloPostprocessor&) = delete;

    bool init(const RppYoloPostprocessConfig& config);

    bool run(const void* yolo_output_device,
             const LetterboxInfo& letterbox,
             std::vector<Detection>& detections,
             rtStream_t stream = nullptr,
             RppPostprocessProfile* profile = nullptr);

    const void* getSlicedScoresDeviceBuffer() const;
    int getSlicedScoreRows() const;
    int getSlicedScoreChannels() const;

private:
    bool buildCastEngine();
    bool buildNmsEngine();
    bool allocateBuffers();
    bool copyConstantsToDevice();
    bool parseCompactDetections(const std::vector<int32_t>& class_ids,
                                const std::vector<uint16_t>& boxes,
                                const LetterboxInfo& letterbox,
                                std::vector<Detection>& detections) const;
    void releaseBuffers();
    void resetEngines();

    RppYoloPostprocessConfig config_;
    bool initialized_ = false;

    std::unique_ptr<infer1::IEngine> cast_engine_;
    std::unique_ptr<infer1::IExecutionContext> cast_context_;
    int cast_input_index_ = -1;
    int cast_output_index_ = -1;

    std::unique_ptr<infer1::IEngine> nms_engine_;
    std::unique_ptr<infer1::IExecutionContext> nms_context_;
    int nms_boxes_index_ = -1;
    int nms_scores_index_ = -1;
    int nms_max_output_index_ = -1;
    int nms_iou_threshold_index_ = -1;
    int nms_score_threshold_index_ = -1;
    int nms_indices_index_ = -1;
    int nms_count_index_ = -1;
    int nms_index_tuple_size_ = 2;
    int nms_indices_elements_ = 0;
    int nms_count_elements_ = 0;

    void* yolo_bf16_device_ = nullptr;
    void* boxes_bf16_device_ = nullptr;
    void* scores_bf16_device_ = nullptr;
    void* max_output_per_class_device_ = nullptr;
    void* iou_threshold_device_ = nullptr;
    void* score_threshold_device_ = nullptr;
    void* nms_outputs_device_ = nullptr;
    void* nms_indices_device_ = nullptr;
    void* nms_count_device_ = nullptr;
    void* nms_outputs_host_ = nullptr;
    void* boxes_bf16_host_ = nullptr;
    size_t nms_outputs_bytes_ = 0;
    size_t boxes_bf16_bytes_ = 0;
};

#endif // XDLTEK_SAMPLES_RPP_YOLO_POSTPROCESSOR_H
