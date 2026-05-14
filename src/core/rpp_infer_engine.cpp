/**
 * @file rpp_infer_engine.cpp
 * @brief Reusable RppRT inference engine wrapper implementation.
 */
#include "rpp_infer_engine.h"

#include "logger.h"
#include "parser_api.h"
#include "sampleCommon.h"

#include <iostream>
#include <stdexcept>

namespace
{
bool parseInputSpatialDims(const infer1::Dims& dims, int& width, int& height)
{
    if (dims.nbDims < 2) {
        return false;
    }

    if (dims.nbDims >= 3 && dims.d[dims.nbDims - 1] == 3) {
        height = dims.d[dims.nbDims - 3];
        width = dims.d[dims.nbDims - 2];
    }
    else {
        height = dims.d[dims.nbDims - 2];
        width = dims.d[dims.nbDims - 1];
    }

    return width > 0 && height > 0;
}
}

bool RppInferEngine::init()
{
    std::unique_ptr<infer1::IBuilder> builder {infer1::createInferBuilder(sample::gLogger.getLogger())};
    if (builder == nullptr) {
        std::cerr << "Unable to create builder object." << std::endl;
        return false;
    }

    std::unique_ptr<infer1::INetworkDefinition> network {builder->createNetwork()};
    if (network == nullptr) {
        std::cerr << "Unable to create network object." << std::endl;
        return false;
    }

    std::unique_ptr<infer1::IBuilderConfig> config {builder->createBuilderConfig()};
    if (config == nullptr) {
        std::cerr << "Unable to create config object." << std::endl;
        return false;
    }

    std::unique_ptr<onnxparser::IParser> parser {onnxparser::createParser(*network, sample::gLogger.getLogger())};
    if (parser == nullptr) {
        std::cerr << "Unable to parse ONNX model file: " << onnx_model_path_ << std::endl;
        return false;
    }
    if (onnx_parser(onnx_model_path_, builder.get(), network.get(), parser.get()) != 0) {
        return false;
    }

    builder->setMaxBatchSize(1);
    config->setMaxWorkspaceSize((size_t)1_GiB);
    config->setFlag(BuilderFlag::kBF16);

    engine_ptr_ = std::shared_ptr<infer1::IEngine>(builder->buildEngineWithConfig(*network, *config));
    if (!engine_ptr_) {
        return false;
    }

    if (config->getFlag(infer1::BuilderFlag::kINT8)) {
        sample::user_visible_log("Since the model includes quantization layers, inference is performed with int8 precision.");
    }

    for (int i = 0; i < engine_ptr_->getNbBindings(); i++) {
        if (engine_ptr_->bindingIsInput(i)) {
            input_index_ = i;
            input_name_ = engine_ptr_->getBindingName(i);
            input_dimensions_ = engine_ptr_->getBindingDimensions(i);
            input_data_type_ = engine_ptr_->getBindingDataType(i);

            if (!parseInputSpatialDims(input_dimensions_, input_width_, input_height_)) {
                std::cerr << "Invalid model input dimensions." << std::endl;
                return false;
            }
            input_tensor_size_ = samplesCommon::volume(input_dimensions_);
        }
        else {
            output_index_ = i;
            output_name_ = engine_ptr_->getBindingName(i);
            output_dimensions_ = engine_ptr_->getBindingDimensions(i);
            output_data_type_ = engine_ptr_->getBindingDataType(i);
            output_tensor_size_ = samplesCommon::volume(output_dimensions_);
        }
    }

    buffer_ptr_ = std::make_shared<samplesCommon::RppBufferManager>(engine_ptr_);
    if (buffer_ptr_ == nullptr) {
        return false;
    }

    context_ptr_.reset(engine_ptr_->createExecutionContext());
    return context_ptr_ != nullptr;
}

size_t RppInferEngine::getInputByteSize() const
{
    return input_tensor_size_ * samplesCommon::getElementSize(input_data_type_);
}

size_t RppInferEngine::getOutputByteSize() const
{
    return output_tensor_size_ * samplesCommon::getElementSize(output_data_type_);
}

void* RppInferEngine::getInputHostBuffer() const
{
    return buffer_ptr_ == nullptr ? nullptr : buffer_ptr_->getHostBuffer(input_name_);
}

void* RppInferEngine::getOutputHostBuffer() const
{
    return buffer_ptr_ == nullptr ? nullptr : buffer_ptr_->getHostBuffer(output_name_);
}

void* RppInferEngine::getInputDeviceBuffer() const
{
    return buffer_ptr_ == nullptr ? nullptr : buffer_ptr_->getDeviceBuffer(input_name_);
}

void* RppInferEngine::getOutputDeviceBuffer() const
{
    return buffer_ptr_ == nullptr ? nullptr : buffer_ptr_->getDeviceBuffer(output_name_);
}

std::vector<void*>& RppInferEngine::getDeviceBindings()
{
    if (buffer_ptr_ == nullptr) {
        throw std::runtime_error("RPP inference engine has not been initialized.");
    }
    return buffer_ptr_->getDeviceBindings();
}

const std::vector<void*>& RppInferEngine::getDeviceBindings() const
{
    if (buffer_ptr_ == nullptr) {
        throw std::runtime_error("RPP inference engine has not been initialized.");
    }
    return buffer_ptr_->getDeviceBindings();
}

bool RppInferEngine::copyInputToDevice()
{
    if (buffer_ptr_ == nullptr) {
        sample::LOG_ERROR() << "RPP inference engine has not been initialized." << std::endl;
        return false;
    }
    buffer_ptr_->copyInputToDevice();
    return true;
}

bool RppInferEngine::copyOutputToHost()
{
    if (buffer_ptr_ == nullptr) {
        sample::LOG_ERROR() << "RPP inference engine has not been initialized." << std::endl;
        return false;
    }
    buffer_ptr_->copyOutputToHost();
    return true;
}

bool RppInferEngine::executeContext()
{
    if (context_ptr_ == nullptr || buffer_ptr_ == nullptr) {
        sample::LOG_ERROR() << "RPP inference engine has not been initialized." << std::endl;
        return false;
    }

    bool ok = context_ptr_->execute(1, buffer_ptr_->getDeviceBindings().data());
    if (!ok) {
        sample::LOG_ERROR() << "do inference failed." << std::endl;
        return false;
    }
    return true;
}

bool RppInferEngine::enqueue(rtStream_t stream)
{
    if (context_ptr_ == nullptr || buffer_ptr_ == nullptr) {
        sample::LOG_ERROR() << "RPP inference engine has not been initialized." << std::endl;
        return false;
    }

    bool ok = context_ptr_->enqueue(1, buffer_ptr_->getDeviceBindings().data(), stream, nullptr);
    if (!ok) {
        sample::LOG_ERROR() << "enqueue inference failed." << std::endl;
        return false;
    }
    return true;
}

bool RppInferEngine::execute()
{
    return executeContext();
}
