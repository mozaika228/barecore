[bits 64]

global _start
global idt_load
global switch_context
global isr_timer_stub
global isr_keyboard_stub
global isr_syscall_stub
global isr_divide_stub
global isr_page_fault_stub

extern kmain
extern irq_timer_handler
extern irq_keyboard_handler
extern syscall_dispatch
extern exception_divide_handler
extern exception_page_fault_handler

section .text

%macro PUSH_REGS 0
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
%endmacro

%macro POP_REGS 0
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
%endmacro

_start:
    ; CI breadcrumb: kernel entry reached.
    mov dx, 0x3F8
    mov al, 'X'
    out dx, al
    mov al, 'Y'
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
    PUSH_REGS
    mov rdi, rsp
    call irq_timer_handler
    POP_REGS
    iretq

isr_keyboard_stub:
    PUSH_REGS
    mov rdi, rsp
    call irq_keyboard_handler
    POP_REGS
    iretq

isr_syscall_stub:
    PUSH_REGS
    mov rdi, rsp
    call syscall_dispatch
    POP_REGS
    iretq

isr_divide_stub:
    PUSH_REGS
    mov rdi, rsp
    call exception_divide_handler
    POP_REGS
    iretq

isr_page_fault_stub:
    PUSH_REGS
    mov rdi, rsp
    mov rsi, [rsp + 15 * 8]
    call exception_page_fault_handler
    POP_REGS
    add rsp, 8
    iretq
