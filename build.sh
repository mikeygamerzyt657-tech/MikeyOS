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
for f in assets/picture.png src/kernel.c src/linker.ld; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: Missing $f"
        exit 1
    fi
done

echo "setting up bootl"

rm -rf bootl/
git clone git@git.sr.ht:~pauli1505/bootl

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
    -o isodir/boot/kernel kernel.o

file isodir/boot/kernel

echo "[5/7] Preparing boot files..."

cp bootl/BOOTX64.EFI isodir/EFI/BOOT/BOOTX64.EFI

echo "[6/7] Creating bootable ISO..."

echo "deleting stale/obj"

rm -f mikeyos.iso

dd if=/dev/zero of=isodir/boot/efi.img bs=1M count=4

mkfs.vfat isodir/boot/efi.img

mmd -i isodir/boot/efi.img ::EFI
mmd -i isodir/boot/efi.img ::EFI/BOOT
mcopy -i isodir/boot/efi.img bootl/BOOTX64.EFI ::EFI/BOOT/BOOTX64.EFI

xorriso -as mkisofs \
    -R -r -J \
    -e boot/efi.img \
    -no-emul-boot \
    isodir -o mikeyos.iso

echo "ISO created: $PWD/mikeyos.iso"

# Step 7: Launch QEMU
echo
echo "[7/7] Launching MikeyOS in QEMU..."

qemu-system-x86_64 \
    -m 512M \
    -vga std \
    -cdrom mikeyos.iso
