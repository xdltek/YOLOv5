#include "rpp_yolo_postprocessor.h"

#include "perf_trace_session.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace
{
using ProfileClock = std::chrono::high_resolution_clock;

/**
 * @brief Minimal logger used while building internal postprocess runtime engines.
 */
class SilentLogger : public infer1::ILogger
{
public:
    void log(Severity severity, const char* message) override
    {
        if (severity <= Severity::kERROR) {
            std::cerr << "[RPP postprocess] " << message << std::endl;
        }
    }
};

/**
 * @brief Provide a quiet logger for small runtime engines created by postprocessing.
 */
SilentLogger& rpp_postprocess_logger()
{
    static SilentLogger logger;
    return logger;
}

/**
 * @brief Clear an optional postprocess profile before filling current-run timings.
 * @param profile Optional profile object owned by the caller.
 */
void reset_profile(RppPostprocessProfile* profile)
{
    if (profile != nullptr) {
        *profile = RppPostprocessProfile{};
    }
}

/**
 * @brief Measure elapsed milliseconds from a saved timestamp to now.
 * @param start Timestamp captured before a measured stage.
 */
double elapsed_ms(ProfileClock::time_point start)
{
    return std::chrono::duration<double, std::milli>(ProfileClock::now() - start).count();
}

/**
 * @brief Print a runtime API error and convert the status into a boolean.
 * @param status Return value from an RPP runtime call.
 * @param what Human-readable operation name for diagnostics.
 */
bool check_rt(rtError_t status, const char* what)
{
    if (status == rtError_t::rtSuccess) {
        return true;
    }
    std::cerr << what << " failed, rtError=" << static_cast<int>(status) << std::endl;
    return false;
}

/**
 * @brief Compute the element count described by an inference dimension object.
 * @param dims Runtime tensor dimensions.
 */
int dims_volume(const infer1::Dims& dims)
{
    int volume = 1;
    for (int i = 0; i < dims.nbDims; ++i) {
        volume *= dims.d[i];
    }
    return volume;
}

/**
 * @brief Return YOLO score channels after the four box coordinates.
 * @param config Postprocess configuration.
 */
int score_channels(const RppYoloPostprocessConfig& config)
{
    return config.output_dimensions - 4;
}

/**
 * @brief Convert a raw BF16 value to float for host-side box restoration.
 * @param value BF16 bits stored in the upper half of a float.
 */
float bf16_to_float(uint16_t value)
{
    uint32_t bits = static_cast<uint32_t>(value) << 16;
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

/**
 * @brief Convert a float threshold to BF16 for RPP NMS runtime inputs.
 * @param value Threshold value in float.
 */
uint16_t float_to_bf16(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return static_cast<uint16_t>(bits >> 16);
}

/**
 * @brief Clip a restored box to source image bounds.
 */
cv::Rect clip_rect_to_image(float left, float top, float right, float bottom, int width, int height)
{
    left = std::max(0.0f, std::min(left, static_cast<float>(width - 1)));
    top = std::max(0.0f, std::min(top, static_cast<float>(height - 1)));
    right = std::max(0.0f, std::min(right, static_cast<float>(width - 1)));
    bottom = std::max(0.0f, std::min(bottom, static_cast<float>(height - 1)));

    int x0 = static_cast<int>(left);
    int y0 = static_cast<int>(top);
    int x1 = static_cast<int>(right);
    int y1 = static_cast<int>(bottom);
    return cv::Rect(x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0));
}

/**
 * @brief Allocate device DDR memory and report failures with the buffer name.
 */
bool allocate_device(void** buffer, size_t bytes, const char* name)
{
    if (bytes == 0) {
        std::cerr << "Invalid allocation size for " << name << std::endl;
        return false;
    }
    if (!check_rt(rtMalloc(buffer, bytes), name)) {
        return false;
    }
    return true;
}

/**
 * @brief Allocate pinned host memory for asynchronous postprocess D2H copies.
 */
bool allocate_host_pinned(void** buffer, size_t bytes, const char* name)
{
    if (bytes == 0) {
        std::cerr << "Invalid pinned allocation size for " << name << std::endl;
        return false;
    }
    if (!check_rt(rtHostAlloc(buffer, bytes, 0), name)) {
        return false;
    }
    return true;
}

}

/**
 * @brief Release all postprocess buffers and runtime engines.
 */
RppYoloPostprocessor::~RppYoloPostprocessor()
{
    // Release memory first, then destroy runtime engines and contexts.
    releaseBuffers();
    resetEngines();
}

/**
 * @brief Validate configuration, build runtime subgraphs, and allocate postprocess buffers.
 */
bool RppYoloPostprocessor::init(const RppYoloPostprocessConfig& config)
{
    PERF_SCOPE_CATE("rpp_yolo_postprocess_init", "postprocess");
    // Validate YOLO output shape and thresholds before creating runtime engines.
    if (config.output_rows <= 0 || config.output_dimensions <= 5 ||
        config.class_count <= 0 || config.max_output_boxes_per_class <= 0) {
        std::cerr << "Invalid RPP YOLO postprocess dimensions." << std::endl;
        return false;
    }
    if (config.output_dimensions != config.class_count + 5) {
        std::cerr << "Unsupported YOLO output contract: expected exactly four box values, "
                  << "one objectness value, and " << config.class_count
                  << " class scores per row." << std::endl;
        return false;
    }
    if (config.confidence_threshold < 0.0f || config.score_threshold < 0.0f ||
        config.nms_threshold < 0.0f || config.nms_threshold > 1.0f) {
        std::cerr << "Invalid RPP YOLO postprocess thresholds." << std::endl;
        return false;
    }

    // Reinitialize from a clean state so a caller can rebuild for a different model shape.
    releaseBuffers();
    resetEngines();
    config_ = config;
    initialized_ = false;

    try {
        // Fuse cast, slicing, and NMS into one SDK runtime engine to avoid three
        // independent device submissions on every frame.
        if (!buildFusedPostprocessEngine()) {
            releaseBuffers();
            resetEngines();
            return false;
        }
        if (!allocateBuffers() || !copyConstantsToDevice()) {
            releaseBuffers();
            resetEngines();
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "RPP YOLO postprocess init failed: " << e.what() << std::endl;
        releaseBuffers();
        resetEngines();
        return false;
    }

    initialized_ = true;
    return true;
}

/**
 * @brief Build one SDK runtime engine for cast, boxes/scores slicing, and NMS.
 */
bool RppYoloPostprocessor::buildFusedPostprocessEngine()
{
    PERF_SCOPE_CATE("rpp_yolo_postprocess_build_fused_engine", "postprocess");
    std::unique_ptr<infer1::IBuilder> builder {infer1::createInferBuilder(rpp_postprocess_logger())};
    if (builder == nullptr) {
        std::cerr << "Failed to create RPP fused postprocess builder." << std::endl;
        return false;
    }
    std::unique_ptr<infer1::INetworkDefinition> network {builder->createNetwork()};
    if (network == nullptr) {
        std::cerr << "Failed to create RPP fused postprocess network." << std::endl;
        return false;
    }
    std::unique_ptr<infer1::IBuilderConfig> builder_config {builder->createBuilderConfig()};
    if (builder_config == nullptr) {
        std::cerr << "Failed to create RPP fused postprocess builder config." << std::endl;
        return false;
    }

    infer1::Dims yolo_dims{};
    yolo_dims.nbDims = 2;
    yolo_dims.d[0] = config_.output_rows;
    yolo_dims.d[1] = config_.output_dimensions;
    infer1::ITensor* yolo_output = network->addInput("fused_yolo_output",
                                                     infer1::DataType::kFLOAT,
                                                     yolo_dims);
    if (yolo_output == nullptr) {
        std::cerr << "Failed to create RPP fused postprocess input." << std::endl;
        return false;
    }

    infer1::IIdentityLayer* cast = network->addIdentity(*yolo_output);
    if (cast == nullptr) {
        std::cerr << "Failed to create RPP fused postprocess cast." << std::endl;
        return false;
    }
    cast->setPrecision(infer1::DataType::kBF);
    cast->setOutputType(0, infer1::DataType::kBF);
    cast->getOutput(0)->setType(infer1::DataType::kBF);

    infer1::Dims box_start{};
    box_start.nbDims = 2;
    box_start.d[0] = 0;
    box_start.d[1] = 0;
    infer1::Dims box_size{};
    box_size.nbDims = 2;
    box_size.d[0] = config_.output_rows;
    box_size.d[1] = 4;
    infer1::Dims stride{};
    stride.nbDims = 2;
    stride.d[0] = 1;
    stride.d[1] = 1;
    infer1::ISliceLayer* boxes_slice = network->addSlice(*cast->getOutput(0),
                                                         box_start,
                                                         box_size,
                                                         stride);
    if (boxes_slice == nullptr) {
        std::cerr << "Failed to create RPP fused boxes slice." << std::endl;
        return false;
    }
    boxes_slice->setPrecision(infer1::DataType::kBF);
    boxes_slice->setOutputType(0, infer1::DataType::kBF);
    boxes_slice->getOutput(0)->setType(infer1::DataType::kBF);
    boxes_slice->getOutput(0)->setName("fused_output_boxes");

    infer1::Dims scores_start{};
    scores_start.nbDims = 2;
    scores_start.d[0] = 0;
    scores_start.d[1] = 4;
    infer1::Dims scores_size{};
    scores_size.nbDims = 2;
    scores_size.d[0] = config_.output_rows;
    scores_size.d[1] = score_channels(config_);
    infer1::ISliceLayer* scores_slice = network->addSlice(*cast->getOutput(0),
                                                          scores_start,
                                                          scores_size,
                                                          stride);
    if (scores_slice == nullptr) {
        std::cerr << "Failed to create RPP fused scores slice." << std::endl;
        return false;
    }
    scores_slice->setPrecision(infer1::DataType::kBF);
    scores_slice->setOutputType(0, infer1::DataType::kBF);
    scores_slice->getOutput(0)->setType(infer1::DataType::kBF);

    infer1::Dims scalar_dims{};
    scalar_dims.nbDims = 1;
    scalar_dims.d[0] = 1;
    infer1::ITensor* max_output_per_class =
        network->addInput("input_max_out_per_class", infer1::DataType::kINT32, scalar_dims);
    infer1::ITensor* iou_threshold =
        network->addInput("input_iou_threshold", infer1::DataType::kBF, scalar_dims);
    infer1::ITensor* score_threshold =
        network->addInput("input_score_threshold", infer1::DataType::kBF, scalar_dims);
    if (max_output_per_class == nullptr || iou_threshold == nullptr || score_threshold == nullptr) {
        std::cerr << "Failed to create RPP fused NMS scalar inputs." << std::endl;
        return false;
    }

    infer1::INMSLayer* nms = network->addNMS(*boxes_slice->getOutput(0),
                                             *scores_slice->getOutput(0),
                                             *max_output_per_class);
    if (nms == nullptr) {
        std::cerr << "Failed to create RPP fused NMS layer." << std::endl;
        return false;
    }
    nms->setBoundingBoxFormat(infer1::BoundingBoxFormat::kCENTER_SIZES);
    nms->setInput(3, *iou_threshold);
    nms->setInput(4, *score_threshold);
    nms->setPrecision(infer1::DataType::kBF);
    nms->setOutputType(0, infer1::DataType::kINT32);
    nms->setOutputType(1, infer1::DataType::kINT32);
    nms->setHasBoxConfidence(config_.has_box_confidence);
    nms->setIsAgnostic(config_.agnostic);
    nms->getOutput(0)->setName("output_indices");
    nms->getOutput(1)->setName("output_num_out_boxes");

    network->markOutput(*boxes_slice->getOutput(0));
    network->markOutput(*nms->getOutput(0));
    network->markOutput(*nms->getOutput(1));

    builder->setMaxBatchSize(1);
    builder_config->setMaxWorkspaceSize(static_cast<size_t>(16) << 20);
    builder_config->setFlag(infer1::BuilderFlag::kBF16);
    fused_engine_.reset(builder->buildEngineWithConfig(*network, *builder_config));
    if (fused_engine_ == nullptr) {
        std::cerr << "Failed to build RPP fused postprocess engine." << std::endl;
        return false;
    }
    fused_context_.reset(fused_engine_->createExecutionContext());
    if (fused_context_ == nullptr) {
        std::cerr << "Failed to create RPP fused postprocess context." << std::endl;
        return false;
    }

    fused_input_index_ = fused_engine_->getBindingIndex("fused_yolo_output");
    fused_boxes_output_index_ = fused_engine_->getBindingIndex("fused_output_boxes");
    nms_max_output_index_ = fused_engine_->getBindingIndex("input_max_out_per_class");
    nms_iou_threshold_index_ = fused_engine_->getBindingIndex("input_iou_threshold");
    nms_score_threshold_index_ = fused_engine_->getBindingIndex("input_score_threshold");
    nms_indices_index_ = fused_engine_->getBindingIndex("output_indices");
    nms_count_index_ = fused_engine_->getBindingIndex("output_num_out_boxes");
    if (fused_input_index_ < 0 || fused_boxes_output_index_ < 0 ||
        nms_max_output_index_ < 0 ||
        nms_iou_threshold_index_ < 0 || nms_score_threshold_index_ < 0 ||
        nms_indices_index_ < 0 || nms_count_index_ < 0) {
        std::cerr << "Invalid RPP fused postprocess binding index." << std::endl;
        return false;
    }

    infer1::Dims indices_dims = fused_engine_->getBindingDimensions(nms_indices_index_);
    infer1::Dims count_dims = fused_engine_->getBindingDimensions(nms_count_index_);
    nms_indices_elements_ = dims_volume(indices_dims);
    nms_count_elements_ = dims_volume(count_dims);
    if (indices_dims.nbDims > 0) {
        int last_dim = indices_dims.d[indices_dims.nbDims - 1];
        if (last_dim == 2 || last_dim == 3) {
            nms_index_tuple_size_ = last_dim;
        }
    }
    if (nms_indices_elements_ <= 0 || nms_count_elements_ <= 0 || nms_index_tuple_size_ <= 0) {
        std::cerr << "Invalid RPP fused NMS output dimensions." << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief Allocate all persistent device and pinned host buffers required by postprocessing.
 */
bool RppYoloPostprocessor::allocateBuffers()
{
    PERF_SCOPE_CATE("rpp_yolo_postprocess_allocate_buffers", "postprocess");
    // Keep the fused engine's BF16 box output in device memory for compact restoration.
    const size_t boxes_elems = static_cast<size_t>(config_.output_rows) * 4U;
    if (!allocate_device(&boxes_bf16_device_, boxes_elems * sizeof(uint16_t), "rtMalloc boxes bf16")) {
        return false;
    }

    if (fused_context_ != nullptr) {
        // Host pinned buffers receive compact NMS indices and BF16 boxes for CPU rendering.
        const size_t nms_indices_bytes = static_cast<size_t>(nms_indices_elements_) * sizeof(int32_t);
        const size_t nms_count_bytes = static_cast<size_t>(nms_count_elements_) * sizeof(int32_t);
        nms_outputs_bytes_ = nms_indices_bytes + nms_count_bytes;
        boxes_bf16_bytes_ = boxes_elems * sizeof(uint16_t);
        if (!allocate_device(&iou_threshold_device_, sizeof(uint16_t), "rtMalloc NMS iou threshold") ||
            !allocate_device(&score_threshold_device_, sizeof(uint16_t), "rtMalloc NMS score threshold") ||
            !allocate_device(&nms_outputs_device_, nms_outputs_bytes_, "rtMalloc NMS outputs") ||
            !allocate_host_pinned(&nms_outputs_host_, nms_outputs_bytes_, "rtHostAlloc NMS outputs") ||
            !allocate_host_pinned(&boxes_bf16_host_, boxes_bf16_bytes_, "rtHostAlloc NMS boxes")) {
            return false;
        }
        nms_indices_device_ = nms_outputs_device_;
        nms_count_device_ = static_cast<unsigned char*>(nms_outputs_device_) + nms_indices_bytes;
    }
    if (fused_context_ != nullptr && nms_max_output_index_ >= 0 &&
        !allocate_device(&max_output_per_class_device_, sizeof(int32_t), "rtMalloc NMS max output per class")) {
        return false;
    }
    return true;
}

/**
 * @brief Copy static NMS threshold inputs to device memory once during initialization.
 */
bool RppYoloPostprocessor::copyConstantsToDevice()
{
    PERF_SCOPE_CATE("rpp_yolo_postprocess_copy_constants_to_device", "postprocess");
    // Thresholds and max-output values are runtime inputs to match the reference addNMS flow.
    if (fused_context_ == nullptr) {
        return true;
    }
    int32_t max_output_per_class = config_.max_output_boxes_per_class;
    uint16_t iou_threshold = float_to_bf16(config_.nms_threshold);
    uint16_t score_threshold = float_to_bf16(config_.score_threshold);
    bool copied = true;
    if (nms_max_output_index_ >= 0) {
        copied = check_rt(rtMemcpy(max_output_per_class_device_,
                                   &max_output_per_class,
                                   sizeof(max_output_per_class),
                                   rtMemcpyHostToDevice),
                          "rtMemcpy NMS max output per class");
    }
    return copied &&
           check_rt(rtMemcpy(iou_threshold_device_, &iou_threshold, sizeof(iou_threshold), rtMemcpyHostToDevice),
                    "rtMemcpy NMS iou threshold") &&
           check_rt(rtMemcpy(score_threshold_device_, &score_threshold, sizeof(score_threshold), rtMemcpyHostToDevice),
                    "rtMemcpy NMS score threshold");
}

/**
 * @brief Execute cast, score slicing, RPP NMS, D2H copy, and compact detection parsing.
 */
bool RppYoloPostprocessor::run(const void* yolo_output_device,
                               const LetterboxInfo& letterbox,
                               std::vector<Detection>& detections,
                               rtStream_t stream,
                               RppPostprocessProfile* profile)
{
    PERF_SCOPE_CATE("rpp_yolo_postprocess_run", "postprocess");
    reset_profile(profile);
    detections.clear();
    // Validate initialized state and letterbox metadata before launching device work.
    if (!initialized_ || yolo_output_device == nullptr) {
        std::cerr << "RPP YOLO postprocessor has not been initialized." << std::endl;
        return false;
    }
    if (letterbox.scale <= 0.0f || letterbox.source_width <= 0 || letterbox.source_height <= 0) {
        std::cerr << "Invalid letterbox metadata for RPP postprocess." << std::endl;
        return false;
    }

    auto total_start = ProfileClock::now();
    // Use the caller's stream when provided; otherwise own a short-lived stream for this run.
    rtStream_t run_stream = stream;
    bool owns_stream = false;
    if (run_stream == nullptr) {
        if (!check_rt(rtStreamCreate(&run_stream), "rtStreamCreate RPP postprocess")) {
            return false;
        }
        owns_stream = true;
    }

    // Submit cast, slicing, and NMS through one SDK engine. Its box output remains
    // available for compact host-side restoration after NMS.
    std::vector<void*> fused_bindings(static_cast<size_t>(fused_engine_->getNbBindings()), nullptr);
    fused_bindings[static_cast<size_t>(fused_input_index_)] = const_cast<void*>(yolo_output_device);
    fused_bindings[static_cast<size_t>(fused_boxes_output_index_)] = boxes_bf16_device_;
    fused_bindings[static_cast<size_t>(nms_max_output_index_)] = max_output_per_class_device_;
    fused_bindings[static_cast<size_t>(nms_iou_threshold_index_)] = iou_threshold_device_;
    fused_bindings[static_cast<size_t>(nms_score_threshold_index_)] = score_threshold_device_;
    fused_bindings[static_cast<size_t>(nms_indices_index_)] = nms_indices_device_;
    fused_bindings[static_cast<size_t>(nms_count_index_)] = nms_count_device_;

    auto stage_start = ProfileClock::now();
    try {
        if (!fused_context_->enqueue(1, fused_bindings.data(), run_stream, nullptr)) {
            std::cerr << "RPP fused postprocess enqueue failed." << std::endl;
            if (owns_stream) {
                rtStreamDestroy(run_stream);
            }
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "RPP fused postprocess failed: " << e.what() << std::endl;
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (!check_rt(rtStreamSynchronize(run_stream), "rtStreamSynchronize RPP fused postprocess")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (profile != nullptr) {
        profile->cast_ms = 0.0;
        profile->nms_slice_ms = 0.0;
        profile->nms_ms = elapsed_ms(stage_start);
    }

    // Queue D2H after device postprocess work has completed, then synchronize only the D2H tail.
    int max_selected = nms_indices_elements_ / nms_index_tuple_size_;
    const size_t nms_indices_bytes = static_cast<size_t>(nms_indices_elements_) * sizeof(int32_t);

    stage_start = ProfileClock::now();
    bool copied = check_rt(rtMemcpyAsync(nms_outputs_host_,
                                         nms_outputs_device_,
                                         nms_outputs_bytes_,
                                         rtMemcpyDeviceToHost,
                                         run_stream),
                           "rtMemcpyAsync RPP NMS outputs D2H") &&
                  check_rt(rtMemcpyAsync(boxes_bf16_host_,
                                         boxes_bf16_device_,
                                         boxes_bf16_bytes_,
                                         rtMemcpyDeviceToHost,
                                         run_stream),
                           "rtMemcpyAsync RPP NMS boxes D2H") &&
                  check_rt(rtStreamSynchronize(run_stream), "rtStreamSynchronize RPP postprocess D2H");
    if (!copied) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    double d2h_ms = elapsed_ms(stage_start);

    const int32_t* indices_data = reinterpret_cast<const int32_t*>(nms_outputs_host_);
    const int32_t* count_data = reinterpret_cast<const int32_t*>(
        static_cast<const unsigned char*>(nms_outputs_host_) + nms_indices_bytes);
    int selected_count = nms_count_elements_ <= 0 ? 0 : count_data[0];
    selected_count = std::max(0, std::min(selected_count, max_selected));

    size_t compact_d2h_bytes = nms_outputs_bytes_ + boxes_bf16_bytes_;
    double compact_d2h_ms = d2h_ms;
    if (selected_count > 0) {
        PERF_SCOPE_CATE("rpp_yolo_postprocess_parse_selected_boxes", "postprocess");
        // Convert runtime NMS index tuples into class IDs and row indices.
        std::vector<int32_t> class_ids(static_cast<size_t>(selected_count), 0);
        std::vector<int32_t> box_indices(static_cast<size_t>(selected_count), 0);
        for (int i = 0; i < selected_count; ++i) {
            size_t base = static_cast<size_t>(i) * static_cast<size_t>(nms_index_tuple_size_);
            if (nms_index_tuple_size_ == 3) {
                class_ids[static_cast<size_t>(i)] = indices_data[base + 1U];
                box_indices[static_cast<size_t>(i)] = indices_data[base + 2U];
            }
            else {
                class_ids[static_cast<size_t>(i)] = indices_data[base];
                box_indices[static_cast<size_t>(i)] = indices_data[base + 1U];
            }
        }

        // Gather only selected BF16 boxes so CPU parsing walks a compact list.
        const uint16_t* boxes_host = static_cast<const uint16_t*>(boxes_bf16_host_);
        const size_t boxes_elems = boxes_bf16_bytes_ / sizeof(uint16_t);
        std::vector<uint16_t> boxes(static_cast<size_t>(selected_count) * 4U);
        for (int i = 0; i < selected_count; ++i) {
            size_t src_offset = static_cast<size_t>(box_indices[static_cast<size_t>(i)]) *
                                4U;
            size_t dst_offset = static_cast<size_t>(i) * 4U;
            if (src_offset + 3U >= boxes_elems) {
                continue;
            }
            boxes[dst_offset] = boxes_host[src_offset];
            boxes[dst_offset + 1U] = boxes_host[src_offset + 1U];
            boxes[dst_offset + 2U] = boxes_host[src_offset + 2U];
            boxes[dst_offset + 3U] = boxes_host[src_offset + 3U];
        }

        // Restore compact boxes from model letterbox coordinates to source image coordinates.
        if (!parseCompactDetections(class_ids, boxes, letterbox, detections)) {
            if (owns_stream) {
                rtStreamDestroy(run_stream);
            }
            return false;
        }
    }
    if (profile != nullptr) {
        profile->device_to_host_ms = compact_d2h_ms;
        profile->device_to_host_bytes = compact_d2h_bytes;
    }

    if (owns_stream) {
        check_rt(rtStreamDestroy(run_stream), "rtStreamDestroy RPP postprocess");
    }
    if (profile != nullptr) {
        profile->total_ms = elapsed_ms(total_start);
    }
    return true;
}

/**
 * @brief Decode compact NMS results into source-coordinate Detection objects.
 */
bool RppYoloPostprocessor::parseCompactDetections(const std::vector<int32_t>& class_ids,
                                                  const std::vector<uint16_t>& boxes,
                                                  const LetterboxInfo& letterbox,
                                                  std::vector<Detection>& detections) const
{
    // CPU work here is intentionally small: it only converts compact selected boxes for drawing.
    for (size_t i = 0; i < class_ids.size(); ++i) {
        int class_id = class_ids[i];
        if (class_id < 0 || class_id >= config_.class_count) {
            continue;
        }

        size_t box_offset = i * 4U;
        if (box_offset + 3U >= boxes.size()) {
            continue;
        }
        float x = bf16_to_float(boxes[box_offset]);
        float y = bf16_to_float(boxes[box_offset + 1U]);
        float w = bf16_to_float(boxes[box_offset + 2U]);
        float h = bf16_to_float(boxes[box_offset + 3U]);
        if (w <= 0.0f || h <= 0.0f) {
            continue;
        }

        float confidence = 0.0f;

        // Undo letterbox padding and scale so boxes match the original input frame.
        float left = ((x - 0.5f * w) - letterbox.pad_x) / letterbox.scale;
        float top = ((y - 0.5f * h) - letterbox.pad_y) / letterbox.scale;
        float right = ((x + 0.5f * w) - letterbox.pad_x) / letterbox.scale;
        float bottom = ((y + 0.5f * h) - letterbox.pad_y) / letterbox.scale;

        Detection detection;
        detection.class_id = class_id;
        detection.confidence = confidence;
        detection.box = clip_rect_to_image(left,
                                           top,
                                           right,
                                           bottom,
                                           letterbox.source_width,
                                           letterbox.source_height);
        if (detection.box.width > 0 && detection.box.height > 0) {
            detections.push_back(detection);
        }
    }
    return true;
}

/**
 * @brief Free postprocess-owned device and pinned host allocations.
 */
void RppYoloPostprocessor::releaseBuffers()
{
    // Device buffers are owned for the lifetime of one initialized postprocessor.
    void** buffers[] = {
        &boxes_bf16_device_,
        &max_output_per_class_device_,
        &iou_threshold_device_,
        &score_threshold_device_,
        &nms_outputs_device_,
    };
    for (void** buffer : buffers) {
        if (*buffer != nullptr) {
            rtFree(*buffer);
            *buffer = nullptr;
        }
    }
    // Pinned host buffers are used only for compact D2H postprocess results.
    void** host_buffers[] = {
        &nms_outputs_host_,
        &boxes_bf16_host_,
    };
    for (void** buffer : host_buffers) {
        if (*buffer != nullptr) {
            rtFreeHost(*buffer);
            *buffer = nullptr;
        }
    }
    nms_indices_device_ = nullptr;
    nms_count_device_ = nullptr;
    nms_outputs_bytes_ = 0;
    boxes_bf16_bytes_ = 0;
}

/**
 * @brief Destroy postprocess runtime engines, contexts, and cached binding indices.
 */
void RppYoloPostprocessor::resetEngines()
{
    // Reset contexts before engines and clear cached binding indices.
    fused_context_.reset();
    fused_engine_.reset();
    fused_input_index_ = -1;
    fused_boxes_output_index_ = -1;
    nms_max_output_index_ = -1;
    nms_iou_threshold_index_ = -1;
    nms_score_threshold_index_ = -1;
    nms_indices_index_ = -1;
    nms_count_index_ = -1;
    nms_index_tuple_size_ = 2;
    nms_indices_elements_ = 0;
    nms_count_elements_ = 0;
}
