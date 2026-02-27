[bits 16]
[org 0x8000]

KERNEL_LBA      equ 9
KERNEL_SECTORS  equ 64
KERNEL_SEGMENT  equ 0x1000
KERNEL_OFFSET   equ 0x0000
KERNEL_DEST     equ 0x00100000

CODE32_SEL      equ 0x08
DATA32_SEL      equ 0x10
CODE64_SEL      equ 0x18
DATA64_SEL      equ 0x20

PML4_BASE       equ 0x00090000
PDPT_BASE       equ 0x00091000
PD_BASE         equ 0x00092000

stage2_start:
    cli
    mov [boot_drive], dl

    call enable_a20
    call load_kernel

    lgdt [gdt32_ptr]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE32_SEL:protected_mode

enable_a20:
    in al, 0x92
    or al, 0x02
    out 0x92, al
    ret

load_kernel:
    mov dl, [boot_drive]
    mov si, dap
    mov ah, 0x42
    int 0x13
    jc disk_error
    ret

disk_error:
    mov si, err_msg
.print:
    lodsb
    test al, al
    jz .hang
    mov ah, 0x0E
    mov bx, 0x0004
    int 0x10
    jmp .print
.hang:
    cli
    hlt
    jmp .hang

[bits 32]
protected_mode:
    mov ax, DATA32_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov esp, 0x0009F000

    ; Copy loaded kernel blob from 0x10000 to 0x100000.
    mov esi, (KERNEL_SEGMENT << 4) + KERNEL_OFFSET
    mov edi, KERNEL_DEST
    mov ecx, (KERNEL_SECTORS * 512) / 4
    rep movsd

    ; Build minimal 4-level page tables for identity map of first 2 MiB.
    mov dword [PML4_BASE + 0], PDPT_BASE | 0x003
    mov dword [PML4_BASE + 4], 0x00000000

    mov dword [PDPT_BASE + 0], PD_BASE | 0x003
    mov dword [PDPT_BASE + 4], 0x00000000

    mov dword [PD_BASE + 0], 0x00000083
    mov dword [PD_BASE + 4], 0x00000000

    mov eax, PML4_BASE
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 5)      ; PAE
    mov cr4, eax

    mov ecx, 0xC0000080   ; EFER
    rdmsr
    or eax, (1 << 8)      ; LME
    wrmsr

    mov eax, cr0
    or eax, 0x80000001    ; PG | PE
    mov cr0, eax

    lgdt [gdt64_ptr]
    jmp CODE64_SEL:long_mode

[bits 64]
long_mode:
    mov ax, DATA64_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov rsp, 0x0009E000
    xor rdi, rdi
    mov rax, KERNEL_DEST
    jmp rax

align 8
boot_drive db 0
err_msg db "Kernel load failed", 0

align 8
dap:
    db 0x10
    db 0x00
    dw KERNEL_SECTORS
    dw KERNEL_OFFSET
    dw KERNEL_SEGMENT
    dq KERNEL_LBA

align 8
gdt32:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF ; 32-bit code
    dq 0x00CF92000000FFFF ; 32-bit data
    dq 0x00AF9A000000FFFF ; 64-bit code
    dq 0x00AF92000000FFFF ; 64-bit data
gdt32_end:

gdt32_ptr:
    dw gdt32_end - gdt32 - 1
    dd gdt32

gdt64_ptr:
    dw gdt32_end - gdt32 - 1
    dd gdt32
