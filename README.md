# Bootloader + Minimal Kernel (x86_64, BIOS path)

This project is a minimal educational OS skeleton:

- BIOS boot sector (stage1, 512 bytes) loads stage2 from disk.
- Stage2 enters protected mode, enables paging, switches to long mode.
- 64-bit kernel initializes IDT and PIC timer IRQ.
- VGA text output works without BIOS.
- Minimal process model with cooperative round-robin context switch.
- Minimal syscall stubs: `write`, `exit`.

## Layout

- `boot/boot.asm` - 512-byte stage1 loader.
- `boot/stage2.asm` - mode switches and kernel handoff.
- `kernel/kernel_entry.asm` - 64-bit kernel entry and context switch asm.
- `kernel/kernel.c` - kernel init, IDT/PIC/timer, scheduler, syscalls.
- `linker.ld` - kernel link script.
- `Makefile` - build image and run in QEMU.

## Build prerequisites

- `nasm`
- `x86_64-elf-gcc` and `x86_64-elf-ld` (or compatible cross toolchain)
- `qemu-system-x86_64`

## Build

```sh
make
```

This creates `build/os.img`.

PowerShell alternative (Windows):

```powershell
./build.ps1
```

## Run

```sh
make run
```

Or with PowerShell after build:

```powershell
qemu-system-x86_64 -drive format=raw,file=build/os.img -serial stdio
```

## Notes

- Stage2 currently loads a fixed number of sectors for the kernel (simple and explicit).
- APIC is not enabled yet; timer uses legacy PIC/IRQ0.
- Syscalls are internal stubs and not user-ring transitions yet.
