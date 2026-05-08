/**
 * @file yolo.cpp
 * @brief YOLOv5 runtime wrapper implementation.
 */
#include "yolo.h"
#include "parser_api.h"
#include "logger.h"
#include "sampleCommon.h"

#include <stdexcept>

/**
 * @brief Parse ONNX graph, configure builder options, and create runtime buffers.
 * @return True when engine creation and binding metadata initialization succeed.
 */
bool Yolo::init_engine() {

    // Create builder as the entry point for network and engine creation.
    std::unique_ptr<infer1::IBuilder> builder {infer1::createInferBuilder(sample::gLogger.getLogger())};
    if (builder == nullptr)
    {
        std::cerr << "Unable to create builder object." << std::endl;
        return false;
    }

    // Create network definition to hold parsed ONNX operators and tensors.
    std::unique_ptr<infer1::INetworkDefinition> network { builder->createNetwork() };
    if (network == nullptr)
    {
        std::cerr << "Unable to create network object." << std::endl;
        return false;
    }

    // Prepare build configuration (workspace, precision flags, etc.).
    std::unique_ptr<infer1::IBuilderConfig> config {builder->createBuilderConfig()};
    if (config == nullptr)
    {
        std::cerr << "Unable to create config object." << std::endl;
        return false;
    }

    // Parse ONNX file into the network structure before building engine.
    std::unique_ptr<onnxparser::IParser> parser { onnxparser::createParser(*network, sample::gLogger.getLogger()) };
    if (parser == nullptr)
    {
        std::cerr << "Unable to parse ONNX model file: " << onnx_model_path_ << std::endl;
        return false;
    }
    // Import ONNX graph into runtime network definition.
    if (onnx_parser(onnx_model_path_, builder.get(), network.get(), parser.get()) != 0) {
        return false;
    }

    // Build executable engine from network and config.
    // Batch size is fixed to 1 for single-image demo inference.
    builder->setMaxBatchSize(1);
    // Workspace controls temporary memory available during engine build.
    config->setMaxWorkspaceSize((size_t) 1_GiB);
    // Prefer BF16 when platform and model support it.
    config->setFlag(BuilderFlag::kBF16);

    // Build the final executable engine that will be used for inference requests.
    engine_ptr_ = std::shared_ptr<infer1::IEngine>(builder->buildEngineWithConfig(*network, *config));
    if (!engine_ptr_.get()) {
        return false;
    }

    auto has_flag = config->getFlag(infer1::BuilderFlag::kINT8);
    if (has_flag) {
        // Inform users when quantized execution path is selected by model/config.
        sample::user_visible_log("Since the model includes quantization layers, inference is performed with int8 precision.");
    }

    // Query binding metadata used later by host/device buffer manager.
    for (int i =0; i < engine_ptr_->getNbBindings(); i++) {
        if (engine_ptr_->bindingIsInput(i)) {
            // Cache input binding metadata used for host buffer sizing and shape mapping.
            input_index_ = i;
            input_name_ = engine_ptr_->getBindingName(i);
            input_dimensions_ = engine_ptr_->getBindingDimensions(i);
            input_data_type_ = engine_ptr_->getBindingDataType(i);

            input_height_ = input_dimensions_.d[1];
            input_width_ = input_dimensions_.d[2];

            input_tensor_size_ = samplesCommon::volume(input_dimensions_);
        }
        else {
            // Cache output binding metadata used for post-processing buffer reads.
            output_index_ = i;
            output_name_ = engine_ptr_->getBindingName(i);
            output_dimensions_ = engine_ptr_->getBindingDimensions(i);
            output_data_type_ = engine_ptr_->getBindingDataType(i);

            output_tensor_size_ = samplesCommon::volume(output_dimensions_);
        }
    }
    // Allocate host/device buffer manager based on final engine bindings.
    buffer_ptr_ = std::make_shared<samplesCommon::RppBufferManager>(engine_ptr_);
    if (buffer_ptr_ == nullptr) {
        return false;
    }
    // Keep one execution context alive for the model instance. The caller can reuse
    // the same input/output buffers and context across multiple inference requests.
    context_ptr_.reset(engine_ptr_->createExecutionContext());
    if (context_ptr_ == nullptr) {
        return false;
    }
    warmup_done_ = false;
    return true;
}

size_t Yolo::getInputByteSize() const
{
    return input_tensor_size_ * samplesCommon::getElementSize(input_data_type_);
}

size_t Yolo::getOutputByteSize() const
{
    return output_tensor_size_ * samplesCommon::getElementSize(output_data_type_);
}

void* Yolo::getInputHostBuffer() const
{
    return buffer_ptr_ == nullptr ? nullptr : buffer_ptr_->getHostBuffer(input_name_);
}

void* Yolo::getOutputHostBuffer() const
{
    return buffer_ptr_ == nullptr ? nullptr : buffer_ptr_->getHostBuffer(output_name_);
}

void* Yolo::getInputDeviceBuffer() const
{
    return buffer_ptr_ == nullptr ? nullptr : buffer_ptr_->getDeviceBuffer(input_name_);
}

void* Yolo::getOutputDeviceBuffer() const
{
    return buffer_ptr_ == nullptr ? nullptr : buffer_ptr_->getDeviceBuffer(output_name_);
}

std::vector<void*>& Yolo::getDeviceBindings()
{
    if (buffer_ptr_ == nullptr) {
        throw std::runtime_error("Yolo engine has not been initialized.");
    }
    return buffer_ptr_->getDeviceBindings();
}

const std::vector<void*>& Yolo::getDeviceBindings() const
{
    if (buffer_ptr_ == nullptr) {
        throw std::runtime_error("Yolo engine has not been initialized.");
    }
    return buffer_ptr_->getDeviceBindings();
}

bool Yolo::copyInputToDevice()
{
    if (buffer_ptr_ == nullptr) {
        sample::LOG_ERROR() << "Yolo engine has not been initialized." << std::endl;
        return false;
    }
    buffer_ptr_->copyInputToDevice();
    return true;
}

bool Yolo::copyOutputToHost()
{
    if (buffer_ptr_ == nullptr) {
        sample::LOG_ERROR() << "Yolo engine has not been initialized." << std::endl;
        return false;
    }
    buffer_ptr_->copyOutputToHost();
    return true;
}

bool Yolo::executeContext()
{
    if (context_ptr_ == nullptr || buffer_ptr_ == nullptr) {
        sample::LOG_ERROR() << "Yolo engine has not been initialized." << std::endl;
        return false;
    }

    bool ok = context_ptr_->execute(1, buffer_ptr_->getDeviceBindings().data());
    if (!ok) {
        sample::LOG_ERROR() << "do inference failed." << std::endl;
        return false;
    }
    return true;
}

bool Yolo::warmup()
{
    if (warmup_done_) {
        return true;
    }

    bool ok = executeContext();
    if (ok) {
        warmup_done_ = true;
    }
    return ok;
}

bool Yolo::execute()
{
    if (!warmup()) {
        return false;
    }
    return executeContext();
}
