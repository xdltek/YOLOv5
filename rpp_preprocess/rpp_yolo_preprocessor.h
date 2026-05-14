#ifndef XDLTEK_SAMPLES_RPP_YOLO_PREPROCESSOR_H
#define XDLTEK_SAMPLES_RPP_YOLO_PREPROCESSOR_H

#include "detection_types.h"

#include <rpp_runtime.h>

#include <cstddef>

enum class PreprocessInputFormat
{
    YUV_I420,
    RGB_HWC,
    BGR_HWC,
    RGB_CHW,
    NV12,
    NV21,
};

enum class PreprocessMemoryType
{
    Host,
    Device,
};

struct PreprocessInput
{
    const void* data = nullptr;
    int width = 0;
    int height = 0;
    PreprocessInputFormat format = PreprocessInputFormat::BGR_HWC;
    PreprocessMemoryType memory_type = PreprocessMemoryType::Host;
    size_t bytes = 0;
};

struct RppPreprocessProfile
{
    double host_to_device_ms = 0.0;
    size_t host_to_device_bytes = 0;
    double model_input_clear_ms = 0.0;
    double yuv_to_rgb_ms = 0.0;
    double resize_normalize_ms = 0.0;
    double total_ms = 0.0;
};

class RppYoloPreprocessor
{
public:
    RppYoloPreprocessor() = default;
    ~RppYoloPreprocessor();

    RppYoloPreprocessor(const RppYoloPreprocessor&) = delete;
    RppYoloPreprocessor& operator=(const RppYoloPreprocessor&) = delete;

    bool init(int max_input_width, int max_input_height, int model_width, int model_height);

    void* getInputDeviceBuffer(PreprocessInputFormat format, int width, int height);

    bool uploadInputToDevice(const PreprocessInput& input,
                             PreprocessInput& device_input,
                             rtStream_t stream = nullptr,
                             double* host_to_device_ms = nullptr);

    bool run(const PreprocessInput& input,
             void* model_input_device,
             LetterboxInfo& letterbox,
             rtStream_t stream = nullptr,
             RppPreprocessProfile* profile = nullptr);

    void releaseSramBuffers();

private:
    bool ensureDeviceBuffer(void** buffer, size_t* capacity, size_t required_bytes);
    bool ensureHostPinnedBuffer(void** buffer, size_t* capacity, size_t required_bytes);
    bool ensureSramBuffer(void** buffer, size_t* capacity, size_t required_bytes);
    bool copyHostInputToDevice(const PreprocessInput& input, rtStream_t stream, void** device_input);
    bool validateInput(const PreprocessInput& input) const;

    int max_input_width_ = 0;
    int max_input_height_ = 0;
    int model_width_ = 0;
    int model_height_ = 0;

    void* host_input_device_ = nullptr;
    size_t host_input_capacity_ = 0;
    void* host_input_pinned_ = nullptr;
    size_t host_input_pinned_capacity_ = 0;
    void* input_sram_ = nullptr;
    size_t input_sram_capacity_ = 0;
    void* rgb_chw_sram_ = nullptr;
    size_t rgb_chw_sram_capacity_ = 0;
    void* model_input_sram_ = nullptr;
    size_t model_input_sram_capacity_ = 0;
};

size_t rpp_preprocess_input_bytes(PreprocessInputFormat format, int width, int height);
LetterboxInfo make_letterbox_info(int source_width, int source_height, int model_width, int model_height);

#endif // XDLTEK_SAMPLES_RPP_YOLO_PREPROCESSOR_H
