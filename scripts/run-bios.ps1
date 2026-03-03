param(
    [string]$CrossPrefix = "x86_64-elf-"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

Push-Location $root
try {
    make CROSS="$CrossPrefix"
    qemu-system-x86_64 -nographic -monitor none `
        -drive format=raw,file=build/os.img `
        -serial stdio `
        -device isa-debug-exit,iobase=0xf4,iosize=0x04
} finally {
    Pop-Location
}
