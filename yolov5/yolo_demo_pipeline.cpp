/**
 * @file yolo_demo_pipeline.cpp
 * @brief High-level YOLOv5 demo pipeline implementation.
 */
#include "yolo_demo_pipeline.h"

#include "logger.h"
#include "postprocess.h"
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
    double preprocess_ms = 0.0;
    double inference_ms = 0.0;
};

double profile_pass_total_ms(const ProfilePassResult& pass)
{
    return pass.preprocess_ms + pass.inference_ms;
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

bool copy_model_output_to_host(Yolo& yolo, std::vector<float>& output_data, double* device_to_host_ms)
{
    if (device_to_host_ms != nullptr) {
        *device_to_host_ms = 0.0;
    }

    auto d2h_start = std::chrono::high_resolution_clock::now();
    if (!yolo.copyOutputToHost()) {
        return false;
    }
    if (device_to_host_ms != nullptr) {
        auto d2h_stop = std::chrono::high_resolution_clock::now();
        *device_to_host_ms = std::chrono::duration<double, std::milli>(d2h_stop - d2h_start).count();
    }

    const float* output_buffer = static_cast<const float*>(yolo.getOutputHostBuffer());
    if (output_buffer == nullptr) {
        std::cerr << "Failed to get YOLO output host buffer." << std::endl;
        return false;
    }
    output_data.assign(output_buffer, output_buffer + yolo.getOutputSize());
    return true;
}

void log_output_stats_if_requested(const std::vector<float>& output_data,
                                   const YoloV5Config& config,
                                   const std::vector<std::string>& class_names)
{
    if (std::getenv("YOLO_DEBUG_OUTPUT") == nullptr) {
        return;
    }

    float max_objectness = 0.0f;
    float max_class_score = 0.0f;
    float max_combined_score = 0.0f;
    int objectness_candidates = 0;
    int decoded_candidates = 0;

    const float* data = output_data.data();
    for (int i = 0; i < config.output_rows; ++i) {
        float objectness = data[4];
        max_objectness = std::max(max_objectness, objectness);
        if (objectness >= config.confidence_threshold) {
            objectness_candidates++;
        }

        float class_score = data[5];
        for (int c = 1; c < static_cast<int>(class_names.size()); ++c) {
            class_score = std::max(class_score, data[5 + c]);
        }
        max_class_score = std::max(max_class_score, class_score);
        max_combined_score = std::max(max_combined_score, objectness * class_score);
        if (objectness >= config.confidence_threshold && class_score > config.score_threshold) {
            decoded_candidates++;
        }

        data += config.output_dimensions;
    }

    sample::user_visible_stream_log("YOLO output stats: rows=", config.output_rows,
                                    ", dims=", config.output_dimensions,
                                    ", max_objectness=", max_objectness,
                                    ", max_class_score=", max_class_score,
                                    ", max_obj_x_class=", max_combined_score,
                                    ", objectness_candidates=", objectness_candidates,
                                    ", decoded_candidates=", decoded_candidates);
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

    void* input_device = preprocessor.getInputDeviceBuffer(input.format, input.width, input.height);
    if (input_device == nullptr) {
        std::cerr << "Failed to allocate profile input device buffer." << std::endl;
        return false;
    }

    size_t required = rpp_preprocess_input_bytes(input.format, input.width, input.height);
    auto h2d_start = std::chrono::high_resolution_clock::now();
    if (rtMemcpy(input_device, input.data, required, rtMemcpyHostToDevice) != rtError_t::rtSuccess) {
        std::cerr << "Failed to copy profile input to device." << std::endl;
        return false;
    }
    auto h2d_stop = std::chrono::high_resolution_clock::now();

    host_to_device_ms = std::chrono::duration<double, std::milli>(h2d_stop - h2d_start).count();
    device_input.data = input_device;
    device_input.memory_type = PreprocessMemoryType::Device;
    device_input.bytes = required;
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
    return true;
}

void log_profile_result(double host_to_device_ms,
                        const ProfilePassResult& measured_pass,
                        double output_d2h_ms)
{
    sample::user_visible_stream_log("YOLO profile warmup: first preprocess + inference pass completed");
    sample::user_visible_stream_log("YOLO profile measured: preprocess=", measured_pass.preprocess_ms,
                                    " ms, inference=", measured_pass.inference_ms,
                                    " ms, preprocess_inference_total=", profile_pass_total_ms(measured_pass),
                                    " ms");
    sample::user_visible_stream_log("YOLO profile IO: H2D=", host_to_device_ms,
                                    " ms, D2H=", output_d2h_ms,
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
    RppYoloPreprocessor preprocessor;
    if (!preprocessor.init(input.width, input.height, yolo.getInputWidth(), yolo.getInputHeight())) {
        return false;
    }

    PreprocessInput device_input;
    double host_to_device_ms = 0.0;
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

    LetterboxInfo second_letterbox;
    ProfilePassResult measured_pass;
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
    log_model_input_stats_if_requested(yolo, second_letterbox);

    std::vector<float> output_data;
    double output_d2h_ms = 0.0;
    if (!copy_model_output_to_host(yolo, output_data, &output_d2h_ms)) {
        return false;
    }

    log_profile_result(host_to_device_ms, measured_pass, output_d2h_ms);
    log_output_stats_if_requested(output_data, config, class_names);

    std::string error;
    if (!decode_yolov5_output(output_data,
                              second_letterbox,
                              class_names,
                              config,
                              detections,
                              &error)) {
        std::cerr << error << std::endl;
        return false;
    }

    return true;
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

    std::vector<float> output_data;
    if (!copy_model_output_to_host(yolo,
                                   output_data,
                                   nullptr)) {
        return false;
    }
    log_output_stats_if_requested(output_data, config, class_names);

    std::string error;
    if (!decode_yolov5_output(output_data,
                              letterbox,
                              class_names,
                              config,
                              detections,
                              &error)) {
        std::cerr << error << std::endl;
        return false;
    }

    return true;
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
