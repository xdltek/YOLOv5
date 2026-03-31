/**
 * @file yolo.h
 * @brief YOLOv5 inference wrapper declaration.
 */
#ifndef XDLTEK_SAMPLES_YOLO_H
#define XDLTEK_SAMPLES_YOLO_H

#include "rpp_buffer_manager.h"

#include <string>
#include <vector>

#include <opencv2/opencv.hpp>


class Yolo {
public:
    explicit Yolo(const std::string& onnx_path)
        : onnx_model_path_(onnx_path) {}

    /**
     * @brief Build and initialize the inference engine from the ONNX model.
     * @return True when engine and buffers are initialized successfully.
     */
    bool init_engine();
    // virtual ~Yolo();

    /**
     * @brief Run inference on a preprocessed input tensor and collect output data.
     * @param processed_image Input tensor blob in OpenCV Mat layout.
     * @param inference_count Number of inference iterations for timing.
     * @param output_data Output vector filled with raw model results.
     * @return True when execution and output copy succeed.
     */
    bool infer(cv::Mat& processed_image, int inference_count, std::vector<float>& output_data);

    int getInputWidth() const { return input_width_; }
    int getInputHeight() const { return input_height_; }
    size_t getOutputSize() const { return output_tensor_size_; }

private:
    //void preprocess(cv::Mat& image);

    std::shared_ptr<infer1::IEngine> engine_ptr_ {nullptr};
    std::shared_ptr<samplesCommon::RppBufferManager> buffer_ptr_ {nullptr};

    int input_index_ = 0;
    int output_index_ = 0;
    int input_width_ = 0;
    int input_height_ = 0;
    size_t input_tensor_size_ = 0;
    size_t output_tensor_size_ = 0;
    std::string onnx_model_path_;

    std::string input_name_;
    std::string output_name_;
    infer1::Dims input_dimensions_;
    infer1::Dims output_dimensions_;
    infer1::DataType input_data_type_;
    infer1::DataType output_data_type_;

};


#endif //XDLTEK_SAMPLES_YOLO_H
