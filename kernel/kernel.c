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
#define SYS_FORK    6
#define SYS_EXEC    7

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x18
#define GDT_USER_CODE   0x20
#define GDT_TSS         0x28
#define USER_STACK_TOP  0x00080000u

#define LAPIC_DEFAULT_BASE 0xFEE00000u
#define HPET_DEFAULT_BASE  0xFED00000u
#define IOAPIC_DEFAULT_BASE 0xFEC00000u

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

typedef struct {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} irq_frame_t;

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

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t gran;
    uint8_t base_high;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t gran;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
} gdt_tss_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} tss_t;

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
    void (*entry)(void);
} task_t;

typedef struct {
    uint64_t rip;
    uint64_t rsp;
    uint64_t rflags;
    uint8_t active;
} user_task_t;

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

typedef struct __attribute__((packed)) {
    uint8_t jmp[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint16_t total_sectors16;
    uint8_t media;
    uint16_t sectors_per_fat16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_sig;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} fat_bpb_t;

typedef struct {
    fat_bpb_t bpb;
    uint32_t fat_start_lba;
    uint32_t root_start_lba;
    uint32_t data_start_lba;
    uint32_t root_dir_sectors;
    uint32_t total_sectors;
    uint32_t total_clusters;
    uint8_t fat_type; /* 12 or 16 */
    uint8_t valid;
} fat_fs_t;

extern void idt_load(idtr_t *idtr);
extern void gdt_load(void *gdtr);
extern void tss_load(uint16_t selector);
extern void switch_context(uint64_t *old_rsp_slot, uint64_t *new_rsp_slot);
extern void enter_user_mode(void (*entry)(void), uint64_t user_stack);
extern void isr_timer_stub(void);
extern void isr_keyboard_stub(void);
extern void isr_syscall_stub(void);
extern void isr_divide_stub(void);
extern void isr_page_fault_stub(void);

static idt_gate_t idt[IDT_ENTRIES];
static idtr_t idtr;

static struct {
    gdt_entry_t entries[5];
    gdt_tss_entry_t tss;
} gdt_blob;
static tss_t tss;

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

static user_task_t user_tasks[2];
static int current_user = -1;
static uint8_t ring3_enabled = 0;
static uint64_t last_preempt_tick = 0;

static char kbd_ring[256];
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;
static uint8_t kbd_shift = 0;

static uint8_t apic_enabled = 0;
static uint32_t lapic_base = LAPIC_DEFAULT_BASE;
static uint8_t hpet_enabled = 0;
static uint64_t hpet_period_fs = 0;
static uint8_t hpet_irq = 2;
static uint8_t ioapic_enabled = 0;

static fat_fs_t fat_fs;
static uint8_t fat_sector[512];
static uint8_t file_buffer[4096];

static const initrd_file_t initrd_files[] = {
    {"README.TXT", "barecore initrd\n"},
    {"MOTD.TXT", "Welcome to barecore shell\n"},
    {"SYSINFO.TXT", "Kernel: x86_64, scheduler: round-robin, timer: APIC/PIT\n"},
};

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
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

static inline void rdmsr(uint32_t msr, uint32_t *lo, uint32_t *hi) {
    __asm__ volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

static inline void wrmsr(uint32_t msr, uint32_t lo, uint32_t hi) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf));
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

static void dump_regs(const regs_t *r) {
    write_cstr("RAX="); write_u64_hex(r->rax); write_cstr(" RBX="); write_u64_hex(r->rbx); write_cstr("\n");
    write_cstr("RCX="); write_u64_hex(r->rcx); write_cstr(" RDX="); write_u64_hex(r->rdx); write_cstr("\n");
    write_cstr("RSI="); write_u64_hex(r->rsi); write_cstr(" RDI="); write_u64_hex(r->rdi); write_cstr("\n");
    write_cstr("RBP="); write_u64_hex(r->rbp); write_cstr(" RSP=?\n");
    write_cstr("R8 ="); write_u64_hex(r->r8);  write_cstr(" R9 ="); write_u64_hex(r->r9);  write_cstr("\n");
    write_cstr("R10="); write_u64_hex(r->r10); write_cstr(" R11="); write_u64_hex(r->r11); write_cstr("\n");
    write_cstr("R12="); write_u64_hex(r->r12); write_cstr(" R13="); write_u64_hex(r->r13); write_cstr("\n");
    write_cstr("R14="); write_u64_hex(r->r14); write_cstr(" R15="); write_u64_hex(r->r15); write_cstr("\n");
}

static void dump_backtrace(uint64_t rbp) {
    write_cstr("Backtrace:\n");
    for (int i = 0; i < 8 && rbp; ++i) {
        uint64_t *frame = (uint64_t *)rbp;
        uint64_t ret = frame[1];
        write_cstr("  "); write_u64_hex(ret); write_cstr("\n");
        rbp = frame[0];
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

static void gdt_set_entry(gdt_entry_t *e, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    e->limit = (uint16_t)(limit & 0xFFFF);
    e->base_low = (uint16_t)(base & 0xFFFF);
    e->base_mid = (uint8_t)((base >> 16) & 0xFF);
    e->access = access;
    e->gran = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    e->base_high = (uint8_t)((base >> 24) & 0xFF);
}

static void gdt_set_tss(gdt_tss_entry_t *e, uint64_t base, uint32_t limit) {
    e->limit = (uint16_t)(limit & 0xFFFF);
    e->base_low = (uint16_t)(base & 0xFFFF);
    e->base_mid = (uint8_t)((base >> 16) & 0xFF);
    e->access = 0x89;
    e->gran = (uint8_t)(((limit >> 16) & 0x0F));
    e->base_high = (uint8_t)((base >> 24) & 0xFF);
    e->base_upper = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    e->reserved = 0;
}

static void init_gdt_tss(void) {
    gdt_set_entry(&gdt_blob.entries[0], 0, 0, 0, 0);
    gdt_set_entry(&gdt_blob.entries[1], 0, 0xFFFFF, 0x9A, 0xA0);
    gdt_set_entry(&gdt_blob.entries[2], 0, 0xFFFFF, 0x92, 0xA0);
    gdt_set_entry(&gdt_blob.entries[3], 0, 0xFFFFF, 0xF2, 0xA0);
    gdt_set_entry(&gdt_blob.entries[4], 0, 0xFFFFF, 0xFA, 0xA0);

    tss = (tss_t){0};
    tss.rsp0 = 0x00200000;
    tss.iopb_offset = sizeof(tss_t);

    gdt_set_tss(&gdt_blob.tss, (uint64_t)&tss, sizeof(tss_t) - 1);

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdtr = {
        .limit = (uint16_t)(sizeof(gdt_blob) - 1),
        .base = (uint64_t)&gdt_blob,
    };

    gdt_load(&gdtr);
    tss_load(GDT_TSS);

    __asm__ volatile(
        "mov %0, %%ds\n\t"
        "mov %0, %%es\n\t"
        "mov %0, %%ss\n\t"
        :
        : "r"((uint16_t)GDT_KERNEL_DATA)
        : "memory");
}

static void idt_set_gate(uint8_t vector, void (*handler)(void), uint8_t flags) {
    uint64_t addr = (uint64_t)handler;
    idt[vector].offset_low = (uint16_t)(addr & 0xFFFF);
    idt[vector].selector = GDT_KERNEL_CODE;
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
    idt_set_gate(VECTOR_SYSCALL, isr_syscall_stub, 0xEE);

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint64_t)&idt[0];
    idt_load(&idtr);
}

static void init_pic(uint8_t mask_timer) {
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

    outb(PIC1_DATA, mask_timer ? 0xFD : 0xFC);
    outb(PIC2_DATA, 0xFF);
}

static void init_pit(uint32_t hz) {
    uint32_t divisor = 1193182U / hz;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

static volatile uint32_t *lapic_reg(uint32_t offset) {
    return (volatile uint32_t *)(uintptr_t)(lapic_base + offset);
}

static void lapic_write(uint32_t offset, uint32_t value) {
    *lapic_reg(offset) = value;
    (void)*lapic_reg(0x20);
}

static void lapic_init(void) {
    uint32_t a, b, c, d;
    cpuid(1, &a, &b, &c, &d);
    if ((d & (1u << 9)) == 0) {
        apic_enabled = 0;
        return;
    }

    uint32_t lo, hi;
    rdmsr(0x1B, &lo, &hi);
    lo |= (1u << 11);
    wrmsr(0x1B, lo, hi);

    lapic_base = (lo & 0xFFFFF000u);
    if (lapic_base == 0) {
        lapic_base = LAPIC_DEFAULT_BASE;
    }

    lapic_write(0xF0, 0x1FF);
    lapic_write(0x3E0, 0x3);
    lapic_write(0x320, VECTOR_TIMER | (1u << 17));
    lapic_write(0x380, 0x100000);
    apic_enabled = 1;
}

static void lapic_eoi(void) {
    lapic_write(0xB0, 0);
}

static volatile uint64_t *hpet_reg(uint32_t offset) {
    return (volatile uint64_t *)(uintptr_t)(HPET_DEFAULT_BASE + offset);
}

static void hpet_init(void) {
    uint64_t cap = *hpet_reg(0x0);
    if (cap == 0 || cap == 0xFFFFFFFFFFFFFFFFULL) {
        hpet_enabled = 0;
        return;
    }
    hpet_period_fs = cap >> 32;
    *hpet_reg(0x10) = 0;
    *hpet_reg(0xF0) = 0;
    *hpet_reg(0x10) = 1;
    hpet_enabled = (hpet_period_fs != 0);
}

static volatile uint32_t *ioapic_reg(uint32_t offset) {
    return (volatile uint32_t *)(uintptr_t)(IOAPIC_DEFAULT_BASE + offset);
}

static void ioapic_write(uint8_t reg, uint32_t value) {
    *ioapic_reg(0x00) = reg;
    *ioapic_reg(0x10) = value;
}

static uint32_t ioapic_read(uint8_t reg) {
    *ioapic_reg(0x00) = reg;
    return *ioapic_reg(0x10);
}

static void ioapic_init(void) {
    uint32_t ver = ioapic_read(0x01);
    uint32_t max_redir = (ver >> 16) & 0xFF;
    for (uint32_t i = 0; i <= max_redir; ++i) {
        ioapic_write((uint8_t)(0x10 + i * 2), 0x00010000);
        ioapic_write((uint8_t)(0x10 + i * 2 + 1), 0x0);
    }
    ioapic_enabled = 1;
}

static void ioapic_set_irq(uint8_t irq, uint8_t vector) {
    ioapic_write((uint8_t)(0x10 + irq * 2), vector);
    ioapic_write((uint8_t)(0x10 + irq * 2 + 1), 0x0);
}

static void hpet_enable_interrupt(void) {
    if (!hpet_enabled || !ioapic_enabled) {
        return;
    }
    uint64_t conf = *hpet_reg(0x100);
    conf &= ~(1u << 1);
    conf &= ~(1u << 2);
    conf &= ~(0x1Fu << 9);
    conf |= (uint64_t)(hpet_irq & 0x1F) << 9;
    conf |= (1u << 2);
    *hpet_reg(0x100) = conf;
    ioapic_set_irq(hpet_irq, VECTOR_TIMER);
}

static void hpet_set_periodic_ms(uint32_t ms) {
    if (!hpet_enabled) {
        return;
    }
    uint64_t fs_per_tick = hpet_period_fs;
    if (fs_per_tick == 0) {
        return;
    }
    uint64_t ticks_fs = (uint64_t)ms * 1000000000000ULL;
    uint64_t hpet_ticks = ticks_fs / fs_per_tick;
    if (hpet_ticks == 0) {
        hpet_ticks = 1;
    }
    uint64_t conf = *hpet_reg(0x100);
    conf |= (1u << 3);
    conf |= (1u << 6);
    *hpet_reg(0x100) = conf;
    *hpet_reg(0x108) = hpet_ticks;
    *hpet_reg(0x108) = hpet_ticks;
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

    *--sp = (uint64_t)entry;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    tasks[idx].pid = next_pid++;
    tasks[idx].rsp = (uint64_t)sp;
    tasks[idx].state = TASK_RUNNABLE;
    tasks[idx].wake_tick = 0;
    tasks[idx].name = name;
    tasks[idx].entry = entry;
    return idx;
}

static void task_reset_stack(task_t *t, void (*entry)(void)) {
    uint64_t *sp = (uint64_t *)(task_stacks[t - tasks] + STACK_SIZE);
    *--sp = (uint64_t)entry;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    t->rsp = (uint64_t)sp;
    t->entry = entry;
}

static void task_exec_current(void (*entry)(void), const char *name) {
    if (current_task < 0 || current_task >= task_count) {
        return;
    }
    task_t *t = &tasks[current_task];
    t->name = name;
    task_reset_stack(t, entry);
    __asm__ volatile("mov %0, %%rsp; ret" : : "r"(t->rsp));
}

static int task_fork_simple(void) {
    if (current_task < 0 || current_task >= task_count) {
        return -1;
    }
    task_t *parent = &tasks[current_task];
    int child = create_task(parent->entry, parent->name);
    return (child < 0) ? -1 : tasks[child].pid;
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
    userspace_write("commands: help ls cat echo clear pid sleep lsdisk catdisk fork exec userdemo userpreempt\n");
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

static void shell_exec(char *line);
static void user_demo(void);
static void user_task_a(void);
static void user_task_b(void);

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

static void ring3_preempt(irq_frame_t *frame) {
    if (!ring3_enabled || current_user < 0) {
        return;
    }
    if ((frame->cs & 3) != 3) {
        return;
    }
    if (ticks - last_preempt_tick < 1) {
        return;
    }
    last_preempt_tick = ticks;

    user_task_t *cur = &user_tasks[current_user];
    cur->rip = frame->rip;
    cur->rsp = frame->rsp;
    cur->rflags = frame->rflags;

    int next = (current_user + 1) & 1;
    if (!user_tasks[next].active) {
        return;
    }
    current_user = next;
    frame->rip = user_tasks[next].rip;
    frame->rsp = user_tasks[next].rsp;
    frame->rflags = user_tasks[next].rflags;
    frame->cs = 0x23;
    frame->ss = 0x1B;
}

void irq_timer_handler(regs_t *regs, irq_frame_t *frame) {
    (void)regs;
    ticks++;
    scheduler_wake_sleepers();
    ring3_preempt(frame);
    if (apic_enabled) {
        lapic_eoi();
    } else {
        outb(PIC1_COMMAND, PIC_EOI);
    }
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
    dump_regs(regs);
    dump_backtrace(regs->rbp);
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
    dump_regs(regs);
    dump_backtrace(regs->rbp);
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

static void ata_wait_bsy(void) {
    while (inb(0x1F7) & 0x80) {
    }
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 100000; ++i) {
        uint8_t s = inb(0x1F7);
        if (s & 0x08) {
            return 1;
        }
        if (s & 0x01) {
            return 0;
        }
    }
    return 0;
}

static int ata_read_sector(uint32_t lba, uint8_t *buf) {
    ata_wait_bsy();
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)(lba & 0xFF));
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F7, 0x20);
    if (!ata_wait_drq()) {
        return 0;
    }
    for (int i = 0; i < 256; ++i) {
        uint16_t w = inw(0x1F0);
        buf[i * 2] = (uint8_t)(w & 0xFF);
        buf[i * 2 + 1] = (uint8_t)(w >> 8);
    }
    return 1;
}

static void fat_init(void) {
    fat_fs.valid = 0;
    if (!ata_read_sector(0, fat_sector)) {
        return;
    }
    fat_bpb_t *bpb = (fat_bpb_t *)fat_sector;
    if (bpb->bytes_per_sector != 512) {
        return;
    }

    fat_fs.bpb = *bpb;
    fat_fs.total_sectors = bpb->total_sectors16 ? bpb->total_sectors16 : bpb->total_sectors32;
    fat_fs.fat_start_lba = bpb->reserved_sectors;
    fat_fs.root_dir_sectors = ((bpb->root_entries * 32) + 511) / 512;
    fat_fs.root_start_lba = fat_fs.fat_start_lba + (bpb->num_fats * bpb->sectors_per_fat16);
    fat_fs.data_start_lba = fat_fs.root_start_lba + fat_fs.root_dir_sectors;

    uint32_t data_sectors = fat_fs.total_sectors - (bpb->reserved_sectors + (bpb->num_fats * bpb->sectors_per_fat16) + fat_fs.root_dir_sectors);
    fat_fs.total_clusters = data_sectors / bpb->sectors_per_cluster;
    fat_fs.fat_type = (fat_fs.total_clusters < 4085) ? 12 : 16;
    fat_fs.valid = 1;
}

static uint32_t fat_cluster_to_lba(uint32_t cluster) {
    return fat_fs.data_start_lba + (cluster - 2) * fat_fs.bpb.sectors_per_cluster;
}

static uint32_t fat_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = (fat_fs.fat_type == 12) ? (cluster + cluster / 2) : (cluster * 2);
    uint32_t fat_sector_lba = fat_fs.fat_start_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    if (!ata_read_sector(fat_sector_lba, fat_sector)) {
        return 0xFFFFFFFF;
    }
    if (fat_fs.fat_type == 12) {
        uint16_t val = (uint16_t)fat_sector[ent_offset] | ((uint16_t)fat_sector[ent_offset + 1] << 8);
        uint16_t next = (cluster & 1) ? (val >> 4) : (val & 0x0FFF);
        return next;
    }
    return (uint16_t)fat_sector[ent_offset] | ((uint16_t)fat_sector[ent_offset + 1] << 8);
}

static int fat_read_root_entry(const char *name, uint32_t *start_cluster, uint32_t *size) {
    if (!fat_fs.valid) {
        return 0;
    }
    char target[11];
    for (int i = 0; i < 11; ++i) {
        target[i] = ' ';
    }
    int idx = 0;
    for (const char *p = name; *p && idx < 11; ++p) {
        if (*p == '.') {
            idx = 8;
            continue;
        }
        target[idx++] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p;
    }

    for (uint32_t s = 0; s < fat_fs.root_dir_sectors; ++s) {
        if (!ata_read_sector(fat_fs.root_start_lba + s, fat_sector)) {
            return 0;
        }
        for (int i = 0; i < 16; ++i) {
            uint8_t *ent = &fat_sector[i * 32];
            if (ent[0] == 0x00) {
                return 0;
            }
            if (ent[0] == 0xE5 || ent[11] == 0x0F) {
                continue;
            }
            int match = 1;
            for (int j = 0; j < 11; ++j) {
                if (ent[j] != (uint8_t)target[j]) {
                    match = 0;
                    break;
                }
            }
            if (!match) {
                continue;
            }
            *start_cluster = (uint16_t)(ent[26] | (ent[27] << 8));
            *size = (uint32_t)(ent[28] | (ent[29] << 8) | (ent[30] << 16) | (ent[31] << 24));
            return 1;
        }
    }
    return 0;
}

static int fat_read_file(const char *name, uint8_t *out, uint32_t max_bytes, uint32_t *out_size) {
    uint32_t cluster = 0;
    uint32_t size = 0;
    if (!fat_read_root_entry(name, &cluster, &size)) {
        return 0;
    }
    uint32_t remaining = size;
    uint32_t offset = 0;
    while (cluster >= 2 && cluster < 0xFFF8 && remaining > 0 && offset < max_bytes) {
        uint32_t lba = fat_cluster_to_lba(cluster);
        for (uint32_t s = 0; s < fat_fs.bpb.sectors_per_cluster; ++s) {
            if (!ata_read_sector(lba + s, fat_sector)) {
                return 0;
            }
            uint32_t chunk = (remaining > 512) ? 512 : remaining;
            if (offset + chunk > max_bytes) {
                chunk = max_bytes - offset;
            }
            for (uint32_t i = 0; i < chunk; ++i) {
                out[offset + i] = fat_sector[i];
            }
            offset += chunk;
            if (remaining >= chunk) {
                remaining -= chunk;
            } else {
                remaining = 0;
            }
            if (remaining == 0 || offset >= max_bytes) {
                break;
            }
        }
        cluster = fat_next_cluster(cluster);
        if (fat_fs.fat_type == 12 && cluster >= 0xFF8) {
            break;
        }
        if (fat_fs.fat_type == 16 && cluster >= 0xFFF8) {
            break;
        }
    }
    *out_size = offset;
    return 1;
}

static void shell_cmd_lsdisk(void) {
    if (!fat_fs.valid) {
        userspace_write("disk fs: not detected\n");
        return;
    }
    userspace_write("disk fs: FAT");
    userspace_write((fat_fs.fat_type == 12) ? "12\n" : "16\n");
}

static void shell_cmd_catdisk(const char *name) {
    if (!fat_fs.valid) {
        userspace_write("disk fs: not detected\n");
        return;
    }
    uint32_t out_size = 0;
    if (!fat_read_file(name, file_buffer, sizeof(file_buffer) - 1, &out_size)) {
        userspace_write("catdisk: not found\n");
        return;
    }
    file_buffer[out_size] = 0;
    userspace_write((char *)file_buffer);
    userspace_write("\n");
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
    if (str_equal(line, "lsdisk")) {
        shell_cmd_lsdisk();
        return;
    }
    if (str_starts_with(line, "catdisk ")) {
        shell_cmd_catdisk(line + 8);
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
    if (str_equal(line, "fork")) {
        int pid = task_fork_simple();
        userspace_write("fork pid=");
        write_u64_hex((uint64_t)pid);
        userspace_write("\n");
        return;
    }
    if (str_starts_with(line, "exec ")) {
        const char *arg = line + 5;
        if (str_equal(arg, "a")) {
            task_exec_current(task_a, "task-a");
        } else if (str_equal(arg, "b")) {
            task_exec_current(task_b, "task-b");
        } else if (str_equal(arg, "shell")) {
            task_exec_current(task_shell, "shell");
        } else {
            userspace_write("exec: unknown target\n");
        }
        return;
    }
    if (str_equal(line, "userdemo")) {
        userspace_write("entering ring3 demo...\n");
        enter_user_mode(user_demo, USER_STACK_TOP);
        return;
    }
    if (str_equal(line, "userpreempt")) {
        userspace_write("starting ring3 preemptive demo...\n");
        user_tasks[0].rip = (uint64_t)(uintptr_t)user_task_a;
        user_tasks[0].rsp = USER_STACK_TOP;
        user_tasks[0].rflags = 0x202;
        user_tasks[0].active = 1;
        user_tasks[1].rip = (uint64_t)(uintptr_t)user_task_b;
        user_tasks[1].rsp = USER_STACK_TOP - 0x10000;
        user_tasks[1].rflags = 0x202;
        user_tasks[1].active = 1;
        current_user = 0;
        ring3_enabled = 1;
        enter_user_mode(user_task_a, user_tasks[0].rsp);
        return;
    }
    userspace_write("unknown command\n");
}

static inline long user_syscall2(long num, long a0, long a1) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(a0), "S"(a1) : "memory");
    return ret;
}

static inline long user_syscall1(long num, long a0) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(a0) : "memory");
    return ret;
}

static void user_write(const char *s) {
    size_t len = 0;
    while (s[len]) {
        len++;
    }
    (void)user_syscall2(SYS_WRITE, (long)(uintptr_t)s, (long)len);
}

static void user_sleep(uint64_t ms) {
    (void)user_syscall1(SYS_SLEEP, (long)ms);
}

static void user_demo(void) {
    user_write("[ring3] user demo start\n");
    for (int i = 0; i < 10; ++i) {
        user_write("[ring3] tick\n");
        user_sleep(100);
    }
    user_write("[ring3] demo done\n");
    for (;;) {
        user_sleep(500);
    }
}

static void user_task_a(void) {
    for (;;) {
        user_write("[ring3] A\n");
        user_sleep(200);
    }
}

static void user_task_b(void) {
    for (;;) {
        user_write("[ring3] B\n");
        user_sleep(250);
    }
}

void kmain(const barecore_boot_info_t *boot_info) {
    serial_put_char('M');

    init_gdt_tss();
    init_console(boot_info);
    clear_console();
    write_cstr("barecore kernel (production path)\n");
    write_cstr("long mode: OK\n");

    init_idt();
    lapic_init();
    hpet_init();
    ioapic_init();
    if (hpet_enabled && ioapic_enabled) {
        hpet_enable_interrupt();
        hpet_set_periodic_ms(10);
        init_pic(1);
    } else {
        init_pic(apic_enabled ? 1 : 0);
        if (!apic_enabled) {
            init_pit(PIT_HZ);
        }
    }

    fat_init();

    create_task(task_a, "task-a");
    create_task(task_b, "task-b");
    create_task(task_shell, "shell");

    write_cstr("scheduler: round-robin\n");
    write_cstr("drivers: ");
    if (hpet_enabled && ioapic_enabled) {
        write_cstr("HPET+IOAPIC timer + PS/2 keyboard");
    } else if (apic_enabled) {
        write_cstr("APIC timer + PS/2 keyboard");
    } else {
        write_cstr("PIT + PS/2 keyboard");
    }
    write_cstr("\n");
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
