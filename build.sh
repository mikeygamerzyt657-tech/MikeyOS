#!/usr/bin/env bash
set -Eeuo pipefail

cd "$(dirname "$0")"

echo "========================================"
echo "       MikeyOS Automatic Builder"
echo "========================================"

# Check required tools
for tool in python3 clang ld xorriso qemu-system-x86_64 file; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: Missing tool: $tool"
        exit 1
    fi
done

# Check required source files
for f in assets/picture.png src/kernel.c src/linker.ld limine.conf; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: Missing $f"
        exit 1
    fi
done

# Check Limine files
for f in \
    limine/limine \
    limine/limine-bios.sys \
    limine/limine-bios-cd.bin \
    limine/limine-uefi-cd.bin \
    limine/BOOTX64.EFI; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: Missing $f"
        exit 1
    fi
done

if [[ ! -x limine/limine ]]; then
    echo "ERROR: limine/limine is not executable."
    echo "Run: chmod +x limine/limine"
    exit 1
fi

# Step 3: Compile the 64-bit kernel
echo
echo "[3/7] Compiling x86-64 kernel..."

clang -std=c11 -O2 -Wall -Wextra \
    -ffreestanding \
    -fno-stack-protector \
    -fno-pic \
    -fno-asynchronous-unwind-tables \
    -m64 -mno-red-zone -mcmodel=kernel \
    -c src/kernel.c -o kernel.o

# Step 4: Link the kernel and embedded image
echo
echo "[4/7] Linking kernel.elf..."

mkdir -p isodir/boot isodir/EFI/BOOT

ld -m elf_x86_64 -nostdlib -static \
    -z max-page-size=0x1000 \
    -T src/linker.ld \
    -o isodir/boot/kernel.elf kernel.o

file isodir/boot/kernel.elf

# Step 5: Prepare Limine boot files
echo
echo "[5/7] Preparing boot files..."

cp limine/limine-bios.sys isodir/
cp limine/limine-bios-cd.bin isodir/
cp limine/limine-uefi-cd.bin isodir/
cp limine/BOOTX64.EFI isodir/EFI/BOOT/BOOTX64.EFI
cp limine.conf isodir/limine.conf

# Step 6: Build and install the bootable ISO
echo
echo "[6/7] Creating bootable ISO..."

rm -f mikeyos.iso

xorriso -as mkisofs \
    -R -r -J \
    -b limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image \
    --protective-msdos-label \
    isodir -o mikeyos.iso

echo "Installing Limine BIOS boot code..."
./limine/limine bios-install mikeyos.iso

echo "ISO created: $PWD/mikeyos.iso"

# Step 7: Launch QEMU
echo
echo "[7/7] Launching MikeyOS in QEMU..."

qemu-system-x86_64 \
    -m 512M \
    -vga std \
    -cdrom mikeyos.iso
