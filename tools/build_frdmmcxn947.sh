#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WS_DIR="${WS_DIR:-$ROOT_DIR/mcuxsdk_ws}"
BUILD_DIR="${BUILD_DIR:-$WS_DIR/build}"
BUILD_TYPE="${1:-debug}"

# shellcheck disable=SC1090
source "$ROOT_DIR/tools/mcux_env.sh"

if [[ ! -d "$WS_DIR/.west" ]]; then
  echo "MCUX workspace missing at: $WS_DIR" >&2
  echo "Run: ./tools/setup_mcuxsdk_ws.sh" >&2
  exit 1
fi

"$ROOT_DIR/tools/patch_mcuxsdk.sh" "$WS_DIR"

(
  cd "$WS_DIR"
  # NOTE: `-p always` is avoided due to a known pristine invocation failure in
  # some MCUX west environments (BINARY_DIR/SOURCE_DIR passed as None).
  # Use a unique `BUILD_DIR` for a clean build when needed.
  west build -d "$BUILD_DIR" mcuxsdk/examples/demo_apps/tof_demo \
    --toolchain armgcc \
    --config "$BUILD_TYPE" \
    -b frdmmcxn947 \
    -Dcore_id=cm33_core0
)

echo "Built: $BUILD_DIR/tof_demo_cm33_core0.bin"
