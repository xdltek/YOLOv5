#ifndef XDLTEK_SAMPLES_RPP_YOLO_POSTPROCESSOR_H
#define XDLTEK_SAMPLES_RPP_YOLO_POSTPROCESSOR_H

#include "detection_types.h"

#include <Infer.h>
#include <rpp_runtime.h>

#include <cstddef>
#include <memory>
#include <vector>

/**
 * @brief YOLOv5 output layout and filtering thresholds used by RPP postprocessing.
 */
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

/**
 * @brief Timing and transfer counters reported by the postprocessing module.
 */
struct RppPostprocessProfile
{
    double cast_ms = 0.0;
    double nms_slice_ms = 0.0;
    double nms_ms = 0.0;
    double device_to_host_ms = 0.0;
    size_t device_to_host_bytes = 0;
    double total_ms = 0.0;
};

/**
 * @brief RPP-backed YOLOv5 output cast, NMS, and compact detection parsing module.
 */
class RppYoloPostprocessor
{
public:
    /**
     * @brief Construct an empty postprocessor; call init() before run().
     */
    RppYoloPostprocessor() = default;
    /**
     * @brief Release all postprocess runtime engines and buffers.
     */
    ~RppYoloPostprocessor();

    /**
     * @brief Disable copying because the object owns runtime engines and buffers.
     */
    RppYoloPostprocessor(const RppYoloPostprocessor&) = delete;
    /**
     * @brief Disable copy assignment because the object owns runtime engines and buffers.
     */
    RppYoloPostprocessor& operator=(const RppYoloPostprocessor&) = delete;

    /**
     * @brief Build postprocess engines and allocate buffers for YOLOv5 output decoding.
     * @param config YOLO output shape, class count, and NMS thresholds.
     */
    bool init(const RppYoloPostprocessConfig& config);

    /**
     * @brief Run RPP cast, pre-slice, NMS, and host-side compact detection parsing.
     * @param yolo_output_device Model output buffer in device memory.
     * @param letterbox Preprocess metadata used to restore boxes to source coordinates.
     * @param detections Output vector populated with retained detections.
     * @param stream Optional runtime stream.
     * @param profile Optional timing profile populated for demo logs.
     */
    bool run(const void* yolo_output_device,
             const LetterboxInfo& letterbox,
             std::vector<Detection>& detections,
             rtStream_t stream = nullptr,
             RppPostprocessProfile* profile = nullptr);

private:
    /**
     * @brief Build the runtime cast engine that converts model FP32 output to BF16.
     */
    bool buildCastEngine();
    /**
     * @brief Build the runtime addNMS engine used for device-side NMS.
     */
    bool buildNmsEngine();
    /**
     * @brief Allocate persistent device and pinned host buffers used by postprocessing.
     */
    bool allocateBuffers();
    /**
     * @brief Copy NMS thresholds and limits into device runtime input buffers.
     */
    bool copyConstantsToDevice();
    /**
     * @brief Convert selected compact BF16 boxes into source-image detections.
     * @param class_ids Selected class ids returned by NMS.
     * @param boxes Selected BF16 box coordinates.
     * @param letterbox Preprocess metadata used to restore source coordinates.
     * @param detections Output detection list.
     */
    bool parseCompactDetections(const std::vector<int32_t>& class_ids,
                                const std::vector<uint16_t>& boxes,
                                const LetterboxInfo& letterbox,
                                std::vector<Detection>& detections) const;
    /**
     * @brief Release all postprocess-owned device and pinned host buffers.
     */
    void releaseBuffers();
    /**
     * @brief Destroy runtime engines and clear cached binding indices.
     */
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
