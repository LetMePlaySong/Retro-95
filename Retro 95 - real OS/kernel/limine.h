#ifndef RETRO95_LIMINE_PROTOCOL_H
#define RETRO95_LIMINE_PROTOCOL_H

/* Minimal Limine Boot Protocol declarations used by this kernel.
 * IDs and layouts are from the upstream limine-protocol specification.
 */
#include <stdint.h>

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b
#define LIMINE_BASE_REVISION(N) { 0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, (N) }
#define LIMINE_BASE_REVISION_SUPPORTED(VAR) ((VAR)[2] == 0)

#define LIMINE_REQUESTS_START_MARKER \
    { 0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, 0x785c6ed015d3e316, 0x181e920a7852b9d9 }
#define LIMINE_REQUESTS_END_MARKER \
    { 0xadc0e0531bb10d03, 0x9572709f31764c62 }

struct limine_framebuffer {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;
    uint64_t mode_count;
    void *modes;
    uint64_t upright_rotation;
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_memmap_response *response;
};

struct limine_executable_cmdline_response {
    uint64_t revision;
    char *cmdline;
};

struct limine_executable_cmdline_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_executable_cmdline_response *response;
};

struct limine_tsc_frequency_response {
    uint64_t revision;
    uint64_t frequency;
};

struct limine_tsc_frequency_request {
    uint64_t id[4];
    uint64_t revision;
    struct limine_tsc_frequency_response *response;
};

#define LIMINE_MEMMAP_USABLE 0

#endif
