/**
 * @file yolo_pipeline.h
 * @brief Reusable YOLOv5 pipeline built from preprocessing, inference, and postprocessing modules.
 */
#ifndef XDLTEK_SAMPLES_YOLOV5_PIPELINE_H
#define XDLTEK_SAMPLES_YOLOV5_PIPELINE_H

#include "detection_types.h"

#include <opencv2/core.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Forward declaration for the reusable RppRT model inference engine.
 */
class RppInferEngine;
/**
 * @brief Forward declaration for the RPP-backed YOLOv5 postprocess module.
 */
class RppYoloPostprocessor;
/**
 * @brief Forward declaration for the RPP-backed YOLOv5 preprocess module.
 */
class RppYoloPreprocessor;

/**
 * @brief User-visible options used to initialize a YOLOv5 pipeline instance.
 */
struct YoloV5PipelineOptions
{
    std::string model_path;
    int inference_count = 1;
    YoloV5Config config;
};

/**
 * @brief Stage-level timing summary reported by demos after one measured pipeline run.
 */
struct YoloV5StageTimes
{
    double input_h2d_ms = 0.0;
    size_t input_h2d_bytes = 0;
    double output_d2h_ms = 0.0;
    size_t output_d2h_bytes = 0;
    double preprocess_ms = 0.0;
    double inference_ms = 0.0;
    double postprocess_ms = 0.0;
    double end_to_end_ms = 0.0;
};

/**
 * @brief Reusable YOLOv5 pipeline that wires preprocessing, inference, and postprocessing.
 */
class YoloV5Pipeline
{
public:
    /**
     * @brief Construct an empty pipeline; call init() before runRgb() or runI420().
     */
    YoloV5Pipeline();
    /**
     * @brief Destroy owned preprocessing, inference, and postprocessing modules.
     */
    ~YoloV5Pipeline();

    /**
     * @brief Disable copying because the pipeline owns runtime resources.
     */
    YoloV5Pipeline(const YoloV5Pipeline&) = delete;
    /**
     * @brief Disable copy assignment because the pipeline owns runtime resources.
     */
    YoloV5Pipeline& operator=(const YoloV5Pipeline&) = delete;

    /**
     * @brief Initialize the model runtime and YOLOv5 postprocess path.
     * @param options Pipeline options, including the ONNX model path.
     * @param class_names Class-name table used to size YOLO postprocessing.
     */
    bool init(const YoloV5PipelineOptions& options, const std::vector<std::string>& class_names);

    /**
     * @brief Run one BGR/RGB host image through preprocessing, inference, and postprocessing.
     * @param image Continuous or cloneable OpenCV image in host memory.
     * @param detections Output detections restored to source-image coordinates.
     * @param times Optional measured stage timings for the completed run.
     */
    bool runRgb(const cv::Mat& image,
                std::vector<Detection>& detections,
                YoloV5StageTimes* times = nullptr);

    /**
     * @brief Run one I420 host frame through preprocessing, inference, and postprocessing.
     * @param yuv_data Pointer to packed I420 frame data in host memory.
     * @param yuv_bytes Size of the packed I420 frame in bytes.
     * @param width Source frame width.
     * @param height Source frame height.
     * @param detections Output detections restored to source-frame coordinates.
     * @param times Optional measured stage timings for the completed run.
     */
    bool runI420(const void* yuv_data,
                 size_t yuv_bytes,
                 int width,
                 int height,
                 std::vector<Detection>& detections,
                 YoloV5StageTimes* times = nullptr);

    /**
     * @brief Return the validated runtime configuration used by this pipeline.
     */
    const YoloV5Config& runtimeConfig() const { return config_; }

private:
    /**
     * @brief Run one generic input descriptor through preprocessing, inference, and postprocessing.
     * @param data Source host buffer.
     * @param bytes Source buffer byte count.
     * @param width Source width.
     * @param height Source height.
     * @param row_stride_bytes Source row stride in bytes.
     * @param channels Source channel count.
     * @param continuous Whether the source buffer is contiguous.
     * @param format PreprocessInputFormat encoded as int to avoid exposing RPP headers in this public header.
     * @param detections Output detections restored to source coordinates.
     * @param times Optional measured stage timings.
     */
    bool runInput(const void* data,
                  size_t bytes,
                  int width,
                  int height,
                  int row_stride_bytes,
                  int channels,
                  bool continuous,
                  int format,
                  std::vector<Detection>& detections,
                  YoloV5StageTimes* times);
    /**
     * @brief Create or reuse preprocessing workspace for a source shape.
     * @param width Source width.
     * @param height Source height.
     */
    bool ensurePreprocessor(int width, int height);
    /**
     * @brief Initialize YOLOv5 postprocessing after model output dimensions are known.
     * @param class_names Class-name table used to size score channels.
     */
    bool initPostprocessor(const std::vector<std::string>& class_names);

    std::unique_ptr<RppInferEngine> infer_engine_;
    std::unique_ptr<RppYoloPreprocessor> preprocessor_;
    std::unique_ptr<RppYoloPostprocessor> postprocessor_;

    YoloV5PipelineOptions options_;
    YoloV5Config config_;
    int preprocessor_width_ = 0;
    int preprocessor_height_ = 0;
    bool initialized_ = false;
};

#endif // XDLTEK_SAMPLES_YOLOV5_PIPELINE_H
