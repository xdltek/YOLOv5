#include "rpp_preprocess_kernels.cuh"

#include <__clang_cuda_builtin_vars.h>
#include <drvapi_error_string.h>
#include <rpp_com.h>
#include <rpp_drv_api.h>
#include <rpp_math.h>
#include <rpp_runtime.h>

#include <algorithm>
#include <cstdint>

__global__ void i420_to_rgb_chw_u8_kernel(const unsigned char* y_base,
                                          const unsigned char* u_base,
                                          const unsigned char* v_base,
                                          unsigned char* r_base,
                                          unsigned char* g_base,
                                          unsigned char* b_base,
                                          int width,
                                          int out_y_offset)
{
    uint32_t block_size = blockDim.x * blockDim.y;
    uint32_t block_offset = block_size * blockIdx.x;
    uint32_t local_offset = threadIdx.y * blockDim.x + threadIdx.x;
    uint32_t y_offset = block_offset + local_offset;
    uint32_t row_index = blockIdx.x * blockDim.y + threadIdx.y + out_y_offset;
    uint32_t uv_offset = (row_index / 2) * (blockDim.x / 2) + (threadIdx.x / 2);

    unsigned char Y = y_base[y_offset];
    unsigned char U = u_base[uv_offset];
    unsigned char V = v_base[uv_offset];

    int C = static_cast<int>(Y) - 16;
    int D = static_cast<int>(U) - 128;
    int E = static_cast<int>(V) - 128;
    int C298 = 298 * C;
    int R = (C298 + 409 * E + 128) >> 8;
    int G = (C298 - 100 * D - 208 * E + 128) >> 8;
    int B = (C298 + 516 * D + 128) >> 8;

    R = rpp_min(rpp_max(R, 0), 255);
    G = rpp_min(rpp_max(G, 0), 255);
    B = rpp_min(rpp_max(B, 0), 255);

    r_base[y_offset] = static_cast<unsigned char>(R);
    g_base[y_offset] = static_cast<unsigned char>(G);
    b_base[y_offset] = static_cast<unsigned char>(B);
}

__global__ void letterbox_resize_norm_hwc_kernel(const unsigned char* input,
                                                 int input_width,
                                                 int input_height,
                                                 int channel_base,
                                                 int channel_step,
                                                 float* output,
                                                 int output_width,
                                                 int output_height,
                                                 int resized_width,
                                                 int resized_height,
                                                 int pad_left,
                                                 int pad_top)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int c = blockIdx.z;

    float scale_x = static_cast<float>(input_width) / static_cast<float>(resized_width);
    float scale_y = static_cast<float>(input_height) / static_cast<float>(resized_height);
    int sx = static_cast<int>((static_cast<float>(x) + 0.5f) * scale_x);
    int sy = static_cast<int>((static_cast<float>(y) + 0.5f) * scale_y);
    sx = rpp_min(rpp_max(sx, 0), input_width - 1);
    sy = rpp_min(rpp_max(sy, 0), input_height - 1);

    int input_channel = channel_base + c * channel_step;
    float normalized = static_cast<float>(input[(sy * input_width + sx) * 3 + input_channel]) * (1.0f / 255.0f);
    int dst_idx = c * output_width * output_height + (y + pad_top) * output_width + x + pad_left;
    output[dst_idx] = normalized;
}

__global__ void letterbox_resize_norm_chw_kernel(const unsigned char* input,
                                                 int input_width,
                                                 int input_height,
                                                 float* output,
                                                 int output_width,
                                                 int output_height,
                                                 int resized_width,
                                                 int resized_height,
                                                 int pad_left,
                                                 int pad_top)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int c = blockIdx.z;

    float scale_x = static_cast<float>(input_width) / static_cast<float>(resized_width);
    float scale_y = static_cast<float>(input_height) / static_cast<float>(resized_height);
    int sx = static_cast<int>((static_cast<float>(x) + 0.5f) * scale_x);
    int sy = static_cast<int>((static_cast<float>(y) + 0.5f) * scale_y);
    sx = rpp_min(rpp_max(sx, 0), input_width - 1);
    sy = rpp_min(rpp_max(sy, 0), input_height - 1);

    int input_plane = c * input_width * input_height;
    float normalized = static_cast<float>(input[input_plane + sy * input_width + sx]) * (1.0f / 255.0f);
    int dst_idx = c * output_width * output_height + (y + pad_top) * output_width + x + pad_left;
    output[dst_idx] = normalized;
}

void launch_i420_to_rgb_chw_u8(rtStream_t stream,
                               const void* yuv,
                               int width,
                               int height,
                               void* rgb_chw)
{
    const int max_thread_number = 8191;
    uint32_t main_block_row_number = std::min(max_thread_number / width, height);
    if (main_block_row_number % 2 != 0) {
        main_block_row_number--;
    }
    uint32_t tail_block_row_number = height % main_block_row_number;

    dim3 threadsPerBlock = {0};
    dim3 blocksPerGrid = {0};
    threadsPerBlock.x = width;
    threadsPerBlock.y = main_block_row_number;
    threadsPerBlock.z = 1;
    blocksPerGrid.x = height / main_block_row_number;
    blocksPerGrid.y = 1;
    blocksPerGrid.z = 1;

    unsigned char* y_base = (unsigned char*)yuv;
    unsigned char* u_base = y_base + width * height;
    unsigned char* v_base = u_base + width * height / 4;
    unsigned char* r_base = (unsigned char*)rgb_chw;
    unsigned char* g_base = r_base + width * height;
    unsigned char* b_base = g_base + width * height;

    i420_to_rgb_chw_u8_kernel<<<blocksPerGrid, threadsPerBlock, 0, stream>>>(
        y_base, u_base, v_base, r_base, g_base, b_base, width, 0);

    if (tail_block_row_number != 0) {
        uint32_t main_total_row_number = threadsPerBlock.y * blocksPerGrid.x;
        uint32_t tail_block_offset = main_total_row_number * width;
        threadsPerBlock.x = width;
        threadsPerBlock.y = tail_block_row_number;
        threadsPerBlock.z = 1;
        blocksPerGrid.x = 1;
        blocksPerGrid.y = 1;
        blocksPerGrid.z = 1;
        i420_to_rgb_chw_u8_kernel<<<blocksPerGrid, threadsPerBlock, 0, stream>>>(
            y_base + tail_block_offset,
            u_base + tail_block_offset / 4,
            v_base + tail_block_offset / 4,
            r_base + tail_block_offset,
            g_base + tail_block_offset,
            b_base + tail_block_offset,
            width,
            main_total_row_number);
    }
}

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
                                                     float,
                                                     float pad_x,
                                                     float pad_y)
{
    int pad_left = static_cast<int>(pad_x);
    int pad_top = static_cast<int>(pad_y);

    dim3 threadsPerBlock = {0};
    dim3 blocksPerGrid = {0};
    threadsPerBlock.x = resized_width;
    threadsPerBlock.y = 1;
    threadsPerBlock.z = 1;
    blocksPerGrid.x = 1;
    blocksPerGrid.y = resized_height;
    blocksPerGrid.z = 3;

    letterbox_resize_norm_hwc_kernel<<<blocksPerGrid, threadsPerBlock, 0, stream>>>(
        static_cast<const unsigned char*>(input),
        input_width,
        input_height,
        input_is_bgr ? 2 : 0,
        input_is_bgr ? -1 : 1,
        output,
        output_width,
        output_height,
        resized_width,
        resized_height,
        pad_left,
        pad_top);
}

void launch_letterbox_resize_norm_chw_u8_to_nchw_f32(rtStream_t stream,
                                                     const void* input,
                                                     int input_width,
                                                     int input_height,
                                                     float* output,
                                                     int output_width,
                                                     int output_height,
                                                     int resized_width,
                                                     int resized_height,
                                                     float,
                                                     float pad_x,
                                                     float pad_y)
{
    int pad_left = static_cast<int>(pad_x);
    int pad_top = static_cast<int>(pad_y);

    dim3 threadsPerBlock = {0};
    dim3 blocksPerGrid = {0};
    threadsPerBlock.x = resized_width;
    threadsPerBlock.y = 1;
    threadsPerBlock.z = 1;
    blocksPerGrid.x = 1;
    blocksPerGrid.y = resized_height;
    blocksPerGrid.z = 3;

    letterbox_resize_norm_chw_kernel<<<blocksPerGrid, threadsPerBlock, 0, stream>>>(
        static_cast<const unsigned char*>(input),
        input_width,
        input_height,
        output,
        output_width,
        output_height,
        resized_width,
        resized_height,
        pad_left,
        pad_top);
}
