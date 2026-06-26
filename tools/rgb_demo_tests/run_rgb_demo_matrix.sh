#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BIN="${BUILD_DIR}/bin/yolov5_rgb_image_demo"
MODEL="${ROOT_DIR}/onnx/yolov5s.onnx"
WORK_DIR="${BUILD_DIR}/rgb_demo_tests"
INPUT_DIR="${WORK_DIR}/inputs"
OUTPUT_DIR="${WORK_DIR}/outputs"
LOG_DIR="${WORK_DIR}/logs"
LOOP_COUNT="${1:-20}"

mkdir -p "${INPUT_DIR}" "${OUTPUT_DIR}" "${LOG_DIR}"

if [[ ! -x "${BIN}" ]]; then
    echo "missing demo binary: ${BIN}" >&2
    echo "run: cmake --build build -j4" >&2
    exit 1
fi

if [[ ! -f "${MODEL}" ]]; then
    echo "missing model: ${MODEL}" >&2
    exit 1
fi

python3 - "${ROOT_DIR}" "${INPUT_DIR}" <<'PY'
import sys
from pathlib import Path

root = Path(sys.argv[1])
out_dir = Path(sys.argv[2])

try:
    import cv2
except Exception as exc:
    raise SystemExit(f"OpenCV python module is required to generate test images: {exc}")

sources = [
    root / "assets" / "test_1.png",
    root / "assets" / "ultralytics_bus.jpg",
    root / "assets" / "ultralytics_zidane.jpg",
]

sizes = [
    ("square_640x640", 640, 640),
    ("wide_1280x720", 1280, 720),
    ("wide_1920x1080", 1920, 1080),
    ("portrait_720x1280", 720, 1280),
]

for src in sources:
    if not src.exists():
        print(f"skip missing source: {src}")
        continue
    image = cv2.imread(str(src), cv2.IMREAD_COLOR)
    if image is None:
        print(f"skip unreadable source: {src}")
        continue
    stem = src.stem
    for name, width, height in sizes:
        resized = cv2.resize(image, (width, height), interpolation=cv2.INTER_LINEAR)
        output = out_dir / f"{stem}_{name}.jpg"
        if not cv2.imwrite(str(output), resized):
            raise SystemExit(f"failed to write {output}")
        print(output)
PY

run_one() {
    local image="$1"
    local stem
    stem="$(basename "${image}")"
    stem="${stem%.*}"
    local output="${OUTPUT_DIR}/${stem}_det.jpg"
    local log="${LOG_DIR}/${stem}.log"

    echo "run: ${stem}"
    "${BIN}" \
        -o "${MODEL}" \
        -i "${image}" \
        --output "${output}" \
        -l "${LOOP_COUNT}" | tee "${log}"
}

while IFS= read -r image; do
    run_one "${image}"
done < <(find "${INPUT_DIR}" -maxdepth 1 -type f -name '*.jpg' | sort)

SUMMARY="${WORK_DIR}/summary.tsv"
{
    printf "case\tinput_h2d_ms\tpreprocess_ms\tinference_ms\tpostprocess_ms\toutput_d2h_ms\tend_to_end_ms\tfps\n"
    for log in "${LOG_DIR}"/*.log; do
        case_name="$(basename "${log}" .log)"
        input_h2d="$(awk '/Input H2D:/ {print $3; exit}' "${log}")"
        preprocess="$(awk '/Preprocess:/ {print $2; exit}' "${log}")"
        inference="$(awk '/Inference:/ {print $2; exit}' "${log}")"
        postprocess="$(awk '/Postprocess:/ {print $2; exit}' "${log}")"
        output_d2h="$(awk '/Output D2H:/ {print $3; exit}' "${log}")"
        end_to_end="$(awk '/All time end to end:/ {print $6; exit}' "${log}")"
        fps="$(awk '/FPS:/ {print $2; exit}' "${log}")"
        printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
            "${case_name}" \
            "${input_h2d}" \
            "${preprocess}" \
            "${inference}" \
            "${postprocess}" \
            "${output_d2h}" \
            "${end_to_end}" \
            "${fps}"
    done
} > "${SUMMARY}"

echo "test outputs: ${WORK_DIR}"
echo "summary: ${SUMMARY}"
