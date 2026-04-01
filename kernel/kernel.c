#include <stddef.h>
#include <stdint.h>

#include "../include/boot_info.h"

#define IDT_ENTRIES 256
#define MAX_TASKS 8
#define MAX_USER_TASKS 4
#define USER_STACK_SIZE 4096
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
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

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
#define MSR_EFER    0xC0000080u
#define MSR_STAR    0xC0000081u
#define MSR_LSTAR   0xC0000082u
#define MSR_FMASK   0xC0000084u

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x18
#define GDT_USER_CODE   0x20
#define GDT_TSS         0x28
#define USER_STACK_TOP  0x00080000u

#define LAPIC_DEFAULT_BASE 0xFEE00000u
#define HPET_DEFAULT_BASE  0xFED00000u
#define IOAPIC_DEFAULT_BASE 0xFEC00000u
#define PML4_BASE_ADDR 0x00090000u
#define PDPT_BASE_ADDR 0x00091000u
#define PD_APIC_BASE_ADDR 0x00093000u
#define PD_BASE_ADDR 0x00092000u
#define PAGE_SIZE 4096u
#define HEAP_BASE 0x0001000000u
#define HEAP_PAGES 64u

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
    uint8_t pid;
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

typedef struct __attribute__((packed)) {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} acpi_rsdp_t;

typedef struct __attribute__((packed)) {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_t;

typedef struct __attribute__((packed)) {
    acpi_sdt_t header;
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t entries[];
} acpi_madt_t;

typedef struct __attribute__((packed)) {
    uint8_t address_space;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} acpi_gas_t;

typedef struct __attribute__((packed)) {
    acpi_sdt_t header;
    uint32_t event_timer_block_id;
    acpi_gas_t base_address;
    uint8_t hpet_number;
    uint16_t min_tick;
    uint8_t page_protection;
} acpi_hpet_t;

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
extern void syscall_entry(void);
uint64_t syscall_stack_top = 0;

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

static user_task_t user_tasks[MAX_USER_TASKS];
static int current_user = -1;
static uint8_t ring3_enabled = 0;
static uint64_t last_preempt_tick = 0;
static uint8_t user_need_resched = 0;
static uint8_t user_task_stacks[MAX_USER_TASKS][USER_STACK_SIZE];

static char kbd_ring[256];
static volatile uint32_t kbd_head = 0;
static volatile uint32_t kbd_tail = 0;
static uint8_t kbd_shift = 0;

static uint8_t apic_enabled = 0;
static uint32_t lapic_base = LAPIC_DEFAULT_BASE;
static uint32_t ioapic_base = IOAPIC_DEFAULT_BASE;
static uint32_t hpet_base = HPET_DEFAULT_BASE;
static uint8_t hpet_enabled = 0;
static uint64_t hpet_period_fs = 0;
static uint8_t hpet_irq = 2;
static uint8_t ioapic_enabled = 0;
static uint8_t acpi_enabled = 0;
static uint8_t heap_enabled = 0;
static uint8_t *heap_brk = (uint8_t *)(uintptr_t)HEAP_BASE;
static uint8_t sched_preempt = 0;
static uint64_t sched_quantum = 5;
static uint8_t syscall_enabled = 0;
static uint8_t syscall_stack[STACK_SIZE];

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

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
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

static inline void invlpg(void *addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
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

static int mem_equal(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) {
            return 0;
        }
    }
    return 1;
}

static uint64_t read_pte(void *table, uint16_t idx) {
    volatile uint64_t *t = (volatile uint64_t *)(uintptr_t)table;
    return t[idx];
}

static void write_pte(void *table, uint16_t idx, uint64_t value) {
    volatile uint64_t *t = (volatile uint64_t *)(uintptr_t)table;
    t[idx] = value;
}

static uint64_t *get_pt_for(uint64_t vaddr) {
    uint64_t pml4 = PML4_BASE_ADDR;
    uint16_t pml4_idx = (uint16_t)((vaddr >> 39) & 0x1FF);
    uint16_t pdpt_idx = (uint16_t)((vaddr >> 30) & 0x1FF);
    uint16_t pd_idx = (uint16_t)((vaddr >> 21) & 0x1FF);

    uint64_t pml4e = read_pte((void *)pml4, pml4_idx);
    if ((pml4e & 1) == 0) {
        return 0;
    }
    uint64_t pdpt = pml4e & ~0xFFFULL;
    uint64_t pdpte = read_pte((void *)pdpt, pdpt_idx);
    if ((pdpte & 1) == 0) {
        return 0;
    }
    uint64_t pd = pdpte & ~0xFFFULL;
    uint64_t pde = read_pte((void *)pd, pd_idx);
    if ((pde & 1) == 0 || (pde & (1u << 7))) {
        return 0;
    }
    return (uint64_t *)(uintptr_t)(pde & ~0xFFFULL);
}

static int map_page_4k(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    uint64_t *pt = get_pt_for(vaddr);
    if (!pt) {
        return 0;
    }
    uint16_t pt_idx = (uint16_t)((vaddr >> 12) & 0x1FF);
    pt[pt_idx] = (paddr & ~0xFFFULL) | (flags & 0xFFFULL);
    invlpg((void *)(uintptr_t)vaddr);
    return 1;
}

static void serial_put_char(char c) {
    uint32_t spin = 10000;
    while ((inb(COM1_PORT + 5) & 0x20) == 0 && spin > 0) {
        spin--;
    }
    outb(COM1_PORT, (uint8_t)c);
}

static void write_hex_u32(uint32_t v) {
    for (int i = 7; i >= 0; --i) {
        uint8_t digit = (uint8_t)((v >> (i * 4)) & 0xF);
        char c = (digit < 10) ? (char)('0' + digit) : (char)('A' + (digit - 10));
        put_char(c);
    }
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

static void disable_pic(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

static void init_pit(uint32_t hz) {
    uint32_t divisor = 1193182U / hz;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

static uint8_t acpi_checksum_ok(const void *ptr, size_t len) {
    const uint8_t *p = (const uint8_t *)ptr;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum == 0;
}

static const acpi_rsdp_t *acpi_find_rsdp(void) {
    const char sig[8] = {'R','S','D',' ','P','T','R',' '};
    uint16_t ebda_seg = *(volatile uint16_t *)(uintptr_t)0x40E;
    uint32_t ebda = ((uint32_t)ebda_seg) << 4;
    if (ebda >= 0x80000 && ebda <= 0x9FC00) {
        for (uint32_t addr = ebda; addr < ebda + 1024; addr += 16) {
            const acpi_rsdp_t *rsdp = (const acpi_rsdp_t *)(uintptr_t)addr;
            if (mem_equal(rsdp->signature, sig, 8)) {
                return rsdp;
            }
        }
    }
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        const acpi_rsdp_t *rsdp = (const acpi_rsdp_t *)(uintptr_t)addr;
        if (mem_equal(rsdp->signature, sig, 8)) {
            return rsdp;
        }
    }
    return 0;
}

static const acpi_sdt_t *acpi_find_sdt(const acpi_rsdp_t *rsdp, const char sig[4]) {
    const acpi_sdt_t *xsdt = 0;
    const acpi_sdt_t *rsdt = 0;
    if (rsdp->revision >= 2 && rsdp->xsdt_addr) {
        xsdt = (const acpi_sdt_t *)(uintptr_t)rsdp->xsdt_addr;
        if (!acpi_checksum_ok(xsdt, xsdt->length)) {
            xsdt = 0;
        }
    }
    if (!xsdt && rsdp->rsdt_addr) {
        rsdt = (const acpi_sdt_t *)(uintptr_t)rsdp->rsdt_addr;
        if (!acpi_checksum_ok(rsdt, rsdt->length)) {
            rsdt = 0;
        }
    }

    const acpi_sdt_t *root = xsdt ? xsdt : rsdt;
    if (!root) {
        return 0;
    }

    if (root == xsdt) {
        size_t count = (root->length - sizeof(acpi_sdt_t)) / 8;
        const uint64_t *entries = (const uint64_t *)((const uint8_t *)root + sizeof(acpi_sdt_t));
        for (size_t i = 0; i < count; ++i) {
            const acpi_sdt_t *hdr = (const acpi_sdt_t *)(uintptr_t)entries[i];
            if (mem_equal(hdr->signature, sig, 4) && acpi_checksum_ok(hdr, hdr->length)) {
                return hdr;
            }
        }
    } else {
        size_t count = (root->length - sizeof(acpi_sdt_t)) / 4;
        const uint32_t *entries = (const uint32_t *)((const uint8_t *)root + sizeof(acpi_sdt_t));
        for (size_t i = 0; i < count; ++i) {
            const acpi_sdt_t *hdr = (const acpi_sdt_t *)(uintptr_t)entries[i];
            if (mem_equal(hdr->signature, sig, 4) && acpi_checksum_ok(hdr, hdr->length)) {
                return hdr;
            }
        }
    }
    return 0;
}

static void map_mmio_2m(uint64_t phys) {
    if (phys < 0xC0000000ULL || phys >= 0x100000000ULL) {
        return;
    }
    uint64_t base = phys & ~0x1FFFFFULL;
    volatile uint64_t *pd_apic = (volatile uint64_t *)(uintptr_t)PD_APIC_BASE_ADDR;
    uint16_t idx = (uint16_t)((base >> 21) & 0x1FF);
    pd_apic[idx] = base | 0x083;
    invlpg((void *)(uintptr_t)base);
}

static void init_kernel_heap(void) {
    uint64_t pdpt = PDPT_BASE_ADDR;
    uint64_t pdpt_entry = read_pte((void *)pdpt, 0);
    if ((pdpt_entry & 1) == 0) {
        return;
    }
    uint64_t pd = pdpt_entry & ~0xFFFULL;
    uint64_t pd_idx = (HEAP_BASE >> 21) & 0x1FF;
    if (read_pte((void *)pd, (uint16_t)pd_idx) & (1u << 7)) {
        return;
    }
    uint64_t pt = PD_BASE_ADDR + 0x1000;
    write_pte((void *)pd, (uint16_t)pd_idx, (pt & ~0xFFFULL) | 0x003);

    uint64_t heap_phys = 0x00100000ULL;
    for (uint32_t i = 0; i < HEAP_PAGES; ++i) {
        uint64_t vaddr = HEAP_BASE + (uint64_t)i * PAGE_SIZE;
        uint64_t paddr = heap_phys + (uint64_t)i * PAGE_SIZE;
        map_page_4k(vaddr, paddr, 0x003);
    }
    heap_brk = (uint8_t *)(uintptr_t)HEAP_BASE;
    heap_enabled = 1;
}

static void init_syscall_abi(void) {
    uint32_t lo, hi;
    uint64_t star = ((uint64_t)GDT_KERNEL_CODE << 32) | ((uint64_t)0x13 << 48);
    wrmsr(MSR_STAR, (uint32_t)star, (uint32_t)(star >> 32));
    wrmsr(MSR_LSTAR, (uint32_t)(uintptr_t)syscall_entry, (uint32_t)((uint64_t)(uintptr_t)syscall_entry >> 32));
    wrmsr(MSR_FMASK, (uint32_t)(1u << 9), 0);
    rdmsr(MSR_EFER, &lo, &hi);
    lo |= 1u;
    wrmsr(MSR_EFER, lo, hi);
    syscall_stack_top = (uint64_t)(uintptr_t)(syscall_stack + sizeof(syscall_stack));
    syscall_enabled = 1;
}

static void *kmalloc(size_t size) {
    if (!heap_enabled || size == 0) {
        return 0;
    }
    size = (size + 15) & ~((size_t)15);
    uint8_t *cur = heap_brk;
    uint64_t end = HEAP_BASE + (uint64_t)HEAP_PAGES * PAGE_SIZE;
    if ((uint64_t)(uintptr_t)(cur + size) > end) {
        return 0;
    }
    heap_brk = cur + size;
    return cur;
}

static uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1u << 31) |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)slot << 11) |
                       ((uint32_t)func << 8) |
                       (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_dump_devices(void) {
    userspace_write("pci: scan\n");
    for (uint8_t bus = 0; bus < 64; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint32_t id = pci_read_config_dword(bus, slot, 0, 0x00);
            if (id == 0xFFFFFFFFu) {
                continue;
            }
            uint16_t vendor = (uint16_t)(id & 0xFFFF);
            uint16_t device = (uint16_t)(id >> 16);
            if (vendor == 0xFFFF) {
                continue;
            }
            uint32_t class_reg = pci_read_config_dword(bus, slot, 0, 0x08);
            uint8_t class_code = (uint8_t)(class_reg >> 24);
            uint8_t subclass = (uint8_t)(class_reg >> 16);
            uint8_t prog_if = (uint8_t)(class_reg >> 8);
            userspace_write("bus=");
            write_u64_hex(bus);
            userspace_write(" slot=");
            write_u64_hex(slot);
            userspace_write(" ven=");
            write_hex_u32(vendor);
            userspace_write(" dev=");
            write_hex_u32(device);
            userspace_write(" class=");
            write_hex_u32(class_code);
            userspace_write(" sub=");
            write_hex_u32(subclass);
            userspace_write(" pi=");
            write_hex_u32(prog_if);
            userspace_write("\n");
        }
    }
}

static void acpi_parse(void) {
    const acpi_rsdp_t *rsdp = acpi_find_rsdp();
    if (!rsdp) {
        return;
    }
    size_t rsdp_len = (rsdp->revision >= 2 && rsdp->length) ? rsdp->length : 20;
    if (!acpi_checksum_ok(rsdp, rsdp_len)) {
        return;
    }

    const acpi_sdt_t *madt_hdr = acpi_find_sdt(rsdp, "APIC");
    if (madt_hdr) {
        const acpi_madt_t *madt = (const acpi_madt_t *)madt_hdr;
        if (madt->lapic_addr && madt->lapic_addr >= 0xC0000000u) {
            lapic_base = madt->lapic_addr;
            map_mmio_2m(lapic_base);
        }
        const uint8_t *ptr = madt->entries;
        const uint8_t *end = (const uint8_t *)madt + madt->header.length;
        while (ptr + 2 <= end) {
            uint8_t type = ptr[0];
            uint8_t len = ptr[1];
            if (len < 2 || ptr + len > end) {
                break;
            }
            if (type == 1 && len >= 12) {
                uint32_t addr = *(const uint32_t *)(ptr + 4);
                if (addr && addr >= 0xC0000000u) {
                    ioapic_base = addr;
                    map_mmio_2m(ioapic_base);
                }
            }
            ptr += len;
        }
        acpi_enabled = 1;
    }

    const acpi_sdt_t *hpet_hdr = acpi_find_sdt(rsdp, "HPET");
    if (hpet_hdr) {
        const acpi_hpet_t *hpet = (const acpi_hpet_t *)hpet_hdr;
        if (hpet->base_address.address_space == 0 &&
            hpet->base_address.address >= 0xC0000000ULL &&
            hpet->base_address.address < 0x100000000ULL) {
            hpet_base = (uint32_t)hpet->base_address.address;
            map_mmio_2m(hpet_base);
            acpi_enabled = 1;
        }
    }
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
    lo = (lo & 0x00000FFFu) | (lapic_base & 0xFFFFF000u);
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
    return (volatile uint64_t *)(uintptr_t)(hpet_base + offset);
}

static uint8_t hpet_pick_irq(void) {
    uint64_t conf = *hpet_reg(0x100);
    uint32_t routes = (uint32_t)(conf >> 32);
    if (routes == 0) {
        return hpet_irq;
    }
    for (uint8_t i = 0; i < 32; ++i) {
        if (routes & (1u << i)) {
            return i;
        }
    }
    return hpet_irq;
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
    if (hpet_enabled) {
        hpet_irq = hpet_pick_irq();
    }
}

static volatile uint32_t *ioapic_reg(uint32_t offset) {
    return (volatile uint32_t *)(uintptr_t)(ioapic_base + offset);
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
    if ((conf & (1u << 4)) == 0) {
        return;
    }
    conf |= (1u << 3);
    conf |= (1u << 6);
    *hpet_reg(0x100) = conf;
    uint64_t now = *hpet_reg(0xF0);
    *hpet_reg(0x108) = now + hpet_ticks;
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
    userspace_write("commands: help ls cat echo clear pid sleep lsdisk catdisk fork exec userdemo userpreempt pciscan\n");
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
static void user_task_c(void);
static void user_task_d(void);

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

static int create_user_task(int slot, void (*entry)(void)) {
    if (slot < 0 || slot >= MAX_USER_TASKS) {
        return -1;
    }
    user_tasks[slot].rip = (uint64_t)(uintptr_t)entry;
    user_tasks[slot].rsp = (uint64_t)(uintptr_t)&user_task_stacks[slot][USER_STACK_SIZE];
    user_tasks[slot].rflags = 0x202;
    user_tasks[slot].active = 1;
    user_tasks[slot].pid = (uint8_t)(slot + 1);
    return user_tasks[slot].pid;
}

static int pick_next_user(int current) {
    int next = current;
    for (int i = 0; i < MAX_USER_TASKS; ++i) {
        next = (next + 1) % MAX_USER_TASKS;
        if (user_tasks[next].active) {
            return next;
        }
    }
    return -1;
}

static void ring3_preempt(irq_frame_t *frame) {
    if (!ring3_enabled || current_user < 0) {
        return;
    }
    if ((frame->cs & 3) != 3) {
        return;
    }
    if (!user_need_resched && (ticks - last_preempt_tick < 1)) {
        return;
    }
    last_preempt_tick = ticks;
    user_need_resched = 0;

    user_task_t *cur = &user_tasks[current_user];
    cur->rip = frame->rip;
    cur->rsp = frame->rsp;
    cur->rflags = frame->rflags;

    int next = pick_next_user(current_user);
    if (next < 0 || next == current_user) {
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
    if (sched_preempt && (ticks % sched_quantum) == 0) {
        schedule();
    }
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

    if (apic_enabled) {
        lapic_eoi();
    } else {
        outb(PIC1_COMMAND, PIC_EOI);
    }
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
        if (ring3_enabled && current_user >= 0) {
            user_need_resched = 1;
        }
        schedule();
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

static int fat_format_name(const char *name, char out[11]) {
    for (int i = 0; i < 11; ++i) {
        out[i] = ' ';
    }
    int idx = 0;
    int ext = 0;
    for (const char *p = name; *p; ++p) {
        if (*p == '/') {
            return 0;
        }
        if (*p == '.') {
            ext = 1;
            idx = 8;
            continue;
        }
        if (idx >= 11) {
            return 0;
        }
        char c = *p;
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 32);
        }
        out[idx++] = c;
        if (!ext && idx == 8 && p[1] && p[1] != '.') {
            return 0;
        }
    }
    return 1;
}

static int fat_entry_match(const uint8_t *ent, const char target[11]) {
    for (int i = 0; i < 11; ++i) {
        if (ent[i] != (uint8_t)target[i]) {
            return 0;
        }
    }
    return 1;
}

static int fat_dir_find_entry(uint32_t dir_cluster, const char *name, uint32_t *start_cluster, uint32_t *size, uint8_t *attr) {
    char target[11];
    if (!fat_format_name(name, target)) {
        return 0;
    }
    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < fat_fs.root_dir_sectors; ++s) {
            if (!ata_read_sector(fat_fs.root_start_lba + s, fat_sector)) {
                return 0;
            }
            for (int i = 0; i < 16; ++i) {
                uint8_t *ent = &fat_sector[i * 32];
                if (ent[0] == 0x00) {
                    return 0;
                }
                if (ent[0] == 0xE5 || ent[11] == 0x0F || (ent[11] & 0x08)) {
                    continue;
                }
                if (!fat_entry_match(ent, target)) {
                    continue;
                }
                *start_cluster = (uint16_t)(ent[26] | (ent[27] << 8));
                *size = (uint32_t)(ent[28] | (ent[29] << 8) | (ent[30] << 16) | (ent[31] << 24));
                *attr = ent[11];
                return 1;
            }
        }
        return 0;
    }

    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0xFFF8) {
        uint32_t lba = fat_cluster_to_lba(cluster);
        for (uint32_t s = 0; s < fat_fs.bpb.sectors_per_cluster; ++s) {
            if (!ata_read_sector(lba + s, fat_sector)) {
                return 0;
            }
            for (int i = 0; i < 16; ++i) {
                uint8_t *ent = &fat_sector[i * 32];
                if (ent[0] == 0x00) {
                    return 0;
                }
                if (ent[0] == 0xE5 || ent[11] == 0x0F || (ent[11] & 0x08)) {
                    continue;
                }
                if (!fat_entry_match(ent, target)) {
                    continue;
                }
                *start_cluster = (uint16_t)(ent[26] | (ent[27] << 8));
                *size = (uint32_t)(ent[28] | (ent[29] << 8) | (ent[30] << 16) | (ent[31] << 24));
                *attr = ent[11];
                return 1;
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
    return 0;
}

static int fat_resolve_path(const char *path, uint32_t *start_cluster, uint32_t *size, uint8_t *attr) {
    if (!path || !path[0]) {
        return 0;
    }
    while (*path == '/') {
        path++;
    }
    uint32_t dir = 0;
    uint32_t cluster = 0;
    uint32_t fsize = 0;
    uint8_t fattr = 0;
    const char *seg = path;
    while (*seg) {
        const char *end = seg;
        while (*end && *end != '/') {
            end++;
        }
        char name[13];
        int len = 0;
        for (const char *p = seg; p < end && len < 12; ++p) {
            name[len++] = *p;
        }
        name[len] = 0;
        if (!fat_dir_find_entry(dir, name, &cluster, &fsize, &fattr)) {
            return 0;
        }
        if (*end == '/') {
            if ((fattr & 0x10) == 0) {
                return 0;
            }
            dir = cluster;
            seg = end + 1;
            while (*seg == '/') {
                seg++;
            }
            continue;
        }
        *start_cluster = cluster;
        *size = fsize;
        *attr = fattr;
        return 1;
    }
    return 0;
}

static void fat_print_name(const uint8_t *ent) {
    char name[13];
    int pos = 0;
    for (int i = 0; i < 8; ++i) {
        if (ent[i] == ' ') {
            break;
        }
        name[pos++] = (char)ent[i];
    }
    if (ent[8] != ' ') {
        name[pos++] = '.';
        for (int i = 8; i < 11; ++i) {
            if (ent[i] == ' ') {
                break;
            }
            name[pos++] = (char)ent[i];
        }
    }
    name[pos] = 0;
    userspace_write(name);
    userspace_write((ent[11] & 0x10) ? "/\n" : "\n");
}

static int fat_list_dir(uint32_t dir_cluster) {
    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < fat_fs.root_dir_sectors; ++s) {
            if (!ata_read_sector(fat_fs.root_start_lba + s, fat_sector)) {
                return 0;
            }
            for (int i = 0; i < 16; ++i) {
                uint8_t *ent = &fat_sector[i * 32];
                if (ent[0] == 0x00) {
                    return 1;
                }
                if (ent[0] == 0xE5 || ent[11] == 0x0F || (ent[11] & 0x08)) {
                    continue;
                }
                fat_print_name(ent);
            }
        }
        return 1;
    }

    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0xFFF8) {
        uint32_t lba = fat_cluster_to_lba(cluster);
        for (uint32_t s = 0; s < fat_fs.bpb.sectors_per_cluster; ++s) {
            if (!ata_read_sector(lba + s, fat_sector)) {
                return 0;
            }
            for (int i = 0; i < 16; ++i) {
                uint8_t *ent = &fat_sector[i * 32];
                if (ent[0] == 0x00) {
                    return 1;
                }
                if (ent[0] == 0xE5 || ent[11] == 0x0F || (ent[11] & 0x08)) {
                    continue;
                }
                fat_print_name(ent);
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
    return 1;
}

static int fat_read_file_by_cluster(uint32_t cluster, uint32_t size, uint8_t *out, uint32_t max_bytes, uint32_t *out_size) {
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
    fat_list_dir(0);
}

static void shell_cmd_lsdisk_path(const char *path) {
    if (!fat_fs.valid) {
        userspace_write("disk fs: not detected\n");
        return;
    }
    if (!path || path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
        fat_list_dir(0);
        return;
    }
    uint32_t cluster = 0;
    uint32_t size = 0;
    uint8_t attr = 0;
    if (!fat_resolve_path(path, &cluster, &size, &attr)) {
        userspace_write("lsdisk: not found\n");
        return;
    }
    if ((attr & 0x10) == 0) {
        userspace_write("lsdisk: not a directory\n");
        return;
    }
    fat_list_dir(cluster);
}

static void shell_cmd_catdisk(const char *name) {
    if (!fat_fs.valid) {
        userspace_write("disk fs: not detected\n");
        return;
    }
    uint32_t out_size = 0;
    uint32_t cluster = 0;
    uint32_t size = 0;
    uint8_t attr = 0;
    if (!fat_resolve_path(name, &cluster, &size, &attr) || (attr & 0x10)) {
        userspace_write("catdisk: not found\n");
        return;
    }
    if (!fat_read_file_by_cluster(cluster, size, file_buffer, sizeof(file_buffer) - 1, &out_size)) {
        userspace_write("catdisk: read error\n");
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
    if (str_starts_with(line, "lsdisk ")) {
        shell_cmd_lsdisk_path(line + 7);
        return;
    }
    if (str_starts_with(line, "catdisk ")) {
        shell_cmd_catdisk(line + 8);
        return;
    }
    if (str_equal(line, "pciscan")) {
        pci_dump_devices();
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
        for (int i = 0; i < MAX_USER_TASKS; ++i) {
            user_tasks[i].active = 0;
        }
        create_user_task(0, user_task_a);
        create_user_task(1, user_task_b);
        create_user_task(2, user_task_c);
        create_user_task(3, user_task_d);
        current_user = 0;
        ring3_enabled = 1;
        user_need_resched = 0;
        enter_user_mode(user_task_a, user_tasks[0].rsp);
        return;
    }
    userspace_write("unknown command\n");
}

static inline long user_syscall2(long num, long a0, long a1) {
    long ret;
    if (syscall_enabled) {
        __asm__ volatile("syscall"
                         : "=a"(ret)
                         : "a"(num), "D"(a0), "S"(a1)
                         : "rcx", "r11", "memory");
    } else {
        __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(a0), "S"(a1) : "memory");
    }
    return ret;
}

static inline long user_syscall1(long num, long a0) {
    long ret;
    if (syscall_enabled) {
        __asm__ volatile("syscall"
                         : "=a"(ret)
                         : "a"(num), "D"(a0)
                         : "rcx", "r11", "memory");
    } else {
        __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "D"(a0) : "memory");
    }
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

static void user_task_c(void) {
    for (;;) {
        user_write("[ring3] C\n");
        user_sleep(300);
    }
}

static void user_task_d(void) {
    for (;;) {
        user_write("[ring3] D\n");
        user_sleep(350);
    }
}

void kmain(const barecore_boot_info_t *boot_info) {
    serial_put_char('M');

    init_gdt_tss();
    init_console(boot_info);
    clear_console();
    write_cstr("barecore kernel (production path)\n");
    write_cstr("long mode: OK\n");

    acpi_parse();
    init_kernel_heap();
    init_syscall_abi();

    init_idt();
    lapic_init();
    hpet_init();
    ioapic_init();
    if (hpet_enabled && ioapic_enabled) {
        hpet_enable_interrupt();
        hpet_set_periodic_ms(10);
        ioapic_set_irq(1, VECTOR_KEYBOARD);
        disable_pic();
    } else {
        if (ioapic_enabled) {
            ioapic_set_irq(1, VECTOR_KEYBOARD);
            disable_pic();
        } else {
            init_pic(apic_enabled ? 1 : 0);
        }
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
