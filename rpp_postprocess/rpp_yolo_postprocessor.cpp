#include "rpp_yolo_postprocessor.h"

#include <nms_pre_slice.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace
{
using ProfileClock = std::chrono::high_resolution_clock;

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

SilentLogger& rpp_postprocess_logger()
{
    static SilentLogger logger;
    return logger;
}

void reset_profile(RppPostprocessProfile* profile)
{
    if (profile != nullptr) {
        *profile = RppPostprocessProfile{};
    }
}

double elapsed_ms(ProfileClock::time_point start)
{
    return std::chrono::duration<double, std::milli>(ProfileClock::now() - start).count();
}

bool check_rt(rtError_t status, const char* what)
{
    if (status == rtError_t::rtSuccess) {
        return true;
    }
    std::cerr << what << " failed, rtError=" << static_cast<int>(status) << std::endl;
    return false;
}

int dims_volume(const infer1::Dims& dims)
{
    int volume = 1;
    for (int i = 0; i < dims.nbDims; ++i) {
        volume *= dims.d[i];
    }
    return volume;
}

int score_channels(const RppYoloPostprocessConfig& config)
{
    return config.output_dimensions - 4;
}

RPPdeviceptr as_rpp_deviceptr(const void* ptr)
{
    return static_cast<RPPdeviceptr>(reinterpret_cast<std::uintptr_t>(ptr));
}

float bf16_to_float(uint16_t value)
{
    uint32_t bits = static_cast<uint32_t>(value) << 16;
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

uint16_t float_to_bf16(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return static_cast<uint16_t>(bits >> 16);
}

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

RppYoloPostprocessor::~RppYoloPostprocessor()
{
    releaseBuffers();
    resetEngines();
}

bool RppYoloPostprocessor::init(const RppYoloPostprocessConfig& config)
{
    if (config.output_rows <= 0 || config.output_dimensions <= 5 ||
        config.class_count <= 0 || config.max_output_boxes_per_class <= 0) {
        std::cerr << "Invalid RPP YOLO postprocess dimensions." << std::endl;
        return false;
    }
    if (config.output_dimensions < config.class_count + 5) {
        std::cerr << "YOLO output dimensions do not match class count." << std::endl;
        return false;
    }
    if (config.confidence_threshold < 0.0f || config.score_threshold < 0.0f ||
        config.nms_threshold < 0.0f || config.nms_threshold > 1.0f) {
        std::cerr << "Invalid RPP YOLO postprocess thresholds." << std::endl;
        return false;
    }

    releaseBuffers();
    resetEngines();
    config_ = config;
    initialized_ = false;

    try {
        if (!buildNmsEngine() || !buildCastEngine()) {
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

bool RppYoloPostprocessor::buildCastEngine()
{
    std::unique_ptr<infer1::IBuilder> builder {infer1::createInferBuilder(rpp_postprocess_logger())};
    if (builder == nullptr) {
        std::cerr << "Failed to create RPP postprocess cast builder." << std::endl;
        return false;
    }
    std::unique_ptr<infer1::INetworkDefinition> network {builder->createNetwork()};
    if (network == nullptr) {
        std::cerr << "Failed to create RPP postprocess cast network." << std::endl;
        return false;
    }
    std::unique_ptr<infer1::IBuilderConfig> builder_config {builder->createBuilderConfig()};
    if (builder_config == nullptr) {
        std::cerr << "Failed to create RPP postprocess cast builder config." << std::endl;
        return false;
    }

    infer1::Dims output_dims{};
    output_dims.nbDims = 2;
    output_dims.d[0] = config_.output_rows;
    output_dims.d[1] = config_.output_dimensions;

    infer1::ITensor* input = network->addInput("cast_input", infer1::DataType::kFLOAT, output_dims);
    if (input == nullptr) {
        std::cerr << "Failed to create RPP postprocess cast input." << std::endl;
        return false;
    }
    infer1::IIdentityLayer* identity = network->addIdentity(*input);
    if (identity == nullptr) {
        std::cerr << "Failed to create RPP postprocess cast identity." << std::endl;
        return false;
    }
    identity->setPrecision(infer1::DataType::kBF);
    identity->setOutputType(0, infer1::DataType::kBF);
    identity->getOutput(0)->setType(infer1::DataType::kBF);
    identity->getOutput(0)->setName("cast_output");
    network->markOutput(*identity->getOutput(0));

    builder->setMaxBatchSize(1);
    builder_config->setMaxWorkspaceSize(static_cast<size_t>(16) << 20);
    builder_config->setFlag(infer1::BuilderFlag::kBF16);

    try {
        cast_engine_.reset(builder->buildEngineWithConfig(*network, *builder_config));
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to build RPP postprocess cast engine: " << e.what() << std::endl;
        throw;
    }
    if (cast_engine_ == nullptr) {
        std::cerr << "Failed to build RPP postprocess cast engine." << std::endl;
        return false;
    }
    cast_context_.reset(cast_engine_->createExecutionContext());
    if (cast_context_ == nullptr) {
        std::cerr << "Failed to create RPP postprocess cast context." << std::endl;
        return false;
    }

    cast_input_index_ = cast_engine_->getBindingIndex("cast_input");
    cast_output_index_ = cast_engine_->getBindingIndex("cast_output");
    if (cast_input_index_ < 0 || cast_output_index_ < 0) {
        std::cerr << "Invalid RPP postprocess cast binding index." << std::endl;
        return false;
    }
    return true;
}

bool RppYoloPostprocessor::buildNmsEngine()
{
    std::unique_ptr<infer1::IBuilder> builder {infer1::createInferBuilder(rpp_postprocess_logger())};
    if (builder == nullptr) {
        std::cerr << "Failed to create RPP NMS builder." << std::endl;
        return false;
    }
    std::unique_ptr<infer1::INetworkDefinition> network {builder->createNetwork()};
    if (network == nullptr) {
        std::cerr << "Failed to create RPP NMS network." << std::endl;
        return false;
    }
    std::unique_ptr<infer1::IBuilderConfig> builder_config {builder->createBuilderConfig()};
    if (builder_config == nullptr) {
        std::cerr << "Failed to create RPP NMS builder config." << std::endl;
        return false;
    }

    infer1::Dims boxes_dims{};
    boxes_dims.nbDims = 2;
    boxes_dims.d[0] = config_.output_rows;
    boxes_dims.d[1] = 4;

    infer1::Dims scores_dims{};
    scores_dims.nbDims = 2;
    scores_dims.d[0] = config_.output_rows;
    scores_dims.d[1] = score_channels(config_);

    infer1::Dims scalar_dims{};
    scalar_dims.nbDims = 1;
    scalar_dims.d[0] = 1;

    infer1::ITensor* boxes = network->addInput("input_boxes", infer1::DataType::kBF, boxes_dims);
    infer1::ITensor* scores = network->addInput("input_scores", infer1::DataType::kBF, scores_dims);
    infer1::ITensor* max_output_per_class =
        network->addInput("input_max_out_per_class", infer1::DataType::kINT32, scalar_dims);
    infer1::ITensor* iou_threshold = network->addInput("input_iou_threshold", infer1::DataType::kBF, scalar_dims);
    infer1::ITensor* score_threshold = network->addInput("input_score_threshold", infer1::DataType::kBF, scalar_dims);
    if (boxes == nullptr || scores == nullptr || max_output_per_class == nullptr ||
        iou_threshold == nullptr || score_threshold == nullptr) {
        std::cerr << "Failed to create RPP NMS inputs." << std::endl;
        return false;
    }

    infer1::INMSLayer* nms = network->addNMS(*boxes, *scores, *max_output_per_class);
    if (nms == nullptr) {
        std::cerr << "Failed to create RPP NMS layer." << std::endl;
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
    network->markOutput(*nms->getOutput(0));
    network->markOutput(*nms->getOutput(1));

    builder->setMaxBatchSize(1);
    builder_config->setMaxWorkspaceSize(1024);
    builder_config->setFlag(infer1::BuilderFlag::kBF16);

    try {
        nms_engine_.reset(builder->buildEngineWithConfig(*network, *builder_config));
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to build RPP NMS engine: " << e.what() << std::endl;
        throw;
    }
    if (nms_engine_ == nullptr) {
        std::cerr << "Failed to build RPP NMS engine." << std::endl;
        return false;
    }
    nms_context_.reset(nms_engine_->createExecutionContext());
    if (nms_context_ == nullptr) {
        std::cerr << "Failed to create RPP NMS context." << std::endl;
        return false;
    }

    nms_boxes_index_ = nms_engine_->getBindingIndex("input_boxes");
    nms_scores_index_ = nms_engine_->getBindingIndex("input_scores");
    nms_max_output_index_ = nms_engine_->getBindingIndex("input_max_out_per_class");
    nms_iou_threshold_index_ = nms_engine_->getBindingIndex("input_iou_threshold");
    nms_score_threshold_index_ = nms_engine_->getBindingIndex("input_score_threshold");
    nms_indices_index_ = nms_engine_->getBindingIndex("output_indices");
    nms_count_index_ = nms_engine_->getBindingIndex("output_num_out_boxes");
    if (nms_boxes_index_ < 0 || nms_scores_index_ < 0 || nms_max_output_index_ < 0 ||
        nms_iou_threshold_index_ < 0 || nms_score_threshold_index_ < 0 ||
        nms_indices_index_ < 0 || nms_count_index_ < 0) {
        std::cerr << "Invalid RPP NMS binding index." << std::endl;
        return false;
    }

    infer1::Dims indices_dims = nms_engine_->getBindingDimensions(nms_indices_index_);
    infer1::Dims count_dims = nms_engine_->getBindingDimensions(nms_count_index_);
    nms_indices_elements_ = dims_volume(indices_dims);
    nms_count_elements_ = dims_volume(count_dims);
    if (indices_dims.nbDims > 0) {
        int last_dim = indices_dims.d[indices_dims.nbDims - 1];
        if (last_dim == 2 || last_dim == 3) {
            nms_index_tuple_size_ = last_dim;
        }
    }
    if (nms_indices_elements_ <= 0 || nms_count_elements_ <= 0 || nms_index_tuple_size_ <= 0) {
        std::cerr << "Invalid RPP NMS output dimensions." << std::endl;
        return false;
    }
    return true;
}

bool RppYoloPostprocessor::allocateBuffers()
{
    const size_t output_elems = static_cast<size_t>(config_.output_rows) *
                                static_cast<size_t>(config_.output_dimensions);
    const size_t boxes_elems = static_cast<size_t>(config_.output_rows) * 4U;
    const size_t scores_elems = static_cast<size_t>(config_.output_rows) *
                                static_cast<size_t>(score_channels(config_));

    if (!allocate_device(&yolo_bf16_device_, output_elems * sizeof(uint16_t), "rtMalloc yolo bf16") ||
        !allocate_device(&boxes_bf16_device_, boxes_elems * sizeof(uint16_t), "rtMalloc boxes bf16") ||
        !allocate_device(&scores_bf16_device_, scores_elems * sizeof(uint16_t), "rtMalloc scores bf16")) {
        return false;
    }

    if (nms_context_ != nullptr) {
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
    if (nms_context_ != nullptr && nms_max_output_index_ >= 0 &&
        !allocate_device(&max_output_per_class_device_, sizeof(int32_t), "rtMalloc NMS max output per class")) {
        return false;
    }
    return true;
}

bool RppYoloPostprocessor::copyConstantsToDevice()
{
    if (nms_context_ == nullptr) {
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

bool RppYoloPostprocessor::run(const void* yolo_output_device,
                               const LetterboxInfo& letterbox,
                               std::vector<Detection>& detections,
                               rtStream_t stream,
                               RppPostprocessProfile* profile)
{
    reset_profile(profile);
    detections.clear();
    if (!initialized_ || yolo_output_device == nullptr) {
        std::cerr << "RPP YOLO postprocessor has not been initialized." << std::endl;
        return false;
    }
    if (letterbox.scale <= 0.0f || letterbox.source_width <= 0 || letterbox.source_height <= 0) {
        std::cerr << "Invalid letterbox metadata for RPP postprocess." << std::endl;
        return false;
    }

    auto total_start = ProfileClock::now();
    rtStream_t run_stream = stream;
    bool owns_stream = false;
    if (run_stream == nullptr) {
        if (!check_rt(rtStreamCreate(&run_stream), "rtStreamCreate RPP postprocess")) {
            return false;
        }
        owns_stream = true;
    }

    std::vector<void*> cast_bindings(static_cast<size_t>(cast_engine_->getNbBindings()), nullptr);
    cast_bindings[static_cast<size_t>(cast_input_index_)] = const_cast<void*>(yolo_output_device);
    cast_bindings[static_cast<size_t>(cast_output_index_)] = yolo_bf16_device_;

    auto stage_start = ProfileClock::now();
    if (!cast_context_->enqueue(1, cast_bindings.data(), run_stream, nullptr)) {
        std::cerr << "RPP postprocess cast enqueue failed." << std::endl;
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (!check_rt(rtStreamSynchronize(run_stream), "rtStreamSynchronize RPP postprocess cast")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (profile != nullptr) {
        profile->cast_ms = elapsed_ms(stage_start);
    }

    stage_start = ProfileClock::now();
    try {
        infer1::rpp_nms_pre_slice_infer(as_rpp_deviceptr(yolo_bf16_device_),
                                        as_rpp_deviceptr(boxes_bf16_device_),
                                        as_rpp_deviceptr(scores_bf16_device_),
                                        config_.output_rows,
                                        score_channels(config_),
                                        run_stream);
    }
    catch (const std::exception& e) {
        std::cerr << "RPP nms_pre_slice failed: " << e.what() << std::endl;
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (!check_rt(rtStreamSynchronize(run_stream), "rtStreamSynchronize RPP nms_pre_slice")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (profile != nullptr) {
        profile->nms_slice_ms = elapsed_ms(stage_start);
    }

    std::vector<void*> nms_bindings(static_cast<size_t>(nms_engine_->getNbBindings()), nullptr);
    nms_bindings[static_cast<size_t>(nms_boxes_index_)] = boxes_bf16_device_;
    nms_bindings[static_cast<size_t>(nms_scores_index_)] = scores_bf16_device_;
    if (nms_max_output_index_ >= 0) {
        nms_bindings[static_cast<size_t>(nms_max_output_index_)] = max_output_per_class_device_;
    }
    nms_bindings[static_cast<size_t>(nms_iou_threshold_index_)] = iou_threshold_device_;
    nms_bindings[static_cast<size_t>(nms_score_threshold_index_)] = score_threshold_device_;
    nms_bindings[static_cast<size_t>(nms_indices_index_)] = nms_indices_device_;
    nms_bindings[static_cast<size_t>(nms_count_index_)] = nms_count_device_;

    stage_start = ProfileClock::now();
    try {
        if (!nms_context_->enqueue(1, nms_bindings.data(), run_stream, nullptr)) {
            std::cerr << "RPP NMS enqueue failed." << std::endl;
            if (owns_stream) {
                rtStreamDestroy(run_stream);
            }
            return false;
        }
        if (!check_rt(rtStreamSynchronize(run_stream), "rtStreamSynchronize RPP NMS")) {
            if (owns_stream) {
                rtStreamDestroy(run_stream);
            }
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "RPP NMS failed: " << e.what() << std::endl;
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (profile != nullptr) {
        profile->nms_ms = elapsed_ms(stage_start);
    }

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
    if (std::getenv("YOLO_DEBUG_OUTPUT") != nullptr) {
        std::cerr << "RPP postprocess selected_count=" << selected_count
                  << ", max_selected=" << max_selected << std::endl;
        int debug_count = std::min(selected_count, 3);
        if (debug_count > 0) {
            std::cerr << "RPP postprocess first indices:";
            for (int i = 0; i < debug_count * nms_index_tuple_size_; ++i) {
                std::cerr << " " << indices_data[i];
            }
            std::cerr << " tuple_size=" << nms_index_tuple_size_ << std::endl;
        }
    }

    size_t compact_d2h_bytes = nms_outputs_bytes_ + boxes_bf16_bytes_;
    double compact_d2h_ms = d2h_ms;
    if (selected_count > 0) {
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
        if (std::getenv("YOLO_DEBUG_OUTPUT") != nullptr && !class_ids.empty() && boxes.size() >= 4U) {
            std::cerr << "RPP postprocess first packed class=" << class_ids[0]
                      << ", box_bf16_as_float=(" << bf16_to_float(boxes[0]) << ","
                      << bf16_to_float(boxes[1]) << "," << bf16_to_float(boxes[2])
                      << "," << bf16_to_float(boxes[3]) << ")" << std::endl;
        }

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
    if (std::getenv("YOLO_DEBUG_OUTPUT") != nullptr) {
        std::cerr << "RPP postprocess parsed detections=" << detections.size();
        if (!detections.empty()) {
            const Detection& detection = detections.front();
            std::cerr << ", first_class=" << detection.class_id
                      << ", first_box=(" << detection.box.x << ","
                      << detection.box.y << "," << detection.box.width
                      << "," << detection.box.height << ")";
        }
        std::cerr << std::endl;
    }

    if (owns_stream) {
        check_rt(rtStreamDestroy(run_stream), "rtStreamDestroy RPP postprocess");
    }
    if (profile != nullptr) {
        profile->total_ms = elapsed_ms(total_start);
    }
    return true;
}

bool RppYoloPostprocessor::parseCompactDetections(const std::vector<int32_t>& class_ids,
                                                  const std::vector<uint16_t>& boxes,
                                                  const LetterboxInfo& letterbox,
                                                  std::vector<Detection>& detections) const
{
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

const void* RppYoloPostprocessor::getSlicedScoresDeviceBuffer() const
{
    return scores_bf16_device_;
}

int RppYoloPostprocessor::getSlicedScoreRows() const
{
    return config_.output_rows;
}

int RppYoloPostprocessor::getSlicedScoreChannels() const
{
    return initialized_ ? score_channels(config_) : 0;
}

void RppYoloPostprocessor::releaseBuffers()
{
    void** buffers[] = {
        &yolo_bf16_device_,
        &boxes_bf16_device_,
        &scores_bf16_device_,
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

void RppYoloPostprocessor::resetEngines()
{
    cast_context_.reset();
    cast_engine_.reset();
    nms_context_.reset();
    nms_engine_.reset();
    cast_input_index_ = -1;
    cast_output_index_ = -1;
    nms_boxes_index_ = -1;
    nms_scores_index_ = -1;
    nms_max_output_index_ = -1;
    nms_iou_threshold_index_ = -1;
    nms_score_threshold_index_ = -1;
    nms_indices_index_ = -1;
    nms_count_index_ = -1;
    nms_index_tuple_size_ = 2;
    nms_indices_elements_ = 0;
    nms_count_elements_ = 0;
}
