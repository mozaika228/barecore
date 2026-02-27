#ifndef BARECORE_BOOT_INFO_H
#define BARECORE_BOOT_INFO_H

#include <stdint.h>

#define BARECORE_BOOTINFO_MAGIC 0x42415245434F5245ULL /* "BARECORE" */

typedef struct {
    uint64_t magic;
    uint64_t framebuffer_base;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint32_t framebuffer_pitch_pixels;
    uint32_t framebuffer_bpp;
    uint32_t framebuffer_format; /* GOP pixel format value */
    uint32_t reserved;
} barecore_boot_info_t;

#endif
