param(
    [string]$CrossPrefix = "x86_64-elf-",
    [int]$Stage2Sectors = 8
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = Join-Path $root "build"

New-Item -ItemType Directory -Force -Path $build | Out-Null

$nasm = "nasm"
$gcc = "${CrossPrefix}gcc"
$ld = "${CrossPrefix}ld"
$objcopy = "${CrossPrefix}objcopy"

& $nasm -f bin (Join-Path $root "boot\boot.asm") -o (Join-Path $build "boot.bin")
& $nasm -f bin (Join-Path $root "boot\stage2.asm") -o (Join-Path $build "stage2.bin")
& $nasm -f elf64 (Join-Path $root "kernel\kernel_entry.asm") -o (Join-Path $build "kernel_entry.o")

& $gcc -ffreestanding -fno-pic -fno-stack-protector -m64 -mcmodel=kernel -mno-red-zone -O2 -Wall -Wextra -I (Join-Path $root "include") `
    -c (Join-Path $root "kernel\kernel.c") -o (Join-Path $build "kernel.o")

& $ld -nostdlib -z max-page-size=0x1000 -T (Join-Path $root "linker.ld") `
    -o (Join-Path $build "kernel.elf") `
    (Join-Path $build "kernel_entry.o") (Join-Path $build "kernel.o")

& $objcopy -O binary (Join-Path $build "kernel.elf") (Join-Path $build "kernel.bin")

$imgPath = Join-Path $build "os.img"
$imgSize = 2880 * 512
$stream = [System.IO.File]::Open($imgPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
$stream.SetLength($imgSize)
$stream.Close()

function Write-Blob {
    param(
        [string]$ImagePath,
        [string]$BlobPath,
        [int]$OffsetBytes
    )
    $img = [System.IO.File]::Open($ImagePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    $blob = [System.IO.File]::ReadAllBytes($BlobPath)
    $img.Seek($OffsetBytes, [System.IO.SeekOrigin]::Begin) | Out-Null
    $img.Write($blob, 0, $blob.Length)
    $img.Close()
}

Write-Blob -ImagePath $imgPath -BlobPath (Join-Path $build "boot.bin") -OffsetBytes 0
Write-Blob -ImagePath $imgPath -BlobPath (Join-Path $build "stage2.bin") -OffsetBytes 512
Write-Blob -ImagePath $imgPath -BlobPath (Join-Path $build "kernel.bin") -OffsetBytes ((1 + $Stage2Sectors) * 512)

Write-Host "Built $imgPath"
