#include <stddef.h>
#include <stdint.h>

#include "../include/boot_info.h"

#define IDT_ENTRIES 256
#define MAX_TASKS 8
#define STACK_SIZE 4096

#define PIT_HZ 100

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

#define PIT_COMMAND  0x43
#define PIT_CHANNEL0 0x40

#define KBD_DATA     0x60

#define COM1_PORT    0x3F8
#define QEMU_EXIT_PORT 0xF4

#define VECTOR_DIVIDE      0
#define VECTOR_PAGE_FAULT  14
#define IRQ_BASE           32
#define VECTOR_TIMER       (IRQ_BASE + 0)
#define VECTOR_KEYBOARD    (IRQ_BASE + 1)
#define VECTOR_SYSCALL     0x80

#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_GETPID  3
#define SYS_SLEEP   4
#define SYS_YIELD   5

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

typedef enum {
    TASK_RUNNABLE = 0,
    TASK_SLEEPING = 1,
    TASK_EXITED = 2
} task_state_t;

typedef struct {
    int pid;
    uint64_t rsp;
    task_state_t state;
    uint64_t wake_tick;
    const char *name;
} task_t;

typedef struct {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch_pixels;
    uint32_t bpp;
    uint32_t format;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t fg;
    uint32_t bg;
    uint8_t enabled;
} fb_console_t;

typedef struct {
    const char *name;
    const char *data;
} initrd_file_t;

extern void idt_load(idtr_t *idtr);
extern void switch_context(uint64_t *old_rsp_slot, uint64_t *new_rsp_slot);
extern void isr_timer_stub(void);
extern void isr_keyboard_stub(void);
extern void isr_syscall_stub(void);
extern void isr_divide_stub(void);
extern void isr_page_fault_stub(void);

static idt_gate_t idt[IDT_ENTRIES];
static idtr_t idtr;

static volatile uint16_t *const vga = (volatile uint16_t *)0xB8000;
static uint16_t vga_pos = 0;
static fb_console_t fb;

static volatile uint64_t ticks = 0;

static task_t tasks[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][STACK_SIZE];
static int task_count = 0;
static int current_task = -1;
static int next_pid = 1;
static uint64_t kernel_rsp = 0;

static char kbd_ring[256];
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;
static uint8_t kbd_shift = 0;

static const initrd_file_t initrd_files[] = {
    {"README.TXT", "barecore initrd\n"},
    {"MOTD.TXT", "Welcome to barecore shell\n"},
    {"SYSINFO.TXT", "Kernel: x86_64, scheduler: round-robin, timer: PIT\n"},
};

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

static inline void cpu_cli(void) {
    __asm__ volatile("cli");
}

static inline uint32_t rgb_to_pixel(uint32_t rgb, uint32_t format) {
    uint32_t r = (rgb >> 16) & 0xFF;
    uint32_t g = (rgb >> 8) & 0xFF;
    uint32_t b = rgb & 0xFF;
    if (format == 0) {
        return (r << 16) | (g << 8) | b;
    }
    return (b << 16) | (g << 8) | r;
}

static void serial_put_char(char c) {
    uint32_t spin = 10000;
    while ((inb(COM1_PORT + 5) & 0x20) == 0 && spin > 0) {
        spin--;
    }
    outb(COM1_PORT, (uint8_t)c);
}

static void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb.enabled || x >= fb.width || y >= fb.height) {
        return;
    }
    ((uint32_t *)(uintptr_t)fb.addr)[y * fb.pitch_pixels + x] = color;
}

static void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t yy = 0; yy < h; ++yy) {
        for (uint32_t xx = 0; xx < w; ++xx) {
            fb_put_pixel(x + xx, y + yy, color);
        }
    }
}

static void fb_draw_char(char c) {
    const uint32_t cw = 8;
    const uint32_t ch = 16;
    uint32_t px = fb.cursor_x * cw;
    uint32_t py = fb.cursor_y * ch;
    uint32_t fg = rgb_to_pixel(fb.fg, fb.format);
    uint32_t bg = rgb_to_pixel(fb.bg, fb.format);

    if (c == '\n') {
        fb.cursor_x = 0;
        fb.cursor_y++;
        return;
    }

    fb_fill_rect(px, py, cw, ch, bg);
    fb_fill_rect(px + 1, py + 1, 6, 1, fg);
    fb_fill_rect(px + 1, py + 10, 6, 1, fg);
    fb_fill_rect(px + 1, py + 1, 1, 10, fg);
    fb_fill_rect(px + 6, py + 1, 1, 10, fg);
    for (uint32_t bit = 0; bit < 6; ++bit) {
        if (((uint8_t)c >> bit) & 1U) {
            fb_fill_rect(px + 1 + bit, py + 12, 1, 3, fg);
        }
    }

    fb.cursor_x++;
    if ((fb.cursor_x + 1) * cw >= fb.width) {
        fb.cursor_x = 0;
        fb.cursor_y++;
    }
    if ((fb.cursor_y + 1) * ch >= fb.height) {
        fb.cursor_x = 0;
        fb.cursor_y = 0;
        fb_fill_rect(0, 0, fb.width, fb.height, bg);
    }
}

static void vga_put_char(char c) {
    if (c == '\n') {
        vga_pos = (uint16_t)((vga_pos / 80 + 1) * 80);
        return;
    }
    vga[vga_pos++] = (uint16_t)(0x0F00 | (uint8_t)c);
    if (vga_pos >= 80 * 25) {
        vga_pos = 0;
    }
}

static void put_char(char c) {
    serial_put_char(c);
    if (fb.enabled) {
        fb_draw_char(c);
    } else {
        vga_put_char(c);
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

static void write_u64_hex(uint64_t value) {
    static const char *hex = "0123456789ABCDEF";
    write_cstr("0x");
    for (int i = 60; i >= 0; i -= 4) {
        put_char(hex[(value >> i) & 0xF]);
    }
}

static void clear_console(void) {
    if (fb.enabled) {
        fb_fill_rect(0, 0, fb.width, fb.height, rgb_to_pixel(fb.bg, fb.format));
        fb.cursor_x = 0;
        fb.cursor_y = 0;
        return;
    }
    for (int i = 0; i < 80 * 25; ++i) {
        vga[i] = 0x0F20;
    }
    vga_pos = 0;
}

static void init_console(const barecore_boot_info_t *bi) {
    fb.enabled = 0;
    fb.cursor_x = 0;
    fb.cursor_y = 0;
    fb.fg = 0xF0F0F0;
    fb.bg = 0x101418;

    if (bi != NULL && bi->magic == BARECORE_BOOTINFO_MAGIC && bi->framebuffer_base != 0 && bi->framebuffer_bpp >= 24) {
        fb.addr = bi->framebuffer_base;
        fb.width = bi->framebuffer_width;
        fb.height = bi->framebuffer_height;
        fb.pitch_pixels = bi->framebuffer_pitch_pixels;
        fb.bpp = bi->framebuffer_bpp;
        fb.format = bi->framebuffer_format;
        fb.enabled = 1;
        clear_console();
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
        idt_set_gate((uint8_t)i, isr_divide_stub, 0x8E);
    }
    idt_set_gate(VECTOR_DIVIDE, isr_divide_stub, 0x8E);
    idt_set_gate(VECTOR_PAGE_FAULT, isr_page_fault_stub, 0x8E);
    idt_set_gate(VECTOR_TIMER, isr_timer_stub, 0x8E);
    idt_set_gate(VECTOR_KEYBOARD, isr_keyboard_stub, 0x8E);
    idt_set_gate(VECTOR_SYSCALL, isr_syscall_stub, 0x8E);

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint64_t)&idt[0];
    idt_load(&idtr);
}

static void init_pic(void) {
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    outb(PIC1_DATA, IRQ_BASE);
    io_wait();
    outb(PIC2_DATA, IRQ_BASE + 8);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    outb(PIC1_DATA, 0xFC); /* unmask IRQ0(timer), IRQ1(keyboard) */
    outb(PIC2_DATA, 0xFF);
}

static void init_pit(uint32_t hz) {
    uint32_t divisor = 1193182U / hz;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

static void kbd_ring_push(char c) {
    uint32_t next = (kbd_head + 1) & 0xFF;
    if (next == kbd_tail) {
        return;
    }
    kbd_ring[kbd_head] = c;
    kbd_head = next;
}

static char kbd_ring_pop(void) {
    if (kbd_head == kbd_tail) {
        return 0;
    }
    char c = kbd_ring[kbd_tail];
    kbd_tail = (kbd_tail + 1) & 0xFF;
    return c;
}

static char scancode_to_ascii(uint8_t sc, uint8_t shift) {
    static const char base[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
    };
    static const char shft[128] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
    };
    if (sc >= 128) {
        return 0;
    }
    return shift ? shft[sc] : base[sc];
}

static int pick_next_task(void) {
    if (task_count == 0) {
        return -1;
    }
    int start = current_task;
    for (int i = 0; i < task_count; ++i) {
        int idx = (start + 1 + i) % task_count;
        if (tasks[idx].state == TASK_RUNNABLE) {
            return idx;
        }
    }
    return -1;
}

static void scheduler_wake_sleepers(void) {
    for (int i = 0; i < task_count; ++i) {
        if (tasks[i].state == TASK_SLEEPING && ticks >= tasks[i].wake_tick) {
            tasks[i].state = TASK_RUNNABLE;
        }
    }
}

static void schedule(void) {
    scheduler_wake_sleepers();

    int next = pick_next_task();
    if (next < 0) {
        if (current_task >= 0) {
            int prev = current_task;
            current_task = -1;
            switch_context(&tasks[prev].rsp, &kernel_rsp);
        }
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

    {
        int prev = current_task;
        current_task = next;
        switch_context(&tasks[prev].rsp, &tasks[next].rsp);
    }
}

static int create_task(void (*entry)(void), const char *name) {
    if (task_count >= MAX_TASKS) {
        return -1;
    }

    int idx = task_count++;
    uint64_t *sp = (uint64_t *)(task_stacks[idx] + STACK_SIZE);

    *--sp = (uint64_t)entry; /* return RIP for switch_context -> ret */
    *--sp = 0;               /* rbp */
    *--sp = 0;               /* rbx */
    *--sp = 0;               /* r12 */
    *--sp = 0;               /* r13 */
    *--sp = 0;               /* r14 */
    *--sp = 0;               /* r15 */

    tasks[idx].pid = next_pid++;
    tasks[idx].rsp = (uint64_t)sp;
    tasks[idx].state = TASK_RUNNABLE;
    tasks[idx].wake_tick = 0;
    tasks[idx].name = name;
    return idx;
}

static int current_pid(void) {
    if (current_task < 0 || current_task >= task_count) {
        return 0;
    }
    return tasks[current_task].pid;
}

static void task_exit_now(void) {
    if (current_task >= 0 && current_task < task_count) {
        tasks[current_task].state = TASK_EXITED;
    }
    schedule();
    for (;;) {
        cpu_halt();
    }
}

static void task_sleep_ticks(uint64_t sleep_ticks) {
    if (current_task >= 0 && current_task < task_count) {
        tasks[current_task].wake_tick = ticks + sleep_ticks;
        tasks[current_task].state = TASK_SLEEPING;
    }
    schedule();
}

static long ksys_write(const char *buf, size_t len) {
    write_text(buf, len);
    return (long)len;
}

static long ksys_getpid(void) {
    return (long)current_pid();
}

static long ksys_sleep(uint64_t ms) {
    uint64_t sleep_ticks = (ms * PIT_HZ + 999) / 1000;
    if (sleep_ticks == 0) {
        sleep_ticks = 1;
    }
    task_sleep_ticks(sleep_ticks);
    return 0;
}

static long ksys_exit(void) {
    task_exit_now();
    return 0;
}

static long userspace_write(const char *s) {
    size_t len = 0;
    while (s[len]) {
        len++;
    }
    return ksys_write(s, len);
}

static long userspace_getpid(void) {
    return ksys_getpid();
}

static void userspace_sleep(uint64_t ms) {
    (void)ksys_sleep(ms);
}

static void userspace_exit(void) {
    (void)ksys_exit();
}

static void shell_print_prompt(void) {
    userspace_write("\n$ ");
}

static int str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int str_starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static void shell_cmd_help(void) {
    userspace_write("commands: help ls cat echo clear pid sleep\n");
}

static void shell_cmd_ls(void) {
    for (size_t i = 0; i < sizeof(initrd_files) / sizeof(initrd_files[0]); ++i) {
        userspace_write(initrd_files[i].name);
        userspace_write("\n");
    }
}

static void shell_cmd_cat(const char *name) {
    for (size_t i = 0; i < sizeof(initrd_files) / sizeof(initrd_files[0]); ++i) {
        if (str_equal(name, initrd_files[i].name)) {
            userspace_write(initrd_files[i].data);
            return;
        }
    }
    userspace_write("cat: not found\n");
}

static void shell_exec(char *line) {
    if (line[0] == 0) {
        return;
    }
    if (str_equal(line, "help")) {
        shell_cmd_help();
        return;
    }
    if (str_equal(line, "ls")) {
        shell_cmd_ls();
        return;
    }
    if (str_equal(line, "clear")) {
        clear_console();
        return;
    }
    if (str_equal(line, "pid")) {
        userspace_write("pid=");
        write_u64_hex((uint64_t)userspace_getpid());
        userspace_write("\n");
        return;
    }
    if (str_starts_with(line, "sleep ")) {
        uint64_t ms = 0;
        const char *p = line + 6;
        while (*p >= '0' && *p <= '9') {
            ms = ms * 10 + (uint64_t)(*p - '0');
            p++;
        }
        userspace_sleep(ms);
        return;
    }
    if (str_starts_with(line, "cat ")) {
        shell_cmd_cat(line + 4);
        return;
    }
    if (str_starts_with(line, "echo ")) {
        userspace_write(line + 5);
        userspace_write("\n");
        return;
    }
    userspace_write("unknown command\n");
}

static char keyboard_read_blocking(void) {
    for (;;) {
        cpu_cli();
        char c = kbd_ring_pop();
        cpu_sti();
        if (c != 0) {
            return c;
        }
        userspace_sleep(10);
    }
}

static void task_shell(void) {
    char line[128];
    userspace_write("\n[bcore shell] type 'help'\n");
    shell_print_prompt();

    for (;;) {
        size_t n = 0;
        for (;;) {
            char c = keyboard_read_blocking();
            if (c == '\r') {
                c = '\n';
            }
            if (c == '\n') {
                userspace_write("\n");
                break;
            }
            if (c == '\b') {
                if (n > 0) {
                    n--;
                    userspace_write("\b \b");
                }
                continue;
            }
            if (n + 1 < sizeof(line)) {
                line[n++] = c;
                put_char(c);
            }
        }
        line[n] = 0;
        shell_exec(line);
        shell_print_prompt();
    }
}

static void task_a(void) {
    for (int i = 0; i < 20; ++i) {
        userspace_write("A");
        userspace_sleep(50);
    }
    userspace_write(" [A exit]\n");
    userspace_exit();
}

static void task_b(void) {
    for (int i = 0; i < 20; ++i) {
        userspace_write("B");
        userspace_sleep(70);
    }
    userspace_write(" [B exit]\n");
    userspace_exit();
}

void irq_timer_handler(regs_t *regs) {
    (void)regs;
    ticks++;
    scheduler_wake_sleepers();
    outb(PIC1_COMMAND, PIC_EOI);
}

void irq_keyboard_handler(regs_t *regs) {
    (void)regs;
    uint8_t sc = inb(KBD_DATA);

    if (sc == 0x2A || sc == 0x36) {
        kbd_shift = 1;
    } else if (sc == 0xAA || sc == 0xB6) {
        kbd_shift = 0;
    } else if ((sc & 0x80U) == 0) {
        char c = scancode_to_ascii(sc, kbd_shift);
        if (c) {
            kbd_ring_push(c);
        }
    }

    outb(PIC1_COMMAND, PIC_EOI);
}

void exception_divide_handler(regs_t *regs) {
    (void)regs;
    write_cstr("\n\n=== EXCEPTION: DIVIDE BY ZERO (#DE) ===\n");
    write_cstr("Kernel halted for safety.\n");
    cpu_cli();
    for (;;) {
        cpu_halt();
    }
}

void exception_page_fault_handler(regs_t *regs, uint64_t error_code) {
    uint64_t cr2;
    (void)regs;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    write_cstr("\n\n=== EXCEPTION: PAGE FAULT (#PF) ===\n");
    write_cstr("fault_addr=");
    write_u64_hex(cr2);
    write_cstr(" error_code=");
    write_u64_hex(error_code);
    write_cstr("\nKernel halted for safety.\n");
    cpu_cli();
    for (;;) {
        cpu_halt();
    }
}

void syscall_dispatch(regs_t *regs) {
    switch (regs->rax) {
    case SYS_WRITE:
        regs->rax = (uint64_t)ksys_write((const char *)(uintptr_t)regs->rdi, (size_t)regs->rsi);
        break;
    case SYS_EXIT:
        if (current_task >= 0 && current_task < task_count) {
            tasks[current_task].state = TASK_EXITED;
        }
        regs->rax = 0;
        break;
    case SYS_GETPID:
        regs->rax = (uint64_t)ksys_getpid();
        break;
    case SYS_SLEEP:
        if (current_task >= 0 && current_task < task_count) {
            uint64_t sleep_ticks = ((uint64_t)regs->rdi * PIT_HZ + 999) / 1000;
            if (sleep_ticks == 0) {
                sleep_ticks = 1;
            }
            tasks[current_task].wake_tick = ticks + sleep_ticks;
            tasks[current_task].state = TASK_SLEEPING;
        }
        regs->rax = 0;
        break;
    case SYS_YIELD:
        regs->rax = 0;
        break;
    default:
        regs->rax = (uint64_t)-1;
        break;
    }
}

void kmain(const barecore_boot_info_t *boot_info) {
    serial_put_char('M'); /* CI breadcrumb: entered kmain */

    init_console(boot_info);
    clear_console();
    write_cstr("barecore kernel (professional profile)\n");
    write_cstr("long mode: OK\n");

    init_idt();
    init_pic();
    init_pit(PIT_HZ);

    create_task(task_a, "task-a");
    create_task(task_b, "task-b");
    create_task(task_shell, "shell");

    write_cstr("scheduler: round-robin\n");
    write_cstr("drivers: PIT + PS/2 keyboard\n");
    write_cstr("syscalls: write exit getpid sleep yield\n");

    cpu_sti();
    schedule();

    for (;;) {
        int live = 0;
        for (int i = 0; i < task_count; ++i) {
            if (tasks[i].state != TASK_EXITED) {
                live = 1;
                break;
            }
        }
        if (!live) {
            write_cstr("\nall tasks exited\n");
            outb(QEMU_EXIT_PORT, 0x10);
        }
        schedule();
    }
}
