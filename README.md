# barecore
[![CI](https://github.com/mozaika228/barecore/actions/workflows/ci.yml/badge.svg)](https://github.com/mozaika228/barecore/actions/workflows/ci.yml)

> Minimal x86_64 operating system from scratch (ASM bootloader -> C kernel -> userspace)

`barecore` is a custom OS where we control the full path: bootloader, mode transitions, paging, interrupts, scheduler, syscalls, filesystem access, and shell.

## Why this project matters

- It shows how a computer boots and runs software without relying on an existing kernel.
- It is a real low-level system, not a toy parser or emulator-only script.
- It connects theory (GDT/IDT/paging/syscalls) to observable runtime behavior.
- It is built for learning, debugging, and experimentation on real x86_64 concepts.

## Project level

`barecore` is a serious mini-OS codebase.  
If you are a student, this is the level of: "I understand how OS fundamentals actually work on hardware."

## What you'll see

After boot:
- kernel initializes
- timer and interrupts are configured
- scheduler starts tasks
- interactive shell appears

Example runtime output:

```text
barecore kernel (production path)
long mode: OK
scheduler: round-robin
drivers: HPET+IOAPIC timer + PS/2 keyboard
syscalls: write exit getpid sleep yield

[bcore shell] type 'help'
$ 
```

## Architecture at a glance

```mermaid
flowchart LR
    A[BIOS/UEFI] --> B[boot/boot.asm]
    B --> C[boot/stage2.asm]
    C --> D[16-bit real mode]
    D --> E[32-bit protected mode]
    E --> F[64-bit long mode]
    F --> G[kernel_entry.asm]
    G --> H[kernel.c]
    H --> I[scheduler + syscalls + shell]
```

Mode transitions:

```text
real mode -> protected mode -> long mode -> kernel runtime -> ring3 demos/userspace
```

## Quick start

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

### Run BIOS image

```bash
make CROSS=x86_64-linux-gnu- run
```

Alternative launcher:

```bash
scripts/run-bios.sh
```

### Build/Run UEFI path

```bash
make CROSS=x86_64-linux-gnu- uefi
make CROSS=x86_64-linux-gnu- run-uefi OVMF=/usr/share/OVMF/OVMF_CODE.fd
```

## Core features

### Interrupts and reliability

- ACPI RSDP + MADT/HPET discovery (LAPIC/IOAPIC/HPET addresses)
- APIC timer with PIT fallback, HPET+IOAPIC path when available
- PS/2 keyboard IRQ handling
- unified panic path for exceptions with register/frame dump + backtrace
- page fault diagnostics with decoded error bits

### Scheduler and processes

- context switch primitive in ASM (`switch_context`)
- round-robin scheduler in C with timer preemption
- simplified process model: `fork`, `exec`, `wait`
- ring3 transition demos and preemptive ring3 task switching

### Memory

- kernel heap on mapped 4K pages
- `kmalloc` / `kfree` (first-fit, split, coalesce)
- shell diagnostics: `memstat`, `memtest`

### Syscalls

- ABI: `syscall/sysret` with `int 0x80` fallback
- implemented: `write`, `exit`, `getpid`, `sleep`, `yield`, `fork`, `exec`, `wait`

### Filesystem and loading

- FAT12/16 path-based read via ATA PIO
- ELF loader in kernel:
  - `runelf <path>` (kernel-space entry jump)
  - `runuser <path>` (validated user-range ELF -> ring3)

### Drivers and tooling

- PCI enumeration (`pciscan`)
- serial + VGA text mode + framebuffer fallback
- gdb helper commands and CI smoke/runtime checks

## Shell commands

- `help`
- `ls`, `cat <file>`, `echo <text>`, `clear`
- `pid`, `sleep <ms>`, `wait [pid]`
- `memstat`, `memtest`
- `lsdisk`, `lsdisk <path>`, `catdisk <path>`
- `runelf <path>`, `runuser <path>`
- `pciscan`
- `fork`, `exec <a|b|shell>`
- `userdemo`, `userpreempt`

## Debugging with gdb + QEMU

Start QEMU paused with gdbstub:

```bash
make CROSS=x86_64-linux-gnu- run-gdb
```

Attach with prepared script:

```bash
make CROSS=x86_64-linux-gnu- gdb-attach
```

Manual attach:

```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) b kmain
(gdb) c
```

## CI checks

Smoke boot marker check:

```bash
make CROSS=x86_64-linux-gnu- ci-smoke
```

Runtime banner checks:

```bash
make CROSS=x86_64-linux-gnu- ci-runtime
```

## Deep technical details

### GDT/IDT layout

GDT:
- stage2: minimal 32/64-bit segments for mode transition
- kernel: kernel code/data, user code/data, TSS descriptor

IDT vectors (configured in `kernel/kernel.c`):
- `0`: divide-by-zero (`#DE`)
- `14`: page fault (`#PF`)
- `32`: timer IRQ0
- `33`: PS/2 keyboard IRQ1
- `0x80`: syscall trap

### Memory map (BIOS path)

- `0x00007C00`: stage1 boot sector
- `0x00008000`: stage2 loader
- `0x00090000`: PML4
- `0x00091000`: PDPT
- `0x00092000`: PD (identity map)
- `0x00093000`: PD for LAPIC/HPET mapping
- `0xFED00000`: HPET MMIO (mapped)
- `0xFEE00000`: LAPIC MMIO (mapped)
- `0x00100000`: kernel image load address
- `0x00200000`: bootstrap stack top
- `0x000B8000`: VGA text buffer fallback

Kernel loader constraint:
- `KERNEL_SECTORS=128` in both:
  - `boot/stage2.asm`
  - `Makefile`

### Syscall ABI (current)

- `rax`: syscall number
- `rdi`, `rsi`: args 0..1
- return value in `rax`

## Release tooling

- current version: `cat VERSION`
- release gates: `make CROSS=x86_64-linux-gnu- release-check`
- build release artifacts + checksums: `make CROSS=x86_64-linux-gnu- dist`
- release notes: `CHANGELOG.md`

## Roadmap

- per-process address-space isolation
- richer ring3 userspace model
- on-disk ext2 reader
- improved framebuffer text/graphics renderer
