/**
 * @file yolo_pipeline.cpp
 * @brief Reusable YOLOv5 pipeline implementation.
 */
#include "yolo_pipeline.h"

#include "perf_trace_session.h"
#include "rpp_infer_engine.h"
#include "rpp_yolo_postprocessor.h"
#include "rpp_yolo_preprocessor.h"

#include <Infer.h>
#include <opencv2/core.hpp>

#include <chrono>
#include <iostream>

namespace
{
/**
 * @brief Fill runtime YOLOv5 tensor shape fields from the initialized inference engine.
 * @param infer_engine Initialized RPP inference engine.
 * @param config Pipeline configuration updated with model input and output dimensions.
 */
bool update_config_from_model(const RppInferEngine& infer_engine, YoloV5Config& config)
{
    // This pipeline currently expects the model binding format used by standard YOLOv5 ONNX exports.
    if (infer_engine.getInputDataType() != infer1::DataType::kFLOAT ||
        infer_engine.getOutputDataType() != infer1::DataType::kFLOAT) {
        std::cerr << "The current YOLOv5 path only supports float input/output tensors." << std::endl;
        return false;
    }

    // Input dimensions drive RPP letterbox preprocessing.
    config.input_width = infer_engine.getInputWidth();
    config.input_height = infer_engine.getInputHeight();

    // Output dimensions drive RPP addNMS configuration.
    infer1::Dims output_dims = infer_engine.getOutputDimensions();
    if (output_dims.nbDims <= 0) {
        std::cerr << "Invalid YOLO output dimensions." << std::endl;
        return false;
    }

    config.output_dimensions = output_dims.d[output_dims.nbDims - 1];
    if (config.output_dimensions <= 0) {
        std::cerr << "Invalid YOLO output dimension size." << std::endl;
        return false;
    }

    config.output_rows = static_cast<int>(infer_engine.getOutputSize() / static_cast<size_t>(config.output_dimensions));
    return true;
}

/**
 * @brief Build RPP postprocess configuration from model shape and class table size.
 * @param config Runtime YOLOv5 pipeline configuration.
 * @param class_count Number of supported class labels.
 */
RppYoloPostprocessConfig make_postprocess_config(const YoloV5Config& config, int class_count)
{
    RppYoloPostprocessConfig postprocess_config;
    postprocess_config.output_rows = config.output_rows;
    postprocess_config.output_dimensions = config.output_dimensions;
    postprocess_config.class_count = class_count;
    postprocess_config.confidence_threshold = config.confidence_threshold;
    postprocess_config.score_threshold = config.score_threshold;
    postprocess_config.nms_threshold = config.nms_threshold;
    postprocess_config.max_output_boxes_per_class = 200;
    postprocess_config.agnostic = true;
    postprocess_config.has_box_confidence = true;
    return postprocess_config;
}

/**
 * @brief Measure elapsed milliseconds between two host timestamps.
 */
double elapsed_ms(const std::chrono::high_resolution_clock::time_point& start,
                  const std::chrono::high_resolution_clock::time_point& stop)
{
    return std::chrono::duration<double, std::milli>(stop - start).count();
}
}

/**
 * @brief Construct an empty pipeline; call init() before running frames.
 */
YoloV5Pipeline::YoloV5Pipeline() = default;

/**
 * @brief Destroy module owners and release runtime resources.
 */
YoloV5Pipeline::~YoloV5Pipeline() = default;

/**
 * @brief Initialize model runtime, derive tensor shapes, and build postprocessing.
 */
bool YoloV5Pipeline::init(const YoloV5PipelineOptions& options, const std::vector<std::string>& class_names)
{
    PERF_SCOPE_CATE("yolov5_pipeline_init", "yolov5");
    // Store caller options so every measured run uses the same model and repeat settings.
    options_ = options;
    config_ = options.config;

    // Initialize the reusable runtime wrapper before deriving tensor-dependent pipeline config.
    infer_engine_ = std::make_unique<RppInferEngine>(options.model_path);
    if (!infer_engine_->init()) {
        std::cerr << "Failed to initialize RPP inference engine." << std::endl;
        return false;
    }

    if (!update_config_from_model(*infer_engine_, config_)) {
        return false;
    }

    // Build the postprocessor after model output dimensions are known.
    if (!initPostprocessor(class_names)) {
        return false;
    }

    initialized_ = true;
    return true;
}

/**
 * @brief Build the YOLOv5 postprocessor after model output dimensions are known.
 */
bool YoloV5Pipeline::initPostprocessor(const std::vector<std::string>& class_names)
{
    PERF_SCOPE_CATE("yolov5_pipeline_init_postprocess", "postprocess");
    // The class table size defines the number of score channels consumed by NMS.
    postprocessor_ = std::make_unique<RppYoloPostprocessor>();
    return postprocessor_->init(make_postprocess_config(config_, static_cast<int>(class_names.size())));
}

/**
 * @brief Create or reuse a preprocessor sized for the requested source image.
 */
bool YoloV5Pipeline::ensurePreprocessor(int width, int height)
{
    // Reuse the existing preprocessor while the new input fits its initialized maximum size.
    if (preprocessor_ != nullptr &&
        preprocessor_width_ >= width &&
        preprocessor_height_ >= height) {
        return true;
    }

    // Rebuild only when a larger input frame requires larger staging buffers.
    PERF_SCOPE_CATE("yolov5_pipeline_init_preprocess", "preprocess");
    preprocessor_ = std::make_unique<RppYoloPreprocessor>();
    if (!preprocessor_->init(width, height, config_.input_width, config_.input_height)) {
        preprocessor_.reset();
        return false;
    }
    preprocessor_width_ = width;
    preprocessor_height_ = height;
    return true;
}

/**
 * @brief Run a host BGR OpenCV image through the complete YOLOv5 pipeline.
 */
bool YoloV5Pipeline::runRgb(const cv::Mat& image,
                            std::vector<Detection>& detections,
                            YoloV5StageTimes* times)
{
    // Convert non-contiguous OpenCV images into a compact host buffer for RPP upload.
    if (image.empty()) {
        std::cerr << "Input image is empty." << std::endl;
        return false;
    }

    cv::Mat continuous_image = image.isContinuous() ? image : image.clone();
    return runInput(continuous_image.data,
                    continuous_image.total() * continuous_image.elemSize(),
                    continuous_image.cols,
                    continuous_image.rows,
                    static_cast<int>(continuous_image.step[0]),
                    continuous_image.channels(),
                    continuous_image.isContinuous(),
                    static_cast<int>(PreprocessInputFormat::BGR_HWC),
                    detections,
                    times);
}

/**
 * @brief Run a host I420 frame through the complete YOLOv5 pipeline.
 */
bool YoloV5Pipeline::runI420(const void* yuv_data,
                             size_t yuv_bytes,
                             int width,
                             int height,
                             std::vector<Detection>& detections,
                             YoloV5StageTimes* times)
{
    // I420 is already a compact single buffer: Y plane, then U plane, then V plane.
    return runInput(yuv_data,
                    yuv_bytes,
                    width,
                    height,
                    width,
                    1,
                    true,
                    static_cast<int>(PreprocessInputFormat::YUV_I420),
                    detections,
                    times);
}

/**
 * @brief Shared input path used by RGB and YUV demos to execute one full pipeline pass.
 */
bool YoloV5Pipeline::runInput(const void* data,
                              size_t bytes,
                              int width,
                              int height,
                              int row_stride_bytes,
                              int channels,
                              bool continuous,
                              int format,
                              std::vector<Detection>& detections,
                              YoloV5StageTimes* times)
{
    PERF_SCOPE_CATE("yolov5_pipeline_run", "yolov5");
    // Fail early before any runtime work when initialization or input metadata is invalid.
    if (!initialized_ || infer_engine_ == nullptr || postprocessor_ == nullptr) {
        std::cerr << "YOLOv5 pipeline has not been initialized." << std::endl;
        return false;
    }
    if (data == nullptr || width <= 0 || height <= 0 || bytes == 0) {
        std::cerr << "Invalid YOLOv5 input." << std::endl;
        return false;
    }
    if (!continuous) {
        std::cerr << "Only continuous host input is supported by the current pipeline." << std::endl;
        return false;
    }
    (void)row_stride_bytes;
    (void)channels;

    // Each call owns one full end-to-end pipeline measurement.
    detections.clear();
    YoloV5StageTimes local_times;
    auto e2e_start = std::chrono::high_resolution_clock::now();

    // Preprocessor workspace depends on source size and model input shape.
    if (!ensurePreprocessor(width, height)) {
        return false;
    }

    // Describe the host input buffer for the RPP preprocessing module.
    PreprocessInput input;
    input.data = data;
    input.width = width;
    input.height = height;
    input.format = static_cast<PreprocessInputFormat>(format);
    input.memory_type = PreprocessMemoryType::Host;
    input.bytes = bytes;

    LetterboxInfo letterbox;
    RppPreprocessProfile preprocess_profile;
    // Preprocess includes H2D source upload, SRAM staging, letterbox, resize, and normalize.
    auto preprocess_start = std::chrono::high_resolution_clock::now();
    {
        PERF_SCOPE_CATE("yolov5_pipeline_preprocess_stage", "preprocess");
        if (!preprocessor_->run(input,
                                infer_engine_->getInputDeviceBuffer(),
                                letterbox,
                                nullptr,
                                &preprocess_profile)) {
            return false;
        }
        preprocessor_->releaseSramBuffers();
    }
    auto preprocess_stop = std::chrono::high_resolution_clock::now();
    local_times.preprocess_ms = preprocess_profile.total_ms > 0.0 ?
                                preprocess_profile.total_ms :
                                elapsed_ms(preprocess_start, preprocess_stop);
    local_times.input_h2d_ms = preprocess_profile.host_to_device_ms;
    local_times.input_h2d_bytes = preprocess_profile.host_to_device_bytes;

    // Inference is defined as OpenRT/RppRT engine execution only.
    auto inference_start = std::chrono::high_resolution_clock::now();
    {
        PERF_SCOPE_CATE("yolov5_pipeline_inference_stage", "inference");
        // Inference is defined as the OpenRT/RppRT ONNX engine execution time only.
        for (int i = 0; i < options_.inference_count; ++i) {
            if (!infer_engine_->execute()) {
                return false;
            }
        }
    }
    auto inference_stop = std::chrono::high_resolution_clock::now();
    local_times.inference_ms = elapsed_ms(inference_start, inference_stop);

    // Postprocess runs RPP cast/slice/NMS and copies compact results back for drawing.
    RppPostprocessProfile postprocess_profile;
    auto postprocess_start = std::chrono::high_resolution_clock::now();
    {
        PERF_SCOPE_CATE("yolov5_pipeline_postprocess_stage", "postprocess");
        if (!postprocessor_->run(infer_engine_->getOutputDeviceBuffer(),
                                 letterbox,
                                 detections,
                                 nullptr,
                                 &postprocess_profile)) {
            return false;
        }
    }
    auto postprocess_stop = std::chrono::high_resolution_clock::now();
    local_times.postprocess_ms = postprocess_profile.total_ms > 0.0 ?
                                 postprocess_profile.total_ms :
                                 elapsed_ms(postprocess_start, postprocess_stop);
    local_times.output_d2h_ms = postprocess_profile.device_to_host_ms;
    local_times.output_d2h_bytes = postprocess_profile.device_to_host_bytes;

    auto e2e_stop = std::chrono::high_resolution_clock::now();
    local_times.end_to_end_ms = elapsed_ms(e2e_start, e2e_stop);

    if (times != nullptr) {
        *times = local_times;
    }
    return true;
}
