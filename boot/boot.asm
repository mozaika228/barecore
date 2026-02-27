[bits 16]
[org 0x7C00]

STAGE2_SECTORS  equ 8
STAGE2_SEGMENT  equ 0x0800
STAGE2_OFFSET   equ 0x0000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov bx, STAGE2_OFFSET
    mov ax, STAGE2_SEGMENT
    mov es, ax

    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp STAGE2_SEGMENT:STAGE2_OFFSET

disk_error:
    mov si, err_msg
.print:
    lodsb
    test al, al
    jz .hang
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .print
.hang:
    cli
    hlt
    jmp .hang

boot_drive db 0
err_msg db "Disk read error", 0

times 510 - ($ - $$) db 0
dw 0xAA55
