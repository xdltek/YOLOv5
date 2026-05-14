cmake_minimum_required(VERSION 3.10)

project(yolov5_demo_workspace VERSION 1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wno-write-strings -Wno-error=deprecated-declarations -Wno-deprecated-declarations -Werror")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath -Wl,$ORIGIN")

set(CMAKE_CUDA_COMPILER_FORCED TRUE)
set(CMAKE_CUDA_COMPILER_ID_RUN 1)
set(CMAKE_CUDA_COMPILER /usr/local/bin/clang++)
set(CMAKE_CUDA_ARCHITECTURES RPP)
set(_CMAKE_CUDA_WHOLE_FLAG "-c")
enable_language(CUDA)
set(CMAKE_CUDA_FLAGS "-fPIC -gdwarf-4 -fno-unroll-loops -O3 -ffp-contract=off -nocudalib -nocudainc -save-temps -std=c++14")

option(YOLO_ENABLE_RPP_PERF "Enable rpp_perf trace integration" OFF)

set(ROOT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(RppRT_DIR /usr/local/rpp)
set(RppRT_LIBS ${RppRT_DIR}/lib)
set(RppRT_INCLUDE_DIRS "${RppRT_DIR}/include")
set(ThirdPartyLib_Include ${ROOT_DIRECTORY}/3rd_party)

message(STATUS "ROOT_DIRECTORY: ${ROOT_DIRECTORY}")

find_package(OpenCV REQUIRED COMPONENTS
    core
    imgproc
    imgcodecs
)

link_directories(${RppRT_LIBS})
set(rpprt_libs RppRT nvonnxparser caffeparser urpp)

set(PROJECT_INCLUDE_DIRS
    ${ROOT_DIRECTORY}/src/core
    ${ROOT_DIRECTORY}/src/perf
    ${ROOT_DIRECTORY}/src/utils
    ${ROOT_DIRECTORY}/src/yolov5
    ${ROOT_DIRECTORY}/rpp_preprocess
    ${ROOT_DIRECTORY}/rpp_postprocess
    ${ThirdPartyLib_Include}
    ${RppRT_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

add_library(yolov5_utils SHARED
    ${ROOT_DIRECTORY}/src/utils/logger.cpp
    ${ROOT_DIRECTORY}/src/utils/logging.cpp
)
target_include_directories(yolov5_utils PUBLIC ${PROJECT_INCLUDE_DIRS})
set_target_properties(yolov5_utils PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

add_library(perf_trace SHARED
    ${ROOT_DIRECTORY}/src/perf/perf_trace_session.cpp
)
target_include_directories(perf_trace PUBLIC ${PROJECT_INCLUDE_DIRS})
target_link_libraries(perf_trace PRIVATE pthread)
if(YOLO_ENABLE_RPP_PERF)
    target_compile_definitions(perf_trace PRIVATE YOLO_ENABLE_RPP_PERF=1)
    target_compile_options(perf_trace PRIVATE -Wno-c99-designator)
    target_link_libraries(perf_trace PRIVATE rpp_perf urpp)
endif()
set_target_properties(perf_trace PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

add_library(rpp_preprocessing SHARED
    ${ROOT_DIRECTORY}/rpp_preprocess/rpp_yolo_preprocessor.cpp
    ${ROOT_DIRECTORY}/rpp_preprocess/rpp_preprocess_kernels.cu
)
target_include_directories(rpp_preprocessing PUBLIC ${PROJECT_INCLUDE_DIRS})
target_link_libraries(rpp_preprocessing PUBLIC perf_trace yolov5_utils ${rpprt_libs})
set_target_properties(rpp_preprocessing PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

add_library(rpp_postprocessing SHARED
    ${ROOT_DIRECTORY}/rpp_postprocess/rpp_yolo_postprocessor.cpp
)
target_include_directories(rpp_postprocessing PUBLIC ${PROJECT_INCLUDE_DIRS})
target_link_libraries(rpp_postprocessing PUBLIC perf_trace yolov5_utils ${rpprt_libs})
set_target_properties(rpp_postprocessing PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

add_library(yolov5_core SHARED
    ${ROOT_DIRECTORY}/src/core/rpp_infer_engine.cpp
    ${ROOT_DIRECTORY}/src/yolov5/labels.cpp
    ${ROOT_DIRECTORY}/src/yolov5/yolo_pipeline.cpp
)
target_include_directories(yolov5_core PUBLIC ${PROJECT_INCLUDE_DIRS})
target_link_libraries(yolov5_core PUBLIC
    perf_trace
    rpp_preprocessing
    rpp_postprocessing
    yolov5_utils
    ${OpenCV_LIBS}
    ${rpprt_libs}
    pthread
)
set_target_properties(yolov5_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

add_library(yolov5_visualization SHARED
    ${ROOT_DIRECTORY}/src/yolov5/visualize.cpp
)
target_include_directories(yolov5_visualization PUBLIC ${PROJECT_INCLUDE_DIRS})
target_link_libraries(yolov5_visualization PUBLIC yolov5_core ${OpenCV_LIBS})
set_target_properties(yolov5_visualization PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

add_executable(yolov5_rgb_image_demo
    ${ROOT_DIRECTORY}/demos/rgb_image_single_process/main.cpp
)
target_include_directories(yolov5_rgb_image_demo PRIVATE ${PROJECT_INCLUDE_DIRS})
target_link_libraries(yolov5_rgb_image_demo PRIVATE yolov5_core yolov5_visualization)
set_target_properties(yolov5_rgb_image_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

add_executable(yolov5_yuv_image_demo
    ${ROOT_DIRECTORY}/demos/yuv_image_single_process/main.cpp
)
target_include_directories(yolov5_yuv_image_demo PRIVATE ${PROJECT_INCLUDE_DIRS})
target_link_libraries(yolov5_yuv_image_demo PRIVATE yolov5_core yolov5_visualization)
set_target_properties(yolov5_yuv_image_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

configure_file("${ROOT_DIRECTORY}/image/test_1.png" "${CMAKE_BINARY_DIR}/test_1.png" COPYONLY)
