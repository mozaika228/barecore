CROSS ?= x86_64-elf-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
NASM := nasm
EFI_CC ?= gcc
EFI_OBJCOPY ?= objcopy

BUILD_DIR := build
STAGE2_SECTORS := 8
KERNEL_SECTORS := 64

CFLAGS := -ffreestanding -fno-pic -fno-stack-protector -m64 -mcmodel=kernel -mno-red-zone -O2 -Wall -Wextra
EFI_CFLAGS ?= -fpic -fshort-wchar -mno-red-zone -Wall -Wextra -I/usr/include/efi -I/usr/include/efi/x86_64
EFI_LDS ?= /usr/lib/elf_x86_64_efi.lds
EFI_CRT ?= /usr/lib/crt0-efi-x86_64.o
EFI_LIBDIR ?= /usr/lib
EFI_LDFLAGS ?= -nostdlib -znocombreloc -T $(EFI_LDS) -shared -Bsymbolic
EFI_LIBS ?= -L$(EFI_LIBDIR) -lefi -lgnuefi
OVMF ?= OVMF.fd

.PHONY: all clean run uefi run-uefi ci-smoke verify-kernel-size

all: $(BUILD_DIR)/os.img

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.bin: boot/boot.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(BUILD_DIR)/stage2.bin: boot/stage2.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(BUILD_DIR)/kernel_entry.o: kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o linker.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T linker.ld -o $@ $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/bootx64.o: uefi/bootx64.c | $(BUILD_DIR)
	$(EFI_CC) $(EFI_CFLAGS) -Iinclude -c $< -o $@

$(BUILD_DIR)/bootx64.so: $(BUILD_DIR)/bootx64.o
	$(EFI_CC) $(EFI_LDFLAGS) $(EFI_CRT) $< -o $@ $(EFI_LIBS)

$(BUILD_DIR)/BOOTX64.EFI: $(BUILD_DIR)/bootx64.so
	$(EFI_OBJCOPY) \
		-j .text -j .sdata -j .data -j .dynamic -j .dynsym \
		-j .rel -j .rela -j .reloc \
		--target=efi-app-x86_64 $< $@

$(BUILD_DIR)/esp/EFI/BOOT/BOOTX64.EFI: $(BUILD_DIR)/BOOTX64.EFI
	mkdir -p $(BUILD_DIR)/esp/EFI/BOOT
	cp $< $@

$(BUILD_DIR)/esp/kernel.bin: $(BUILD_DIR)/kernel.bin
	mkdir -p $(BUILD_DIR)/esp
	cp $< $@

uefi: $(BUILD_DIR)/esp/EFI/BOOT/BOOTX64.EFI $(BUILD_DIR)/esp/kernel.bin

verify-kernel-size: $(BUILD_DIR)/kernel.bin
	@size=$$(wc -c < $(BUILD_DIR)/kernel.bin); \
	max=$$(( $(KERNEL_SECTORS) * 512 )); \
	if [ $$size -gt $$max ]; then \
		echo "kernel.bin too large: $$size bytes (max $$max). Increase KERNEL_SECTORS in boot/stage2.asm and Makefile."; \
		exit 1; \
	fi

$(BUILD_DIR)/os.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin verify-kernel-size
	dd if=/dev/zero of=$@ bs=512 count=2880
	dd if=$(BUILD_DIR)/boot.bin of=$@ conv=notrunc
	dd if=$(BUILD_DIR)/stage2.bin of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$@ bs=512 seek=$$((1 + $(STAGE2_SECTORS))) conv=notrunc

run: $(BUILD_DIR)/os.img
	qemu-system-x86_64 -drive format=raw,file=$(BUILD_DIR)/os.img -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04

run-uefi: uefi
	qemu-system-x86_64 -bios $(OVMF) -drive format=raw,file=fat:rw:$(BUILD_DIR)/esp -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04

ci-smoke: $(BUILD_DIR)/os.img
	timeout 30s qemu-system-x86_64 -nographic -monitor none -no-reboot -no-shutdown \
		-drive format=raw,file=$(BUILD_DIR)/os.img \
		-serial stdio \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		2>&1 | tee $(BUILD_DIR)/qemu.log
	grep -q "Kernel: long mode OK" $(BUILD_DIR)/qemu.log
	grep -q "Scheduler: round-robin start" $(BUILD_DIR)/qemu.log
	grep -q "All tasks finished" $(BUILD_DIR)/qemu.log

clean:
	rm -rf $(BUILD_DIR)
