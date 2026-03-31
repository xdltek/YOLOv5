/**
 * @file main.cpp
 * @brief YOLOv5 demo entry and post-processing flow.
 *
 * Reference:
 * - ultralytics: https://docs.ultralytics.com/zh/yolov5/tutorials/model_export/
 * - yolov5-opencv-cpp-python: https://github.com/doleron/yolov5-opencv-cpp-python
 */


#include "yolo.h"
#include "argparse/argparse.hpp"
#include "utils.hpp"
#include "logger.h"

#include <opencv2/opencv.hpp>
#include <algorithm>
#include <string>
#include <vector>


/**
 * @brief MS COCO 80 class names in YOLOv5 default order (embedded; no external names file).
 *
 * Use these labels only when your ONNX model was trained or exported with the same COCO class
 * ordering as standard YOLOv5 pretrained weights. For other label sets, extend this demo locally.
 *
 * @return Reference to a static list used for overlay text in `detect()` visualization.
 */
const std::vector<std::string>& coco80_class_labels()
{
    static const std::vector<std::string> kLabels = {
        "person",        "bicycle",       "car",           "motorcycle",    "airplane",      "bus",           "train",         "truck",
        "boat",          "traffic light", "fire hydrant",  "stop sign",     "parking meter", "bench",         "bird",          "cat",
        "dog",           "horse",         "sheep",         "cow",           "elephant",      "bear",          "zebra",         "giraffe",
        "backpack",      "umbrella",      "handbag",       "tie",           "suitcase",      "frisbee",       "skis",          "snowboard",
        "sports ball",   "kite",          "baseball bat",  "baseball glove", "skateboard",   "surfboard",     "tennis racket", "bottle",
        "wine glass",    "cup",           "fork",          "knife",         "spoon",         "bowl",          "banana",        "apple",
        "sandwich",      "orange",        "broccoli",      "carrot",        "hot dog",       "pizza",         "donut",         "cake",
        "chair",         "couch",         "potted plant",  "bed",           "dining table",  "toilet",        "tv",            "laptop",
        "mouse",         "remote",        "keyboard",      "cell phone",    "microwave",     "oven",          "toaster",       "sink",
        "refrigerator",  "book",          "clock",         "vase",          "scissors",      "teddy bear",    "hair drier",    "toothbrush",
    };
    return kLabels;
}


/**
 * @brief Resolve a human-readable label for overlay; safe if `class_id` is outside the COCO-80 table.
 */
std::string detection_class_label(int class_id, const std::vector<std::string>& names)
{
    if (class_id >= 0 && class_id < static_cast<int>(names.size()))
        return names[static_cast<size_t>(class_id)];
    return "class_" + std::to_string(class_id);
}

const std::vector<cv::Scalar> colors = {cv::Scalar(255, 255, 0), cv::Scalar(0, 255, 0), cv::Scalar(0, 255, 255), cv::Scalar(255, 0, 0)};

const float INPUT_WIDTH = 640.0;
const float INPUT_HEIGHT = 640.0;
// Candidate must pass class-score filtering before NMS.
const float SCORE_THRESHOLD = 0.2;
// Overlap threshold used to suppress duplicate boxes.
const float NMS_THRESHOLD = 0.4;
// Objectness threshold used before class-score evaluation.
const float CONFIDENCE_THRESHOLD = 0.4;

struct Detection
{
    // Index into class-name list (`coco80_class_labels()` for standard COCO models).
    int class_id;
    // Confidence score of the retained detection.
    float confidence;
    // Bounding rectangle in original image coordinates.
    cv::Rect box;
};

/**
 * @brief Pad the source image to a square canvas expected by YOLOv5 preprocessing.
 * @param source Input image.
 * @return Square image with original content copied to top-left.
 */
cv::Mat format_yolov5(const cv::Mat &source) {
    // Use max(width, height) to create a square canvas required by YOLOv5 input shape.
    int col = source.cols;
    int row = source.rows;
    int _max = MAX(col, row);
    cv::Mat result = cv::Mat::zeros(_max, _max, CV_8UC3);
    // Place original image at top-left; padded region remains zeros (black).
    source.copyTo(result(cv::Rect(0, 0, col, row)));
    return result;
}

/**
 * @brief Convert an image to NCHW blob format for model inference.
 * @param image Input image data.
 * @param scalefactor Scale factor applied during conversion.
 * @param size Optional target size.
 * @param mean Optional per-channel mean subtraction value.
 * @param swapRB Whether to swap B and R channels.
 * @param crop Whether to keep aspect ratio by center-cropping.
 * @param ddepth Output element type.
 * @return 4D blob in [N, C, H, W] layout.
 */
cv::Mat blob_from_image(cv::InputArray image,
                        double scalefactor = 1.0,
                        const cv::Size& size = cv::Size(),
                        const cv::Scalar& mean = cv::Scalar(),
                        bool swapRB = false,
                        bool crop = false,
                        int ddepth = CV_32F)
{
    // Materialize the input as a cv::Mat and validate it.
    cv::Mat input = image.getMat();
    CV_Assert(!input.empty());

    // Resize to target shape, optionally with aspect-ratio-preserving center crop.
    cv::Mat resized;
    if (size.width > 0 && size.height > 0) {
        if (crop) {
            // Compute crop ROI that matches target aspect ratio.
            double input_aspect = static_cast<double>(input.cols) / input.rows;
            double target_aspect = static_cast<double>(size.width) / size.height;

            if (input_aspect > target_aspect) {
                // Input is wider than target ratio: crop width first.
                int crop_width = static_cast<int>(input.rows * target_aspect);
                int x_offset = (input.cols - crop_width) / 2;
                cv::Rect roi(x_offset, 0, crop_width, input.rows);
                cv::resize(input(roi), resized, size);
            }
            else {
                // Input is taller than target ratio: crop height first.
                int crop_height = static_cast<int>(input.cols / target_aspect);
                int y_offset = (input.rows - crop_height) / 2;
                cv::Rect roi(0, y_offset, input.cols, crop_height);
                cv::resize(input(roi), resized, size);
            }
        }
        else {
            // Direct resize without cropping.
            cv::resize(input, resized, size);
        }
    }
    else {
        // No target size provided: keep original spatial resolution.
        resized = input.clone();
    }

    // Swap color channels when model expects RGB input.
    if (swapRB) {
        cv::cvtColor(resized, resized, cv::COLOR_BGR2RGB);
    }

    // Convert to floating-point tensor and apply global scaling.
    cv::Mat floatImage;
    resized.convertTo(floatImage, ddepth, scalefactor);

    // Apply mean subtraction when mean is provided.
    if (mean != cv::Scalar()) {
        // Build a broadcastable mean matrix in the same shape/type.
        cv::Mat meanMat(floatImage.size(), floatImage.type(), mean);
        meanMat *= scalefactor; // Keep scaling consistent with pixel conversion.

        // Normalize by subtracting mean values.
        cv::subtract(floatImage, meanMat, floatImage);
    }

    // Split channels for NCHW memory arrangement.
    std::vector<cv::Mat> channels;
    cv::split(floatImage, channels);

    // Allocate 4D blob with shape [1, C, H, W].
    int dims[] = { 1, static_cast<int>(channels.size()), floatImage.rows, floatImage.cols };
    cv::Mat blob(4, dims, ddepth);

    // Copy each channel into its corresponding blob slice.
    for (int c = 0; c < channels.size(); c++) {
        // View the target channel plane in the blob.
        cv::Mat channelBlob(blob.size[2], blob.size[3], ddepth, blob.ptr(0, c));

        // Copy source channel data into blob channel plane.
        channels[c].copyTo(channelBlob);
    }

    return blob;
}


/**
 * @brief Apply manual non-maximum suppression on candidate boxes.
 * @param bboxes Candidate bounding boxes.
 * @param scores Confidence score of each candidate.
 * @param score_threshold Minimum score for candidate filtering.
 * @param nms_threshold IoU threshold for suppression.
 * @param indices Output kept box indices.
 * @param eta Adaptive-threshold decay factor.
 * @param top_k Optional limit on sorted candidates.
 */
void nms_boxes(const std::vector<cv::Rect>& bboxes,
               const std::vector<float>& scores,
               float score_threshold,
               float nms_threshold,
               std::vector<int>& indices,
               float eta = 1.0f,
               int top_k = 0)
{
    // Reset output index container.
    indices.clear();

    // Validate input consistency before processing.
    if (bboxes.empty() || scores.empty() || bboxes.size() != scores.size()) {
        return;
    }

    // Filter out low-confidence candidates.
    std::vector<int> candidate_indices;
    for (int i = 0; i < scores.size(); i++) {
        if (scores[i] >= score_threshold) {
            candidate_indices.push_back(i);
        }
    }

    // Return early when no valid candidates remain.
    if (candidate_indices.empty()) {
        return;
    }

    // Sort candidates by descending confidence.
    std::sort(candidate_indices.begin(), candidate_indices.end(),
              [&scores](int a, int b) {
                  return scores[a] > scores[b];
              });

    // Apply top-k truncation when requested.
    if (top_k > 0 && top_k < static_cast<int>(candidate_indices.size())) {
        candidate_indices.resize(top_k);
    }

    // Precompute box areas for IoU computation.
    std::vector<float> areas;
    for (const auto& rect : bboxes) {
        areas.push_back(static_cast<float>(rect.width * rect.height));
    }

    // Initialize adaptive NMS threshold.
    float adaptive_threshold = nms_threshold;

    // Iteratively keep the best candidate and suppress overlaps.
    while (!candidate_indices.empty()) {
        // Pick the current highest-score box.
        int best_idx = candidate_indices[0];
        indices.push_back(best_idx);

        // Stop when there is no additional candidate to compare.
        if (candidate_indices.size() == 1) {
            break;
        }

        // Collect candidates that survive suppression.
        std::vector<int> remaining_indices;
        remaining_indices.reserve(candidate_indices.size() - 1);

        // Compare IoU between selected box and remaining boxes.
        const cv::Rect& best_rect = bboxes[best_idx];

        for (int i = 1; i < candidate_indices.size(); i++) {
            int idx = candidate_indices[i];
            const cv::Rect& current_rect = bboxes[idx];

            // Compute intersection rectangle.
            int x1 = std::max(best_rect.x, current_rect.x);
            int y1 = std::max(best_rect.y, current_rect.y);
            int x2 = std::min(best_rect.x + best_rect.width, current_rect.x + current_rect.width);
            int y2 = std::min(best_rect.y + best_rect.height, current_rect.y + current_rect.height);

            // Compute intersection area.
            float intersection = 0.0f;
            if (x2 > x1 && y2 > y1) {
                intersection = static_cast<float>((x2 - x1) * (y2 - y1));
            }

            // Compute union area.
            float union_area = areas[best_idx] + areas[idx] - intersection;

            // Compute IoU value.
            float iou = intersection / union_area;

            // Keep boxes that are below suppression threshold.
            if (iou <= adaptive_threshold) {
                remaining_indices.push_back(idx);
            }
        }

        // Update candidate set for next suppression round.
        candidate_indices = std::move(remaining_indices);

        // Decay threshold only when adaptive mode is enabled.
        if (eta != 1.0f) {
            adaptive_threshold *= eta;
        }
    }
}


/**
 * @brief Run preprocessing, inference, and post-processing for one image.
 * @param image Input image.
 * @param model_path ONNX model path.
 * @param output Output detection list.
 * @param className Class labels.
 * @param inference_count Number of inference loops.
 */
void detect(cv::Mat &image, const std::string model_path,
            std::vector<Detection> &output, const std::vector<std::string> &className,
            int inference_count) {
    // Convert input to square layout and then to model-ready blob.
    auto input_image = format_yolov5(image);

    // Convert to normalized float blob: scale 1/255, resize 640x640, BGR->RGB.
    cv::Mat blob = blob_from_image(input_image, 1./255., cv::Size(INPUT_WIDTH, INPUT_HEIGHT),  cv::Scalar(), true, false);

    // Create inference wrapper and execute model.
    Yolo yolo(model_path);
    yolo.init_engine();

    std::vector<float> output_data;
    yolo.infer(blob, inference_count, output_data);

    // Map box coordinates from model input scale back to padded image scale.
    // Because preprocessing pads to a square canvas, scaling is done against input_image.
    float x_factor = input_image.cols / INPUT_WIDTH;
    float y_factor = input_image.rows / INPUT_HEIGHT;

    float *data = output_data.data();

    // YOLOv5 head layout: [x, y, w, h, obj_conf, class_scores...].
    const int dimensions = 85;
    const int rows = 25200;

    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    // Decode model output rows and collect candidates above confidence threshold.
    for (int i = 0; i < rows; ++i) {

        float confidence = data[4];
        if (confidence >= CONFIDENCE_THRESHOLD) {

            // Class scores start at offset 5 after box + objectness fields.
            float * classes_scores = data + 5;
            cv::Mat scores(1, className.size(), CV_32FC1, classes_scores);
            cv::Point class_id;
            double max_class_score;
            // Select the best class for the current candidate row.
            minMaxLoc(scores, 0, &max_class_score, 0, &class_id);
            if (max_class_score > SCORE_THRESHOLD) {

                confidences.push_back(confidence);

                class_ids.push_back(class_id.x);

                float x = data[0];
                float y = data[1];
                float w = data[2];
                float h = data[3];
                // Convert center-width-height format into top-left-width-height box.
                int left = int((x - 0.5 * w) * x_factor);
                int top = int((y - 0.5 * h) * y_factor);
                int width = int(w * x_factor);
                int height = int(h * y_factor);
                boxes.push_back(cv::Rect(left, top, width, height));
            }

        }

        // Move pointer to the next candidate row.
        data += dimensions;

    }

    // Suppress overlapping boxes and build final detection list.
    std::vector<int> nms_result;
    nms_boxes(boxes, confidences, SCORE_THRESHOLD, NMS_THRESHOLD, nms_result);

    for (int i = 0; i < nms_result.size(); i++) {
        int idx = nms_result[i];
        Detection result;
        result.class_id = class_ids[idx];
        result.confidence = confidences[idx];
        result.box = boxes[idx];
        output.push_back(result);
    }
}

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

    cv::Mat frame = cv::imread(image_path);
    if (frame.empty()) {
        // OpenCV failed to decode image bytes into valid matrix data.
        std::cerr << "Failed to load image: " << image_path << std::endl;
        return EXIT_FAILURE;
    }

    // Run detection and draw visualization overlays.
    std::vector<Detection> output;
    detect(frame, model_path, output, class_list,  inference_count);

    // Draw each retained detection on top of the original image.
    int detections = output.size();

    for (int i = 0; i < detections; ++i)
    {

        auto detection = output[i];
        auto box = detection.box;
        auto classId = detection.class_id;
        const auto color = colors[classId % colors.size()];
        cv::rectangle(frame, box, color, 3);

        // Draw label background and class text near the bounding box.
        cv::rectangle(frame, cv::Point(box.x, box.y - 20), cv::Point(box.x + box.width, box.y), color, cv::FILLED);
        std::string label = detection_class_label(classId, class_list);
        cv::putText(frame, label, cv::Point(box.x, box.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));
    }

    std::ostringstream fps_label;
    fps_label << std::fixed << std::setprecision(2);
    std::string fps_label_str = fps_label.str();

    cv::putText(frame, fps_label_str.c_str(), cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);

    bool success = cv::imwrite("output.jpg", frame);

    if (success) {
        std::cout << "Image saved successfully as output.jpg" << std::endl;
    } else {
        std::cerr << "Failed to save image" << std::endl;
    }

    return 0;
}