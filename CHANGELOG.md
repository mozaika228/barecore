# Changelog

## 1.0.0 - 2026-04-01

### Release
- Promoted `1.0.0-rc1` to final `1.0.0`.
- No functional changes from `1.0.0-rc1`; this is a stabilization/finalization tag.

## 1.0.0-rc1 - 2026-04-01

### Boot and Platform
- BIOS and UEFI boot paths maintained.
- 64-bit long mode transition with GDT/IDT setup.
- ACPI MADT/HPET discovery for LAPIC/IOAPIC/HPET base addresses.

### Interrupts and Reliability
- Timer paths: PIT fallback, APIC timer, HPET+IOAPIC path.
- Keyboard routed through IOAPIC when available.
- Unified panic path for exceptions with register/backtrace/frame diagnostics.
- Improved page fault diagnostics with decoded error flags.

### Processes, Scheduling, Syscalls
- Round-robin scheduler with timer preemption.
- Ring3 transition demos and preemptive ring3 task switching.
- Syscall ABI via `syscall/sysret` with `int 0x80` fallback.
- Added `wait` syscall semantics with child reaping.

### Memory
- Kernel heap moved from bump allocator to first-fit `kmalloc/kfree`.
- Free-block split and adjacent-block coalescing.
- Heap diagnostics commands: `memstat`, `memtest`.
- Defensive bounds checks in `write` syscall path.

### Filesystem and Loading
- FAT12/16 path-based directory/file operations.
- ELF loader command `runelf`.
- Ring3 user ELF loader command `runuser` with user-range validation.

### Drivers and Tooling
- PCI device enumeration command `pciscan`.
- GDB helper target and script:
  - `make ... run-gdb`
  - `make ... gdb-attach`
- Release tooling targets:
  - `print-version`
  - `dist`
  - `release-check`
