<a id="english"></a>

# Perf Trace Module

Language: [English](#english) | [中文](#chinese)

This directory contains the optional `rpp_perf` integration used by the YOLOv5 demos. It wraps trace-window creation, scoped host events, and RPP driver-record forwarding behind a small reusable C++ interface.

The module is inactive by default. It becomes active only when the project is configured with `YOLO_ENABLE_RPP_PERF=ON`.

## Files

- `perf_trace_session.h`
  Public helper API, RAII session owner, and `PERF_SCOPE` macros.
- `perf_trace_session.cpp`
  `rpp_perf` implementation guarded by `YOLO_ENABLE_RPP_PERF`.

## Build Configuration

Enable trace support at CMake configure time:

```bash
cmake -S . -B build -DYOLO_ENABLE_RPP_PERF=ON
cmake --build build -j4
```

When the option is off, calls to `PERF_SCOPE` remain valid but become no-ops. If a trace is requested in a non-perf build, the helper prints a rebuild hint for `YOLO_ENABLE_RPP_PERF=ON`.

## CMake Integration

The project builds the perf wrapper as a standalone target:

```cmake
option(YOLO_ENABLE_RPP_PERF "Enable rpp_perf trace integration" OFF)

add_library(perf_trace SHARED
    ${ROOT_DIRECTORY}/src/perf/perf_trace_session.cpp
)

target_include_directories(perf_trace PUBLIC
    ${ROOT_DIRECTORY}/src/perf
)

target_link_libraries(perf_trace PRIVATE pthread)

if(YOLO_ENABLE_RPP_PERF)
    target_compile_definitions(perf_trace PUBLIC YOLO_ENABLE_RPP_PERF=1)
    target_compile_options(perf_trace PRIVATE -Wno-c99-designator)
    target_link_libraries(perf_trace PRIVATE rpp_perf urpp)
endif()
```

Targets that emit trace scopes link against `perf_trace`:

```cmake
target_link_libraries(my_demo PRIVATE perf_trace)
```

If the target already links a library that exposes `perf_trace` as a public dependency, no extra demo-level link is required. In this project, `yolov5_core`, `rpp_yolo_preprocess`, and `rpp_yolo_postprocess` already link the perf wrapper.

## Demo Behavior

The RGB and YUV demos automatically create a perf session in perf-enabled builds:

```cpp
PerfTraceSession trace_session("yolov5_rgb_image_demo",
                               kPerfTraceEnabled,
                               "../trace");
trace_session.enableDriverTrace(0);
```

The trace file is written under:

```text
build/trace/
```

The demo prints the generated trace path when the session starts.

## Call Flow

```text
+-------------------------+
| Demo main               |
| create PerfTraceSession |
+-------------------------+
            |
            v
+-------------------------+
| TRACE_START             |
| open rpp_perf window    |
+-------------------------+
            |
            v
+-------------------------+
| enableDriverTrace(0)    |
| attach drv_api records  |
+-------------------------+
            |
            v
+-------------------------+
| PERF_SCOPE_CATE(...)    |
| host stage trace events |
+-------------------------+
            |
            v
+-------------------------+
| TRACE_END_ALL           |
| close JSON trace        |
+-------------------------+
```

## Driver API Records

To make the generated JSON include RPP driver/runtime records, two conditions must both be true:

1. The project is built with:

```bash
-DYOLO_ENABLE_RPP_PERF=ON
```

2. The active session calls:

```cpp
trace_session.enableDriverTrace(0);
```

Internally this calls:

```cpp
rppLogsDumpToTraceWindows(g_trace_window, mode);
```

The current demos pass mode `0`. This follows the OpenRT sandbox style and forwards driver records into the same trace window as the host `PERF_SCOPE` events.

If the JSON contains only host demo events and no driver records, check:

- The binary was rebuilt after enabling `YOLO_ENABLE_RPP_PERF`.
- `enableDriverTrace(0)` is called after the session is active.
- The RPP system perf configuration allows driver/device records to be forwarded.
- The process has permission to write into `build/trace`.

## Module Integration

For a demo or library target:

1. Include the header:

```cpp
#include "perf_trace_session.h"
```

2. Define the build-controlled switch:

```cpp
#ifdef YOLO_ENABLE_RPP_PERF
constexpr bool kPerfTraceEnabled = true;
#else
constexpr bool kPerfTraceEnabled = false;
#endif
```

3. Create one session near the start of `main()` after argument parsing:

```cpp
PerfTraceSession trace_session("my_demo_name",
                               kPerfTraceEnabled,
                               "../trace");
trace_session.enableDriverTrace(0);
```

4. Add scoped events around meaningful stages:

```cpp
{
    PERF_SCOPE_CATE("my_demo_preprocess", "preprocess");
    // stage work
}
```

Use stable names and categories so traces from different executable modes keep the same field meaning.

## Scope Naming

Recommended categories:

- `preprocess`
- `inference`
- `postprocess`
- `io`
- `yolov5`

Recommended naming style:

```text
component_stage_action
```

Examples:

```cpp
PERF_SCOPE_CATE("yolov5_pipeline_preprocess_stage", "preprocess");
PERF_SCOPE_CATE("rpp_yolo_postprocess_run", "postprocess");
```

## Notes

- The trace session owns process-wide trace state. Create one session per process.
- `PerfScope` is intentionally cheap when tracing is disabled.
- Driver trace forwarding is disabled automatically when the session shuts down.
- Demo timing printed to stdout remains independent from `rpp_perf`; stdout timing is always available, while JSON traces require perf-enabled builds.

---

<a id="chinese"></a>

# Perf Trace 模块

语言：[English](#english) | [中文](#chinese)

本目录包含 YOLOv5 demo 使用的可选 `rpp_perf` 集成。它用一个较小的可复用 C++ 接口封装 trace window 创建、host 侧范围事件，以及 RPP driver 记录转发。

默认情况下该模块不启用。只有项目使用 `YOLO_ENABLE_RPP_PERF=ON` 配置构建时，trace 功能才会生效。

## 文件说明

- `perf_trace_session.h`
  对外 helper API、RAII session 对象，以及 `PERF_SCOPE` 宏。
- `perf_trace_session.cpp`
  由 `YOLO_ENABLE_RPP_PERF` 宏保护的 `rpp_perf` 实现。

## 构建配置

在 CMake 配置阶段开启 trace：

```bash
cmake -S . -B build -DYOLO_ENABLE_RPP_PERF=ON
cmake --build build -j4
```

当该选项关闭时，`PERF_SCOPE` 调用仍然可以保留在代码中，但会变成 no-op。如果未开启 perf 的构建里请求 trace，helper 会打印使用 `YOLO_ENABLE_RPP_PERF=ON` 重新构建的提示。

## CMake 接入

项目将 perf wrapper 构建为独立 target：

```cmake
option(YOLO_ENABLE_RPP_PERF "Enable rpp_perf trace integration" OFF)

add_library(perf_trace SHARED
    ${ROOT_DIRECTORY}/src/perf/perf_trace_session.cpp
)

target_include_directories(perf_trace PUBLIC
    ${ROOT_DIRECTORY}/src/perf
)

target_link_libraries(perf_trace PRIVATE pthread)

if(YOLO_ENABLE_RPP_PERF)
    target_compile_definitions(perf_trace PUBLIC YOLO_ENABLE_RPP_PERF=1)
    target_compile_options(perf_trace PRIVATE -Wno-c99-designator)
    target_link_libraries(perf_trace PRIVATE rpp_perf urpp)
endif()
```

需要输出 trace scope 的 target 链接 `perf_trace`：

```cmake
target_link_libraries(my_demo PRIVATE perf_trace)
```

如果当前 target 已经链接了一个公开依赖 `perf_trace` 的库，则 demo 层不需要重复链接。本项目中，`yolov5_core`、`rpp_yolo_preprocess`、`rpp_yolo_postprocess` 已经链接 perf wrapper。

## Demo 行为

RGB 和 YUV demo 在 perf-enabled 构建中会自动创建 perf session：

```cpp
PerfTraceSession trace_session("yolov5_rgb_image_demo",
                               kPerfTraceEnabled,
                               "../trace");
trace_session.enableDriverTrace(0);
```

trace 文件会写入：

```text
build/trace/
```

session 启动时，demo 会打印生成的 trace 路径。

## 调用流程

```text
+-------------------------+
| Demo main               |
| 创建 PerfTraceSession   |
+-------------------------+
            |
            v
+-------------------------+
| TRACE_START             |
| 打开 rpp_perf window    |
+-------------------------+
            |
            v
+-------------------------+
| enableDriverTrace(0)    |
| 挂接 drv_api 记录       |
+-------------------------+
            |
            v
+-------------------------+
| PERF_SCOPE_CATE(...)    |
| host 阶段 trace 事件    |
+-------------------------+
            |
            v
+-------------------------+
| TRACE_END_ALL           |
| 关闭 JSON trace         |
+-------------------------+
```

## Driver API 记录

如果希望生成的 JSON 中包含 RPP driver/runtime 记录，需要同时满足两个条件：

1. 项目使用如下选项构建：

```bash
-DYOLO_ENABLE_RPP_PERF=ON
```

2. 活跃 session 调用：

```cpp
trace_session.enableDriverTrace(0);
```

内部会调用：

```cpp
rppLogsDumpToTraceWindows(g_trace_window, mode);
```

当前 demo 传入 mode `0`。该方式参考 OpenRT sandbox，将 driver 记录转发到和 host `PERF_SCOPE` 事件相同的 trace window 中。

如果 JSON 中只有 demo host 事件，没有 driver 记录，可以检查：

- binary 是否在开启 `YOLO_ENABLE_RPP_PERF` 后重新构建。
- `enableDriverTrace(0)` 是否在 session active 后调用。
- RPP 系统 perf 配置是否允许 driver/device 记录被转发。
- 当前进程是否有权限写入 `build/trace`。

## 模块接入

demo 或 library target 中接入时：

1. 引入头文件：

```cpp
#include "perf_trace_session.h"
```

2. 定义由构建宏控制的开关：

```cpp
#ifdef YOLO_ENABLE_RPP_PERF
constexpr bool kPerfTraceEnabled = true;
#else
constexpr bool kPerfTraceEnabled = false;
#endif
```

3. 在 `main()` 参数解析之后创建一个 session：

```cpp
PerfTraceSession trace_session("my_demo_name",
                               kPerfTraceEnabled,
                               "../trace");
trace_session.enableDriverTrace(0);
```

4. 在关键阶段添加范围事件：

```cpp
{
    PERF_SCOPE_CATE("my_demo_preprocess", "preprocess");
    // stage work
}
```

使用稳定的名称和 category，可以让不同执行模式的 trace 字段保持相同含义。

## Scope 命名

推荐 category：

- `preprocess`
- `inference`
- `postprocess`
- `io`
- `yolov5`

推荐命名风格：

```text
component_stage_action
```

示例：

```cpp
PERF_SCOPE_CATE("yolov5_pipeline_preprocess_stage", "preprocess");
PERF_SCOPE_CATE("rpp_yolo_postprocess_run", "postprocess");
```

## 注意事项

- trace session 拥有进程级 trace 状态。每个进程创建一个 session 即可。
- tracing 关闭时，`PerfScope` 开销很低。
- session 关闭时会自动停止 driver trace forwarding。
- demo stdout 打印的时间和 `rpp_perf` 相互独立；stdout 时间始终可用，JSON trace 需要 perf-enabled 构建。
