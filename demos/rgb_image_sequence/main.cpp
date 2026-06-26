/**
 * @file main.cpp
 * @brief YOLOv5 RGB/BGR image sequence demo.
 */
#include "argparse/argparse.hpp"
#include "labels.h"
#include "logger.h"
#include "perf_trace_session.h"
#include "utils.hpp"
#include "visualization.h"
#include "yolo_pipeline.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
#ifdef YOLO_ENABLE_RPP_PERF
constexpr bool kPerfTraceEnabled = true;
#else
constexpr bool kPerfTraceEnabled = false;
#endif
constexpr const char* kPerfTraceDir = "../trace";

/**
 * @brief Convert byte counts to KiB for the latency summary.
 * @param bytes Number of bytes transferred.
 */
double bytes_to_kib(size_t bytes)
{
    return static_cast<double>(bytes) / 1024.0;
}

/**
 * @brief Convert a path extension to lowercase for image filtering.
 * @param path Candidate input path.
 */
std::string lowercase_extension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return ext;
}

/**
 * @brief Check whether a path extension is supported by OpenCV image loading.
 * @param path Candidate input path.
 */
bool is_supported_image_path(const std::filesystem::path& path)
{
    // Keep the list narrow and predictable for directory scanning.
    std::string ext = lowercase_extension(path);
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp";
}

/**
 * @brief Collect non-recursive image paths from a directory.
 * @param image_dir Directory containing image files.
 * @param image_paths Output list sorted by path for reproducible execution.
 */
bool collect_images_from_directory(const std::string& image_dir, std::vector<std::string>& image_paths)
{
    // Directory mode intentionally stays non-recursive so output names remain unambiguous.
    image_paths.clear();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(image_dir, ec)) {
        if (ec) {
            std::cerr << "Failed to iterate image directory: " << image_dir << std::endl;
            return false;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (is_supported_image_path(entry.path())) {
            image_paths.push_back(entry.path().string());
        }
    }
    std::sort(image_paths.begin(), image_paths.end());
    return true;
}

/**
 * @brief Read image paths from a text file.
 * @param list_path Text file path; empty lines and lines beginning with # are ignored.
 * @param image_paths Output list in file order.
 */
bool collect_images_from_list(const std::string& list_path, std::vector<std::string>& image_paths)
{
    // List mode preserves caller order and allows images from different directories.
    image_paths.clear();
    std::ifstream file(list_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open image list: " << list_path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Trim simple leading and trailing whitespace without pulling in extra helpers.
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;
        }
        size_t last = line.find_last_not_of(" \t\r\n");
        std::string path = line.substr(first, last - first + 1);
        if (!path.empty() && path[0] != '#') {
            image_paths.push_back(expand_user_path(path));
        }
    }
    return true;
}

/**
 * @brief Build a rendered output path for one input image.
 * @param output_dir Directory where rendered images are written.
 * @param image_path Source image path.
 * @param image_index Zero-based index in the image sequence.
 */
std::string make_output_path(const std::string& output_dir, const std::string& image_path, size_t image_index)
{
    // Prefix the source stem with a sequence index so image-list inputs with duplicate stems do not overwrite.
    std::filesystem::path input_path(image_path);
    std::filesystem::path output_path(output_dir);
    std::ostringstream name;
    name << std::setw(6) << std::setfill('0') << image_index << "_" << input_path.stem().string() << "_detect.jpg";
    output_path /= name.str();
    return output_path.string();
}

/**
 * @brief Add one loop timing sample into the accumulated total.
 * @param total Accumulated timing summary.
 * @param sample One measured pipeline run.
 */
void accumulate_times(YoloV5StageTimes& total, const YoloV5StageTimes& sample)
{
    // Byte counters are summed across all measured frames and averaged later.
    total.input_h2d_ms += sample.input_h2d_ms;
    total.input_h2d_bytes += sample.input_h2d_bytes;
    total.output_d2h_ms += sample.output_d2h_ms;
    total.output_d2h_bytes += sample.output_d2h_bytes;
    total.preprocess_ms += sample.preprocess_ms;
    total.inference_ms += sample.inference_ms;
    total.postprocess_ms += sample.postprocess_ms;
    total.end_to_end_ms += sample.end_to_end_ms;
}

/**
 * @brief Average accumulated stage timings across measured frames.
 * @param total Sum of all measured frame timings.
 * @param count Number of measured frames.
 */
YoloV5StageTimes average_times(const YoloV5StageTimes& total, int count)
{
    YoloV5StageTimes average;
    // Keep the zero-count fallback defensive even though callers validate inputs.
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

/**
 * @brief Print the measured full-pipeline timing summary after warmup.
 * @param times Averaged stage timings collected from measured frames.
 * @param image_count Number of input images processed.
 * @param loop_count Number of measured loops per image.
 * @param output_dir Directory where rendered images were saved.
 */
void print_summary(const YoloV5StageTimes& times, int image_count, int loop_count, const std::string& output_dir)
{
    // FPS is derived from the averaged end-to-end latency across all measured image runs.
    double fps = times.end_to_end_ms > 0.0 ? 1000.0 / times.end_to_end_ms : 0.0;
    std::cout << std::fixed << std::setprecision(3)
              << "\nSummary:\n"
              << "Image count: " << image_count << "\n"
              << "Loop count per image: " << loop_count << "\n"
              << "Measured frames: " << image_count * loop_count << "\n"
              << "Loop time: average per measured image run after warmup\n"
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
              << "Output dir: " << output_dir << std::endl;
}
}

/**
 * @brief Run the RGB/BGR image sequence demo from CLI parsing through rendered outputs.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument values.
 */
int main(int argc, char** argv)
{
    // Step 1: declare the command-line contract for this demo.
    argparse::ArgumentParser program("yolov5 rgb image sequence demo", "0.1", argparse::default_arguments::help);

    program.add_argument("-o", "--onnx")
            .required()
            .help("path to the YOLOv5 ONNX model file.");
    program.add_argument("--image-dir")
            .default_value(std::string("../assets"))
            .help("directory containing RGB/BGR images. Used when --image-list is empty.");
    program.add_argument("--image-list")
            .default_value(std::string(""))
            .help("text file containing one RGB/BGR image path per line.");
    program.add_argument("--output-dir")
            .default_value(std::string("../output/rgb_image_sequence"))
            .help("directory for rendered output images.");
    program.add_argument("-l", "--loop")
            .default_value(1)
            .help("measured full-pipeline loop count per image after one warmup pass.")
            .scan<'i', int>();
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

    // Step 3: read parsed options and expand user-friendly paths such as ~/model.onnx.
    std::string model_path = expand_user_path(program.get<std::string>("--onnx"));
    std::string image_dir = expand_user_path(program.get<std::string>("--image-dir"));
    std::string image_list = expand_user_path(program.get<std::string>("--image-list"));
    std::string output_dir = expand_user_path(program.get<std::string>("--output-dir"));
    int repeat_count = program.get<int>("--loop");
    if (repeat_count <= 0) {
        std::cerr << "--loop must be greater than 0." << std::endl;
        return EXIT_FAILURE;
    }

    // Step 4: keep runtime logs quiet by default, but expose verbose RppRT logs on request.
    sample::gLogger.setReportableSeverity(program["--verbose"] == true ?
                                          infer1::ILogger::Severity::kVERBOSE :
                                          infer1::ILogger::Severity::kERROR);

    // Step 5: validate model and collect the image sequence before model build.
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Cannot found onnx model file, path: " << model_path << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<std::string> image_paths;
    if (!image_list.empty()) {
        if (!std::filesystem::exists(image_list) || !collect_images_from_list(image_list, image_paths)) {
            return EXIT_FAILURE;
        }
    }
    else {
        if (!std::filesystem::exists(image_dir) || !std::filesystem::is_directory(image_dir)) {
            std::cerr << "Cannot found image directory, path: " << image_dir << std::endl;
            return EXIT_FAILURE;
        }
        if (!collect_images_from_directory(image_dir, image_paths)) {
            return EXIT_FAILURE;
        }
    }
    if (image_paths.empty()) {
        std::cerr << "No supported RGB/BGR images found." << std::endl;
        return EXIT_FAILURE;
    }

    std::error_code ec;
    std::filesystem::create_directories(output_dir, ec);
    if (ec) {
        std::cerr << "Failed to create output directory: " << output_dir << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Model path: " << model_path << "\n"
              << "Inference image source: " << (!image_list.empty() ? image_list : image_dir) << "\n"
              << "Image count: " << image_paths.size() << "\n"
              << "Output dir: " << output_dir << std::endl;

    // Step 6: start rpp_perf automatically in perf-enabled builds so init and run events share one trace.
    PerfTraceSession trace_session("yolov5_rgb_image_sequence_demo", kPerfTraceEnabled, kPerfTraceDir);
    // Enable driver-side trace records only when this build owns an active perf session.
    trace_session.enableDriverTrace(0);

    // Step 7: initialize the reusable YOLOv5 pipeline once and reuse it for every image.
    YoloV5PipelineOptions pipeline_options;
    pipeline_options.model_path = model_path;
    pipeline_options.inference_count = 1;

    YoloV5Pipeline pipeline;
    const std::vector<std::string>& class_list = coco80_class_labels();
    if (!pipeline.init(pipeline_options, class_list)) {
        return EXIT_FAILURE;
    }

    // Step 8: run one full pass on the first valid image before measurement.
    cv::Mat warmup_frame = cv::imread(image_paths.front());
    if (warmup_frame.empty()) {
        std::cerr << "Failed to load warmup image: " << image_paths.front() << std::endl;
        return EXIT_FAILURE;
    }
    std::vector<Detection> warmup_output;
    {
        PERF_SCOPE_CATE("demo_sequence_warmup_run", "yolov5");
        if (!pipeline.runRgb(warmup_frame, warmup_output, nullptr)) {
            return EXIT_FAILURE;
        }
    }

    // Step 9: process each image sequentially in one process and accumulate measured timings.
    YoloV5StageTimes total_times;
    int measured_frames = 0;
    for (size_t image_index = 0; image_index < image_paths.size(); ++image_index) {
        const std::string& image_path = image_paths[image_index];
        // Load one image at a time so memory usage stays bounded for large directories.
        cv::Mat frame = cv::imread(image_path);
        if (frame.empty()) {
            std::cerr << "Failed to load image: " << image_path << std::endl;
            return EXIT_FAILURE;
        }

        std::vector<Detection> output;
        for (int i = 0; i < repeat_count; ++i) {
            PERF_SCOPE_CATE("demo_sequence_measured_loop", "yolov5");
            YoloV5StageTimes loop_times;
            if (!pipeline.runRgb(frame, output, &loop_times)) {
                return EXIT_FAILURE;
            }
            accumulate_times(total_times, loop_times);
            ++measured_frames;
        }

        // Render only once per image using the last measured detection result.
        draw_detections(frame, output, class_list);
        std::string output_path = make_output_path(output_dir, image_path, image_index);
        if (!save_detection_image(output_path, frame)) {
            std::cerr << "Failed to save image: " << output_path << std::endl;
            return EXIT_FAILURE;
        }
    }

    print_summary(average_times(total_times, measured_frames),
                  static_cast<int>(image_paths.size()),
                  repeat_count,
                  output_dir);
    return EXIT_SUCCESS;
}
