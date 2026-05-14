#include "yolo_perf_trace.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef YOLO_ENABLE_RPP_PERF
#include <rpp_drv_api.h>
#include <rpp_perf.h>
#endif

namespace
{
#ifdef YOLO_ENABLE_RPP_PERF
uint32_t g_trace_window = 0;
bool g_trace_started = false;
bool g_driver_trace_enabled = false;

bool trace_requested()
{
    return std::getenv("YOLO_TRACE") != nullptr || std::getenv("YOLO_PROFILE") != nullptr;
}

std::string trace_dir()
{
    const char* configured_dir = std::getenv("YOLO_TRACE_DIR");
    if (configured_dir != nullptr && configured_dir[0] != '\0') {
        return configured_dir;
    }
    return "trace";
}
#endif
}

bool yolo_perf_trace_start_if_requested(const char* prefix)
{
#ifdef YOLO_ENABLE_RPP_PERF
    if (g_trace_started) {
        return true;
    }
    if (!trace_requested()) {
        return false;
    }

    std::string dir = trace_dir();
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    if (error) {
        std::cerr << "Failed to create YOLO trace directory: " << dir
                  << ", error=" << error.message() << std::endl;
        return false;
    }

    rpp_perf_set_trace_dir(dir.c_str());
    g_trace_window = TRACE_START(prefix == nullptr ? "yolov5_demo" : prefix);
    if (g_trace_window == 0) {
        std::cerr << "Failed to start rpp_perf trace." << std::endl;
        return false;
    }

    g_trace_started = true;
    const char* path = rpp_perf_win_path(g_trace_window);
    if (path != nullptr) {
        std::cout << "YOLO rpp_perf trace: " << path << std::endl;
    }
    return true;
#else
    (void)prefix;
    return false;
#endif
}

bool yolo_perf_trace_enable_driver(unsigned int mode)
{
#ifdef YOLO_ENABLE_RPP_PERF
    if (!g_trace_started || g_trace_window == 0) {
        return false;
    }
    if (g_driver_trace_enabled) {
        return true;
    }

    RPPresult result = rppLogsDumpToTraceWindows(g_trace_window, mode);
    if (result != RPP_SUCCESS) {
        std::cerr << "Failed to enable RPP driver trace, RPPresult="
                  << static_cast<int>(result) << std::endl;
        return false;
    }

    g_driver_trace_enabled = true;
    return true;
#else
    (void)mode;
    return false;
#endif
}

void yolo_perf_trace_disable_driver()
{
#ifdef YOLO_ENABLE_RPP_PERF
    if (!g_driver_trace_enabled) {
        return;
    }
    RPPresult result = rppLogsDumpToTraceWindows(RPP_INVALID_WID, 0);
    if (result != RPP_SUCCESS) {
        std::cerr << "Failed to disable RPP driver trace, RPPresult="
                  << static_cast<int>(result) << std::endl;
    }
    g_driver_trace_enabled = false;
#endif
}

void yolo_perf_trace_shutdown()
{
#ifdef YOLO_ENABLE_RPP_PERF
    if (!g_trace_started) {
        return;
    }
    yolo_perf_trace_disable_driver();
    TRACE_END_ALL();
    g_trace_started = false;
    g_trace_window = 0;
#endif
}

bool yolo_perf_trace_enabled()
{
#ifdef YOLO_ENABLE_RPP_PERF
    return g_trace_started && _rpp_perf_enabled(g_trace_window);
#else
    return false;
#endif
}

void yolo_perf_trace_begin(const char* name, const char* category)
{
#ifdef YOLO_ENABLE_RPP_PERF
    if (yolo_perf_trace_enabled()) {
        TRACE_FUNC_CATE(name, category == nullptr ? "yolov5" : category);
    }
#else
    (void)name;
    (void)category;
#endif
}

void yolo_perf_trace_end(const char* name, const char* category)
{
#ifdef YOLO_ENABLE_RPP_PERF
    if (yolo_perf_trace_enabled()) {
        TRACE_FUNC_END_CATE(name, category == nullptr ? "yolov5" : category);
    }
#else
    (void)name;
    (void)category;
#endif
}
