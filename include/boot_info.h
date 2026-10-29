#ifndef BARECORE_BOOT_INFO_H
#define BARECORE_BOOT_INFO_H

#include <stdint.h>

#define BARECORE_BOOTINFO_MAGIC 0x42415245434F5245ULL /* "BARECORE" */
#define BARECORE_MEMORY_MAP_MAX 128u
#define BARECORE_MEMORY_USABLE 1u
#define BARECORE_MEMORY_RESERVED 2u
#define BARECORE_MEMORY_SOURCE_BIOS 1u
#define BARECORE_MEMORY_SOURCE_UEFI 2u

typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} barecore_memory_region_t;

typedef struct {
    uint64_t magic;
    uint64_t framebuffer_base;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch_pixels;
    uint32_t framebuffer_bpp;
    uint32_t framebuffer_format; /* GOP pixel format value */
    uint32_t reserved;
    uint32_t memory_map_count;
    uint32_t memory_map_source;
    barecore_memory_region_t memory_map[BARECORE_MEMORY_MAP_MAX];
} barecore_boot_info_t;

#endif
