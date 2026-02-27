[bits 64]

global _start
global idt_load
global switch_context
global isr_timer_stub
global isr_syscall_stub

extern kmain
extern irq_timer_handler
extern syscall_dispatch

section .text

_start:
    mov dx, 0x3FD
    mov ecx, 0x10000
.wait_tx:
    in al, dx
    test al, 0x20
    jnz .send_mark
    loop .wait_tx
.send_mark:
    mov dx, 0x3F8
    mov al, 'X'
    out dx, al

    mov rsp, 0x00200000
    call kmain
.halt:
    hlt
    jmp .halt

idt_load:
    lidt [rdi]
    ret

; void switch_context(uint64_t* old_rsp_slot, uint64_t* new_rsp_slot)
switch_context:
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp

    mov [rdi], rsp
    mov rsp, [rsi]

    pop rbp
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    ret

isr_timer_stub:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call irq_timer_handler

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    iretq

isr_syscall_stub:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call syscall_dispatch

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    iretq
