#ifndef XDLTEK_SAMPLES_YOLO_PERF_TRACE_H
#define XDLTEK_SAMPLES_YOLO_PERF_TRACE_H

bool yolo_perf_trace_start_if_requested(const char* prefix);
bool yolo_perf_trace_enable_driver(unsigned int mode = 0);
void yolo_perf_trace_disable_driver();
void yolo_perf_trace_shutdown();
bool yolo_perf_trace_enabled();
void yolo_perf_trace_begin(const char* name, const char* category);
void yolo_perf_trace_end(const char* name, const char* category);

class YoloPerfTraceSession
{
public:
    explicit YoloPerfTraceSession(const char* prefix)
    {
        active_ = yolo_perf_trace_start_if_requested(prefix);
    }

    ~YoloPerfTraceSession()
    {
        if (active_) {
            yolo_perf_trace_shutdown();
        }
    }

    YoloPerfTraceSession(const YoloPerfTraceSession&) = delete;
    YoloPerfTraceSession& operator=(const YoloPerfTraceSession&) = delete;

private:
    bool active_ = false;
};

class YoloPerfScope
{
public:
    YoloPerfScope(const char* name, const char* category = "yolov5")
        : name_(name), category_(category)
    {
        yolo_perf_trace_begin(name_, category_);
    }

    ~YoloPerfScope()
    {
        yolo_perf_trace_end(name_, category_);
    }

    YoloPerfScope(const YoloPerfScope&) = delete;
    YoloPerfScope& operator=(const YoloPerfScope&) = delete;

private:
    const char* name_ = nullptr;
    const char* category_ = nullptr;
};

#define YOLO_PERF_CONCAT_INNER(a, b) a##b
#define YOLO_PERF_CONCAT(a, b) YOLO_PERF_CONCAT_INNER(a, b)
#define YOLO_PERF_SCOPE(name) YoloPerfScope YOLO_PERF_CONCAT(yolo_perf_scope_, __LINE__)(name)
#define YOLO_PERF_SCOPE_CATE(name, category) YoloPerfScope YOLO_PERF_CONCAT(yolo_perf_scope_, __LINE__)(name, category)

#endif // XDLTEK_SAMPLES_YOLO_PERF_TRACE_H
