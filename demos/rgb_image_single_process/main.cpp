/**
 * @file main.cpp
 * @brief Single-process YOLOv5 RGB/BGR image demo.
 */
#include "argparse/argparse.hpp"
#include "labels.h"
#include "logger.h"
#include "perf_trace_session.h"
#include "utils.hpp"
#include "visualize.h"
#include "yolo_pipeline.h"

#include <opencv2/imgcodecs.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
double bytes_to_kib(size_t bytes)
{
    return static_cast<double>(bytes) / 1024.0;
}

void print_summary(const YoloV5StageTimes& times, const std::string& output_path)
{
    double fps = times.end_to_end_ms > 0.0 ? 1000.0 / times.end_to_end_ms : 0.0;
    std::cout << std::fixed << std::setprecision(3)
              << "\nSummary:\n"
              << "Input H2D: " << times.input_h2d_ms << " ms, "
              << times.input_h2d_bytes << " bytes, "
              << bytes_to_kib(times.input_h2d_bytes) << " KiB\n"
              << "Preprocess: " << times.preprocess_ms << " ms\n"
              << "Inference: " << times.inference_ms << " ms\n"
              << "Postprocess: " << times.postprocess_ms << " ms\n"
              << "Output D2H: " << times.output_d2h_ms << " ms, "
              << times.output_d2h_bytes << " bytes, "
              << bytes_to_kib(times.output_d2h_bytes) << " KiB\n"
              << "All time end to end: " << times.end_to_end_ms << " ms\n"
              << "FPS: " << fps << "\n"
              << "Output path: " << output_path << std::endl;
}

YoloV5StageTimes average_times(const YoloV5StageTimes& total, int count)
{
    YoloV5StageTimes average;
    double scale = count > 0 ? 1.0 / static_cast<double>(count) : 1.0;
    average.input_h2d_ms = total.input_h2d_ms * scale;
    average.input_h2d_bytes = count > 0 ? total.input_h2d_bytes / static_cast<size_t>(count) : total.input_h2d_bytes;
    average.output_d2h_ms = total.output_d2h_ms * scale;
    average.output_d2h_bytes = count > 0 ? total.output_d2h_bytes / static_cast<size_t>(count) : total.output_d2h_bytes;
    average.preprocess_ms = total.preprocess_ms * scale;
    average.inference_ms = total.inference_ms * scale;
    average.postprocess_ms = total.postprocess_ms * scale;
    average.end_to_end_ms = total.end_to_end_ms * scale;
    return average;
}
}

int main(int argc, char** argv)
{
    argparse::ArgumentParser program("yolov5 rgb image demo", "0.1", argparse::default_arguments::help);

    program.add_argument("-o", "--onnx")
            .required()
            .help("path to the YOLOv5 ONNX model file.");
    program.add_argument("-i", "--image")
            .default_value(std::string("test_1.png"))
            .help("path of RGB/BGR image file.");
    program.add_argument("--output")
            .default_value(std::string("output_rgb.jpg"))
            .help("path of visualization output image.");
    program.add_argument("--loop")
            .default_value(1)
            .help("measured full-pipeline loop count after one warmup pass.")
            .scan<'i', int>();
    program.add_argument("--perf")
            .help("save rpp_perf trace json.")
            .default_value(false)
            .implicit_value(true);
    program.add_argument("--perf-dir")
            .default_value(std::string("trace"))
            .help("rpp_perf trace output directory.");
    program.add_argument("-v", "--verbose")
            .help("show verbose runtime log.")
            .default_value(false)
            .implicit_value(true);

    auto preprocessed_arguments = preprocess_args(argc, argv);
    std::vector<const char*> fixed_arguments;
    to_char_argument_vector(preprocessed_arguments, argv, fixed_arguments);
    try {
        program.parse_args(static_cast<int>(fixed_arguments.size()), fixed_arguments.data());
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program << std::endl;
        return EXIT_FAILURE;
    }

    std::string model_path = expand_user_path(program.get<std::string>("--onnx"));
    std::string image_path = expand_user_path(program.get<std::string>("--image"));
    std::string output_path = expand_user_path(program.get<std::string>("--output"));
    int repeat_count = program.get<int>("--loop");
    if (repeat_count <= 0) {
        std::cerr << "--loop must be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }

    sample::gLogger.setReportableSeverity(program["--verbose"] == true ?
                                          infer1::ILogger::Severity::kVERBOSE :
                                          infer1::ILogger::Severity::kERROR);

    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Cannot found onnx model file, path: " << model_path << std::endl;
        return EXIT_FAILURE;
    }
    if (!std::filesystem::exists(image_path)) {
        std::cerr << "Cannot found image file, path: " << image_path << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Model path: " << model_path << "\n"
              << "Inference image path: " << image_path << std::endl;

    cv::Mat frame = cv::imread(image_path);
    if (frame.empty()) {
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return EXIT_FAILURE;
    }

    PerfTraceSession trace_session("yolov5_rgb_image_demo",
                                   program["--perf"] == true,
                                   expand_user_path(program.get<std::string>("--perf-dir")));

    YoloV5PipelineOptions pipeline_options;
    pipeline_options.model_path = model_path;
    pipeline_options.inference_count = 1;

    YoloV5Pipeline pipeline;
    const std::vector<std::string>& class_list = coco80_class_labels();
    if (!pipeline.init(pipeline_options, class_list)) {
        return EXIT_FAILURE;
    }

    std::vector<Detection> warmup_output;
    if (!pipeline.runRgb(frame, warmup_output, nullptr)) {
        return EXIT_FAILURE;
    }

    std::vector<Detection> output;
    YoloV5StageTimes total_times;
    for (int i = 0; i < repeat_count; ++i) {
        YoloV5StageTimes loop_times;
        if (!pipeline.runRgb(frame, output, &loop_times)) {
            return EXIT_FAILURE;
        }
        total_times.input_h2d_ms += loop_times.input_h2d_ms;
        total_times.input_h2d_bytes += loop_times.input_h2d_bytes;
        total_times.output_d2h_ms += loop_times.output_d2h_ms;
        total_times.output_d2h_bytes += loop_times.output_d2h_bytes;
        total_times.preprocess_ms += loop_times.preprocess_ms;
        total_times.inference_ms += loop_times.inference_ms;
        total_times.postprocess_ms += loop_times.postprocess_ms;
        total_times.end_to_end_ms += loop_times.end_to_end_ms;
    }

    draw_detections(frame, output, class_list);
    if (!save_detection_image(output_path, frame)) {
        std::cerr << "Failed to save image: " << output_path << std::endl;
        return EXIT_FAILURE;
    }

    print_summary(average_times(total_times, repeat_count), output_path);
    return EXIT_SUCCESS;
}
