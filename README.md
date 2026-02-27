# barecore
[![CI](https://github.com/mozaika228/barecore/actions/workflows/ci.yml/badge.svg)](https://github.com/mozaika228/barecore/actions/workflows/ci.yml)

Educational x86_64 OS skeleton:
- BIOS bootloader (stage1 512B + stage2)
- UEFI loader (`BOOTX64.EFI`)
- Real mode -> protected mode -> long mode
- GDT/IDT setup and timer interrupts (PIC/PIT)
- VGA text mode output (`0xB8000`) with framebuffer fallback for UEFI
- Context switch (save/restore registers)
- Round-robin scheduler
- Two test processes (`A` and `B`)
- Minimal syscalls: `write`, `exit`, `yield`

Current BIOS loader limit:
- Stage2 loads a fixed `KERNEL_SECTORS=256` (must stay in sync with `boot/stage2.asm` and `Makefile`)

## Mode transition scheme

```text
BIOS path:
  16-bit real mode (boot.asm @ 0x7C00)
    -> load stage2
    -> enable A20
    -> load GDT
    -> CR0.PE=1
  32-bit protected mode (stage2.asm)
    -> build PML4/PDPT/PD
    -> CR4.PAE=1
    -> EFER.LME=1
    -> CR0.PG=1
  64-bit long mode
    -> jump to kernel entry (_start)

UEFI path:
  UEFI firmware (already 64-bit)
    -> BOOTX64.EFI loads kernel.bin at 0x00100000
    -> collects GOP framebuffer info
    -> ExitBootServices
    -> jump to kernel entry (_start)
```

## Project layout

- `boot/boot.asm` - BIOS stage1 (512 bytes)
- `boot/stage2.asm` - BIOS stage2 + mode transitions
- `kernel/kernel_entry.asm` - kernel entry, IDT load, ISR stubs, context switch asm
- `kernel/kernel.c` - IDT/PIC/PIT, console output, scheduler, syscall dispatcher
- `uefi/bootx64.c` - UEFI loader + GOP framebuffer handoff
- `include/boot_info.h` - shared boot info contract (UEFI -> kernel)
- `linker.ld` - kernel linker script
- `Makefile` - BIOS/UEFI build and run targets

## Quick start (Linux, detailed)

### 1) Install dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
  nasm make qemu-system-x86 \
  gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu \
  gnu-efi ovmf
```

### 2) Build BIOS image (via Makefile)

```bash
make CROSS=x86_64-linux-gnu-
```

Result: `build/os.img`

### 3) Run BIOS image in QEMU

```bash
qemu-system-x86_64 \
  -drive format=raw,file=build/os.img \
  -serial stdio \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04
```

### 4) Build UEFI loader + ESP tree

```bash
make CROSS=x86_64-linux-gnu- uefi OVMF=/usr/share/OVMF/OVMF_CODE.fd
```

Result:
- `build/esp/EFI/BOOT/BOOTX64.EFI`
- `build/esp/kernel.bin`

### 5) Run UEFI in QEMU

```bash
qemu-system-x86_64 \
  -bios /usr/share/OVMF/OVMF_CODE.fd \
  -drive format=raw,file=fat:rw:build/esp \
  -serial stdio \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04
```

## Quick start (manual NASM + linker commands)

```bash
mkdir -p build
nasm -f bin boot/boot.asm -o build/boot.bin
nasm -f bin boot/stage2.asm -o build/stage2.bin
nasm -f elf64 kernel/kernel_entry.asm -o build/kernel_entry.o
x86_64-linux-gnu-gcc -ffreestanding -fno-pic -fno-stack-protector -m64 -mcmodel=kernel -mno-red-zone -O2 -Wall -Wextra -Iinclude -c kernel/kernel.c -o build/kernel.o
x86_64-linux-gnu-ld -nostdlib -z max-page-size=0x1000 -T linker.ld -o build/kernel.elf build/kernel_entry.o build/kernel.o
x86_64-linux-gnu-objcopy -O binary build/kernel.elf build/kernel.bin
dd if=/dev/zero of=build/os.img bs=512 count=2880
dd if=build/boot.bin of=build/os.img conv=notrunc
dd if=build/stage2.bin of=build/os.img bs=512 seek=1 conv=notrunc
dd if=build/kernel.bin of=build/os.img bs=512 seek=9 conv=notrunc
```

## What is implemented

- GDT setup: in BIOS stage2 before protected/long mode jumps
- IDT setup: in `kernel.c` with timer and syscall vectors
- 64-bit transition: CR0/CR4/EFER + 4-level paging in stage2
- Output:
  - BIOS: VGA text mode (`0xB8000`)
  - UEFI: framebuffer fallback via GOP handoff
  - Serial: COM1 mirror for debugging/CI
- Context switch: `switch_context` saves/restores callee-saved regs + stack pointer
- Scheduler: cooperative round-robin
- Test processes:
  - `task_a()` prints `A`
  - `task_b()` prints `B`

## CI

GitHub Actions workflow `.github/workflows/ci.yml`:
- builds BIOS image with NASM + cross gcc
- runs QEMU smoke test
- validates boot log markers from serial output
