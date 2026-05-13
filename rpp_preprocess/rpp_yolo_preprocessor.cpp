#include "rpp_yolo_preprocessor.h"

#include "rpp_preprocess_kernels.cuh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
using ProfileClock = std::chrono::high_resolution_clock;

bool is_yuv_format(PreprocessInputFormat format)
{
    return format == PreprocessInputFormat::YUV_I420 ||
           format == PreprocessInputFormat::NV12 ||
           format == PreprocessInputFormat::NV21;
}

bool is_supported_format(PreprocessInputFormat format)
{
    return format == PreprocessInputFormat::YUV_I420 ||
           format == PreprocessInputFormat::RGB_HWC ||
           format == PreprocessInputFormat::BGR_HWC ||
           format == PreprocessInputFormat::RGB_CHW;
}

bool check_rt(rtError_t status, const char* what)
{
    if (status == rtError_t::rtSuccess) {
        return true;
    }
    std::cerr << what << " failed, rtError=" << static_cast<int>(status) << std::endl;
    return false;
}

double elapsed_ms(ProfileClock::time_point start)
{
    return std::chrono::duration<double, std::milli>(ProfileClock::now() - start).count();
}

void reset_profile(RppPreprocessProfile* profile)
{
    if (profile != nullptr) {
        *profile = RppPreprocessProfile{};
    }
}

void log_u8_device_stats_if_requested(const char* label, const void* device_buffer, size_t byte_count)
{
    if (std::getenv("YOLO_DEBUG_OUTPUT") == nullptr || device_buffer == nullptr || byte_count == 0) {
        return;
    }

    std::vector<unsigned char> host_data(byte_count);
    if (!check_rt(rtMemcpy(host_data.data(), device_buffer, byte_count, rtMemcpyDeviceToHost), "rtMemcpy debug D2H")) {
        return;
    }

    int min_value = std::numeric_limits<int>::max();
    int max_value = std::numeric_limits<int>::lowest();
    size_t non_zero_count = 0;
    unsigned long long sum = 0;
    for (unsigned char value : host_data) {
        int int_value = static_cast<int>(value);
        min_value = std::min(min_value, int_value);
        max_value = std::max(max_value, int_value);
        sum += static_cast<unsigned long long>(value);
        if (value != 0) {
            non_zero_count++;
        }
    }

    std::cout << label << " stats: bytes=" << byte_count
              << ", min=" << min_value
              << ", max=" << max_value
              << ", mean=" << static_cast<double>(sum) / static_cast<double>(byte_count)
              << ", non_zero=" << non_zero_count << std::endl;
}
}

RppYoloPreprocessor::~RppYoloPreprocessor()
{
    if (host_input_device_ != nullptr) {
        rtFree(host_input_device_);
        host_input_device_ = nullptr;
    }
    if (input_sram_ != nullptr) {
        rtFreeSram(input_sram_);
        input_sram_ = nullptr;
    }
    if (rgb_chw_sram_ != nullptr) {
        rtFreeSram(rgb_chw_sram_);
        rgb_chw_sram_ = nullptr;
    }
    if (model_input_sram_ != nullptr) {
        rtFreeSram(model_input_sram_);
        model_input_sram_ = nullptr;
    }
}

bool RppYoloPreprocessor::init(int max_input_width, int max_input_height, int model_width, int model_height)
{
    if (max_input_width <= 0 || max_input_height <= 0 || model_width <= 0 || model_height <= 0) {
        std::cerr << "Invalid RPP preprocessor dimensions." << std::endl;
        return false;
    }

    max_input_width_ = max_input_width;
    max_input_height_ = max_input_height;
    model_width_ = model_width;
    model_height_ = model_height;
    return true;
}

void* RppYoloPreprocessor::getInputDeviceBuffer(PreprocessInputFormat format, int width, int height)
{
    size_t required = rpp_preprocess_input_bytes(format, width, height);
    if (required == 0) {
        return nullptr;
    }
    if (!ensureDeviceBuffer(&host_input_device_, &host_input_capacity_, required)) {
        return nullptr;
    }
    return host_input_device_;
}

bool RppYoloPreprocessor::run(const PreprocessInput& input,
                              void* model_input_device,
                              LetterboxInfo& letterbox,
                              rtStream_t stream,
                              RppPreprocessProfile* profile)
{
    reset_profile(profile);
    if (!validateInput(input) || model_input_device == nullptr) {
        return false;
    }

    rtStream_t run_stream = stream;
    bool owns_stream = false;
    if (run_stream == nullptr) {
        if (!check_rt(rtStreamCreate(&run_stream), "rtStreamCreate")) {
            return false;
        }
        owns_stream = true;
    }

    void* device_input = nullptr;
    auto stage_start = ProfileClock::now();
    if (!copyHostInputToDevice(input, &device_input)) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (profile != nullptr) {
        profile->host_to_device_ms = input.memory_type == PreprocessMemoryType::Host ? elapsed_ms(stage_start) : 0.0;
    }
    auto preprocess_start = ProfileClock::now();

    size_t input_bytes = rpp_preprocess_input_bytes(input.format, input.width, input.height);
    log_u8_device_stats_if_requested("RPP preprocess input", device_input, input_bytes);
    if (!ensureSramBuffer(&input_sram_, &input_sram_capacity_, input_bytes)) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (!check_rt(rtMemcpy(input_sram_, device_input, input_bytes, rtMemcpyDeviceToSram), "rtMemcpy input DDR to SRAM")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }

    letterbox = make_letterbox_info(input.width, input.height, model_width_, model_height_);

    size_t model_input_bytes = static_cast<size_t>(model_width_) *
                               static_cast<size_t>(model_height_) *
                               3U *
                               sizeof(float);
    if (!ensureSramBuffer(&model_input_sram_, &model_input_sram_capacity_, model_input_bytes)) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (!check_rt(rtMemset(model_input_device, 0, model_input_bytes), "rtMemset model input")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (!check_rt(rtMemcpy(model_input_sram_, model_input_device, model_input_bytes, rtMemcpyDeviceToSram),
                  "rtMemcpy zero model input DDR to SRAM")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }

    if (input.format == PreprocessInputFormat::YUV_I420) {
        launch_letterbox_resize_norm_i420_to_nchw_f32(run_stream,
                                                      input_sram_,
                                                      input.width,
                                                      input.height,
                                                      static_cast<float*>(model_input_sram_),
                                                      model_width_,
                                                      model_height_,
                                                      letterbox.resized_width,
                                                      letterbox.resized_height,
                                                      letterbox.scale,
                                                      letterbox.pad_x,
                                                      letterbox.pad_y);
    }
    else if (input.format == PreprocessInputFormat::RGB_CHW) {
        launch_letterbox_resize_norm_chw_u8_to_nchw_f32(run_stream,
                                                        input_sram_,
                                                        input.width,
                                                        input.height,
                                                        static_cast<float*>(model_input_sram_),
                                                        model_width_,
                                                        model_height_,
                                                        letterbox.resized_width,
                                                        letterbox.resized_height,
                                                        letterbox.scale,
                                                        letterbox.pad_x,
                                                        letterbox.pad_y);
    }
    else {
        bool input_is_bgr = input.format == PreprocessInputFormat::BGR_HWC;
        launch_letterbox_resize_norm_hwc_u8_to_nchw_f32(run_stream,
                                                        input_sram_,
                                                        input.width,
                                                        input.height,
                                                        input_is_bgr,
                                                        static_cast<float*>(model_input_sram_),
                                                        model_width_,
                                                        model_height_,
                                                        letterbox.resized_width,
                                                        letterbox.resized_height,
                                                        letterbox.scale,
                                                        letterbox.pad_x,
                                                        letterbox.pad_y);
    }

    if (!check_rt(rtGetLastError(), "RPP preprocessing kernel launch")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }

    if (!check_rt(rtStreamSynchronize(run_stream), "rtStreamSynchronize preprocess kernels")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (!check_rt(rtMemcpy(model_input_device, model_input_sram_, model_input_bytes, rtMemcpySramToDevice),
                  "rtMemcpy model input SRAM to DDR")) {
        if (owns_stream) {
            rtStreamDestroy(run_stream);
        }
        return false;
    }
    if (profile != nullptr) {
        profile->total_ms = elapsed_ms(preprocess_start);
    }

    bool ok = true;
    if (owns_stream) {
        check_rt(rtStreamDestroy(run_stream), "rtStreamDestroy");
    }
    return ok;
}

bool RppYoloPreprocessor::ensureDeviceBuffer(void** buffer, size_t* capacity, size_t required_bytes)
{
    if (buffer == nullptr || capacity == nullptr || required_bytes == 0) {
        return false;
    }
    if (*buffer != nullptr && *capacity >= required_bytes) {
        return true;
    }
    if (*buffer != nullptr) {
        if (!check_rt(rtFree(*buffer), "rtFree")) {
            return false;
        }
        *buffer = nullptr;
        *capacity = 0;
    }
    if (!check_rt(rtMalloc(buffer, required_bytes), "rtMalloc")) {
        return false;
    }
    *capacity = required_bytes;
    return true;
}

bool RppYoloPreprocessor::ensureSramBuffer(void** buffer, size_t* capacity, size_t required_bytes)
{
    if (buffer == nullptr || capacity == nullptr || required_bytes == 0) {
        return false;
    }
    if (*buffer != nullptr && *capacity >= required_bytes) {
        return true;
    }
    if (*buffer != nullptr) {
        if (!check_rt(rtFreeSram(*buffer), "rtFreeSram")) {
            return false;
        }
        *buffer = nullptr;
        *capacity = 0;
    }
    if (!check_rt(rtMallocSram(buffer, required_bytes), "rtMallocSram")) {
        return false;
    }
    *capacity = required_bytes;
    return true;
}

bool RppYoloPreprocessor::copyHostInputToDevice(const PreprocessInput& input, void** device_input)
{
    if (device_input == nullptr) {
        return false;
    }
    if (input.memory_type == PreprocessMemoryType::Device) {
        *device_input = const_cast<void*>(input.data);
        return true;
    }

    size_t required = rpp_preprocess_input_bytes(input.format, input.width, input.height);
    if (input.bytes < required) {
        std::cerr << "Input bytes are smaller than required preprocessing bytes." << std::endl;
        return false;
    }
    if (!ensureDeviceBuffer(&host_input_device_, &host_input_capacity_, required)) {
        return false;
    }
    if (!check_rt(rtMemcpy(host_input_device_, input.data, required, rtMemcpyHostToDevice), "rtMemcpy H2D")) {
        return false;
    }
    *device_input = host_input_device_;
    return true;
}

bool RppYoloPreprocessor::validateInput(const PreprocessInput& input) const
{
    if (input.data == nullptr || input.width <= 0 || input.height <= 0) {
        std::cerr << "Invalid preprocessing input." << std::endl;
        return false;
    }
    if (input.width > max_input_width_ || input.height > max_input_height_) {
        std::cerr << "Preprocessing input exceeds initialized maximum dimensions." << std::endl;
        return false;
    }
    if (is_yuv_format(input.format) && input.format != PreprocessInputFormat::YUV_I420) {
        std::cerr << "Unsupported YUV format. Only I420 is implemented." << std::endl;
        return false;
    }
    if (input.format == PreprocessInputFormat::YUV_I420 &&
        ((input.width % 2) != 0 || (input.height % 2) != 0)) {
        std::cerr << "I420 input width and height must be even." << std::endl;
        return false;
    }
    if (!is_supported_format(input.format)) {
        std::cerr << "Unsupported preprocessing input format." << std::endl;
        return false;
    }
    size_t required = rpp_preprocess_input_bytes(input.format, input.width, input.height);
    if (required == 0 || input.bytes < required) {
        std::cerr << "Invalid preprocessing input byte size." << std::endl;
        return false;
    }
    return true;
}

size_t rpp_preprocess_input_bytes(PreprocessInputFormat format, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return 0;
    }
    size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    switch (format) {
        case PreprocessInputFormat::YUV_I420:
        case PreprocessInputFormat::NV12:
        case PreprocessInputFormat::NV21:
            return pixels * 3U / 2U;
        case PreprocessInputFormat::RGB_HWC:
        case PreprocessInputFormat::BGR_HWC:
        case PreprocessInputFormat::RGB_CHW:
            return pixels * 3U;
        default:
            return 0;
    }
}

LetterboxInfo make_letterbox_info(int source_width, int source_height, int model_width, int model_height)
{
    LetterboxInfo info;
    info.source_width = source_width;
    info.source_height = source_height;
    info.model_width = model_width;
    info.model_height = model_height;

    float scale_w = static_cast<float>(model_width) / static_cast<float>(source_width);
    float scale_h = static_cast<float>(model_height) / static_cast<float>(source_height);
    info.scale = std::min(scale_w, scale_h);
    info.resized_width = std::max(1, static_cast<int>(std::round(static_cast<float>(source_width) * info.scale)));
    info.resized_height = std::max(1, static_cast<int>(std::round(static_cast<float>(source_height) * info.scale)));
    float half_pad_x = (static_cast<float>(model_width) - static_cast<float>(info.resized_width)) * 0.5f;
    float half_pad_y = (static_cast<float>(model_height) - static_cast<float>(info.resized_height)) * 0.5f;
    info.pad_x = static_cast<float>(static_cast<int>(std::round(half_pad_x - 0.1f)));
    info.pad_y = static_cast<float>(static_cast<int>(std::round(half_pad_y - 0.1f)));
    return info;
}
