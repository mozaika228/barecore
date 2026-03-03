#!/usr/bin/env bash
set -euo pipefail

make CROSS="${CROSS:-x86_64-linux-gnu-}"
exec qemu-system-x86_64 -nographic -monitor none \
  -drive format=raw,file=build/os.img \
  -serial stdio \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04
