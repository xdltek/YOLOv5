/**
 * @file main.cpp
 * @brief Single-process YOLOv5 I420 YUV image demo.
 */
#include "argparse/argparse.hpp"
#include "labels.h"
#include "logger.h"
#include "perf_trace_session.h"
#include "utils.hpp"
#include "visualization.h"
#include "yolo_pipeline.h"

#include <opencv2/imgproc.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
/**
 * @brief Convert byte counts to KiB for the latency summary.
 * @param bytes Number of bytes transferred.
 */
double bytes_to_kib(size_t bytes)
{
    return static_cast<double>(bytes) / 1024.0;
}

/**
 * @brief Print the measured full-pipeline timing summary after warmup.
 * @param times Averaged stage timings collected from measured loops.
 * @param loop_count Number of measured loops included in the average.
 * @param output_path Path where the rendered detection image was saved.
 */
void print_summary(const YoloV5StageTimes& times, int loop_count, const std::string& output_path)
{
    // FPS is derived from the end-to-end latency measured after warmup.
    double fps = times.end_to_end_ms > 0.0 ? 1000.0 / times.end_to_end_ms : 0.0;
    std::cout << std::fixed << std::setprecision(3)
              << "\nSummary:\n"
              << "Loop count: " << loop_count << "\n"
              << "Loop time: average per measured loop after warmup\n"
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

/**
 * @brief Average accumulated stage timings across measured loops.
 * @param total Sum of all measured loop timings.
 * @param count Number of measured loops.
 */
YoloV5StageTimes average_times(const YoloV5StageTimes& total, int count)
{
    YoloV5StageTimes average;
    // Keep count validation local so callers can safely pass the requested loop count.
    double scale = count > 0 ? 1.0 / static_cast<double>(count) : 1.0;
    // Timing fields are accumulated as doubles and scaled into per-frame averages.
    average.input_h2d_ms = total.input_h2d_ms * scale;
    // Transfer sizes are byte counters, so integer division keeps the printed size exact per loop.
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

/**
 * @brief Run the I420 YUV image demo from CLI parsing through rendered output.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument values.
 */
int main(int argc, char** argv)
{
    // Step 1: declare the command-line contract for this demo.
    argparse::ArgumentParser program("yolov5 yuv image demo", "0.1", argparse::default_arguments::help);

    program.add_argument("-o", "--onnx")
            .required()
            .help("path to the YOLOv5 ONNX model file.");
    program.add_argument("--yuv")
            .default_value(std::string("../assets/test_0.yuv"))
            .help("path of one-frame I420 YUV file.");
    program.add_argument("--yuv-width")
            .default_value(1280)
            .help("I420 YUV frame width.")
            .scan<'i', int>();
    program.add_argument("--yuv-height")
            .default_value(720)
            .help("I420 YUV frame height.")
            .scan<'i', int>();
    program.add_argument("--output")
            .default_value(std::string("../output/output_yuv.jpg"))
            .help("path of visualization output image.");
    program.add_argument("-l", "--loop")
            .default_value(1)
            .help("measured full-pipeline loop count after one warmup pass.")
            .scan<'i', int>();
    program.add_argument("-p", "--perf")
            .help("save rpp_perf trace json.")
            .default_value(false)
            .implicit_value(true);
    program.add_argument("--perf-dir")
            .default_value(std::string("../trace"))
            .help("rpp_perf trace output directory.");
    program.add_argument("-v", "--verbose")
            .help("show verbose runtime log.")
            .default_value(false)
            .implicit_value(true);

    // Step 2: normalize command-line spelling before argparse parses the options.
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

    // Step 3: read parsed options and expand user-friendly paths such as ~/frame.yuv.
    std::string model_path = expand_user_path(program.get<std::string>("--onnx"));
    std::string yuv_path = expand_user_path(program.get<std::string>("--yuv"));
    std::string output_path = expand_user_path(program.get<std::string>("--output"));
    int yuv_width = program.get<int>("--yuv-width");
    int yuv_height = program.get<int>("--yuv-height");
    int repeat_count = program.get<int>("--loop");
    if (repeat_count <= 0) {
        std::cerr << "--loop must be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }
    if (yuv_width <= 0 || yuv_height <= 0 || (yuv_width % 2) != 0 || (yuv_height % 2) != 0) {
        std::cerr << "--yuv-width and --yuv-height must be positive even values for I420 input." << std::endl;
        return EXIT_FAILURE;
    }

    // Step 4: keep runtime logs quiet by default, but expose verbose RppRT logs on request.
    sample::gLogger.setReportableSeverity(program["--verbose"] == true ?
                                          infer1::ILogger::Severity::kVERBOSE :
                                          infer1::ILogger::Severity::kERROR);

    // Step 5: validate all files before model build so failures are immediate and readable.
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Cannot found onnx model file, path: " << model_path << std::endl;
        return EXIT_FAILURE;
    }
    if (!std::filesystem::exists(yuv_path)) {
        std::cerr << "Cannot found yuv file, path: " << yuv_path << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Model path: " << model_path << "\n"
              << "Inference YUV path: " << yuv_path << std::endl;

    // I420 stores full-size Y plus quarter-size U and V planes.
    size_t yuv_bytes = static_cast<size_t>(yuv_width) * static_cast<size_t>(yuv_height) * 3U / 2U;
    std::vector<unsigned char> yuv_data(yuv_bytes);
    std::ifstream yuv_file(yuv_path, std::ios::binary);
    if (!yuv_file.read(reinterpret_cast<char*>(yuv_data.data()), static_cast<std::streamsize>(yuv_data.size()))) {
        std::cerr << "Failed to read one I420 frame from: " << yuv_path << std::endl;
        return EXIT_FAILURE;
    }

    // This host-side conversion is only for rendering the final boxes on a viewable image.
    cv::Mat yuv_mat(yuv_height * 3 / 2, yuv_width, CV_8UC1, yuv_data.data());
    cv::Mat frame;
    cv::cvtColor(yuv_mat, frame, cv::COLOR_YUV2BGR_I420);

    // Step 6: optionally start rpp_perf before initialization so build and runtime events share one trace.
    PerfTraceSession trace_session("yolov5_yuv_image_demo",
                                   program["--perf"] == true,
                                   expand_user_path(program.get<std::string>("--perf-dir")));
    // Enable driver-side trace records only when this demo owns an active perf session.
    trace_session.enableDriverTrace(0);

    // Step 7: initialize the reusable YOLOv5 pipeline around the requested model.
    YoloV5PipelineOptions pipeline_options;
    pipeline_options.model_path = model_path;
    pipeline_options.inference_count = 1;

    YoloV5Pipeline pipeline;
    const std::vector<std::string>& class_list = coco80_class_labels();
    if (!pipeline.init(pipeline_options, class_list)) {
        return EXIT_FAILURE;
    }

    // Run one complete pass before measurement so printed latency reflects warmed execution.
    std::vector<Detection> warmup_output;
    {
        PERF_SCOPE_CATE("demo_warmup_run", "yolov5");
        if (!pipeline.runI420(yuv_data.data(), yuv_data.size(), yuv_width, yuv_height, warmup_output, nullptr)) {
            return EXIT_FAILURE;
        }
    }

    // Step 8: run the measured loop and accumulate stage timings for a stable average.
    std::vector<Detection> output;
    YoloV5StageTimes total_times;
    for (int i = 0; i < repeat_count; ++i) {
        PERF_SCOPE_CATE("demo_measured_loop", "yolov5");
        YoloV5StageTimes loop_times;
        if (!pipeline.runI420(yuv_data.data(), yuv_data.size(), yuv_width, yuv_height, output, &loop_times)) {
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

    // Step 9: render the last measured detections onto the BGR view of the input YUV frame.
    draw_detections(frame, output, class_list);
    if (!save_detection_image(output_path, frame)) {
        std::cerr << "Failed to save image: " << output_path << std::endl;
        return EXIT_FAILURE;
    }

    print_summary(average_times(total_times, repeat_count), repeat_count, output_path);
    return EXIT_SUCCESS;
}
