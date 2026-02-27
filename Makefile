CROSS ?= x86_64-elf-
CC := $(CROSS)gcc
LD := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
NASM := nasm

BUILD_DIR := build
STAGE2_SECTORS := 8
KERNEL_SECTORS := 64

CFLAGS := -ffreestanding -fno-pic -fno-stack-protector -m64 -mcmodel=kernel -mno-red-zone -O2 -Wall -Wextra

.PHONY: all clean run

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
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o linker.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T linker.ld -o $@ $(BUILD_DIR)/kernel_entry.o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/os.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880
	dd if=$(BUILD_DIR)/boot.bin of=$@ conv=notrunc
	dd if=$(BUILD_DIR)/stage2.bin of=$@ bs=512 seek=1 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$@ bs=512 seek=$$((1 + $(STAGE2_SECTORS))) conv=notrunc

run: $(BUILD_DIR)/os.img
	qemu-system-x86_64 -drive format=raw,file=$(BUILD_DIR)/os.img -serial stdio

clean:
	rm -rf $(BUILD_DIR)
