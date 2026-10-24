#!/bin/bash
# lemonadep-prep.sh: OnePlus 9 Pro (lemonadep / SM8350) Device Preparation & Build Helper
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$REPO_ROOT"

echo "=== OnePlus 9 Pro (lemonadep / SM8350) Port Helper ==="
echo ""

DTB_DIR="dtb-lemonadep"
mkdir -p "$DTB_DIR"
mkdir -p shim/out

# Step 1: Check for live device or existing dump
if [ -f "$DTB_DIR/lemonadep-live.dtb" ]; then
    echo "[✓] Found existing live DTB: $DTB_DIR/lemonadep-live.dtb"
elif command -v adb >/dev/null && adb get-state 2>/dev/null | grep -q "device"; then
    echo "[+] OnePlus 9 Pro detected via ADB. Pulling live device tree..."
    adb shell cat /sys/firmware/fdt > "$DTB_DIR/lemonadep-live.dtb"
    echo "[✓] Saved live DTB to $DTB_DIR/lemonadep-live.dtb"
else
    echo "[!] No live device connected and no $DTB_DIR/lemonadep-live.dtb found."
    echo "    To dump from device manually when connected:"
    echo "      adb shell cat /sys/firmware/fdt > $DTB_DIR/lemonadep-live.dtb"
    echo "    Proceeding with verified upstream SM8350 hardware definitions..."
fi

# Step 2: Decompile DTB if available
if [ -f "$DTB_DIR/lemonadep-live.dtb" ]; then
    echo "[+] Decompiling live DTB..."
    dtc -I dtb -O dts -o "$DTB_DIR/lemonadep-live.dts" "$DTB_DIR/lemonadep-live.dtb" 2>/dev/null || true
    if [ -f "$DTB_DIR/lemonadep-live.dts" ]; then
        echo "[✓] Live DTS generated: $DTB_DIR/lemonadep-live.dts"
        echo ""
        echo "--- Hardware Scan from Live Device Tree ---"
        echo "Framebuffer / Splash:"
        grep -E "cont_splash|simple-framebuffer|framebuffer@" "$DTB_DIR/lemonadep-live.dts" -A 4 || echo "  (not found via simple grep)"
        echo "Touch IC:"
        grep -E "goodix|synaptics|focaltech|touchscreen" "$DTB_DIR/lemonadep-live.dts" -A 4 || echo "  (not found via simple grep)"
        echo "Keys:"
        grep -E "gpio-keys|pm8350|pon@" "$DTB_DIR/lemonadep-live.dts" -A 4 || echo "  (not found via simple grep)"
        echo "-------------------------------------------"
    fi
fi

# Step 3: Generate Apple Device Tree for lemonadep
echo ""
echo "[+] Generating Apple Device Tree for OnePlus 9 Pro (lemonadep)..."
python3 tools/mkadt.py shim/out/lemonadep.adt --target lemonadep
echo "[✓] Generated shim/out/lemonadep.adt"

# Step 4: Build Shim Bootloader for lemonadep
echo ""
echo "[+] Building baremetal shim bootloader for OnePlus 9 Pro..."
TARGET=lemonadep ./shim/build.sh
echo "[✓] Generated shim/out/shim-boot-lemonadep.img"

echo ""
echo "=== Summary & Next Steps ==="
echo "1. Device ADT:  shim/out/lemonadep.adt"
echo "2. Boot Image:  shim/out/shim-boot-lemonadep.img"
echo ""
echo "To test the early boot diagnostic shim on hardware:"
echo "  adb reboot bootloader"
echo "  fastboot boot shim/out/shim-boot-lemonadep.img"
echo ""
