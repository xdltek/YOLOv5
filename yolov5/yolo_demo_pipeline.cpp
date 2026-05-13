/**
 * @file yolo_demo_pipeline.cpp
 * @brief High-level YOLOv5 demo pipeline implementation.
 */
#include "yolo_demo_pipeline.h"

#include "logger.h"
#include "rpp_yolo_postprocessor.h"
#include "rpp_yolo_preprocessor.h"
#include "yolo.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
bool update_config_from_model(const Yolo& yolo, YoloV5Config& config)
{
    if (yolo.getInputDataType() != infer1::DataType::kFLOAT ||
        yolo.getOutputDataType() != infer1::DataType::kFLOAT) {
        std::cerr << "The current demo path only supports float input/output tensors." << std::endl;
        return false;
    }

    config.input_width = yolo.getInputWidth();
    config.input_height = yolo.getInputHeight();

    infer1::Dims output_dims = yolo.getOutputDimensions();
    if (output_dims.nbDims <= 0) {
        std::cerr << "Invalid YOLO output dimensions." << std::endl;
        return false;
    }

    config.output_dimensions = output_dims.d[output_dims.nbDims - 1];
    if (config.output_dimensions <= 0) {
        std::cerr << "Invalid YOLO output dimension size." << std::endl;
        return false;
    }

    config.output_rows = static_cast<int>(yolo.getOutputSize() / static_cast<size_t>(config.output_dimensions));
    return true;
}

bool profile_requested()
{
    return std::getenv("YOLO_PROFILE") != nullptr;
}

struct ProfilePassResult
{
    RppPreprocessProfile preprocess;
    double preprocess_ms = 0.0;
    double inference_ms = 0.0;
    int inference_count = 0;
};

double profile_pass_total_ms(const ProfilePassResult& pass)
{
    return pass.preprocess_ms + pass.inference_ms;
}

double bytes_to_kib(size_t bytes)
{
    return static_cast<double>(bytes) / 1024.0;
}

bool execute_yolo(Yolo& yolo,
                  int inference_count,
                  double* warmup_ms,
                  double* inference_ms,
                  bool log_summary = true)
{
    if (warmup_ms != nullptr) {
        *warmup_ms = 0.0;
    }
    if (inference_ms != nullptr) {
        *inference_ms = 0.0;
    }

    bool needs_warmup = !yolo.isWarmupDone();
    auto warmup_start = std::chrono::high_resolution_clock::now();
    if (!yolo.warmup()) {
        return false;
    }
    if (warmup_ms != nullptr && needs_warmup) {
        auto warmup_stop = std::chrono::high_resolution_clock::now();
        *warmup_ms = std::chrono::duration<double, std::milli>(warmup_stop - warmup_start).count();
    }

    auto infer_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < inference_count; i++)
    {
        if (!yolo.execute())
        {
            throw std::runtime_error("execute failed.");
        }
    }

    auto infer_stop = std::chrono::high_resolution_clock::now();
    float infer_cost = std::chrono::duration<float, std::milli>(infer_stop - infer_start).count();
    if (inference_ms != nullptr) {
        *inference_ms = static_cast<double>(infer_cost);
    }

    if (log_summary) {
        if (inference_count == 1) {
            sample::user_visible_stream_log("inference takes: ", infer_cost, "  milliseconds, frames per second: ", int(1000.0f / infer_cost));
        }
        else {
            float average_cost = infer_cost / (float(inference_count) * 1.0f);
            sample::user_visible_stream_log("inference [", inference_count, "] times, total takes: ", infer_cost, "  milliseconds, average inference time: ", average_cost, ", frames per second: ", int(1000.0f / average_cost));
        }
    }

    return true;
}

bool run_rpp_preprocess(Yolo& yolo,
                        const PreprocessInput& input,
                        LetterboxInfo& letterbox,
                        RppPreprocessProfile* profile)
{
    void* input_device = yolo.getInputDeviceBuffer();
    if (input_device == nullptr) {
        std::cerr << "Failed to get YOLO input device buffer." << std::endl;
        return false;
    }

    RppYoloPreprocessor preprocessor;
    if (!preprocessor.init(input.width, input.height, yolo.getInputWidth(), yolo.getInputHeight())) {
        return false;
    }

    return preprocessor.run(input, input_device, letterbox, nullptr, profile);
}

bool copy_input_to_profile_device(RppYoloPreprocessor& preprocessor,
                                  const PreprocessInput& input,
                                  PreprocessInput& device_input,
                                  double& host_to_device_ms)
{
    host_to_device_ms = 0.0;
    device_input = input;
    if (input.memory_type == PreprocessMemoryType::Device) {
        return true;
    }

    if (!preprocessor.uploadInputToDevice(input, device_input, nullptr, &host_to_device_ms)) {
        std::cerr << "Failed to copy profile input to device." << std::endl;
        return false;
    }
    return true;
}

bool enqueue_profile_inference(Yolo& yolo, rtStream_t stream, double* inference_ms)
{
    auto infer_start = std::chrono::high_resolution_clock::now();
    if (!yolo.enqueue(stream)) {
        return false;
    }
    if (rtStreamSynchronize(stream) != rtError_t::rtSuccess) {
        std::cerr << "Failed to synchronize profile inference stream." << std::endl;
        return false;
    }
    if (inference_ms != nullptr) {
        auto infer_stop = std::chrono::high_resolution_clock::now();
        *inference_ms = std::chrono::duration<double, std::milli>(infer_stop - infer_start).count();
    }
    return true;
}

bool run_profile_warmup_pass(Yolo& yolo,
                             RppYoloPreprocessor& preprocessor,
                             const PreprocessInput& device_input,
                             rtStream_t inference_stream,
                             LetterboxInfo& letterbox)
{
    void* model_input_device = yolo.getInputDeviceBuffer();
    if (model_input_device == nullptr) {
        std::cerr << "Failed to get YOLO input device buffer." << std::endl;
        return false;
    }

    if (!preprocessor.run(device_input, model_input_device, letterbox)) {
        return false;
    }

    return enqueue_profile_inference(yolo, inference_stream, nullptr);
}

bool run_measured_profile_pass(Yolo& yolo,
                               RppYoloPreprocessor& preprocessor,
                               const PreprocessInput& device_input,
                               int inference_count,
                               rtStream_t inference_stream,
                               LetterboxInfo& letterbox,
                               ProfilePassResult& pass)
{
    void* model_input_device = yolo.getInputDeviceBuffer();
    if (model_input_device == nullptr) {
        std::cerr << "Failed to get YOLO input device buffer." << std::endl;
        return false;
    }

    RppPreprocessProfile preprocess_profile;
    if (!preprocessor.run(device_input, model_input_device, letterbox, nullptr, &preprocess_profile)) {
        return false;
    }
    pass.preprocess = preprocess_profile;
    pass.preprocess_ms = preprocess_profile.total_ms;

    auto infer_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < inference_count; ++i) {
        if (!yolo.enqueue(inference_stream)) {
            return false;
        }
    }
    if (rtStreamSynchronize(inference_stream) != rtError_t::rtSuccess) {
        std::cerr << "Failed to synchronize profile inference stream." << std::endl;
        return false;
    }
    auto infer_stop = std::chrono::high_resolution_clock::now();
    pass.inference_ms = std::chrono::duration<double, std::milli>(infer_stop - infer_start).count();
    pass.inference_count = inference_count;
    return true;
}

void log_profile_result(double host_to_device_ms,
                        const ProfilePassResult& measured_pass,
                        double output_d2h_ms,
                        const RppPostprocessProfile* postprocess_profile)
{
    double postprocess_total_ms = postprocess_profile == nullptr ? 0.0 : postprocess_profile->total_ms;
    double total_ms = host_to_device_ms + measured_pass.preprocess_ms +
                      measured_pass.inference_ms + postprocess_total_ms;

    sample::user_visible_stream_log("YOLO profile warmup: preprocess + inference + postprocess warmup completed");
    sample::user_visible_stream_log("YOLO profile preprocess: H2D=", host_to_device_ms,
                                    " ms, clear=", measured_pass.preprocess.model_input_clear_ms,
                                    " ms, resize_norm=", measured_pass.preprocess.resize_normalize_ms,
                                    " ms, total=", measured_pass.preprocess_ms,
                                    " ms");
    sample::user_visible_stream_log("YOLO profile inference: total=", measured_pass.inference_ms,
                                    " ms, loop_count=", measured_pass.inference_count,
                                    ", preprocess_inference_total=", profile_pass_total_ms(measured_pass),
                                    " ms");
    if (postprocess_profile != nullptr) {
        sample::user_visible_stream_log("YOLO profile postprocess: path=rpp, cast=", postprocess_profile->cast_ms,
                                        " ms, nms_slice=", postprocess_profile->nms_slice_ms,
                                        " ms, nms=", postprocess_profile->nms_ms,
                                        " ms, D2H=", postprocess_profile->device_to_host_ms,
                                        " ms, D2H_bytes=", postprocess_profile->device_to_host_bytes,
                                        " (", bytes_to_kib(postprocess_profile->device_to_host_bytes), " KiB)",
                                        ", total=", postprocess_profile->total_ms,
                                        " ms");
    }
    sample::user_visible_stream_log("YOLO profile IO: input_H2D=", host_to_device_ms,
                                    " ms, postprocess_D2H=", output_d2h_ms,
                                    " ms");
    sample::user_visible_stream_log("YOLO profile total: end_to_end=", total_ms,
                                    " ms");
}

void log_model_input_stats_if_requested(Yolo& yolo, const LetterboxInfo& letterbox)
{
    if (std::getenv("YOLO_DEBUG_OUTPUT") == nullptr) {
        return;
    }

    const size_t input_size = yolo.getInputSize();
    std::vector<float> input_data(input_size);
    if (rtMemcpy(input_data.data(),
                 yolo.getInputDeviceBuffer(),
                 input_size * sizeof(float),
                 rtMemcpyDeviceToHost) != rtError_t::rtSuccess) {
        std::cerr << "Failed to copy model input for debug stats." << std::endl;
        return;
    }

    float min_value = std::numeric_limits<float>::max();
    float max_value = std::numeric_limits<float>::lowest();
    double sum = 0.0;
    size_t non_zero_count = 0;
    for (float value : input_data) {
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
        sum += static_cast<double>(value);
        if (value != 0.0f) {
            non_zero_count++;
        }
    }

    sample::user_visible_stream_log("YOLO input stats: model_w=", yolo.getInputWidth(),
                                    ", model_h=", yolo.getInputHeight(),
                                    ", input_elems=", input_size,
                                    ", min=", min_value,
                                    ", max=", max_value,
                                    ", mean=", static_cast<float>(sum / static_cast<double>(input_size)),
                                    ", non_zero=", non_zero_count,
                                    ", letterbox_scale=", letterbox.scale,
                                    ", pad_x=", letterbox.pad_x,
                                    ", pad_y=", letterbox.pad_y,
                                    ", resized_w=", letterbox.resized_width,
                                    ", resized_h=", letterbox.resized_height);
}

bool run_profile_detection(Yolo& yolo,
                           const PreprocessInput& input,
                           const YoloV5Config& config,
                           int inference_count,
                           const std::vector<std::string>& class_names,
                           std::vector<Detection>& detections)
{
    double host_to_device_ms = 0.0;
    LetterboxInfo second_letterbox;
    ProfilePassResult measured_pass;
    {
        RppYoloPreprocessor preprocessor;
        if (!preprocessor.init(input.width, input.height, yolo.getInputWidth(), yolo.getInputHeight())) {
            return false;
        }

        PreprocessInput device_input;
        if (!copy_input_to_profile_device(preprocessor, input, device_input, host_to_device_ms)) {
            return false;
        }

        rtStream_t inference_stream = nullptr;
        if (rtStreamCreate(&inference_stream) != rtError_t::rtSuccess) {
            std::cerr << "Failed to create profile inference stream." << std::endl;
            return false;
        }

        LetterboxInfo first_letterbox;
        if (!run_profile_warmup_pass(yolo,
                                     preprocessor,
                                     device_input,
                                     inference_stream,
                                     first_letterbox)) {
            rtStreamDestroy(inference_stream);
            return false;
        }

        if (!run_measured_profile_pass(yolo,
                                       preprocessor,
                                       device_input,
                                       inference_count,
                                       inference_stream,
                                       second_letterbox,
                                       measured_pass)) {
            rtStreamDestroy(inference_stream);
            return false;
        }
        rtStreamDestroy(inference_stream);
    }
    log_model_input_stats_if_requested(yolo, second_letterbox);

    RppYoloPostprocessConfig postprocess_config;
    postprocess_config.output_rows = config.output_rows;
    postprocess_config.output_dimensions = config.output_dimensions;
    postprocess_config.class_count = static_cast<int>(class_names.size());
    postprocess_config.confidence_threshold = config.confidence_threshold;
    postprocess_config.score_threshold = config.score_threshold;
    postprocess_config.nms_threshold = config.nms_threshold;
    postprocess_config.max_output_boxes_per_class = 200;
    postprocess_config.agnostic = true;
    postprocess_config.has_box_confidence = true;

    RppYoloPostprocessor postprocessor;
    RppPostprocessProfile postprocess_profile;
    bool rpp_postprocess_ok = false;
    try {
        if (postprocessor.init(postprocess_config)) {
            std::vector<Detection> postprocess_warmup_detections;
            rpp_postprocess_ok = postprocessor.run(yolo.getOutputDeviceBuffer(),
                                                   second_letterbox,
                                                   postprocess_warmup_detections) &&
                                 postprocessor.run(yolo.getOutputDeviceBuffer(),
                                                   second_letterbox,
                                                   detections,
                                                   nullptr,
                                                   &postprocess_profile);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "RPP postprocess failed with exception: " << e.what() << std::endl;
    }
    if (rpp_postprocess_ok) {
        log_profile_result(host_to_device_ms,
                           measured_pass,
                           postprocess_profile.device_to_host_ms,
                           &postprocess_profile);
        return true;
    }

    std::cerr << "RPP postprocess failed." << std::endl;
    return false;
}

bool run_detection(Yolo& yolo,
                   const PreprocessInput& input,
                   const YoloV5Config& config,
                   int inference_count,
                   const std::vector<std::string>& class_names,
                   std::vector<Detection>& detections)
{
    LetterboxInfo letterbox;
    bool profiling_enabled = profile_requested();
    if (profiling_enabled) {
        return run_profile_detection(yolo, input, config, inference_count, class_names, detections);
    }

    if (!run_rpp_preprocess(yolo,
                            input,
                            letterbox,
                            nullptr)) {
        return false;
    }
    log_model_input_stats_if_requested(yolo, letterbox);

    if (!execute_yolo(yolo,
                      inference_count,
                      nullptr,
                      nullptr)) {
        return false;
    }

    RppYoloPostprocessConfig postprocess_config;
    postprocess_config.output_rows = config.output_rows;
    postprocess_config.output_dimensions = config.output_dimensions;
    postprocess_config.class_count = static_cast<int>(class_names.size());
    postprocess_config.confidence_threshold = config.confidence_threshold;
    postprocess_config.score_threshold = config.score_threshold;
    postprocess_config.nms_threshold = config.nms_threshold;
    postprocess_config.max_output_boxes_per_class = 200;
    postprocess_config.agnostic = true;
    postprocess_config.has_box_confidence = true;

    RppYoloPostprocessor postprocessor;
    bool rpp_postprocess_ok = false;
    try {
        rpp_postprocess_ok = postprocessor.init(postprocess_config) &&
                             postprocessor.run(yolo.getOutputDeviceBuffer(), letterbox, detections);
    }
    catch (const std::exception& e) {
        std::cerr << "RPP postprocess failed with exception: " << e.what() << std::endl;
    }
    if (rpp_postprocess_ok) {
        return true;
    }

    std::cerr << "RPP postprocess failed." << std::endl;
    return false;
}
}

bool detect_yolov5(const cv::Mat& image,
                   const YoloV5PipelineOptions& options,
                   const std::vector<std::string>& class_names,
                   std::vector<Detection>& detections)
{
    Yolo yolo(options.model_path);
    if (!yolo.init_engine()) {
        std::cerr << "Failed to initialize YOLO engine." << std::endl;
        return false;
    }

    YoloV5Config runtime_config = options.config;
    if (!update_config_from_model(yolo, runtime_config)) {
        return false;
    }

    cv::Mat continuous_image = image.isContinuous() ? image : image.clone();
    PreprocessInput input;
    input.data = continuous_image.data;
    input.width = continuous_image.cols;
    input.height = continuous_image.rows;
    input.format = PreprocessInputFormat::BGR_HWC;
    input.memory_type = PreprocessMemoryType::Host;
    input.bytes = continuous_image.total() * continuous_image.elemSize();

    return run_detection(yolo, input, runtime_config, options.inference_count, class_names, detections);
}

bool detect_yolov5_i420(const void* yuv_data,
                        size_t yuv_bytes,
                        int width,
                        int height,
                        const YoloV5PipelineOptions& options,
                        const std::vector<std::string>& class_names,
                        std::vector<Detection>& detections)
{
    Yolo yolo(options.model_path);
    if (!yolo.init_engine()) {
        std::cerr << "Failed to initialize YOLO engine." << std::endl;
        return false;
    }

    YoloV5Config runtime_config = options.config;
    if (!update_config_from_model(yolo, runtime_config)) {
        return false;
    }

    PreprocessInput input;
    input.data = yuv_data;
    input.width = width;
    input.height = height;
    input.format = PreprocessInputFormat::YUV_I420;
    input.memory_type = PreprocessMemoryType::Host;
    input.bytes = yuv_bytes;

    return run_detection(yolo, input, runtime_config, options.inference_count, class_names, detections);
}
