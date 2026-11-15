#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

if [[ "${YOLO_COLOR:-always}" != "never" ]]; then
    BLUE=$'\033[1;34m'; CYAN=$'\033[1;36m'; GREEN=$'\033[1;32m'; RED=$'\033[1;31m'; RESET=$'\033[0m'
else
    BLUE=""; CYAN=""; GREEN=""; RED=""; RESET=""
fi

show_help() {
    cat <<'EOF'
Usage: scripts/benchmark.sh [options]

Options:
  -o, --onnx PATH       ONNX model path. Uses weights/yolov5s.onnx when present.
  -d, --demo NAME       Workload to run: all, rgb, or yuv. Default: all
  -i, --input PATH      RGB image path or I420 YUV path.
  -l, --loop COUNT      Measured loop count. Default: 20
      --yuv-width N     I420 width for yuv demo. Default: 1280
      --yuv-height N    I420 height for yuv demo. Default: 720
  -h, --help            Show this help message.

Logs are saved to build/logs/benchmark_<demo>_<timestamp>.log.
EOF
}

model=""
demo="all"
input=""
loop_count="20"
yuv_width="1280"
yuv_height="720"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -o|--onnx)
            model="$2"
            shift 2
            ;;
        -d|--demo)
            demo="$2"
            shift 2
            ;;
        -i|--input)
            input="$2"
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

if [[ "${demo}" != "all" && "${demo}" != "rgb" && "${demo}" != "yuv" ]]; then
    echo "--demo must be all, rgb, or yuv." >&2
    exit 1
fi

if [[ "${demo}" == "all" && -n "${input}" ]]; then
    # RGB and YUV consume different input formats, so one custom input is valid
    # only when the benchmark targets a single workload.
    echo "--input requires --demo rgb or --demo yuv when benchmarking a single workload." >&2
    exit 1
fi

# Keep benchmark logs under build/ so generated artifacts remain ignored by Git.
log_dir="${ROOT_DIR}/build/logs"
mkdir -p "${log_dir}"
timestamp="$(date +%Y%m%d_%H%M%S)"
log_path="${log_dir}/benchmark_${demo}_${timestamp}.log"

# Benchmark is a thin wrapper around run.sh so path handling and rpp_server
# protection stay in one place.
cmd=("${SCRIPT_DIR}/run.sh" --demo "${demo}" --loop "${loop_count}")
[[ -n "${model}" ]] && cmd+=(--onnx "${model}")
[[ -n "${input}" ]] && cmd+=(--input "${input}")
if [[ "${demo}" == "yuv" || "${demo}" == "all" ]]; then
    cmd+=(--yuv-width "${yuv_width}" --yuv-height "${yuv_height}")
fi

{
    printf '%s' "${BLUE}"
    cat <<EOF
+------------------------------------------------------------------+
|             XDL YOLOv5 PERFORMANCE BENCHMARK                     |
+------------------------------------------------------------------+
  Test scope               : ${demo^^}
  Measured frames          : ${loop_count} per workload (+ 1 warmup)
  Performance log          : ${log_path}
+------------------------------------------------------------------+
EOF
    printf '%s' "${RESET}"
    echo "${CYAN}[BENCH   ]${RESET} Benchmark started; live progress follows below."
    "${cmd[@]}"
    echo "${GREEN}[ARCHIVE ]${RESET} Benchmark log saved: ${log_path}"
} 2>&1 | tee "${log_path}"
