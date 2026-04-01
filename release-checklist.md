# BareCore Release Checklist

Use this checklist before tagging a release or demo build.

## 1) Repository State

- [ ] `git status` is clean on `main`
- [ ] latest feature/fix commits are pushed to `origin/main`
- [ ] version/tag name is decided (example: `v0.2.0`)

## 2) Build Validation

- [ ] BIOS build passes:
  - `make CROSS=x86_64-linux-gnu-`
- [ ] UEFI build passes:
  - `make CROSS=x86_64-linux-gnu- uefi`
- [ ] kernel image size check passes (`verify-kernel-size` in Makefile)

## 3) Boot and Runtime Smoke

- [ ] BIOS run boots to kernel banner:
  - `make CROSS=x86_64-linux-gnu- run`
- [ ] UEFI run boots to kernel banner:
  - `make CROSS=x86_64-linux-gnu- run-uefi OVMF=/usr/share/OVMF/OVMF_CODE.fd`
- [ ] shell is interactive (keyboard input, prompt, command execution)

## 4) CI Targets

- [ ] smoke CI target passes:
  - `make CROSS=x86_64-linux-gnu- ci-smoke`
- [ ] runtime CI target passes:
  - `make CROSS=x86_64-linux-gnu- ci-runtime`
- [ ] GitHub Actions badge is green

## 5) Core Feature Checks

- [ ] timer path works (`HPET+IOAPIC` or fallback `APIC/PIT`)
- [ ] exceptions show panic diagnostics (`#DE`, `#PF`)
- [ ] syscall path works (`syscall` with `int 0x80` fallback)
- [ ] scheduler preemption works (timer-driven context switching)
- [ ] ring3 demo commands work:
  - `userdemo`
  - `userpreempt`

## 6) Filesystem and Loader

- [ ] FAT commands work:
  - `lsdisk`
  - `lsdisk /path`
  - `catdisk /path/file`
- [ ] ELF loader command works:
  - `runelf /path/file.ELF`
- [ ] PCI scan command works:
  - `pciscan`

## 7) Debugging Tooling

- [ ] QEMU gdbstub starts:
  - `make CROSS=x86_64-linux-gnu- run-gdb`
- [ ] debugger attaches with symbols:
  - `make CROSS=x86_64-linux-gnu- gdb-attach`
- [ ] `kmain` breakpoint is hit in GDB

## 8) Documentation Sanity

- [ ] README reflects current syscall ABI and shell commands
- [ ] memory map / interrupt model sections are up to date
- [ ] known limitations are documented

## 9) Release Artifacts

- [ ] retain artifacts:
  - `build/kernel.elf`
  - `build/kernel.bin`
  - `build/os.img`
- [ ] release tooling passes:
  - `make CROSS=x86_64-linux-gnu- print-version`
  - `make CROSS=x86_64-linux-gnu- release-check`
  - `make CROSS=x86_64-linux-gnu- dist`
- [ ] create release notes (highlights + breaking changes + known issues)
- [ ] create and push tag:
  - `git tag <version>`
  - `git push origin <version>`
