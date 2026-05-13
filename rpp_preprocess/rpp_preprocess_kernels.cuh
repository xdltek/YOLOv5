#ifndef XDLTEK_SAMPLES_RPP_PREPROCESS_KERNELS_CUH
#define XDLTEK_SAMPLES_RPP_PREPROCESS_KERNELS_CUH

#include <rpp_runtime.h>

void launch_i420_to_rgb_chw_u8(rtStream_t stream,
                               const void* yuv,
                               int width,
                               int height,
                               void* rgb_chw);

void launch_letterbox_resize_norm_i420_to_nchw_f32(rtStream_t stream,
                                                   const void* yuv,
                                                   int input_width,
                                                   int input_height,
                                                   float* output,
                                                   int output_width,
                                                   int output_height,
                                                   int resized_width,
                                                   int resized_height,
                                                   float scale,
                                                   float pad_x,
                                                   float pad_y);

void launch_letterbox_resize_norm_hwc_u8_to_nchw_f32(rtStream_t stream,
                                                     const void* input,
                                                     int input_width,
                                                     int input_height,
                                                     bool input_is_bgr,
                                                     float* output,
                                                     int output_width,
                                                     int output_height,
                                                     int resized_width,
                                                     int resized_height,
                                                     float scale,
                                                     float pad_x,
                                                     float pad_y);

void launch_letterbox_resize_norm_chw_u8_to_nchw_f32(rtStream_t stream,
                                                     const void* input,
                                                     int input_width,
                                                     int input_height,
                                                     float* output,
                                                     int output_width,
                                                     int output_height,
                                                     int resized_width,
                                                     int resized_height,
                                                     float scale,
                                                     float pad_x,
                                                     float pad_y);

#endif // XDLTEK_SAMPLES_RPP_PREPROCESS_KERNELS_CUH
