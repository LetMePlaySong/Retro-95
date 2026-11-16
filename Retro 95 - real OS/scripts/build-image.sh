#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
ISO_ROOT="$BUILD/iso-root"
LIMINE_VERSION="12.10.0"
LIMINE_ARCHIVE="$BUILD/limine-binary.zip"
LIMINE_DIR="$BUILD/limine/limine-binary"
ISO="$BUILD/retro95.iso"
CLANG="${CLANG:-clang}"

for tool in "$CLANG" make curl unzip xorriso; do
    command -v "$tool" >/dev/null 2>&1 || { echo "Missing required tool: $tool" >&2; exit 1; }
done
mkdir -p "$BUILD" "$LIMINE_DIR"

if [[ ! -x "$LIMINE_DIR/limine" || ! -f "$LIMINE_DIR/limine-bios.sys" ]]; then
    echo "Downloading official Limine v${LIMINE_VERSION} binary release..."
    curl --fail --location --silent --show-error \
        "https://github.com/Limine-Bootloader/Limine/releases/download/v${LIMINE_VERSION}/limine-binary.zip" \
        --output "$LIMINE_ARCHIVE"
    rm -rf "$BUILD/limine"
    mkdir -p "$BUILD/limine"
    unzip -q "$LIMINE_ARCHIVE" -d "$BUILD/limine"
    if [[ ! -x "$LIMINE_DIR/limine" ]]; then
        make -C "$LIMINE_DIR"
    fi
fi

"$CLANG" --target=x86_64-unknown-none-elf -std=c11 -O2 -Wall -Wextra -Werror \
    -ffreestanding -fno-stack-protector \
   -fno-pic -fno-pie -fno-builtin -m64 -mcmodel=kernel -mno-red-zone \
   -mno-mmx -mno-sse -mno-sse2 -I"$ROOT/kernel" -c "$ROOT/kernel/kernel.c" \
   -o "$BUILD/kernel.o"
"$CLANG" --target=x86_64-unknown-none-elf -fuse-ld=lld -nostdlib -static \
    -m64 -mcmodel=kernel -Wl,-T,"$ROOT/linker.ld" \
   -Wl,--build-id=none -Wl,-z,max-page-size=0x1000 "$BUILD/kernel.o" \
   -o "$BUILD/kernel.elf"

rm -rf "$ISO_ROOT"
mkdir -p "$ISO_ROOT/EFI/BOOT" "$ISO_ROOT/boot/retro95" "$ISO_ROOT/boot/limine"
cp "$BUILD/kernel.elf" "$ISO_ROOT/boot/retro95/kernel.elf"
cp "$ROOT/limine.conf" "$ISO_ROOT/limine.conf"
cp "$LIMINE_DIR/limine-bios.sys" "$ISO_ROOT/limine-bios.sys"
cp "$LIMINE_DIR/limine-bios-cd.bin" "$ISO_ROOT/limine-bios-cd.bin"
cp "$LIMINE_DIR/limine-uefi-cd.bin" "$ISO_ROOT/limine-uefi-cd.bin"

# Official Limine binary releases include the x86-64 EFI application.
if [[ ! -f "$LIMINE_DIR/BOOTX64.EFI" ]]; then
    echo "BOOTX64.EFI not found in the Limine release archive." >&2
    exit 1
fi
cp "$LIMINE_DIR/BOOTX64.EFI" "$ISO_ROOT/EFI/BOOT/BOOTX64.EFI"

xorriso -as mkisofs -R -r -J -b limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
    -apm-block-size 2048 --efi-boot limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    "$ISO_ROOT" -o "$ISO"
"$LIMINE_DIR/limine" bios-install "$ISO"
echo "Built hybrid BIOS/UEFI ISO: $ISO"
echo "No physical disk or EFI boot entry was modified."
