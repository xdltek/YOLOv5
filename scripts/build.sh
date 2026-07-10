#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Keep the default model local and ignored by Git. The build script only makes
# sure the expected demo model exists before CMake config starts.
DEFAULT_MODEL_DIR="${ROOT_DIR}/weights"
DEFAULT_MODEL_PATH="${DEFAULT_MODEL_DIR}/yolov5s.onnx"
DEFAULT_MODEL_URL="https://raw.githubusercontent.com/xdltek/onnx_model_tool/main/examples/onnx/yolov5s.onnx"
DEFAULT_MODEL_MD5="12f4516124a90056f01692bae254ec6a"
CMAKE_GENERATOR_NAME="Unix Makefiles"

show_help() {
    cat <<'EOF'
Usage: scripts/build.sh [options]

Options:
  -b, --build-dir DIR   Build directory. Default: build
      --perf            Enable YOLO_ENABLE_RPP_PERF.
      --skip-model      Do not download or verify the default YOLOv5s ONNX model.
  -h, --help            Show this help message.
EOF
}

ensure_default_model() {
    mkdir -p "${DEFAULT_MODEL_DIR}"

    # Reuse an existing model only when the checksum matches the released file.
    if [[ -f "${DEFAULT_MODEL_PATH}" ]] &&
        echo "${DEFAULT_MODEL_MD5}  ${DEFAULT_MODEL_PATH}" | md5sum -c -
    then
        return 0
    fi

    if [[ -f "${DEFAULT_MODEL_PATH}" ]]; then
        echo "Default model checksum mismatch; re-downloading ${DEFAULT_MODEL_PATH}"
    fi

    if ! command -v wget >/dev/null 2>&1; then
        echo "wget is required to download ${DEFAULT_MODEL_URL}" >&2
        exit 1
    fi

    # Download to a temporary file first so an interrupted fetch never replaces
    # a valid model path with a partial ONNX file.
    local tmp_model="${DEFAULT_MODEL_PATH}.tmp"
    echo "Downloading default model: ${DEFAULT_MODEL_URL}"
    wget -O "${tmp_model}" "${DEFAULT_MODEL_URL}"
    echo "${DEFAULT_MODEL_MD5}  ${tmp_model}" | md5sum -c -
    mv "${tmp_model}" "${DEFAULT_MODEL_PATH}"
}

resolve_repo_path() {
    local path="$1"
    # Normalize user paths so safety checks catch variants such as "." or "./".
    if [[ "${path}" == /* ]]; then
        readlink -m "${path}"
    else
        readlink -m "${ROOT_DIR}/${path}"
    fi
}

clean_build_outputs() {
    # Never treat the repository root or filesystem root as a build directory.
    if [[ "${build_dir}" == "/" || "${build_dir}" == "${ROOT_DIR}" ]]; then
        echo "Refusing to clean unsafe build directory: ${build_dir}" >&2
        exit 1
    fi

    if [[ ! -d "${build_dir}" ]]; then
        return 0
    fi

    echo "Cleaning previous build outputs in ${build_dir}"
    # Remove only generated CMake/build/runtime outputs. Source assets and
    # weights live outside the build directory and are intentionally preserved.
    rm -rf "${build_dir}/CMakeCache.txt" \
           "${build_dir}/CMakeFiles" \
           "${build_dir}/CTestTestfile.cmake" \
           "${build_dir}/Testing" \
           "${build_dir}/Makefile" \
           "${build_dir}/build.ninja" \
           "${build_dir}/rules.ninja" \
           "${build_dir}/.ninja_deps" \
           "${build_dir}/.ninja_log" \
           "${build_dir}/cmake_install.cmake" \
           "${build_dir}/compile_commands.json" \
           "${build_dir}/install_manifest.txt" \
           "${build_dir}/bin" \
           "${build_dir}/lib" \
           "${build_dir}/assets" \
           "${build_dir}/output" \
           "${build_dir}/trace" \
           "${build_dir}/logs"
}

ensure_compatible_build_dir() {
    # A build directory created by another generator cannot be reused safely by
    # Makefiles, so clear only the generator-specific metadata.
    local cache_file="${build_dir}/CMakeCache.txt"
    if [[ ! -f "${cache_file}" ]]; then
        return 0
    fi

    local cached_generator
    cached_generator="$(awk -F= '/^CMAKE_GENERATOR:INTERNAL=/{print $2}' "${cache_file}")"
    if [[ -z "${cached_generator}" || "${cached_generator}" == "${CMAKE_GENERATOR_NAME}" ]]; then
        return 0
    fi

    echo "Existing build directory uses CMake generator '${cached_generator}', recreating CMake cache for '${CMAKE_GENERATOR_NAME}'."
    rm -rf "${build_dir}/CMakeCache.txt" \
           "${build_dir}/CMakeFiles" \
           "${build_dir}/build.ninja" \
           "${build_dir}/rules.ninja" \
           "${build_dir}/.ninja_deps" \
           "${build_dir}/.ninja_log" \
           "${build_dir}/cmake_install.cmake" \
           "${build_dir}/Makefile"
}

build_dir="build"
enable_perf="OFF"
skip_model="OFF"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build-dir)
            build_dir="$2"
            shift 2
            ;;
        --perf)
            enable_perf="ON"
            shift
            ;;
        --skip-model)
            skip_model="ON"
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            show_help >&2
            exit 1
            ;;
    esac
done

if [[ "${skip_model}" != "ON" ]]; then
    ensure_default_model
fi

# Resolve after argument parsing so every downstream path uses one canonical
# build directory value.
build_dir="$(resolve_repo_path "${build_dir}")"
clean_build_outputs
ensure_compatible_build_dir
cmake -S "${ROOT_DIR}" -B "${build_dir}" -G "${CMAKE_GENERATOR_NAME}" -DYOLO_ENABLE_RPP_PERF="${enable_perf}"
cmake --build "${build_dir}" -j"$(nproc)"
