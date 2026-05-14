/**
 * @file perf_trace_session.h
 * @brief Optional rpp_perf trace session helpers.
 */
#ifndef XDLTEK_SAMPLES_PERF_TRACE_SESSION_H
#define XDLTEK_SAMPLES_PERF_TRACE_SESSION_H

#include <string>

bool perf_trace_start(const char* prefix, bool enabled, const char* output_dir);
bool perf_trace_enable_driver(unsigned int mode = 0);
void perf_trace_disable_driver();
void perf_trace_shutdown();
bool perf_trace_enabled();
void perf_trace_begin(const char* name, const char* category);
void perf_trace_end(const char* name, const char* category);

class PerfTraceSession
{
public:
    PerfTraceSession(const char* prefix, bool enabled, const std::string& output_dir = "trace")
    {
        active_ = perf_trace_start(prefix, enabled, output_dir.c_str());
    }

    ~PerfTraceSession()
    {
        if (active_) {
            perf_trace_shutdown();
        }
    }

    bool active() const { return active_; }
    bool enableDriverTrace(unsigned int mode = 0) const;

    PerfTraceSession(const PerfTraceSession&) = delete;
    PerfTraceSession& operator=(const PerfTraceSession&) = delete;

private:
    bool active_ = false;
};

class PerfScope
{
public:
    PerfScope(const char* name, const char* category = "yolov5")
        : name_(name), category_(category)
    {
        perf_trace_begin(name_, category_);
    }

    ~PerfScope()
    {
        perf_trace_end(name_, category_);
    }

    PerfScope(const PerfScope&) = delete;
    PerfScope& operator=(const PerfScope&) = delete;

private:
    const char* name_ = nullptr;
    const char* category_ = nullptr;
};

#define PERF_CONCAT_INNER(a, b) a##b
#define PERF_CONCAT(a, b) PERF_CONCAT_INNER(a, b)
#define PERF_SCOPE(name) PerfScope PERF_CONCAT(perf_scope_, __LINE__)(name)
#define PERF_SCOPE_CATE(name, category) PerfScope PERF_CONCAT(perf_scope_, __LINE__)(name, category)

#endif // XDLTEK_SAMPLES_PERF_TRACE_SESSION_H
