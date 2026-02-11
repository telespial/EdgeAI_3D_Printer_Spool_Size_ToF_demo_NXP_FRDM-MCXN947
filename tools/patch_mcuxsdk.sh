#!/usr/bin/env bash
set -euo pipefail

# Patch known build-breakers in the upstream MCUX SDK checkout.
# This repo aims to be "clone + run scripts" reproducible without manual edits.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WS_DIR="${1:-${WS_DIR:-$ROOT_DIR/mcuxsdk_ws}}"

KISS_FASTFIR_C="$WS_DIR/mcuxsdk/middleware/eiq/tensorflow-lite/third_party/kissfft/tools/kiss_fastfir.c"
TOF_CMAKELISTS="$WS_DIR/mcuxsdk/examples/demo_apps/tof_demo/CMakeLists.txt"

echo "[patch] ws: $WS_DIR"

if [[ -f "$KISS_FASTFIR_C" ]]; then
  # GCC -Werror + -Wunused-variable can make this upstream "tools" source fail.
  # Make it deterministic and idempotent.
  if grep -q "__attribute__\\s*(([[:space:]]*unused[[:space:]]*))" "$KISS_FASTFIR_C"; then
    echo "[patch] ok: kiss_fastfir.c already patched"
  else
    if grep -qE '^static int verbose=0;[[:space:]]*$' "$KISS_FASTFIR_C"; then
      echo "[patch] fix: kiss_fastfir.c unused verbose"
      perl -0777 -pi -e 's|^static int verbose=0;\\s*$|/* Patched by tof_demo: MCUX SDK builds with -Werror; mark unused. */\\nstatic int verbose __attribute__((unused)) = 0;|m' "$KISS_FASTFIR_C"
    else
      echo "[patch] warn: kiss_fastfir.c verbose line not found; leaving unmodified"
    fi
  fi
else
  echo "[patch] skip: not found: $KISS_FASTFIR_C"
fi

# Ensure the example wrapper pulls in repo sources (when the workspace was created
# before adding new files).
if [[ -f "$TOF_CMAKELISTS" ]]; then
  echo "[patch] fix: normalize tof_demo CMakeLists sources"
  perl -0777 -pi -e 's|(mcux_add_source\\(\\s+BASE_PATH \\$\\{TOF_ROOT\\}\\s+SOURCES)(.*?)(\\)\\s+mcux_add_include)|$1\\n            src\\/tof_demo\\.c\\n            src\\/tmf8828_quick\\.c\\n            src\\/par_lcd_s035\\.c\\n            src\\/platform\\/display_hal\\.c\\n)\\n\\nmcux_add_include|ms' "$TOF_CMAKELISTS" || true
fi
