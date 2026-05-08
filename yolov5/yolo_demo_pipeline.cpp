/**
 * @file yolo_demo_pipeline.cpp
 * @brief High-level YOLOv5 demo pipeline implementation.
 */
#include "yolo_demo_pipeline.h"

#include "logger.h"
#include "postprocess.h"
#include "preprocess.h"
#include "yolo.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
bool copy_host_blob_to_model_input(Yolo& yolo, const cv::Mat& blob)
{
    if (yolo.getInputDataType() != infer1::DataType::kFLOAT || yolo.getOutputDataType() != infer1::DataType::kFLOAT) {
        std::cerr << "The current demo path only supports float input/output tensors." << std::endl;
        return false;
    }

    cv::Mat continuous_blob = blob.isContinuous() ? blob : blob.clone();
    size_t blob_bytes = continuous_blob.total() * continuous_blob.elemSize();
    if (blob_bytes != yolo.getInputByteSize()) {
        std::cerr << "Input blob byte size does not match model input byte size." << std::endl;
        return false;
    }

    void* input_buffer = yolo.getInputHostBuffer();
    if (input_buffer == nullptr) {
        std::cerr << "Failed to get YOLO input host buffer." << std::endl;
        return false;
    }
    memcpy(input_buffer, continuous_blob.data, blob_bytes);

    return yolo.copyInputToDevice();
}

bool execute_yolo(Yolo& yolo, int inference_count)
{
    if (!yolo.warmup()) {
        return false;
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

    if (inference_count == 1) {
        sample::user_visible_stream_log("inference takes: ", infer_cost, "  milliseconds, frames per second: ", int(1000.0f / infer_cost));
    }
    else {
        float average_cost = infer_cost / (float(inference_count) * 1.0f);
        sample::user_visible_stream_log("inference [", inference_count, "] times, total takes: ", infer_cost, "  milliseconds, average inference time: ", average_cost, ", frames per second: ", int(1000.0f / average_cost));
    }

    return true;
}

bool copy_model_output_to_host(Yolo& yolo, std::vector<float>& output_data)
{
    if (!yolo.copyOutputToHost()) {
        return false;
    }

    const float* output_buffer = static_cast<const float*>(yolo.getOutputHostBuffer());
    if (output_buffer == nullptr) {
        std::cerr << "Failed to get YOLO output host buffer." << std::endl;
        return false;
    }
    output_data.assign(output_buffer, output_buffer + yolo.getOutputSize());
    return true;
}

bool run_preprocess(const cv::Mat& image, const YoloV5Config& config, YoloPreprocessResult& preprocess_result)
{
    std::string error;
    if (!preprocess_yolov5(image, config, preprocess_result, &error)) {
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

    YoloPreprocessResult preprocess_result;
    if (!run_preprocess(image, options.config, preprocess_result)) {
        return false;
    }
    if (!copy_host_blob_to_model_input(yolo, preprocess_result.input_blob)) {
        return false;
    }

    if (!execute_yolo(yolo, options.inference_count)) {
        return false;
    }

    std::vector<float> output_data;
    if (!copy_model_output_to_host(yolo, output_data)) {
        return false;
    }

    std::string error;
    if (!decode_yolov5_output(output_data,
                              preprocess_result.padded_image.size(),
                              class_names,
                              options.config,
                              detections,
                              &error)) {
        std::cerr << error << std::endl;
        return false;
    }

    return true;
}
