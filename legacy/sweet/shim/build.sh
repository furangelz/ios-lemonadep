#!/bin/bash
# Build the shim and wrap it in a boot.img v2 that mirrors the stock PixelOS header.
set -euo pipefail
cd "$(dirname "$0")"
CC=aarch64-linux-gnu-gcc
OBJCOPY=aarch64-linux-gnu-objcopy
TARGET="${TARGET:-sweet}"
CFLAGS="-O2 -ffreestanding -nostdlib -fno-pic -mgeneral-regs-only -mstrict-align -fno-stack-protector -Wall -Wextra"
HOST_CFLAGS=""

if [ "$TARGET" = "lemonadep" ]; then
	CFLAGS="$CFLAGS -DTARGET_LEMONADEP"
	HOST_CFLAGS="-DTARGET_LEMONADEP"
	DTB_PATH="../dtb-lemonadep/boot_unpacked/dtb"
	OUT_IMG="out/shim-boot-lemonadep.img"
else
	DTB_PATH="../dtb/boot_unpacked/dtb"
	OUT_IMG="out/shim-boot.img"
fi

mkdir -p out
for f in start.S shim.c console.c mmu.c fdt.c loader.c ufs.c; do
	$CC $CFLAGS -c $f -o out/${f%.*}.o
done
# The image runs wherever ABL puts it with no relocation pass: only the
# header's image_size (start.o, linked at 0) may be an absolute address.
abs=$(aarch64-linux-gnu-readelf -r out/shim.o out/console.o out/mmu.o out/fdt.o out/loader.o out/ufs.o | grep -c ABS64 || true)
[ "$abs" = 0 ] || { echo "error: $abs absolute relocations in C objects" >&2; exit 1; }
$CC $CFLAGS -static -T shim.ld -Wl,--build-id=none -Wl,--no-warn-rwx-segments -Wl,-N \
	out/start.o out/shim.o out/console.o out/mmu.o out/fdt.o out/loader.o out/ufs.o -o out/shim.elf
$OBJCOPY -O binary out/shim.elf out/Image
# ABL copies the file; pad to image_size so .bss/stacks exist as zeroed bytes.
size=$(aarch64-linux-gnu-nm out/shim.elf | awk '$3=="_end"{print strtonum("0x"$1)}')
truncate -s "$size" out/Image
# Ramdisk: an xnu payload (tools/mkpayload.py) if given as $1, else an empty
# newc cpio (ABL dislikes a zero-size ramdisk on some builds) -> diagnostics.
if [ -n "${1:-}" ]; then
	cp "$1" out/ramdisk.bin
else
	(cd out && rm -rf rd && mkdir rd && cd rd && find . | cpio -o -H newc --quiet | gzip -9 > ../ramdisk.bin)
fi

if [ "$TARGET" = "lemonadep" ]; then
	# SM8350 (Snapdragon 888 / Android 11+) uses boot image header v3 (GKI).
	# DTB is loaded by ABL from the onboard vendor_boot partition.
	mkbootimg --header_version 3 --kernel out/Image --ramdisk out/ramdisk.bin \
		--os_version 17.0.0 --os_patch_level 2035-06 \
		--cmdline "console=null" -o "$OUT_IMG"
else
	if [ ! -f "$DTB_PATH" ]; then
		echo "notice: $DTB_PATH not found; generating fallback stub DTB for packaging" >&2
		echo '/dts-v1/; / { };' | dtc -I dts -O dtb -o out/fallback.dtb 2>/dev/null
		DTB_PATH="out/fallback.dtb"
	fi

	mkbootimg --header_version 2 --kernel out/Image --ramdisk out/ramdisk.bin \
		--dtb "$DTB_PATH" --base 0 --kernel_offset 0x8000 --ramdisk_offset 0x1000000 \
		--tags_offset 0x100 --dtb_offset 0x1f00000 --pagesize 4096 \
		--os_version 17.0.0 --os_patch_level 2035-06 \
		--cmdline "console=null" -o "$OUT_IMG"
fi
# Host render of the console layout.
gcc -O1 -Wall $HOST_CFLAGS -o out/host_test console.c host_test.c && ./out/host_test
ls -l out/Image "$OUT_IMG"
