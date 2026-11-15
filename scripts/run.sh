#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# RPP SDK installations that ship rpp_server require it before application launch.
# If this binary is absent, the script leaves server management to the runtime.
RPP_SERVER_BIN="/usr/local/rpp/bin/rpp_server"

if [[ "${YOLO_COLOR:-always}" != "never" ]]; then
    BLUE=$'\033[1;34m'; CYAN=$'\033[1;36m'; GREEN=$'\033[1;32m'; RED=$'\033[1;31m'; RESET=$'\033[0m'
else
    BLUE=""; CYAN=""; GREEN=""; RED=""; RESET=""
fi

show_help() {
    cat <<'EOF'
Usage: scripts/run.sh [options]

Options:
  -m, --model PATH      ONNX model path. Required unless weights/yolov5s.onnx exists.
  -d, --demo NAME       Workload selector: all, rgb, or yuv. Default: all
  -i, --input PATH      RGB image path or I420 YUV path.
  -o, --output PATH     Rendered output path.
  -b, --build-dir DIR   Build directory. Default: build
  -l, --loop COUNT      Measured loop count. Default: 1
      --yuv-width N     I420 width for the YUV workload. Default: 1280
      --yuv-height N    I420 height for the YUV workload. Default: 720
  -h, --help            Show this help message.
EOF
}

resolve_repo_path() {
    local path="$1"
    # Interpret relative CLI paths from the repository root so this script works
    # no matter where the user invokes it from.
    if [[ "${path}" == /* ]]; then
        printf '%s\n' "${path}"
    else
        printf '%s\n' "${ROOT_DIR}/${path}"
    fi
}

is_rpp_server_running() {
    # Match both the full SDK path and the process name for different launch styles.
    pgrep -f "${RPP_SERVER_BIN}" >/dev/null 2>&1 || pgrep -x "rpp_server" >/dev/null 2>&1
}

ensure_rpp_server() {
    # Older or simulator environments may not require a standalone rpp_server.
    if [[ ! -x "${RPP_SERVER_BIN}" ]]; then
        return 0
    fi

    if is_rpp_server_running; then
        echo "RPP server: already running"
        return 0
    fi

    mkdir -p "${build_dir}/logs"
    local log_path="${build_dir}/logs/rpp_server.log"
    # Start the service once and keep its stdout/stderr for field debugging.
    echo "RPP server: starting ${RPP_SERVER_BIN}"
    nohup "${RPP_SERVER_BIN}" > "${log_path}" 2>&1 &
    sleep 1

    if ! is_rpp_server_running; then
        echo "Failed to start rpp_server. See ${log_path}" >&2
        exit 1
    fi
}

run_workload() {
    local selected_workload="$1"
    local selected_input="$2"
    local selected_output="$3"
    local bin
    local args

    case "${selected_workload}" in
        rgb)
            # Default RGB assets are copied into build/assets during CMake config.
            bin="${build_dir}/bin/yolov5_rgb_image_demo"
            [[ -z "${selected_input}" ]] && selected_input="${build_dir}/assets/test_1.png"
            [[ -z "${selected_output}" ]] && selected_output="${build_dir}/output/output_rgb.jpg"
            selected_input="$(resolve_repo_path "${selected_input}")"
            selected_output="$(resolve_repo_path "${selected_output}")"
            args=(-m "${model}" -l "${loop_count}")
            args+=(-i "${selected_input}" -o "${selected_output}")
            ;;
        yuv)
            # The bundled YUV sample is a single packed I420 frame.
            bin="${build_dir}/bin/yolov5_yuv_frame_demo"
            [[ -z "${selected_input}" ]] && selected_input="${build_dir}/assets/test_0.yuv"
            [[ -z "${selected_output}" ]] && selected_output="${build_dir}/output/output_yuv.jpg"
            selected_input="$(resolve_repo_path "${selected_input}")"
            selected_output="$(resolve_repo_path "${selected_output}")"
            args=(-m "${model}" -l "${loop_count}" --yuv-width "${yuv_width}" --yuv-height "${yuv_height}")
            args+=(-i "${selected_input}" -o "${selected_output}")
            ;;
        *)
            echo "--demo must be all, rgb, or yuv." >&2
            exit 1
            ;;
    esac

    if [[ ! -x "${bin}" ]]; then
        echo "${RED}[ERROR   ]${RESET} Application executable not found: ${bin}" >&2
        echo "${CYAN}[ACTION  ]${RESET} Build the project first with: scripts/build.sh" >&2
        exit 1
    fi

    printf '%s' "${BLUE}"
    cat <<EOF

+------------------------------------------------------------------+
|         XDL AI ACCELERATION - YOLOv5 OBJECT DETECTION            |
+------------------------------------------------------------------+
  Workload                 : ${selected_workload^^} image inference
  Model                    : ${model}
  Input                    : ${selected_input}
  Measured frames          : ${loop_count} (+ 1 warmup)
  Result image             : ${selected_output}
+------------------------------------------------------------------+
EOF
    printf '%s' "${RESET}"
    echo "${CYAN}[START   ]${RESET} Launching accelerated inference pipeline"
    "${bin}" "${args[@]}"
}

model=""
workload="all"
input=""
output=""
build_dir="build"
loop_count="1"
yuv_width="1280"
yuv_height="720"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -m|--model)
            model="$2"
            shift 2
            ;;
        -d|--demo)
            workload="$2"
            shift 2
            ;;
        -i|--input)
            input="$2"
            shift 2
            ;;
        -o|--output)
            output="$2"
            shift 2
            ;;
        -b|--build-dir)
            build_dir="$2"
            shift 2
            ;;
        -l|--loop)
            loop_count="$2"
            shift 2
            ;;
        --yuv-width)
            yuv_width="$2"
            shift 2
            ;;
        --yuv-height)
            yuv_height="$2"
            shift 2
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

if [[ ! "${loop_count}" =~ ^[1-9][0-9]*$ ]]; then
    echo "${RED}[ERROR   ]${RESET} --loop must be a positive integer." >&2
    exit 1
fi

if [[ -z "${model}" ]]; then
    default_model="${ROOT_DIR}/weights/yolov5s.onnx"
    if [[ -f "${default_model}" ]]; then
        model="${default_model}"
        echo "${GREEN}[MODEL   ]${RESET} --model not specified; using default: ${model}"
    else
        echo "${RED}[ERROR   ]${RESET} No ONNX model was specified." >&2
        echo "${CYAN}[DETAIL  ]${RESET} This package does not include a model, and the optional fallback was not found: ${default_model}" >&2
        echo "${CYAN}[ACTION  ]${RESET} Provide one with: scripts/run.sh --model /path/to/model.onnx" >&2
        exit 1
    fi
fi

model="$(resolve_repo_path "${model}")"
if [[ ! -f "${model}" ]]; then
    echo "${RED}[ERROR   ]${RESET} ONNX model not found: ${model}" >&2
    exit 1
fi
build_dir="$(resolve_repo_path "${build_dir}")"

case "${workload}" in
    all)
        # A single custom input path cannot represent both BGR image and I420
        # frame inputs, so require an explicit single-workload selection.
        if [[ -n "${input}" || -n "${output}" ]]; then
            echo "--input and --output require --demo rgb or --demo yuv when running a single workload." >&2
            exit 1
        fi
        ensure_rpp_server
        # Run default workloads serially so logs and output files remain predictable.
        run_workload rgb "" ""
        run_workload yuv "" ""
        ;;
    rgb|yuv)
        ensure_rpp_server
        run_workload "${workload}" "${input}" "${output}"
        ;;
    *)
        echo "--demo must be all, rgb, or yuv." >&2
        exit 1
        ;;
esac
