# barecore
[![CI](https://github.com/mozaika228/barecore/actions/workflows/ci.yml/badge.svg)](https://github.com/mozaika228/barecore/actions/workflows/ci.yml)

`barecore` is a low-level x86_64 OS project with an ASM boot path and a C kernel.

## Architecture Overview

```text
BIOS boot flow
--------------
boot/boot.asm (16-bit, 512 bytes, @0x7C00)
  -> loads boot/stage2.asm from disk
  -> transfers control to stage2

boot/stage2.asm
  real mode:
    - A20 enable
    - kernel.bin read via INT 13h extensions
  protected mode:
    - GDT load
    - CR0.PE=1
  long mode transition:
    - 4-level paging setup (PML4/PDPT/PD, identity map)
    - CR4.PAE=1, EFER.LME=1, CR0.PG=1
    - far jump to 64-bit code
  -> jump to kernel entry @ 0x00100000

kernel/kernel_entry.asm
  - _start (64-bit)
  - IDT loader helper
  - context switch primitive
  - ISR stubs for:
      #DE, #PF, IRQ0(timer), IRQ1(keyboard), int 0x80(syscall)

kernel/kernel.c
  - console (serial + VGA / framebuffer fallback)
  - exceptions and IRQ handlers
  - scheduler and task model
  - syscalls and mini-shell
  - in-memory initrd-like file table
```

## GDT/IDT Layout

### GDT
- stage2: minimal 32/64-bit segments for mode transition
- kernel: full 64-bit GDT with:
  - kernel code/data
  - user code/data (ring3)
  - TSS descriptor (RSP0 stack)

### IDT (configured in `kernel/kernel.c`)
- `0`: divide-by-zero (`#DE`)
- `14`: page fault (`#PF`)
- `32`: timer IRQ0 (APIC or PIT fallback)
- `33`: PS/2 keyboard IRQ1
- `0x80`: syscall trap

## Memory Map

Key regions (BIOS path):
- `0x00007C00`: stage1 boot sector
- `0x00008000`: stage2 loader
- `0x00090000`: PML4
- `0x00091000`: PDPT
- `0x00092000`: PD (identity map)
- `0x00093000`: PD for LAPIC mapping
- `0xFED00000`: HPET MMIO (mapped)
- `0xFEE00000`: LAPIC MMIO (mapped)
- `0x00100000`: kernel image load address
- `0x00200000`: kernel bootstrap stack top
- `0x000B8000`: VGA text buffer (BIOS console fallback)

BIOS kernel loader constraint:
- fixed `KERNEL_SECTORS=128` in both:
  - `boot/stage2.asm`
  - `Makefile`

## Kernel Features

### Interrupts and Exceptions
- APIC timer (fallback to PIT), HPET+IOAPIC interrupt path when available
- PS/2 keyboard IRQ (`IRQ1`)
- divide-by-zero handler with explicit panic message
- page-fault handler with fault address (`CR2`) and error code
- register dump + simple backtrace on exceptions

### Scheduler and Processes
- context switch in ASM (`switch_context`)
- round-robin scheduler in C
- sleep/wake based on PIT ticks
- three tasks by default:
  - `task_a` (prints `A`)
  - `task_b` (prints `B`)
  - `task_shell` (interactive shell)
- simplified `fork` (spawns a new task from current entry)
- simplified `exec` (replaces current task entry)

### Syscalls
ABI (current, ring3 via `int 0x80`):
- `rax`: syscall number
- `rdi`, `rsi`: args 0..1
- return in `rax`

Implemented syscalls:
- `write`
- `exit`
- `getpid`
- `sleep`
- `yield`
- `fork` (simplified)
- `exec` (simplified)

### Console and Graphics
- serial output (`COM1`) for debugging/CI
- VGA text mode (`0xB8000`) on BIOS path
- framebuffer fallback on UEFI path (GOP metadata from `uefi/bootx64.c`)
- pixel primitive API in kernel (`fb_put_pixel`)

### Filesystem
- in-memory initrd-like table for `ls` / `cat`
- on-disk FAT12/16 reader via ATA PIO:
  - `lsdisk`
  - `catdisk <FILE>`

### Shell
Keyboard-driven shell commands:
- `help`
- `ls`
- `cat <file>`
- `echo <text>`
- `clear`
- `pid`
- `sleep <ms>`
- `lsdisk`
- `catdisk <file>`
- `fork`
- `exec <a|b|shell>`
- `userdemo` (ring3 transition demo)

## Build & Run

### Dependencies (Linux)

```bash
sudo apt-get update
sudo apt-get install -y \
  nasm make qemu-system-x86 gdb \
  gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu \
  gnu-efi ovmf
```

### Build BIOS image

```bash
make CROSS=x86_64-linux-gnu-
```

Output:
- `build/kernel.elf`
- `build/kernel.bin`
- `build/os.img`

### Run BIOS image

```bash
make CROSS=x86_64-linux-gnu- run
```

Or:

```bash
scripts/run-bios.sh
```

### Build/Run UEFI path

```bash
make CROSS=x86_64-linux-gnu- uefi
make CROSS=x86_64-linux-gnu- run-uefi OVMF=/usr/share/OVMF/OVMF_CODE.fd
```

## Debugging with gdb + QEMU

Start QEMU stopped with gdb-server on `:1234`:

```bash
make CROSS=x86_64-linux-gnu- run-gdb
```

In another terminal:

```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) b kmain
(gdb) c
```

Alternative launcher:

```bash
scripts/run-bios-gdb.sh
```

## CI and Automated Output Checks

`ci-smoke` verifies deterministic boot breadcrumbs through `kmain` entry:
- expected marker stream contains `SLPGJKXYM`

```bash
make CROSS=x86_64-linux-gnu- ci-smoke
```

`ci-runtime` validates runtime banner lines:

```bash
make CROSS=x86_64-linux-gnu- ci-runtime
```

## Roadmap

- HPET timer backend
- ring3 user scheduler (multi-user tasks)
- on-disk ext2 reader
- richer framebuffer text/graphics renderer
