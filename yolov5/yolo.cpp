/**
 * @file yolo.cpp
 * @brief YOLOv5 runtime wrapper implementation.
 */
#include "yolo.h"
#include "parser_api.h"
#include "logger.h"
#include "sampleCommon.h"

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
            input_name_ = engine_ptr_->getBindingName(i);
            input_dimensions_ = engine_ptr_->getBindingDimensions(i);
            input_data_type_ = engine_ptr_->getBindingDataType(i);

            input_height_ = input_dimensions_.d[1];
            input_width_ = input_dimensions_.d[2];

            input_tensor_size_ = samplesCommon::volume(input_dimensions_);
        }
        else {
            // Cache output binding metadata used for post-processing buffer reads.
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
    return true;
}

/**
 * @brief Copy input tensor to device, execute inference, and read back output tensor.
 * @param processed_image Input tensor blob produced by preprocessing.
 * @param inference_count Number of execution loops used for profiling.
 * @param output_data Output vector that receives flattened model output.
 * @return True when inference and host output collection complete.
 */
bool Yolo::infer(cv::Mat& processed_image, int inference_count, std::vector<float>& output_data) {

    // Flatten OpenCV blob memory to contiguous float vector expected by runtime host buffer.
    std::vector<float> input_tensor_values;
    input_tensor_values.assign((float*)processed_image.datastart, (float*)processed_image.dataend);

    // Copy host input tensor into the runtime-managed host buffer.
    // getElementSize handles runtime data type width (fp32/fp16/int8...) correctly.
    size_t input_data_size = input_tensor_size_ * samplesCommon::getElementSize(input_data_type_);
    memcpy(buffer_ptr_->getHostBuffer(input_name_), input_tensor_values.data(), input_data_size);

    // Create per-run execution context; bindings are reused from buffer manager.
    std::unique_ptr<infer1::IExecutionContext> context {engine_ptr_->createExecutionContext()};
    // Push prepared host input into device memory before execution.
    buffer_ptr_->copyInputToDevice();

    samplesCommon::PreciseCpuTimer infer_timer;

    // Warm-up run to avoid counting one-time initialization overhead.
    bool ok = context->execute(1, buffer_ptr_->getDeviceBindings().data());
    if (!ok)
    {
        sample::LOG_ERROR() << "do inference failed." << std::endl;
        return false;
    }

    infer_timer.reset();
    infer_timer.start();

    // Execute multiple rounds for stable latency/FPS measurement.
    for (int i = 0; i < inference_count; i++)
    {
        ok = context->execute(1, buffer_ptr_->getDeviceBindings().data());
        if (!ok)
        {
            throw std::runtime_error("execute failed.");
        }
    }

    infer_timer.stop();
    float infer_cost = infer_timer.milliseconds();

    if (inference_count == 1) {
        // Report single-run latency and derived FPS.
        sample::user_visible_stream_log("inference takes: ", infer_cost, "  milliseconds, frames per second: ", int(1000.0f / infer_cost));
    }
    else {
        // Report aggregated and averaged metrics for loop-mode benchmarking.
        float average_cost = infer_cost / (float(inference_count) * 1.0f);
        sample::user_visible_stream_log("inference [", inference_count, "] times, total takes: ", infer_cost, "  milliseconds, average inference time: ", average_cost, ", frames per second: ", int(1000.0f / average_cost));
    }

    // Pull inference outputs back to host memory for post-processing.
    buffer_ptr_->copyOutputToHost();

    // Flatten output tensor from host buffer into std::vector for downstream decode logic.
    output_data.clear();
    float* output_data_ptr = (float*)buffer_ptr_->getHostBuffer(output_name_);
    for (int i = 0; i < output_tensor_size_; i++) {
        output_data.emplace_back(output_data_ptr[i]);
    }

    return true;
}
