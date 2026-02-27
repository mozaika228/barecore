#include <stddef.h>
#include <stdint.h>

#define IDT_ENTRIES 256
#define MAX_TASKS 4
#define STACK_SIZE 4096

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

#define PIT_COMMAND  0x43
#define PIT_CHANNEL0 0x40

#define IRQ0_VECTOR    32
#define SYSCALL_VECTOR 0x80

#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_YIELD 3

typedef struct {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
} regs_t;

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} idt_gate_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

typedef struct {
    uint64_t rsp;
    uint8_t active;
    uint8_t exited;
} task_t;

extern void idt_load(idtr_t *idtr);
extern void isr_timer_stub(void);
extern void isr_syscall_stub(void);
extern void switch_context(uint64_t *old_rsp_slot, uint64_t *new_rsp_slot);

static idt_gate_t idt[IDT_ENTRIES];
static idtr_t idtr;

static volatile uint16_t *const vga = (volatile uint16_t *)0xB8000;
static uint16_t vga_pos = 0;

static volatile uint64_t ticks = 0;
static task_t tasks[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][STACK_SIZE];
static int task_count = 0;
static int current_task = -1;
static uint64_t kernel_rsp = 0;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void io_wait(void) {
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

static inline void cpu_halt(void) {
    __asm__ volatile("hlt");
}

static inline void cpu_sti(void) {
    __asm__ volatile("sti");
}

static void put_char(char c) {
    if (c == '\n') {
        vga_pos = (uint16_t)((vga_pos / 80 + 1) * 80);
        return;
    }
    vga[vga_pos++] = (uint16_t)(0x0F00 | (uint8_t)c);
    if (vga_pos >= 80 * 25) {
        vga_pos = 0;
    }
}

static void write_text(const char *s, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        put_char(s[i]);
    }
}

static void write_cstr(const char *s) {
    while (*s) {
        put_char(*s++);
    }
}

static void idt_set_gate(uint8_t vector, void (*handler)(void), uint8_t flags) {
    uint64_t addr = (uint64_t)handler;
    idt[vector].offset_low = (uint16_t)(addr & 0xFFFF);
    idt[vector].selector = 0x08;
    idt[vector].ist = 0;
    idt[vector].type_attr = flags;
    idt[vector].offset_mid = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vector].zero = 0;
}

static void init_idt(void) {
    for (int i = 0; i < IDT_ENTRIES; ++i) {
        idt_set_gate((uint8_t)i, isr_timer_stub, 0x8E);
    }
    idt_set_gate(IRQ0_VECTOR, isr_timer_stub, 0x8E);
    idt_set_gate(SYSCALL_VECTOR, isr_syscall_stub, 0x8E);

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint64_t)&idt[0];
    idt_load(&idtr);
}

static void init_pic(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    (void)mask1;
    (void)mask2;

    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    outb(PIC1_DATA, 0xFE);
    outb(PIC2_DATA, 0xFF);
}

static void init_pit(uint32_t hz) {
    uint32_t divisor = 1193182U / hz;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

static int pick_next_task(void) {
    if (task_count == 0) {
        return -1;
    }

    int start = current_task;
    for (int i = 0; i < task_count; ++i) {
        int candidate = (start + 1 + i) % task_count;
        if (tasks[candidate].active && !tasks[candidate].exited) {
            return candidate;
        }
    }
    return -1;
}

static void schedule(void) {
    int next = pick_next_task();
    if (next < 0) {
        return;
    }

    if (current_task < 0) {
        current_task = next;
        switch_context(&kernel_rsp, &tasks[next].rsp);
        return;
    }

    if (next == current_task) {
        return;
    }

    int prev = current_task;
    current_task = next;
    switch_context(&tasks[prev].rsp, &tasks[next].rsp);
}

static int create_task(void (*entry)(void)) {
    if (task_count >= MAX_TASKS) {
        return -1;
    }

    int idx = task_count++;
    uint64_t *sp = (uint64_t *)(task_stacks[idx] + STACK_SIZE);

    *--sp = (uint64_t)entry;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    tasks[idx].rsp = (uint64_t)sp;
    tasks[idx].active = 1;
    tasks[idx].exited = 0;
    return idx;
}

static long sys_write(const char *buf, size_t len) {
    write_text(buf, len);
    return (long)len;
}

static void task_exit(void) {
    if (current_task >= 0 && current_task < task_count) {
        tasks[current_task].exited = 1;
        tasks[current_task].active = 0;
    }
    schedule();
    for (;;) {
        cpu_halt();
    }
}

void irq_timer_handler(regs_t *regs) {
    (void)regs;
    ticks++;
    outb(PIC1_COMMAND, PIC_EOI);
}

void syscall_dispatch(regs_t *regs) {
    switch (regs->rax) {
    case SYS_WRITE:
        regs->rax = (uint64_t)sys_write((const char *)regs->rdi, (size_t)regs->rsi);
        break;
    case SYS_EXIT:
        task_exit();
        break;
    case SYS_YIELD:
        schedule();
        break;
    default:
        regs->rax = (uint64_t)-1;
        break;
    }
}

static inline long syscall2(long num, long a0, long a1) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "D"(a0), "S"(a1)
                     : "memory");
    return ret;
}

static inline long syscall1(long num, long a0) {
    long ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "D"(a0)
                     : "memory");
    return ret;
}

static void userspace_write(const char *s) {
    size_t len = 0;
    while (s[len]) {
        len++;
    }
    (void)syscall2(SYS_WRITE, (long)(uintptr_t)s, (long)len);
}

static void userspace_yield(void) {
    (void)syscall1(SYS_YIELD, 0);
}

static void userspace_exit(void) {
    (void)syscall1(SYS_EXIT, 0);
}

static void task_a(void) {
    for (int i = 0; i < 20; ++i) {
        userspace_write("A");
        for (volatile int d = 0; d < 100000; ++d) {
        }
        userspace_yield();
    }
    userspace_write(" [A exit]\n");
    userspace_exit();
}

static void task_b(void) {
    for (int i = 0; i < 20; ++i) {
        userspace_write("B");
        for (volatile int d = 0; d < 100000; ++d) {
        }
        userspace_yield();
    }
    userspace_write(" [B exit]\n");
    userspace_exit();
}

void kmain(void) {
    write_cstr("Kernel: long mode OK\n");

    init_idt();
    init_pic();
    init_pit(100);
    cpu_sti();

    create_task(task_a);
    create_task(task_b);

    write_cstr("Scheduler: starting\n");
    schedule();

    for (;;) {
        int live = 0;
        for (int i = 0; i < task_count; ++i) {
            if (tasks[i].active && !tasks[i].exited) {
                live = 1;
                break;
            }
        }
        if (!live) {
            write_cstr("All tasks finished\n");
            for (;;) {
                cpu_halt();
            }
        }
        userspace_yield();
    }
}
