/**
 * @file main.cpp
 * @brief YOLOv5 demo entry and post-processing flow.
 *
 * Reference:
 * - ultralytics: https://docs.ultralytics.com/zh/yolov5/tutorials/model_export/
 * - yolov5-opencv-cpp-python: https://github.com/doleron/yolov5-opencv-cpp-python
 */


#include "labels.h"
#include "visualize.h"
#include "yolo_demo_pipeline.h"
#include "argparse/argparse.hpp"
#include "utils.hpp"
#include "logger.h"

#include <opencv2/imgcodecs.hpp>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>


/**
 * @brief Parse arguments, run YOLOv5 inference, and export visualization result.
 * @param argc Number of command line arguments.
 * @param argv Command line argument array.
 * @return EXIT_SUCCESS on completion, otherwise EXIT_FAILURE.
 */
int main(int argc, char **argv)
{
    // Define CLI for model path, image path, log level, and loop count.
    argparse::ArgumentParser program("yolov5 demo", "0.1", argparse::default_arguments::help);

    program.add_argument("-o", "--onnx")
            .required()
            .help("path to the YOLOv5 ONNX model file (not bundled; obtain via your export workflow).");
    program.add_argument("-i", "--image")
            .default_value(std::string("test_1.png"))
            .help("path of image file.");
    program.add_argument("-v", "--verbose")
            .help("show verbose log")
            .default_value(false)
            .implicit_value(true);
    program.add_argument("--loop")
            .default_value(1)
            .help("Loop inference count used for runtime timing")
            .scan<'i', int>();

    // Normalize shell-style arguments and parse with argparse.
    auto preprocessed_arguments = preprocess_args(argc, argv);
    std::vector<const char*> fixed_arguments;
    to_char_argument_vector(preprocessed_arguments, argv, fixed_arguments);
    try {
        program.parse_args(int(fixed_arguments.size()), fixed_arguments.data());
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Resolve user paths such as "~/" before filesystem checks.
    auto image_path = expand_user_path(program.get<std::string>("--image"));
    auto model_path = expand_user_path(program.get<std::string>("--onnx"));

    if (program["--verbose"] == true) {
        // Verbose mode helps inspect runtime/internal execution details.
        sample::gLogger.setReportableSeverity(infer1::ILogger::Severity::kVERBOSE);
    }
    else {
        sample::gLogger.setReportableSeverity(infer1::ILogger::Severity::kERROR);
    }

    if (!std::filesystem::exists(model_path)) {
        // Fail early to avoid runtime initialization with invalid model path.
        std::cerr << "Cannot found onnx model file, path: " << model_path << std::endl;
        return EXIT_FAILURE;
    }
    if (!image_path.empty() && !std::filesystem::exists(image_path)) {
        // Input image is required for this demo pipeline.
        std::cerr << "Cannot found image file, path: " << image_path << std::endl;
        return EXIT_FAILURE;
    }

    sample::user_visible_stream_log("ONNX model path: ", model_path);
    sample::user_visible_stream_log("Image file path: ", image_path);

    int inference_count = program.get<int>("--loop");

    const std::vector<std::string>& class_list = coco80_class_labels();
    YoloV5PipelineOptions pipeline_options;
    pipeline_options.model_path = model_path;
    pipeline_options.inference_count = inference_count;

    cv::Mat frame = cv::imread(image_path);
    if (frame.empty()) {
        // OpenCV failed to decode image bytes into valid matrix data.
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return EXIT_FAILURE;
    }

    // Run detection and draw visualization overlays.
    std::vector<Detection> output;
    if (!detect_yolov5(frame, pipeline_options, class_list, output)) {
        return EXIT_FAILURE;
    }

    draw_detections(frame, output, class_list);
    bool success = save_detection_image("output.jpg", frame);

    if (success) {
        std::cout << "Image saved successfully as output.jpg" << std::endl;
    } else {
        std::cerr << "Failed to save image" << std::endl;
    }

    return 0;
}
