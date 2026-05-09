/**
 * @file yolo.h
 * @brief YOLOv5 inference wrapper declaration.
 */
#ifndef XDLTEK_SAMPLES_YOLO_H
#define XDLTEK_SAMPLES_YOLO_H

#include "rpp_buffer_manager.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>


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

    int getInputWidth() const { return input_width_; }
    int getInputHeight() const { return input_height_; }
    size_t getInputSize() const { return input_tensor_size_; }
    size_t getOutputSize() const { return output_tensor_size_; }
    size_t getInputByteSize() const;
    size_t getOutputByteSize() const;

    const std::string& getInputName() const { return input_name_; }
    const std::string& getOutputName() const { return output_name_; }
    infer1::Dims getInputDimensions() const { return input_dimensions_; }
    infer1::Dims getOutputDimensions() const { return output_dimensions_; }
    infer1::DataType getInputDataType() const { return input_data_type_; }
    infer1::DataType getOutputDataType() const { return output_data_type_; }

    void* getInputHostBuffer() const;
    void* getOutputHostBuffer() const;
    void* getInputDeviceBuffer() const;
    void* getOutputDeviceBuffer() const;

    infer1::IExecutionContext* getExecutionContext() const { return context_ptr_.get(); }
    std::vector<void*>& getDeviceBindings();
    const std::vector<void*>& getDeviceBindings() const;

    bool copyInputToDevice();
    bool copyOutputToHost();
    bool warmup();
    bool execute();
    bool enqueue(rtStream_t stream);
    bool isWarmupDone() const { return warmup_done_; }

private:
    //void preprocess(cv::Mat& image);
    bool executeContext();

    std::shared_ptr<infer1::IEngine> engine_ptr_ {nullptr};
    std::shared_ptr<samplesCommon::RppBufferManager> buffer_ptr_ {nullptr};
    std::unique_ptr<infer1::IExecutionContext> context_ptr_ {nullptr};

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
    infer1::DataType input_data_type_ {infer1::DataType::kFLOAT};
    infer1::DataType output_data_type_ {infer1::DataType::kFLOAT};
    bool warmup_done_ = false;

};


#endif //XDLTEK_SAMPLES_YOLO_H
