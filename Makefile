# MikeyOS Makefile

CC = clang
LD = ld
XORRISO = xorriso
QEMU = qemu-system-x86_64
DD = dd
MKFS = mkfs.vfat
MMD = mmd
MCOPY = mcopy

CFLAGS = -std=c11 -O2 -Wall -Wextra \
         -ffreestanding \
         -fno-stack-protector \
         -fno-pic \
         -fno-asynchronous-unwind-tables \
         -m64 -mno-red-zone -mcmodel=kernel

LDFLAGS = -m elf_x86_64 -nostdlib -static \
          -z max-page-size=0x1000 \
          -T src/linker.ld

.PHONY: all run clean check-tools

all: mikeyos.iso

check-tools:
	@for tool in python3 clang ld xorriso qemu-system-x86_64 file; do \
		if ! command -v "$$tool" >/dev/null 2>&1; then \
			echo "ERROR: Missing tool: $$tool"; \
			exit 1; \
		fi; \
	done
	@for f in assets/picture.png src/kernel.c src/linker.ld; do \
		if [[ ! -f "$$f" ]]; then \
			echo "ERROR: Missing $$f"; \
			exit 1; \
		fi; \
	done

bootl:
	@echo "Setting up bootl..."
	rm -rf bootl
	git clone git@git.sr.ht:~pauli1505/bootl
	$(MAKE) -C bootl

kernel.o: src/kernel.c src/linker.ld | check-tools
	@echo "[3/7] Compiling x86-64 kernel..."
	$(CC) $(CFLAGS) -c src/kernel.c -o kernel.o

isodir/boot/kernel: kernel.o src/linker.ld
	@echo "[4/7] Linking kernel.elf..."
	mkdir -p isodir/boot isodir/EFI/BOOT
	$(LD) $(LDFLAGS) -o isodir/boot/kernel kernel.o
	file isodir/boot/kernel

mikeyos.iso: isodir/boot/kernel bootl
	@echo "[5/7] Preparing boot files..."
	cp bootl/BOOTX64.EFI isodir/EFI/BOOT/BOOTX64.EFI
	@echo "[6/7] Creating bootable ISO..."
	rm -f mikeyos.iso
	$(DD) if=/dev/zero of=isodir/boot/efi.img bs=1M count=4 status=none
	$(MKFS) isodir/boot/efi.img >/dev/null 2>&1
	$(MMD) -i isodir/boot/efi.img ::EFI || true
	$(MMD) -i isodir/boot/efi.img ::EFI/BOOT || true
	$(MCOPY) -i isodir/boot/efi.img bootl/BOOTX64.EFI ::EFI/BOOT/BOOTX64.EFI
	# Copy kernel into the efi.img FAT partition for bootl to find
	$(MMD) -i isodir/boot/efi.img ::boot || true
	$(MCOPY) -i isodir/boot/efi.img isodir/boot/kernel ::boot/kernel
	$(XORRISO) -as mkisofs \
		-R -r -J \
		-e boot/efi.img \
		-no-emul-boot \
		isodir -o mikeyos.iso
	@echo "ISO created: $$(pwd)/mikeyos.iso"
	
mikeyos_vars.fd:
	sudo cp /usr/share/ovmf/x64/OVMF_VARS.4m.fd mikeyos_vars.fd

run: mikeyos.iso mikeyos_vars.fd
	@echo "[7/7] Launching MikeyOS in QEMU..."
	$(QEMU) \
		-m 512M \
		-vga std \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/ovmf/x64/OVMF_CODE.4m.fd \
		-drive if=pflash,format=raw,file=mikeyos_vars.fd \
		-cdrom mikeyos.iso

clean:
	@echo "Cleaning up..."
	rm -rf kernel.o isodir mikeyos.iso bootl