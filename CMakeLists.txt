cmake_minimum_required(VERSION 3.10)

# Root build entry for the YOLOv5 demo workspace.
project(yolov5_demo_workspace VERSION 1.0)

# Export compilation database for IDE tooling (e.g., clangd).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Apply project-wide C++ and linker settings.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wno-write-strings -Wno-error=deprecated-declarations -Wno-deprecated-declarations -Werror")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath -Wl,$ORIGIN")

# RPP kernels are compiled through clang++ with the CUDA language frontend.
set(CMAKE_CUDA_COMPILER_FORCED TRUE)
set(CMAKE_CUDA_COMPILER_ID_RUN 1)
set(CMAKE_CUDA_COMPILER /usr/local/bin/clang++)
set(CMAKE_CUDA_ARCHITECTURES RPP)
set(_CMAKE_CUDA_WHOLE_FLAG "-c")
enable_language(CUDA)
set(CMAKE_CUDA_FLAGS "-fPIC -gdwarf-4 -fno-unroll-loops -O3 -ffp-contract=off -nocudalib -nocudainc -save-temps -std=c++14")

message(STATUS "ROOT_DIRECTORY: ${CMAKE_CURRENT_SOURCE_DIR}")

# Resolve OpenCV components required by the demo pipeline.
find_package(OpenCV REQUIRED COMPONENTS
    core
    imgproc
    imgcodecs
)
include_directories(${OpenCV_INCLUDE_DIRS})

# Define shared include roots used by demo sources.
set(ROOT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(ThirdPartyLib_Include ${ROOT_DIRECTORY}/3rd_party)
set(Common_Include ${ROOT_DIRECTORY}/common)

include_directories(${ThirdPartyLib_Include})
include_directories(${Common_Include})

# Configure RppRT headers and libraries from system installation path.
set(RppRT_DIR /usr/local/rpp)
set(RppRT_LIBS ${RppRT_DIR}/lib)
set(RppRT_INCLUDE_DIRS "${RppRT_DIR}/include")
link_directories(${RppRT_LIBS})
include_directories(${RppRT_INCLUDE_DIRS})

# Build demo directly in build root (no build/yolov5 subfolder).
set(SAMPLE_NAME yolov5_demo)
set(SAMPLE_SOURCE_FILES
    ${ROOT_DIRECTORY}/yolov5/main.cpp
    ${ROOT_DIRECTORY}/yolov5/labels.cpp
    ${ROOT_DIRECTORY}/yolov5/postprocess.cpp
    ${ROOT_DIRECTORY}/yolov5/preprocess.cpp
    ${ROOT_DIRECTORY}/yolov5/visualize.cpp
    ${ROOT_DIRECTORY}/yolov5/yolo.cpp
    ${ROOT_DIRECTORY}/yolov5/yolo_demo_pipeline.cpp
    ${ROOT_DIRECTORY}/common/logger.cpp
)
set(RPP_PREPROCESS_SOURCES
    ${ROOT_DIRECTORY}/rpp_preprocess/rpp_yolo_preprocessor.cpp
    ${ROOT_DIRECTORY}/rpp_preprocess/rpp_preprocess_kernels.cu
)
set(LOGGER_SOURCES
    ${ROOT_DIRECTORY}/common/logging.cpp
)
set(rpprt_libs RppRT nvonnxparser caffeparser urpp)

include_directories(
    ${ROOT_DIRECTORY}/yolov5
    ${ROOT_DIRECTORY}/rpp_preprocess
    ${ROOT_DIRECTORY}/common
)

add_library(rpp_preprocessing SHARED ${RPP_PREPROCESS_SOURCES})
target_include_directories(rpp_preprocessing
    PRIVATE
    ${ROOT_DIRECTORY}/yolov5
    ${ROOT_DIRECTORY}/rpp_preprocess
    ${ROOT_DIRECTORY}/common
    ${RppRT_INCLUDE_DIRS}
)
target_link_libraries(rpp_preprocessing ${rpprt_libs})
set_target_properties(rpp_preprocessing PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "$ORIGIN;${RppRT_LIBS}"
)

add_executable(${SAMPLE_NAME}
    ${SAMPLE_SOURCE_FILES}
    ${LOGGER_SOURCES}
    ${PARSE_SOURCES}
    ${CJSON_SOURCES}
    ${RPP_PREF_SOURCES}
)
target_link_libraries(${SAMPLE_NAME} rpp_preprocessing ${OpenCV_LIBS} ${rpprt_libs} pthread)

# Copy sample image into build root (ONNX and class names are not bundled; see README).
configure_file("${ROOT_DIRECTORY}/image/test_1.png" "${CMAKE_BINARY_DIR}/test_1.png" COPYONLY)
