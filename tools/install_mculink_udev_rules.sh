#!/usr/bin/env bash
set -euo pipefail

# Install udev rules so LinkServer can access NXP MCU-Link (CMSIS-DAP) without running as root.
#
# This requires sudo and a re-plug (or udev trigger).

RULE_FILE="/etc/udev/rules.d/99-nxp-linkserver.rules"

cat <<'EOF'
[udev] Installing udev rules for NXP MCU-Link (VID:PID 1fc9:0143).
[udev] This enables non-root access via group 'plugdev' and uaccess.
EOF

sudo tee "$RULE_FILE" >/dev/null <<'EOF'
# NXP MCU-Link (FRDM-MCXN947) CMSIS-DAP access for LinkServer (hidraw)
SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1fc9", ATTRS{idProduct}=="0143", MODE="0660", GROUP="plugdev", TAG+="uaccess"

# Fallback: allow USB node access too (some tools use libusb directly)
SUBSYSTEM=="usb", ATTR{idVendor}=="1fc9", ATTR{idProduct}=="0143", MODE="0660", GROUP="plugdev", TAG+="uaccess"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger

echo "[udev] Installed: $RULE_FILE"
echo "[udev] Unplug/replug the FRDM board if LinkServer still reports 'No probes detected'."

