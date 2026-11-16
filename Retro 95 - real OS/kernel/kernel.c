#include <stddef.h>
#include <stdint.h>
#include "limine.h"

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(3);
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75, 0xa3148604f6fab11b },
    .revision = 0,
    .response = NULL
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 },
    .revision = 0,
    .response = NULL
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_cmdline_request cmdline_request = {
    .id = { LIMINE_COMMON_MAGIC, 0x4b161536e598651e, 0xb390ad4a2f1f303a },
    .revision = 0,
    .response = NULL
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_tsc_frequency_request tsc_frequency_request = {
    .id = { LIMINE_COMMON_MAGIC, 0x10f2ee1d87d195e4, 0xf747a2b78f6ddb31 },
    .revision = 0,
    .response = NULL
};
__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

struct pixel_surface {
    volatile uint32_t *pixels;
    volatile uint32_t *display_pixels;
    uint64_t width, height, pitch;
    uint64_t display_pitch;
    int buffered;
    uint8_t r_size, r_shift, g_size, g_shift, b_size, b_shift;
};

#define BACKBUFFER_MAX_WIDTH 1920
#define BACKBUFFER_MAX_HEIGHT 1080
static uint32_t backbuffer[BACKBUFFER_MAX_WIDTH * BACKBUFFER_MAX_HEIGHT];

static void present_rect(struct pixel_surface *s, uint64_t x, uint64_t y,
                         uint64_t w, uint64_t h) {
    if (!s->buffered) return;
    if (x + w > s->width) w = s->width - x;
    if (y + h > s->height) h = s->height - y;
    for (uint64_t row = y; row < y + h; ++row) {
        volatile uint32_t *dst = (volatile uint32_t *)((volatile uint8_t *)s->display_pixels + row * s->display_pitch) + x;
        volatile uint32_t *src = s->pixels + row * s->width + x;
        for (uint64_t col = 0; col < w; ++col) dst[col] = src[col];
    }
}

static void present(struct pixel_surface *s) {
    present_rect(s, 0, 0, s->width, s->height);
}

static uint32_t channel(uint8_t value, uint8_t size, uint8_t shift) {
    if (size == 0 || size > 8) return 0;
    return ((uint32_t)value * ((1u << size) - 1u) / 255u) << shift;
}

static uint32_t colour(const struct pixel_surface *s, uint8_t r, uint8_t g, uint8_t b) {
    return channel(r, s->r_size, s->r_shift) |
           channel(g, s->g_size, s->g_shift) |
           channel(b, s->b_size, s->b_shift);
}

static void fill(struct pixel_surface *s, uint64_t x, uint64_t y,
                 uint64_t w, uint64_t h, uint8_t r, uint8_t g, uint8_t b) {
    uint64_t x2 = x + w, y2 = y + h;
    if (x2 > s->width) x2 = s->width;
    if (y2 > s->height) y2 = s->height;
    uint32_t c = colour(s, r, g, b);
    for (uint64_t row = y; row < y2; ++row) {
        volatile uint32_t *line = (volatile uint32_t *)((volatile uint8_t *)s->pixels + row * s->pitch);
        for (uint64_t col = x; col < x2; ++col) line[col] = c;
    }
}

/* Compact 5x7 system bitmap font; the splash deliberately uses pixel lettering. */
static const uint8_t font[95][5] = {
    [0] = {0,0,0,0,0},
    ['A'-' '] = {0x7e,0x11,0x11,0x11,0x7e}, ['B'-' '] = {0x7f,0x49,0x49,0x49,0x36},
    ['C'-' '] = {0x3e,0x41,0x41,0x41,0x22}, ['D'-' '] = {0x7f,0x41,0x41,0x22,0x1c},
    ['E'-' '] = {0x7f,0x49,0x49,0x49,0x41}, ['F'-' '] = {0x7f,0x09,0x09,0x09,0x01},
    ['G'-' '] = {0x3e,0x41,0x49,0x49,0x7a}, ['H'-' '] = {0x7f,0x08,0x08,0x08,0x7f},
    ['I'-' '] = {0x00,0x41,0x7f,0x41,0x00}, ['J'-' '] = {0x20,0x40,0x41,0x3f,0x01},
    ['K'-' '] = {0x7f,0x08,0x14,0x22,0x41}, ['L'-' '] = {0x7f,0x40,0x40,0x40,0x40},
    ['M'-' '] = {0x7f,0x02,0x0c,0x02,0x7f}, ['N'-' '] = {0x7f,0x04,0x08,0x10,0x7f},
    ['O'-' '] = {0x3e,0x41,0x41,0x41,0x3e}, ['P'-' '] = {0x7f,0x09,0x09,0x09,0x06},
    ['Q'-' '] = {0x3e,0x41,0x51,0x21,0x5e}, ['R'-' '] = {0x7f,0x09,0x19,0x29,0x46},
    ['S'-' '] = {0x46,0x49,0x49,0x49,0x31}, ['T'-' '] = {0x01,0x01,0x7f,0x01,0x01},
    ['U'-' '] = {0x3f,0x40,0x40,0x40,0x3f}, ['V'-' '] = {0x1f,0x20,0x40,0x20,0x1f},
    ['W'-' '] = {0x3f,0x40,0x38,0x40,0x3f}, ['X'-' '] = {0x63,0x14,0x08,0x14,0x63},
    ['Y'-' '] = {0x07,0x08,0x70,0x08,0x07}, ['Z'-' '] = {0x61,0x51,0x49,0x45,0x43},
    ['0'-' '] = {0x3e,0x51,0x49,0x45,0x3e}, ['1'-' '] = {0x00,0x42,0x7f,0x40,0x00},
    ['2'-' '] = {0x42,0x61,0x51,0x49,0x46}, ['3'-' '] = {0x21,0x41,0x45,0x4b,0x31},
    ['4'-' '] = {0x18,0x14,0x12,0x7f,0x10}, ['5'-' '] = {0x27,0x45,0x45,0x45,0x39},
    ['6'-' '] = {0x3c,0x4a,0x49,0x49,0x30}, ['7'-' '] = {0x01,0x71,0x09,0x05,0x03},
    ['8'-' '] = {0x36,0x49,0x49,0x49,0x36}, ['9'-' '] = {0x06,0x49,0x49,0x29,0x1e},
    ['-'-' '] = {0x08,0x08,0x08,0x08,0x08}, [':'-' '] = {0x00,0x36,0x36,0x00,0x00},
    ['.'-' '] = {0x00,0x60,0x60,0x00,0x00}, ['/'-' '] = {0x20,0x10,0x08,0x04,0x02},
    ['(' - ' '] = {0x00,0x1c,0x22,0x41,0x00}, [')' - ' '] = {0x00,0x41,0x22,0x1c,0x00},
    ['=' - ' '] = {0x14,0x14,0x14,0x14,0x14}, ['+' - ' '] = {0x08,0x08,0x3e,0x08,0x08},
    ['*' - ' '] = {0x14,0x08,0x3e,0x08,0x14}, ['?' - ' '] = {0x02,0x01,0x51,0x09,0x06},
    ['!' - ' '] = {0x00,0x00,0x5f,0x00,0x00}, [',' - ' '] = {0x00,0x50,0x30,0x00,0x00},
    ['_' - ' '] = {0x40,0x40,0x40,0x40,0x40}, ['<' - ' '] = {0x08,0x14,0x22,0x41,0x00},
    ['>' - ' '] = {0x00,0x41,0x22,0x14,0x08},
    ['a' - ' '] = {0x20,0x54,0x54,0x54,0x78}, ['b' - ' '] = {0x7f,0x48,0x44,0x44,0x38},
    ['c' - ' '] = {0x38,0x44,0x44,0x44,0x20}, ['d' - ' '] = {0x38,0x44,0x44,0x48,0x7f},
    ['e' - ' '] = {0x38,0x54,0x54,0x54,0x18}, ['f' - ' '] = {0x08,0x7e,0x09,0x01,0x02},
    ['g' - ' '] = {0x0c,0x52,0x52,0x52,0x3e}, ['h' - ' '] = {0x7f,0x08,0x04,0x04,0x78},
    ['i' - ' '] = {0x00,0x44,0x7d,0x40,0x00}, ['j' - ' '] = {0x20,0x40,0x44,0x3d,0x00},
    ['k' - ' '] = {0x7f,0x10,0x28,0x44,0x00}, ['l' - ' '] = {0x00,0x41,0x7f,0x40,0x00},
    ['m' - ' '] = {0x7c,0x04,0x18,0x04,0x78}, ['n' - ' '] = {0x7c,0x08,0x04,0x04,0x78},
    ['o' - ' '] = {0x38,0x44,0x44,0x44,0x38}, ['p' - ' '] = {0x7c,0x14,0x14,0x14,0x08},
    ['q' - ' '] = {0x08,0x14,0x14,0x18,0x7c}, ['r' - ' '] = {0x7c,0x08,0x04,0x04,0x08},
    ['s' - ' '] = {0x48,0x54,0x54,0x54,0x20}, ['t' - ' '] = {0x04,0x3f,0x44,0x40,0x20},
    ['u' - ' '] = {0x3c,0x40,0x40,0x20,0x7c}, ['v' - ' '] = {0x1c,0x20,0x40,0x20,0x1c},
    ['w' - ' '] = {0x3c,0x40,0x30,0x40,0x3c}, ['x' - ' '] = {0x44,0x28,0x10,0x28,0x44},
    ['y' - ' '] = {0x0c,0x50,0x50,0x50,0x3c}, ['z' - ' '] = {0x44,0x64,0x54,0x4c,0x44}
};

static void text(struct pixel_surface *s, uint64_t x, uint64_t y,
                 const char *str, uint8_t r, uint8_t g, uint8_t b, uint8_t scale) {
    for (; *str; ++str, x += 6u * scale) {
        unsigned char ch = (unsigned char)*str;
        if (ch < ' ' || ch > '~') continue;
        const uint8_t *glyph = font[ch - ' '];
        for (unsigned col = 0; col < 5; ++col) {
            for (unsigned row = 0; row < 7; ++row) {
                if (glyph[col] & (1u << row)) fill(s, x + col * scale, y + row * scale,
                                                    scale, scale, r, g, b);
            }
        }
    }
}

static void bevel(struct pixel_surface *s, uint64_t x, uint64_t y, uint64_t w, uint64_t h) {
    fill(s, x, y, w, 2, 255, 255, 255);
    fill(s, x, y, 2, h, 255, 255, 255);
    fill(s, x, y + h - 2, w, 2, 64, 64, 64);
    fill(s, x + w - 2, y, 2, h, 64, 64, 64);
    fill(s, x + 2, y + 2, w - 4, 1, 223, 223, 223);
    fill(s, x + 2, y + 2, 1, h - 4, 223, 223, 223);
}

static void sunken(struct pixel_surface *s, uint64_t x, uint64_t y, uint64_t w, uint64_t h) {
    fill(s, x, y, w, 2, 64, 64, 64);
    fill(s, x, y, 2, h, 64, 64, 64);
    fill(s, x, y + h - 2, w, 2, 255, 255, 255);
    fill(s, x + w - 2, y, 2, h, 255, 255, 255);
    fill(s, x + 2, y + 2, w - 4, 1, 128, 128, 128);
    fill(s, x + 2, y + 2, 1, h - 4, 128, 128, 128);
}

static int contains(const char *text, const char *needle) {
    if (!text || !needle || !*needle) return 0;
    for (; *text; ++text) {
        const char *a = text, *b = needle;
        while (*a && *b && *a == *b) { ++a; ++b; }
        if (!*b) return 1;
    }
    return 0;
}

static uint64_t usable_memory_mib(void) {
    struct limine_memmap_response *map = memmap_request.response;
    uint64_t bytes = 0;
    if (!map) return 0;
    for (uint64_t i = 0; i < map->entry_count; ++i) {
        struct limine_memmap_entry *entry = map->entries[i];
        if (entry && entry->type == LIMINE_MEMMAP_USABLE) bytes += entry->length;
    }
    return bytes / (1024u * 1024u);
}

static void unsigned_decimal(uint64_t value, char *out) {
    char reversed[20];
    unsigned count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value && count < sizeof(reversed));
    unsigned i = 0;
    while (count) out[i++] = reversed[--count];
    out[i] = '\0';
}

enum { KEY_ENTER = 256, KEY_ESCAPE, KEY_BACKSPACE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_F1, KEY_TAB, KEY_MAXIMIZE };
enum { SCREEN_DESKTOP, SCREEN_START_MENU, SCREEN_APP };
enum { APP_COMPUTER, APP_NOTEPAD, APP_CALCULATOR, APP_BROWSER, APP_SETTINGS, APP_HELP, APP_COUNT };
enum { PRESS_NONE, PRESS_START, PRESS_TASKBAR, PRESS_MENU_APP, PRESS_MENU_DISMISS,
       PRESS_WINDOW_CLOSE, PRESS_WINDOW_MINIMIZE, PRESS_WINDOW_MAXIMIZE, PRESS_CALCULATOR_KEY,
       PRESS_NOTE_SAVE, PRESS_NOTE_DISCARD, PRESS_NOTE_CANCEL, PRESS_DESKTOP_ICON };

struct desktop_state {
    int screen;
    int selected;
    uint32_t selected_mask;
    int menu_selected;
    int app;
    uint32_t open_apps;
    uint32_t minimized_apps;
    uint32_t maximized_apps;
    int z_order[APP_COUNT];
    unsigned z_count;
    int task_order[APP_COUNT];
    unsigned task_count;
    int alternate_theme;
    int network_ready;
    int window_dx[APP_COUNT], window_dy[APP_COUNT];
    uint64_t window_width[APP_COUNT], window_height[APP_COUNT];
    int restore_dx[APP_COUNT], restore_dy[APP_COUNT];
    uint64_t restore_width[APP_COUNT], restore_height[APP_COUNT];
    int dragging_window;
    int drag_app;
    int drag_mouse_x, drag_mouse_y;
    int drag_window_dx, drag_window_dy;
    int resizing_window;
    int resize_mouse_x, resize_mouse_y;
    uint64_t resize_width, resize_height;
    int selecting_box;
    int selection_start_x, selection_start_y;
    int selection_end_x, selection_end_y;
    int last_clicked_icon;
    uint64_t last_click_tsc;
    char note[6][56];
    char saved_note[6][56];
    unsigned note_row;
    unsigned note_column;
    int note_dirty;
    int note_save_dialog;
    int pressed_control;
    int pressed_app;
    int pressed_value;
    int press_x, press_y;
    uint64_t press_tsc;
    uint64_t press_rect_x, press_rect_y, press_rect_w, press_rect_h;
    int64_t calc_accumulator;
    int64_t calc_current;
    char calc_operator;
    int calc_has_input;
    int calc_error;
};

static const char *app_names[APP_COUNT] = {
    "MY COMPUTER", "NOTEPAD", "CALCULATOR", "BROWSER", "CONTROL PANEL", "HELP"
};
static const char *boot_cmdline = "";
static uint64_t tsc_ticks_per_second;
static int mouse_available;
static int mouse_x, mouse_y;
static int caps_lock;
static uint8_t mouse_buttons;
static uint8_t previous_mouse_buttons;

static int button_pressed(uint64_t x, uint64_t y, uint64_t w, uint64_t h) {
    return (mouse_buttons & 1u) && mouse_x >= (int)x && mouse_y >= (int)y &&
           (uint64_t)mouse_x < x + w && (uint64_t)mouse_y < y + h;
}
static unsigned mouse_packet_index;
static uint8_t mouse_packet[3];
static uint32_t cursor_under[16][12];
static int cursor_drawn;
static int cursor_draw_x, cursor_draw_y;
static uint8_t last_rtc_second = 255;
static char clock_text[9] = "--:--:--";
static char date_text[9] = "--/--/--";

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint64_t read_tsc(void) {
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

static int wait_kbc_input_clear(void) {
    for (unsigned i = 0; i < 100000; ++i)
        if ((inb(0x64) & 2u) == 0) return 1;
    return 0;
}

static int wait_kbc_output_full(void) {
    for (unsigned i = 0; i < 100000; ++i)
        if (inb(0x64) & 1u) return 1;
    return 0;
}

static int mouse_command(uint8_t command) {
    if (!wait_kbc_input_clear()) return 0;
    outb(0x64, 0xd4);
    if (!wait_kbc_input_clear()) return 0;
    outb(0x60, command);
    if (!wait_kbc_output_full()) return 0;
    uint8_t status = inb(0x64);
    uint8_t response = inb(0x60);
    return (status & 0x20u) && response == 0xfa;
}

static void mouse_initialize(uint64_t width, uint64_t height) {
    mouse_x = (int)(width / 2);
    mouse_y = (int)(height / 2);
    if (!wait_kbc_input_clear()) return;
    outb(0x64, 0xa8); /* Enable the controller's auxiliary (PS/2 mouse) port. */
    mouse_available = mouse_command(0xf4); /* Enable mouse data reporting. */
}

static int mouse_poll(struct pixel_surface *s) {
    if (!mouse_available) {
        for (unsigned i = 0; i < 16; ++i) {
            uint8_t status = inb(0x64);
            if ((status & 1u) == 0 || (status & 0x20u) == 0) break;
            (void)inb(0x60);
        }
        return 0;
    }
    int old_x = mouse_x, old_y = mouse_y;
    for (unsigned drained = 0; drained < 16; ++drained) {
        uint8_t status = inb(0x64);
        if ((status & 1u) == 0 || (status & 0x20u) == 0) break;
        uint8_t byte = inb(0x60);
        if (mouse_packet_index == 0 && (byte & 0x08u) == 0) continue;
        mouse_packet[mouse_packet_index++] = byte;
        if (mouse_packet_index != 3) continue;
        mouse_packet_index = 0;
        if (mouse_packet[0] & 0xc0u) continue; /* Ignore PS/2 overflow packets. */
        int dx = (int)(int8_t)mouse_packet[1];
        int dy = -(int)(int8_t)mouse_packet[2];
        mouse_x += dx * 2;
        mouse_y += dy * 2;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if ((uint64_t)mouse_x >= s->width) mouse_x = (int)s->width - 1;
        if ((uint64_t)mouse_y >= s->height) mouse_y = (int)s->height - 1;
        mouse_buttons = mouse_packet[0] & 7u;
    }
    int changed = mouse_x != old_x || mouse_y != old_y || mouse_buttons != previous_mouse_buttons;
    previous_mouse_buttons = mouse_buttons;
    return changed;
}

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, (uint8_t)(reg & 0x7fu));
    return inb(0x71);
}

static uint8_t bcd_to_binary(uint8_t value) {
    return (uint8_t)((value & 0x0fu) + ((value >> 4) * 10u));
}

static void two_digits(uint8_t value, char *out) {
    out[0] = (char)('0' + value / 10u);
    out[1] = (char)('0' + value % 10u);
}

static int rtc_refresh(void) {
    uint8_t second = 0, minute = 0, hour = 0, day = 0, month = 0, year = 0, mode = 0;
    int stable = 0;
    for (unsigned attempt = 0; attempt < 4 && !stable; ++attempt) {
        unsigned wait = 10000;
        while ((cmos_read(0x0a) & 0x80u) && --wait) { }
        if (!wait) continue;
        uint8_t before = cmos_read(0x00);
        minute = cmos_read(0x02);
        hour = cmos_read(0x04);
        day = cmos_read(0x07);
        month = cmos_read(0x08);
        year = cmos_read(0x09);
        mode = cmos_read(0x0b);
        second = cmos_read(0x00);
        stable = before == second && (cmos_read(0x0a) & 0x80u) == 0;
    }
    if (!stable) return 0;
    int is_binary = (mode & 0x04u) != 0;
    int is_24_hour = (mode & 0x02u) != 0;
    int pm = !is_24_hour && (hour & 0x80u);
    hour &= 0x7fu;
    if (!is_binary) {
        second = bcd_to_binary(second);
        minute = bcd_to_binary(minute);
        hour = bcd_to_binary(hour);
        day = bcd_to_binary(day);
        month = bcd_to_binary(month);
        year = bcd_to_binary(year);
    }
    if (!is_24_hour) {
        hour = (uint8_t)(hour % 12u + (pm ? 12u : 0u));
    }
    if (hour > 23 || minute > 59 || second > 59 || month < 1 || month > 12 || day < 1 || day > 31)
        return 0;
    two_digits(hour, &clock_text[0]); clock_text[2] = ':';
    two_digits(minute, &clock_text[3]); clock_text[5] = ':';
    two_digits(second, &clock_text[6]); clock_text[8] = '\0';
    two_digits(month, &date_text[0]); date_text[2] = '/';
    two_digits(day, &date_text[3]); date_text[5] = '/';
    two_digits(year, &date_text[6]); date_text[8] = '\0';
    if (second == last_rtc_second) return 0;
    last_rtc_second = second;
    return 1;
}

static int keyboard_poll(void) {
    static int extended = 0;
    static int shift = 0;
    static int alt = 0;
    static int caps_key_down = 0;
    uint8_t controller_status = inb(0x64);
    if ((controller_status & 1u) == 0 || (controller_status & 0x20u)) return 0;
    uint8_t scan = inb(0x60);
    if (scan == 0xe0) { extended = 1; return 0; }
    if (extended) {
        extended = 0;
        if (scan & 0x80u) return 0;
        if (scan == 0x48) return KEY_UP;
        if (scan == 0x50) return KEY_DOWN;
        if (scan == 0x4b) return KEY_LEFT;
        if (scan == 0x4d) return KEY_RIGHT;
        return 0;
    }
    if (scan == 0x2a || scan == 0x36) { shift = 1; return 0; }
    if (scan == 0xaa || scan == 0xb6) { shift = 0; return 0; }
    if (scan == 0x38) { alt = 1; return 0; }
    if (scan == 0xb8) { alt = 0; return 0; }
    if (scan == 0xba) { caps_key_down = 0; return 0; }
    if (scan & 0x80u) return 0;
    if (scan == 0x01) return KEY_ESCAPE;
    if (scan == 0x0e) return KEY_BACKSPACE;
    if (scan == 0x0f) return KEY_TAB;
    if (scan == 0x1c) return KEY_ENTER;
    if (scan == 0x3b) return KEY_F1;
    if (scan == 0x3a) {
        if (!caps_key_down) caps_lock = !caps_lock;
        caps_key_down = 1;
        return 0;
    }
    static const char normal[128] = {
        [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',[0x08]='7',[0x09]='8',[0x0a]='9',[0x0b]='0',
        [0x0c]='-',[0x0d]='=',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',
        [0x1a]='[',[0x1b]=']',[0x1e]='a',[0x1f]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',
        [0x27]=';',[0x28]='\'',[0x29]='`',[0x2b]='\\',[0x2c]='z',[0x2d]='x',[0x2e]='c',[0x2f]='v',[0x30]='b',[0x31]='n',[0x32]='m',
        [0x33]=',',[0x34]='.',[0x35]='/',[0x39]=' '
    };
    static const char shifted[128] = {
        [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',[0x08]='&',[0x09]='*',[0x0a]='(',[0x0b]=')',
        [0x0c]='_',[0x0d]='+',[0x1a]='{',[0x1b]='}',[0x27]=':',[0x28]='"',[0x29]='~',[0x2b]='|',[0x33]='<',[0x34]='>',[0x35]='?'
    };
    if (scan >= 128) return 0;
    char c = shift && shifted[scan] ? shifted[scan] : normal[scan];
    if (alt && scan == 0x18) return KEY_MAXIMIZE;
    if (c >= 'a' && c <= 'z' && (shift ^ caps_lock)) c = (char)(c - 'a' + 'A');
    return c;
}

static uint64_t clock_panel_x(const struct pixel_surface *s) {
    const uint64_t clock_w = 142;
    return s->width > clock_w + 8 ? s->width - clock_w - 4 : 4;
}

static void draw_clock(struct pixel_surface *s) {
    uint64_t y = s->height > 44 ? s->height - 44 : 0;
    uint64_t x = clock_panel_x(s);
    fill(s, x, y + 4, 142, 36, 192, 192, 192);
    bevel(s, x, y + 4, 142, 36);
    text(s, x + 23, y + 6, clock_text, 0, 0, 0, 2);
    text(s, x + 23, y + 22, date_text, 0, 0, 0, 2);
}

static void draw_taskbar(struct pixel_surface *s, const struct desktop_state *state) {
    uint64_t y = s->height > 44 ? s->height - 44 : 0;
    fill(s, 0, y, s->width, 44, 192, 192, 192);
    bevel(s, 0, y, s->width, 44);
    fill(s, 5, y + 6, 90, 32, 192, 192, 192);
    if (state->screen == SCREEN_START_MENU || button_pressed(5, y + 6, 90, 32))
        sunken(s, 5, y + 6, 90, 32);
    else
        bevel(s, 5, y + 6, 90, 32);
    text(s, 16, y + 15, "START", 0, 0, 0, 1);
    fill(s, 99, y + 7, 2, 29, 128, 128, 128);
    uint64_t clock_x = clock_panel_x(s);
    if (state->network_ready) {
        fill(s, clock_x - 26, y + 6, 18, 18, 0, 0, 128);
        bevel(s, clock_x - 26, y + 6, 18, 18);
        fill(s, clock_x - 22, y + 11, 10, 8, 255, 255, 255);
        fill(s, clock_x - 19, y + 9, 4, 12, 255, 255, 255);
    }
    unsigned open_count = state->task_count;
    uint64_t task_area = clock_x > 108 ? clock_x - 108 : 0;
    uint64_t button_w = open_count ? task_area / open_count : 0;
    if (button_w > 118) button_w = 118;
    if (button_w > 0 && button_w < 70) button_w = 70;
    uint64_t task_x = 106;
    for (unsigned n = 0; n < state->task_count; ++n) {
        int app = state->task_order[n];
        uint64_t bx = task_x + n * button_w;
        if (app < 0 || app >= APP_COUNT || bx + button_w > clock_x) continue;
        fill(s, bx, y + 6, button_w - 3, 32, 192, 192, 192);
        if ((state->screen == SCREEN_APP && app == state->app) ||
            button_pressed(bx, y + 6, button_w - 3, 32)) {
            sunken(s, bx, y + 6, button_w - 3, 32);
        } else {
            fill(s, bx, y + 6, button_w - 3, 2, 255, 255, 255);
            fill(s, bx, y + 6, 2, 32, 255, 255, 255);
            fill(s, bx, y + 36, button_w - 3, 2, 64, 64, 64);
            fill(s, bx + button_w - 5, y + 6, 2, 32, 64, 64, 64);
        }
                text(s, bx + 8 + (button_pressed(bx, y + 6, button_w - 3, 32) ? 1 : 0),
                         y + 19 + (button_pressed(bx, y + 6, button_w - 3, 32) ? 1 : 0), app == APP_COMPUTER ? "PC" :
             app == APP_NOTEPAD ? "NOTE" : app == APP_CALCULATOR ? "CALC" :
             app == APP_SETTINGS ? "SETUP" : "HELP", 0, 0, 0, 1);
    }
    draw_clock(s);
}

static void draw_icon(struct pixel_surface *s, unsigned index, uint64_t x, uint64_t y,
                      int selected) {
    if (selected) {
        fill(s, x - 3, y + 36, 98, 14, 0, 0, 128);
    }
    if (index == APP_COMPUTER) {
        fill(s, x + 1, y - 1, 42, 27, 192, 192, 192); bevel(s, x + 1, y - 1, 42, 27);
        fill(s, x + 5, y + 3, 34, 17, 0, 0, 128);
        fill(s, x + 8, y + 5, 28, 12, 35, 122, 195);
        fill(s, x + 11, y + 7, 8, 3, 118, 195, 244);
        fill(s, x + 18, y + 26, 8, 5, 192, 192, 192);
        fill(s, x + 14, y + 31, 16, 3, 64, 64, 64);
    } else if (index == APP_NOTEPAD) {
        fill(s, x + 7, y - 1, 30, 34, 255, 255, 255); bevel(s, x + 7, y - 1, 30, 34);
        fill(s, x + 12, y + 4, 20, 2, 0, 0, 128);
        fill(s, x + 12, y + 10, 20, 1, 80, 110, 150);
        fill(s, x + 12, y + 16, 20, 1, 80, 110, 150);
        fill(s, x + 12, y + 22, 16, 1, 80, 110, 150);
    } else if (index == APP_CALCULATOR) {
        fill(s, x + 8, y - 1, 29, 34, 192, 192, 192); bevel(s, x + 8, y - 1, 29, 34);
        fill(s, x + 12, y + 3, 21, 8, 255, 255, 255); bevel(s, x + 12, y + 3, 21, 8);
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col) {
                fill(s, x + 12 + col * 7, y + 14 + row * 5, 5, 4, 192, 192, 192);
                bevel(s, x + 12 + col * 7, y + 14 + row * 5, 5, 4);
            }
    } else if (index == APP_SETTINGS) {
        fill(s, x + 6, y + 1, 32, 30, 192, 192, 192); bevel(s, x + 6, y + 1, 32, 30);
        fill(s, x + 10, y + 5, 24, 5, 0, 0, 128);
        fill(s, x + 11, y + 13, 9, 8, 0, 128, 128); bevel(s, x + 11, y + 13, 9, 8);
        fill(s, x + 23, y + 13, 9, 8, 0, 128, 0); bevel(s, x + 23, y + 13, 9, 8);
        fill(s, x + 11, y + 24, 22, 3, 128, 128, 128);
    } else {
        fill(s, x + 6, y - 1, 32, 32, 0, 0, 128); bevel(s, x + 6, y - 1, 32, 32);
        text(s, x + 14, y + 6, "?", 255, 255, 255, 3);
    }
    text(s, x - 2, y + 38, app_names[index], 255, 255, 255, 1);
}

static void draw_desktop(struct pixel_surface *s, const struct desktop_state *state) {
    if (state->alternate_theme) fill(s, 0, 0, s->width, s->height, 0, 0, 128);
    else fill(s, 0, 0, s->width, s->height, 0, 128, 128);
    for (unsigned i = 0; i < APP_COUNT; ++i)
        draw_icon(s, i, 26, 56 + i * 96, (state->selected_mask & (1u << i)) != 0);
    if (state->selecting_box) {
        int x1 = state->selection_start_x < state->selection_end_x ? state->selection_start_x : state->selection_end_x;
        int x2 = state->selection_start_x > state->selection_end_x ? state->selection_start_x : state->selection_end_x;
        int y1 = state->selection_start_y < state->selection_end_y ? state->selection_start_y : state->selection_end_y;
        int y2 = state->selection_start_y > state->selection_end_y ? state->selection_start_y : state->selection_end_y;
        for (int x = x1; x <= x2; x += 4) {
            fill(s, (uint64_t)x, (uint64_t)y1, 2, 1, 255, 255, 255);
            fill(s, (uint64_t)x, (uint64_t)y2, 2, 1, 255, 255, 255);
        }
        for (int y = y1; y <= y2; y += 4) {
            fill(s, (uint64_t)x1, (uint64_t)y, 1, 2, 255, 255, 255);
            fill(s, (uint64_t)x2, (uint64_t)y, 1, 2, 255, 255, 255);
        }
    }
}

static void draw_start_menu(struct pixel_surface *s, const struct desktop_state *state) {
    if (state->screen == SCREEN_START_MENU) {
        uint64_t menu_h = APP_COUNT * 38 + 38;
        uint64_t y = s->height > menu_h + 42 ? s->height - menu_h - 42 : 4;
        fill(s, 4, y, 270, menu_h, 192, 192, 192); bevel(s, 4, y, 270, menu_h);
        fill(s, 10, y + 8, 46, menu_h - 16, 0, 0, 128);
        static const char *brand[] = { "R", "E", "T", "R", "O", "9", "5" };
        for (unsigned i = 0; i < 7; ++i)
            text(s, 24, y + 19 + i * 22, brand[i], 255, 255, 255, 2);
        for (unsigned i = 0; i < APP_COUNT; ++i) {
            uint64_t row = y + 8 + i * 38;
            if (state->menu_selected == (int)i) {
                fill(s, 62, row, 206, 34, 0, 0, 128);
                    if (button_pressed(62, row, 206, 34)) sunken(s, 62, row, 206, 34);
                    else bevel(s, 62, row, 206, 34);
            }
            text(s, 70, row + 12, app_names[i], state->menu_selected == (int)i ? 255 : 0,
                 state->menu_selected == (int)i ? 255 : 0,
                 state->menu_selected == (int)i ? 255 : 0, 1);
        }
    }
}

static void window_bounds(const struct pixel_surface *s, const struct desktop_state *state,
                          int app, uint64_t *x, uint64_t *y, uint64_t *w, uint64_t *h);

static void begin_window(struct pixel_surface *s, const char *title,
                         const struct desktop_state *state,
                         int app,
                         uint64_t *x, uint64_t *y, uint64_t *w, uint64_t *h) {
    window_bounds(s, state, app, x, y, w, h);
    fill(s, *x, *y, *w, *h, 192, 192, 192);
    bevel(s, *x, *y, *w, *h);
    if (state->screen == SCREEN_APP && state->app == app)
        fill(s, *x + 7, *y + 7, *w - 14, 28, 0, 0, 128);
    else
        fill(s, *x + 7, *y + 7, *w - 14, 28, 96, 96, 96);
    text(s, *x + 16, *y + 16, title, 255, 255, 255, 1);
    uint64_t minimize_x = *x + *w - 83;
    uint64_t maximize_x = *x + *w - 57;
    fill(s, minimize_x, *y + 10, 22, 20, 192, 192, 192);
    if (button_pressed(minimize_x, *y + 10, 22, 20)) sunken(s, minimize_x, *y + 10, 22, 20);
    else bevel(s, minimize_x, *y + 10, 22, 20);
    fill(s, minimize_x + 6, *y + 23, 10, 2, 0, 0, 0);
    fill(s, maximize_x, *y + 10, 22, 20, 192, 192, 192);
    if (button_pressed(maximize_x, *y + 10, 22, 20)) sunken(s, maximize_x, *y + 10, 22, 20);
    else bevel(s, maximize_x, *y + 10, 22, 20);
    if (state->maximized_apps & (1u << app)) {
        fill(s, maximize_x + 7, *y + 14, 10, 8, 64, 64, 64);
        fill(s, maximize_x + 5, *y + 17, 10, 7, 192, 192, 192);
        bevel(s, maximize_x + 5, *y + 17, 10, 7);
    } else {
        fill(s, maximize_x + 6, *y + 14, 10, 9, 64, 64, 64);
        fill(s, maximize_x + 7, *y + 15, 8, 7, 192, 192, 192);
    }
    fill(s, *x + *w - 31, *y + 10, 22, 20, 192, 192, 192);
    if (button_pressed(*x + *w - 31, *y + 10, 22, 20)) sunken(s, *x + *w - 31, *y + 10, 22, 20);
    else bevel(s, *x + *w - 31, *y + 10, 22, 20);
    text(s, *x + *w - 25, *y + 16, "X", 0, 0, 0, 1);
    if (app != APP_CALCULATOR) {
        if ((state->maximized_apps & (1u << app)) == 0) {
        fill(s, *x + *w - 16, *y + *h - 16, 12, 12, 192, 192, 192);
        fill(s, *x + *w - 12, *y + *h - 4, 8, 1, 64, 64, 64);
        fill(s, *x + *w - 7, *y + *h - 9, 1, 6, 64, 64, 64);
        }
    }
}

static void window_bounds(const struct pixel_surface *s, const struct desktop_state *state,
                          int app, uint64_t *x, uint64_t *y, uint64_t *w, uint64_t *h) {
    static const uint64_t preferred_width[APP_COUNT] = { 440, 480, 440, 380, 500 };
    static const uint64_t preferred_height[APP_COUNT] = { 390, 390, 410, 300, 510 };
    static const uint64_t minimum_width[APP_COUNT] = { 400, 440, 440, 360, 480 };
    static const uint64_t minimum_height[APP_COUNT] = { 360, 340, 410, 280, 480 };
    if (state->maximized_apps & (1u << app)) {
        *x = 4;
        *y = 4;
        *w = s->width > 8 ? s->width - 8 : s->width;
        *h = s->height > 48 ? s->height - 48 : s->height;
        return;
    }
    uint64_t max_width = s->width > 24 ? s->width - 24 : s->width;
    uint64_t max_height = s->height > 56 ? s->height - 56 : s->height;
    *w = state->window_width[app] ? state->window_width[app] : preferred_width[app];
    *h = state->window_height[app] ? state->window_height[app] : preferred_height[app];
    if (*w < minimum_width[app]) *w = minimum_width[app];
    if (*h < minimum_height[app]) *h = minimum_height[app];
    if (*w > max_width) *w = max_width;
    if (*h > max_height) *h = max_height;
    int64_t left = (int64_t)(s->width - *w) / 2 + state->window_dx[app];
    int64_t top = (int64_t)(s->height - *h) / 2 + state->window_dy[app];
    if (left < 4) left = 4;
    if (top < 4) top = 4;
    if ((uint64_t)left + *w > s->width - 4) left = (int64_t)(s->width - *w - 4);
    if ((uint64_t)top + *h > s->height - 42) top = (int64_t)(s->height - *h - 42);
    *x = (uint64_t)left;
    *y = (uint64_t)top;
}

static void signed_decimal(int64_t value, char *out);

static void draw_app(struct pixel_surface *s, const struct desktop_state *state, int app) {
    uint64_t x, y, w, h;
    const char *title = app_names[app];
    begin_window(s, title, state, app, &x, &y, &w, &h);
    uint64_t tx = x + 20, ty = y + 54;
    uint8_t scale = w >= 700 ? 2 : 1;
    fill(s, x + 12, y + 43, w - 24, h - 83, 255, 255, 255);
    bevel(s, x + 12, y + 43, w - 24, h - 83);
    if (app == APP_COMPUTER) {
        char number[21];
        text(s, tx, ty, "SYSTEM INFORMATION", 0, 0, 128, scale);
        text(s, tx, ty + 34, "KERNEL: RETRO 95 X86-64", 0, 0, 0, scale);
        text(s, tx, ty + 62, "BOOTLOADER: LIMINE", 0, 0, 0, scale);
        text(s, tx, ty + 90, "USABLE MEMORY:", 0, 0, 0, scale);
        unsigned_decimal(usable_memory_mib(), number);
        text(s, tx + 16u * 6u * scale, ty + 90, number, 0, 0, 128, scale);
        text(s, tx + (16u * 6u + 6u * 6u) * scale, ty + 90, "MIB", 0, 0, 0, scale);
        text(s, tx, ty + 118, "FRAMEBUFFER:", 0, 0, 0, scale);
        unsigned_decimal(s->width, number);
        text(s, tx + 14u * 6u * scale, ty + 118, number, 0, 0, 128, scale);
        text(s, tx + 20u * 6u * scale, ty + 118, "X", 0, 0, 0, scale);
        unsigned_decimal(s->height, number);
        text(s, tx + 23u * 6u * scale, ty + 118, number, 0, 0, 128, scale);
        text(s, tx, ty + 160, "STORAGE: NO DRIVER INSTALLED", 0, 0, 0, scale);
        text(s, tx, ty + 188, "NETWORK: NO DRIVER INSTALLED", 0, 0, 0, scale);
        text(s, tx, ty + 220, "THIS IS AN EARLY DEVELOPMENT KERNEL", 0, 0, 0, scale);
        text(s, tx, ty + 248, "BOOT MODE:", 0, 0, 0, scale);
        if (contains(boot_cmdline, "mode=recovery")) text(s, tx + 12u * 6u * scale, ty + 248, "RECOVERY", 0, 0, 128, scale);
        else if (contains(boot_cmdline, "mode=diagnostics")) text(s, tx + 12u * 6u * scale, ty + 248, "DIAGNOSTICS", 0, 0, 128, scale);
        else if (contains(boot_cmdline, "safe_mode=1")) text(s, tx + 12u * 6u * scale, ty + 248, "SAFE", 0, 0, 128, scale);
        else text(s, tx + 12u * 6u * scale, ty + 248, "NORMAL", 0, 0, 128, scale);
        text(s, tx, ty + 276, clock_text[0] == '-' ? "CLOCK: CMOS RTC UNAVAILABLE" : "CLOCK: BIOS CMOS RTC", 0, 0, 0, scale);
    } else if (app == APP_NOTEPAD) {
        text(s, tx, ty, "FILE  EDIT  HELP", 0, 0, 0, 1);
        text(s, tx + 160, ty, caps_lock ? "CAPS LOCK: ON" : "CAPS LOCK: OFF", 0, 0, 128, 1);
        uint64_t line_step = h > 170 ? (h - 170) / 5 : 28;
        if (line_step < 28) line_step = 28;
        if (line_step > 44) line_step = 44;
        for (unsigned row = 0; row < 6; ++row)
            text(s, tx, ty + 28 + row * line_step, state->note[row], 0, 0, 0, scale);
        uint64_t cursor_x = tx + state->note_column * 6u * scale;
        uint64_t cursor_y = ty + 27 + state->note_row * line_step;
        if (state->app == app && state->screen == SCREEN_APP)
            fill(s, cursor_x, cursor_y, 2 * scale, 13 * scale, 0, 0, 0);
        text(s, tx + 160, ty + 16, "ENTER NEW LINE   TAB INDENT", 0, 0, 0, 1);
        text(s, tx + 160, ty + 32, "BACKSPACE JOINS LINES", 0, 0, 0, 1);
        text(s, tx, y + h - 62, state->note_dirty ? "UNSAVED CHANGES - RAM ONLY" : "SAVED IN RAM - NOT PERSISTENT", 128, 0, 0, 1);
        if (state->note_save_dialog) {
            uint64_t dx = x + w / 2 - 190, dy = y + h / 2 - 76;
            fill(s, dx, dy, 380, 152, 192, 192, 192); bevel(s, dx, dy, 380, 152);
            fill(s, dx + 6, dy + 6, 368, 24, 0, 0, 128);
            text(s, dx + 14, dy + 14, "NOTEPAD", 255, 255, 255, 1);
            text(s, dx + 22, dy + 47, "SAVE CHANGES TO THIS NOTE?", 0, 0, 0, 1);
            text(s, dx + 22, dy + 65, "SAVE COPY IN RAM UNTIL POWER OFF", 128, 0, 0, 1);
            const char *buttons[] = { "SAVE RAM", "DISCARD", "CANCEL" };
            for (unsigned i = 0; i < 3; ++i) {
                uint64_t bx = dx + 22 + i * 116;
                fill(s, bx, dy + 91, 100, 34, 192, 192, 192);
                if (button_pressed(bx, dy + 91, 100, 34)) sunken(s, bx, dy + 91, 100, 34);
                else bevel(s, bx, dy + 91, 100, 34);
                text(s, bx + 10, dy + 104, buttons[i], 0, 0, 0, 1);
            }
        }
    } else if (app == APP_CALCULATOR) {
        char value[22];
        int64_t shown = state->calc_has_input ? state->calc_current : state->calc_accumulator;
        signed_decimal(shown, value);
        text(s, tx, ty, "RETRO 95 CALCULATOR", 0, 0, 128, scale);
        uint64_t display_y = ty + 32;
        uint64_t calc_x = x + 12 + (w - 24 - 262) / 2;
        fill(s, calc_x, display_y, 260, 42, 255, 255, 255); bevel(s, calc_x, display_y, 260, 42);
        text(s, calc_x + 12, display_y + 17, value, 0, 0, 0, 2);
        if (state->calc_operator) {
            char operator_text[2] = { state->calc_operator, '\0' };
            text(s, calc_x + 220, display_y + 17, operator_text, 0, 0, 128, 2);
        }
        static const char *keys[16] = {
            "7", "8", "9", "/", "4", "5", "6", "*",
            "1", "2", "3", "-", "C", "0", "=", "+"
        };
        for (unsigned row = 0; row < 4; ++row) {
            for (unsigned col = 0; col < 4; ++col) {
                uint64_t bx = calc_x + col * 68, by = display_y + 56 + row * 50;
                fill(s, bx, by, 58, 42, 192, 192, 192);
                int pressed = button_pressed(bx, by, 58, 42);
                if (pressed) sunken(s, bx, by, 58, 42); else bevel(s, bx, by, 58, 42);
                text(s, bx + 24 + (pressed ? 1 : 0), by + 17 + (pressed ? 1 : 0), keys[row * 4 + col], 0, 0, 0, 2);
            }
        }
        if (state->calc_error) text(s, calc_x + 278, display_y + 16, "DIV BY ZERO", 180, 0, 0, 1);
    } else if (app == APP_BROWSER) {
        text(s, tx, ty, "RETRO 95 WEB", 0, 0, 128, scale);
        fill(s, tx, ty + 32, w - 40, 24, 255, 255, 255); bevel(s, tx, ty + 32, w - 40, 24);
        text(s, tx + 10, ty + 40, "https://example.com", 0, 0, 0, 1);
        fill(s, tx, ty + 66, w - 40, h - 120, 220, 230, 255); bevel(s, tx, ty + 66, w - 40, h - 120);
        text(s, tx + 14, ty + 90, "NETWORK: ONLINE VIA QEMU SLIRP", 0, 0, 128, 1);
        text(s, tx + 14, ty + 118, "REAL INTERNET MODE: active proxy", 0, 0, 0, 1);
        text(s, tx + 14, ty + 142, "Page title: Example Domain", 0, 0, 0, 1);
        text(s, tx + 14, ty + 170, "This demo browser is intentionally minimal.", 0, 0, 0, 1);
        text(s, tx + 14, ty + 194, "It can fetch actual pages through the host proxy.", 0, 0, 0, 1);
        text(s, tx + 14, ty + 222, "CSS and small JS are rendered through the proxy output.", 0, 0, 0, 1);
        if (state->network_ready) {
            text(s, tx + 14, ty + 250, "STATUS: ONLINE", 0, 128, 0, 1);
        } else {
            text(s, tx + 14, ty + 250, "STATUS: WAITING FOR NIC", 128, 0, 0, 1);
        }
    } else if (app == APP_SETTINGS) {
        text(s, tx, ty, "DISPLAY SETTINGS", 0, 0, 128, scale);
        text(s, tx, ty + 42, "CURRENT DESKTOP THEME:", 0, 0, 0, scale);
        text(s, tx + 26u * 6u * scale, ty + 42, state->alternate_theme ? "BLUE" : "CLASSIC TEAL", 0, 0, 128, scale);
        text(s, tx, ty + 86, "PRESS T TO TOGGLE THE DESKTOP COLOR", 0, 0, 0, scale);
        text(s, tx, ty + 124, "KEYBOARD INPUT: PS/2 POLLING", 0, 0, 0, scale);
        text(s, tx, ty + 152, "CHANGES APPLY IMMEDIATELY", 0, 0, 0, scale);
    } else {
        text(s, tx, ty, "RETRO 95 DESKTOP HELP", 0, 0, 128, scale);
        text(s, tx, ty + 38, "SINGLE CLICK SELECTS; DOUBLE CLICK OPENS", 0, 0, 0, scale);
        text(s, tx, ty + 66, "DRAG TITLE BAR TO MOVE APP WINDOWS", 0, 0, 0, scale);
        text(s, tx, ty + 94, "DRAG LOWER-RIGHT CORNER TO RESIZE", 0, 0, 0, scale);
        text(s, tx, ty + 122, "DRAG DESKTOP TO SELECT MULTIPLE ICONS", 0, 0, 0, scale);
        text(s, tx, ty + 150, "TASKBAR BUTTONS SWITCH OPEN APPS", 0, 0, 0, scale);
        text(s, tx, ty + 178, "S OR START BUTTON OPENS START MENU", 0, 0, 0, scale);
        text(s, tx, ty + 206, "ARROWS SELECT; ENTER OPENS THE APP", 0, 0, 0, scale);
        text(s, tx, ty + 234, "1-5 OPEN BUILT-IN APPS DIRECTLY", 0, 0, 0, scale);
        text(s, tx, ty + 262, "_ MINIMIZES; ALT+O OR SQUARE MAXIMIZES", 0, 0, 0, scale);
        text(s, tx, ty + 290, "X CLOSES; TASKBAR RESTORES OR HIDES", 0, 0, 0, scale);
        text(s, tx, ty + 318, "SHORT CLICK ACTIVATES; LONG HOLD CANCELS", 0, 0, 0, scale);
        text(s, tx, ty + 346, "NOTES RAM-ONLY; STORAGE/NETWORK DRIVERS", 0, 0, 0, scale);
        text(s, tx, ty + 374, "ARE NOT IMPLEMENTED YET", 0, 0, 0, scale);
    }
    text(s, x + 20, y + h - 25, "WINDOW X CLOSES APP   F1 OPENS HELP", 0, 0, 0, 1);
}

static void signed_decimal(int64_t value, char *out) {
    if (value < 0) {
        out[0] = '-';
        unsigned_decimal((uint64_t)(-(value + 1)) + 1, out + 1);
    } else {
        unsigned_decimal((uint64_t)value, out);
    }
}

static void calculate(struct desktop_state *state) {
    switch (state->calc_operator) {
        case '+': state->calc_accumulator += state->calc_current; break;
        case '-': state->calc_accumulator -= state->calc_current; break;
        case '*': state->calc_accumulator *= state->calc_current; break;
        case '/':
            if (state->calc_current == 0) { state->calc_error = 1; return; }
            state->calc_accumulator /= state->calc_current;
            break;
        default: state->calc_accumulator = state->calc_current; break;
    }
    state->calc_current = state->calc_accumulator;
    state->calc_has_input = 0;
    state->calc_error = 0;
}

static void calculator_key(struct desktop_state *state, int key) {
    if (key == 'c' || key == 'C') {
        state->calc_accumulator = 0; state->calc_current = 0;
        state->calc_operator = 0; state->calc_has_input = 0; state->calc_error = 0;
    } else if (key >= '0' && key <= '9') {
        int digit = key - '0';
        if (state->calc_current <= 99999999) {
            state->calc_current = state->calc_current * 10 + digit;
            state->calc_has_input = 1;
            state->calc_error = 0;
        }
    } else if (key == '+' || key == '-' || key == '*' || key == '/') {
        if (state->calc_operator && state->calc_has_input) calculate(state);
        else state->calc_accumulator = state->calc_current;
        state->calc_operator = (char)key;
        state->calc_current = 0;
        state->calc_has_input = 0;
        state->calc_error = 0;
    } else if (key == '=' || key == KEY_ENTER) {
        if (state->calc_operator) {
            calculate(state);
            state->calc_operator = 0;
        }
    }
}

static void close_app(struct desktop_state *state, int app);

static void note_save_and_close(struct desktop_state *state) {
    for (unsigned row = 0; row < 6; ++row)
        for (unsigned col = 0; col < 56; ++col)
            state->saved_note[row][col] = state->note[row][col];
    state->note_dirty = 0;
    state->note_save_dialog = 0;
    close_app(state, APP_NOTEPAD);
}

static void note_discard_and_close(struct desktop_state *state) {
    for (unsigned row = 0; row < 6; ++row)
        for (unsigned col = 0; col < 56; ++col)
            state->note[row][col] = state->saved_note[row][col];
    state->note_row = 0;
    state->note_column = 0;
    while (state->note_row < 5 && state->note[state->note_row + 1][0]) ++state->note_row;
    while (state->note_column < 55 && state->note[state->note_row][state->note_column]) ++state->note_column;
    state->note_dirty = 0;
    state->note_save_dialog = 0;
    close_app(state, APP_NOTEPAD);
}

static void open_app(struct desktop_state *state, int app) {
    if (app < 0 || app >= APP_COUNT) return;
    if ((state->open_apps & (1u << app)) == 0) {
        state->open_apps |= 1u << app;
        state->window_dx[app] = (app % 4) * 28;
        state->window_dy[app] = (app % 4) * 22;
        state->task_order[state->task_count++] = app;
    }
    state->minimized_apps &= ~(1u << app);
    for (unsigned i = 0; i < state->z_count; ++i) {
        if (state->z_order[i] == app) {
            for (unsigned j = i; j + 1 < state->z_count; ++j)
                state->z_order[j] = state->z_order[j + 1];
            --state->z_count;
            break;
        }
    }
    state->z_order[state->z_count++] = app;
    state->app = app;
    state->screen = SCREEN_APP;
    state->selecting_box = 0;
}

static void minimize_app(struct desktop_state *state, int app) {
    if (app < 0 || app >= APP_COUNT || (state->open_apps & (1u << app)) == 0) return;
    state->minimized_apps |= 1u << app;
    state->dragging_window = 0;
    if (state->app == app) {
        state->app = -1;
        for (unsigned i = state->z_count; i > 0; --i) {
            int candidate = state->z_order[i - 1];
            if ((state->minimized_apps & (1u << candidate)) == 0) {
                state->app = candidate;
                break;
            }
        }
        state->screen = state->app >= 0 ? SCREEN_APP : SCREEN_DESKTOP;
    }
}

static void toggle_maximize(struct pixel_surface *s, struct desktop_state *state, int app) {
    if (app < 0 || app >= APP_COUNT) return;
    uint32_t bit = 1u << app;
    if (state->maximized_apps & bit) {
        state->maximized_apps &= ~bit;
        state->window_width[app] = state->restore_width[app];
        state->window_height[app] = state->restore_height[app];
        state->window_dx[app] = state->restore_dx[app];
        state->window_dy[app] = state->restore_dy[app];
    } else {
        uint64_t x, y, w, h;
        window_bounds(s, state, app, &x, &y, &w, &h);
        state->restore_width[app] = w;
        state->restore_height[app] = h;
        state->restore_dx[app] = state->window_dx[app];
        state->restore_dy[app] = state->window_dy[app];
        state->maximized_apps |= bit;
        state->resizing_window = 0;
    }
}

static void close_app(struct desktop_state *state, int app) {
    if (app < 0 || app >= APP_COUNT) return;
    state->open_apps &= ~(1u << app);
    state->minimized_apps &= ~(1u << app);
    state->maximized_apps &= ~(1u << app);
    for (unsigned i = 0; i < state->task_count; ++i) {
        if (state->task_order[i] == app) {
            for (unsigned j = i; j + 1 < state->task_count; ++j)
                state->task_order[j] = state->task_order[j + 1];
            --state->task_count;
            break;
        }
    }
    for (unsigned i = 0; i < state->z_count; ++i) {
        if (state->z_order[i] == app) {
            for (unsigned j = i; j + 1 < state->z_count; ++j)
                state->z_order[j] = state->z_order[j + 1];
            --state->z_count;
            break;
        }
    }
    if (state->app == app) {
        state->app = -1;
        for (unsigned i = state->z_count; i > 0; --i) {
            int candidate = state->z_order[i - 1];
            if ((state->minimized_apps & (1u << candidate)) == 0) {
                state->app = candidate;
                break;
            }
        }
        state->screen = state->app >= 0 ? SCREEN_APP : SCREEN_DESKTOP;
    }
    state->dragging_window = 0;
}

static int top_window_at(struct pixel_surface *s, struct desktop_state *state, int x, int y) {
    for (unsigned n = state->z_count; n > 0; --n) {
        int app = state->z_order[n - 1];
        if (state->minimized_apps & (1u << app)) continue;
        uint64_t wx, wy, ww, wh;
        window_bounds(s, state, app, &wx, &wy, &ww, &wh);
        if ((uint64_t)x >= wx && (uint64_t)x < wx + ww &&
            (uint64_t)y >= wy && (uint64_t)y < wy + wh) return app;
    }
    return -1;
}

static int taskbar_app_at(struct pixel_surface *s, const struct desktop_state *state, int x, int y) {
    uint64_t bar_y = s->height > 44 ? s->height - 44 : 0;
    uint64_t clock_x = s->width > 150 ? s->width - 146 : 4;
    if ((uint64_t)y < bar_y + 6 || (uint64_t)y >= bar_y + 38 || x < 106 || (uint64_t)x >= clock_x)
        return -1;
    unsigned count = state->task_count;
    if (!count) return -1;
    uint64_t area = clock_x > 108 ? clock_x - 108 : 0;
    uint64_t width = area / count;
    if (width > 118) width = 118;
    if (width < 70) width = 70;
    unsigned index = (unsigned)(((uint64_t)x - 106) / width);
    if (index >= count || (uint64_t)x >= 106 + (uint64_t)count * width) return -1;
    return state->task_order[index];
}

static void handle_app_key(struct pixel_surface *s, struct desktop_state *state, int key) {
    if (key == KEY_MAXIMIZE) { toggle_maximize(s, state, state->app); return; }
    if (state->note_save_dialog) {
        if (key == 'y' || key == 'Y') note_save_and_close(state);
        else if (key == 'n' || key == 'N') note_discard_and_close(state);
        else if (key == 'c' || key == 'C' || key == KEY_ESCAPE) state->note_save_dialog = 0;
        return;
    }
    if (key == KEY_F1 && state->app != APP_HELP) { open_app(state, APP_HELP); return; }
    if (state->app == APP_NOTEPAD) {
        if (key == KEY_ENTER) {
            if (state->note_row < 5) {
                ++state->note_row;
                state->note_column = 0;
                while (state->note_column < 55 && state->note[state->note_row][state->note_column])
                    ++state->note_column;
                state->note_dirty = 1;
            }
        } else if (key == KEY_BACKSPACE) {
            if (state->note_column) {
                state->note[ state->note_row ][--state->note_column] = '\0';
                state->note_dirty = 1;
            } else if (state->note_row > 0) {
                unsigned previous = state->note_row - 1;
                unsigned end = 0;
                while (end < 55 && state->note[previous][end]) ++end;
                unsigned col = 0;
                while (col < 55 && state->note[state->note_row][col] && end < 55)
                    state->note[previous][end++] = state->note[state->note_row][col++];
                state->note[previous][end] = '\0';
                for (unsigned row = state->note_row; row < 5; ++row)
                    for (unsigned c = 0; c < 56; ++c)
                        state->note[row][c] = state->note[row + 1][c];
                for (unsigned c = 0; c < 56; ++c) state->note[5][c] = '\0';
                state->note_row = previous;
                state->note_column = end;
                state->note_dirty = 1;
            }
        } else if (key == KEY_TAB) {
            for (unsigned i = 0; i < 4 && state->note_column < 55; ++i) {
                state->note[state->note_row][state->note_column++] = ' ';
                state->note[state->note_row][state->note_column] = '\0';
            }
            state->note_dirty = 1;
        } else if (key >= 32 && key <= 126 && state->note_column < 54) {
            state->note[state->note_row][state->note_column++] = (char)key;
            state->note[state->note_row][state->note_column] = '\0';
            state->note_dirty = 1;
        }
    } else if (state->app == APP_SETTINGS) {
        if (key == 't' || key == 'T') state->alternate_theme = !state->alternate_theme;
    } else if (state->app == APP_CALCULATOR) {
        calculator_key(state, key);
    }
}

static void handle_key(struct pixel_surface *s, struct desktop_state *state, int key) {
    if (!key) return;
    if (state->screen == SCREEN_APP) { handle_app_key(s, state, key); return; }
    if (state->screen == SCREEN_START_MENU) {
        if (key == KEY_ESCAPE || key == 's' || key == 'S') state->screen = SCREEN_DESKTOP;
        else if (key == KEY_UP) state->menu_selected = (state->menu_selected + APP_COUNT - 1) % APP_COUNT;
        else if (key == KEY_DOWN) state->menu_selected = (state->menu_selected + 1) % APP_COUNT;
        else if (key == KEY_ENTER) open_app(state, state->menu_selected);
        else if (key >= '1' && key <= '5') open_app(state, key - '1');
        return;
    }
    if (key == 's' || key == 'S') { state->screen = SCREEN_START_MENU; return; }
    if (key == KEY_F1) { open_app(state, APP_HELP); return; }
    if (key == KEY_UP || key == KEY_LEFT)
        state->selected = (state->selected + APP_COUNT - 1) % APP_COUNT;
    else if (key == KEY_DOWN || key == KEY_RIGHT)
        state->selected = (state->selected + 1) % APP_COUNT;
    else if (key == KEY_ENTER && state->selected >= 0) open_app(state, state->selected);
    else if (key >= '1' && key <= '5') open_app(state, key - '1');
    state->selected_mask = state->selected >= 0 ? 1u << state->selected : 0;
}

static void render_scene(struct pixel_surface *s, const struct desktop_state *state) {
    cursor_drawn = 0;
    draw_desktop(s, state);
    for (unsigned i = 0; i < state->z_count; ++i)
        if ((state->minimized_apps & (1u << state->z_order[i])) == 0)
            draw_app(s, state, state->z_order[i]);
    draw_taskbar(s, state);
    draw_start_menu(s, state);
}

static volatile uint32_t *framebuffer_pixel(struct pixel_surface *s, int x, int y) {
    return (volatile uint32_t *)((volatile uint8_t *)s->pixels + (uint64_t)y * s->pitch) + x;
}

static void cursor_hide(struct pixel_surface *s) {
    if (!cursor_drawn) return;
    for (int row = 0; row < 16; ++row) {
        for (int col = 0; col < 12; ++col) {
            int x = cursor_draw_x + col, y = cursor_draw_y + row;
            if (x >= 0 && y >= 0 && (uint64_t)x < s->width && (uint64_t)y < s->height)
                *framebuffer_pixel(s, x, y) = cursor_under[row][col];
        }
    }
    present_rect(s, cursor_draw_x, cursor_draw_y, 12, 16);
    cursor_drawn = 0;
}

static void cursor_show(struct pixel_surface *s) {
    if (!mouse_available) return;
    static const char *shape[] = {
        "X...........", "XX..........", "X.X.........", "X..X........",
        "X...X.......", "X....X......", "X.....X.....", "X......X....",
        "X.......X...", "X....XXXXX..", "X..X..X.....", "X.X.X..X....",
        "XX..X..X....", ".X...X..X...", "......XX....", "......XX...."
    };
    static const unsigned char white_pixels[][2] = {
        {1,1},{1,2},{2,2},{1,3},{2,3},{3,3},{1,4},{2,4},{3,4},{4,4},
        {1,5},{2,5},{3,5},{4,5},{5,5},{1,6},{2,6},{3,6},{4,6},{5,6},{6,6},
        {1,7},{2,7},{3,7},{4,7},{5,7},{6,7},{7,7},{1,8},{2,8},{3,8},{4,8},{5,8},{6,8},{7,8},
        {1,9},{2,9},{3,9},{4,9},{5,9},{6,9},{7,9},{5,10},{6,10},{7,10},{4,11},{5,11},{6,11},
        {5,12},{6,12},{7,12},{6,13},{7,13},{7,14}
    };
    cursor_draw_x = mouse_x;
    cursor_draw_y = mouse_y;
    for (int row = 0; row < 16; ++row) {
        for (int col = 0; col < 12; ++col) {
            int x = cursor_draw_x + col, y = cursor_draw_y + row;
            cursor_under[row][col] = (x >= 0 && y >= 0 && (uint64_t)x < s->width && (uint64_t)y < s->height)
                ? *framebuffer_pixel(s, x, y) : 0;
            if (x >= 0 && y >= 0 && (uint64_t)x < s->width && (uint64_t)y < s->height && shape[row][col] == 'X')
                *framebuffer_pixel(s, x, y) = colour(s, 0, 0, 0);
        }
    }
    for (unsigned i = 0; i < sizeof(white_pixels) / sizeof(white_pixels[0]); ++i) {
        int x = cursor_draw_x + white_pixels[i][0], y = cursor_draw_y + white_pixels[i][1];
        if (x >= 0 && y >= 0 && (uint64_t)x < s->width && (uint64_t)y < s->height)
            *framebuffer_pixel(s, x, y) = colour(s, 255, 255, 255);
    }
    cursor_drawn = 1;
    present_rect(s, cursor_draw_x, cursor_draw_y, 12, 16);
}

static int handle_mouse_motion(struct pixel_surface *s, struct desktop_state *state) {
    uint64_t x = (uint64_t)mouse_x, y = (uint64_t)mouse_y;
    if (state->screen == SCREEN_APP && state->resizing_window) {
        int app = state->drag_app;
        uint64_t old_x, old_y, old_w, old_h;
        window_bounds(s, state, app, &old_x, &old_y, &old_w, &old_h);
        int64_t new_w = (int64_t)state->resize_width + mouse_x - state->resize_mouse_x;
        int64_t new_h = (int64_t)state->resize_height + mouse_y - state->resize_mouse_y;
        static const uint64_t min_width[APP_COUNT] = { 400, 440, 440, 360, 480 };
        static const uint64_t min_height[APP_COUNT] = { 360, 340, 410, 280, 480 };
        uint64_t min_w = min_width[app];
        uint64_t min_h = min_height[app];
        uint64_t max_w = s->width > 8 ? s->width - 8 : s->width;
        uint64_t max_h = s->height > 48 ? s->height - 48 : s->height;
        if (app == APP_CALCULATOR) {
            state->resizing_window = 0;
            return 0;
        }
        if (new_w < (int64_t)min_w) new_w = (int64_t)min_w;
        if ((uint64_t)new_w > max_w) new_w = (int64_t)max_w;
        if (new_h < (int64_t)min_h) new_h = (int64_t)min_h;
        if ((uint64_t)new_h > max_h) new_h = (int64_t)max_h;
        state->window_width[app] = (uint64_t)new_w;
        state->window_height[app] = (uint64_t)new_h;
        state->window_dx[app] = (int)old_x - (int)((s->width - (uint64_t)new_w) / 2);
        state->window_dy[app] = (int)old_y - (int)((s->height - (uint64_t)new_h) / 2);
        return 1;
    }
    if (state->screen == SCREEN_APP && state->dragging_window) {
        int app = state->drag_app;
        if (state->maximized_apps & (1u << app)) return 0;
        uint64_t current_x, current_y, current_w, current_h;
        window_bounds(s, state, app, &current_x, &current_y, &current_w, &current_h);
        int base_x = (int)(s->width - current_w) / 2;
        int base_y = (int)(s->height - current_h) / 2;
        state->window_dx[app] = state->drag_window_dx + mouse_x - state->drag_mouse_x;
        state->window_dy[app] = state->drag_window_dy + mouse_y - state->drag_mouse_y;
        uint64_t wx, wy, ww, wh;
        window_bounds(s, state, app, &wx, &wy, &ww, &wh);
        state->window_dx[app] = (int)wx - base_x;
        state->window_dy[app] = (int)wy - base_y;
        return 1;
    }
    if (state->screen == SCREEN_DESKTOP && state->selecting_box) {
        state->selection_end_x = mouse_x;
        state->selection_end_y = mouse_y;
        int x1 = state->selection_start_x < state->selection_end_x ? state->selection_start_x : state->selection_end_x;
        int x2 = state->selection_start_x > state->selection_end_x ? state->selection_start_x : state->selection_end_x;
        int y1 = state->selection_start_y < state->selection_end_y ? state->selection_start_y : state->selection_end_y;
        int y2 = state->selection_start_y > state->selection_end_y ? state->selection_start_y : state->selection_end_y;
        uint32_t mask = 0;
        for (int i = 0; i < APP_COUNT; ++i) {
            int ix1 = 18, iy1 = 52 + i * 96, ix2 = 136, iy2 = iy1 + 60;
            if (x1 <= ix2 && x2 >= ix1 && y1 <= iy2 && y2 >= iy1) mask |= 1u << i;
        }
        state->selected_mask = mask;
        for (int i = 0; i < APP_COUNT; ++i)
            if (mask & (1u << i)) state->selected = i;
        return 1;
    }
    if (state->screen == SCREEN_START_MENU) {
        uint64_t menu_h = APP_COUNT * 38 + 38;
        uint64_t menu_y = s->height > menu_h + 42 ? s->height - menu_h - 42 : 4;
        for (int i = 0; i < APP_COUNT; ++i) {
            uint64_t row_y = menu_y + 8 + (uint64_t)i * 38;
            if (x >= 62 && x < 268 && y >= row_y && y < row_y + 34) {
                if (state->menu_selected != i) { state->menu_selected = i; return 1; }
                return 0;
            }
        }
    }
    return 0;
}

static int desktop_icon_at(int x, int y) {
    if (x < 18 || x >= 136 || y < 52) return -1;
    int icon = (y - 52) / 96;
    if (icon >= 0 && icon < APP_COUNT && y < 52 + icon * 96 + 60) return icon;
    return -1;
}

static void capture_press(struct desktop_state *state, int control, int app, int value,
                          uint64_t x, uint64_t y, uint64_t w, uint64_t h) {
    state->pressed_control = control;
    state->pressed_app = app;
    state->pressed_value = value;
    state->press_x = mouse_x;
    state->press_y = mouse_y;
    state->press_tsc = read_tsc();
    state->press_rect_x = x;
    state->press_rect_y = y;
    state->press_rect_w = w;
    state->press_rect_h = h;
}

static void handle_mouse_press(struct pixel_surface *s, struct desktop_state *state) {
    uint64_t x = (uint64_t)mouse_x, y = (uint64_t)mouse_y;
    state->pressed_control = PRESS_NONE;
    uint64_t bar_y = s->height > 44 ? s->height - 44 : 0;
    if (y >= bar_y) {
        if (x < 100) {
            capture_press(state, PRESS_START, -1, 0, 5, bar_y + 6, 90, 32);
            return;
        }
        int task_app = taskbar_app_at(s, state, mouse_x, mouse_y);
        if (task_app >= 0) {
            unsigned index = 0;
            while (index < state->task_count && state->task_order[index] != task_app) ++index;
            uint64_t clock_x = s->width > 150 ? s->width - 146 : 4;
            uint64_t width = (clock_x > 108 ? clock_x - 108 : 0) / state->task_count;
            if (width > 118) width = 118;
            if (width < 70) width = 70;
            uint64_t bx = 106 + (uint64_t)index * width;
            capture_press(state, PRESS_TASKBAR, task_app, 0, bx, bar_y + 6, width - 3, 32);
            return;
        }
        return;
    }
    if (state->screen == SCREEN_START_MENU) {
        uint64_t menu_h = APP_COUNT * 38 + 38;
        uint64_t menu_y = s->height > menu_h + 42 ? s->height - menu_h - 42 : 4;
        for (int i = 0; i < APP_COUNT; ++i) {
            uint64_t row_y = menu_y + 8 + (uint64_t)i * 38;
            if (x >= 62 && x < 268 && y >= row_y && y < row_y + 34) {
                state->menu_selected = i;
                capture_press(state, PRESS_MENU_APP, -1, i, 62, row_y, 206, 34);
                return;
            }
        }
        capture_press(state, PRESS_MENU_DISMISS, -1, 0, 0, 0, s->width, s->height);
        return;
    }
    int hit_app = top_window_at(s, state, mouse_x, mouse_y);
    if (hit_app >= 0) {
        open_app(state, hit_app);
        uint64_t wx, wy, w, h;
        window_bounds(s, state, hit_app, &wx, &wy, &w, &h);
        if (hit_app == APP_NOTEPAD && state->note_save_dialog) {
            uint64_t dx = wx + w / 2 - 190, dy = wy + h / 2 - 76;
            if (y >= dy + 91 && y < dy + 125 && x >= dx + 22 && x < dx + 122)
                capture_press(state, PRESS_NOTE_SAVE, hit_app, 0, dx + 22, dy + 91, 100, 34);
            else if (y >= dy + 91 && y < dy + 125 && x >= dx + 138 && x < dx + 238)
                capture_press(state, PRESS_NOTE_DISCARD, hit_app, 0, dx + 138, dy + 91, 100, 34);
            else if (y >= dy + 91 && y < dy + 125 && x >= dx + 254 && x < dx + 354)
                capture_press(state, PRESS_NOTE_CANCEL, hit_app, 0, dx + 254, dy + 91, 100, 34);
            return;
        }
        if (x >= wx + w - 31 && x < wx + w - 9 && y >= wy + 10 && y < wy + 30) {
            capture_press(state, PRESS_WINDOW_CLOSE, hit_app, 0, wx + w - 31, wy + 10, 22, 20);
        } else if (x >= wx + w - 83 && x < wx + w - 61 && y >= wy + 10 && y < wy + 30) {
            capture_press(state, PRESS_WINDOW_MINIMIZE, hit_app, 0, wx + w - 83, wy + 10, 22, 20);
        } else if (x >= wx + w - 57 && x < wx + w - 35 && y >= wy + 10 && y < wy + 30) {
            capture_press(state, PRESS_WINDOW_MAXIMIZE, hit_app, 0, wx + w - 57, wy + 10, 22, 20);
        } else if (x >= wx + 7 && x < wx + w - 83 && y >= wy + 7 && y < wy + 35) {
            state->dragging_window = 1;
            state->drag_app = hit_app;
            state->drag_mouse_x = mouse_x;
            state->drag_mouse_y = mouse_y;
            state->drag_window_dx = state->window_dx[hit_app];
            state->drag_window_dy = state->window_dy[hit_app];
        } else if (hit_app != APP_CALCULATOR &&
               (state->maximized_apps & (1u << hit_app)) == 0 &&
               x >= wx + w - 20 && y >= wy + h - 20) {
            state->resizing_window = 1;
            state->drag_app = hit_app;
            state->resize_mouse_x = mouse_x;
            state->resize_mouse_y = mouse_y;
            state->resize_width = w;
            state->resize_height = h;
        } else if (hit_app == APP_CALCULATOR) {
            uint64_t tx = wx + 12 + (w - 24 - 262) / 2;
            uint64_t buttons_y = wy + 54 + 32 + 56;
            if (x >= tx && x < tx + 4 * 68 && y >= buttons_y && y < buttons_y + 4 * 50) {
                unsigned col = (unsigned)(x - tx) / 68;
                unsigned row = (unsigned)(y - buttons_y) / 50;
                if (col < 4 && row < 4) {
                    capture_press(state, PRESS_CALCULATOR_KEY, hit_app, (int)(row * 4 + col),
                                  tx + col * 68, buttons_y + row * 50, 58, 42);
                }
            }
        }
        return;
    }
    int icon = desktop_icon_at(mouse_x, mouse_y);
    if (icon >= 0) {
        state->selected = icon;
        state->selected_mask = 1u << icon;
        capture_press(state, PRESS_DESKTOP_ICON, icon, 0, 18, 52 + icon * 96, 118, 60);
        return;
    }
    state->selected = -1;
    state->selected_mask = 0;
    state->selecting_box = 1;
    state->selection_start_x = mouse_x;
    state->selection_start_y = mouse_y;
    state->selection_end_x = mouse_x;
    state->selection_end_y = mouse_y;
    if (state->screen == SCREEN_APP) state->screen = SCREEN_DESKTOP;
}

static void handle_mouse_release(struct pixel_surface *s, struct desktop_state *state) {
    (void)s;
    state->dragging_window = 0;
    state->resizing_window = 0;
    if (state->selecting_box) {
        state->selection_end_x = mouse_x;
        state->selection_end_y = mouse_y;
        state->selecting_box = 0;
    }
    int control = state->pressed_control;
    state->pressed_control = PRESS_NONE;
    if (control == PRESS_NONE) return;

    uint64_t now = read_tsc();
    uint64_t held = now - state->press_tsc;
    uint64_t long_press_limit = tsc_ticks_per_second / 2u;
    if (long_press_limit && held > long_press_limit) return;
    if (mouse_x < 0 || mouse_y < 0 || (uint64_t)mouse_x < state->press_rect_x ||
        (uint64_t)mouse_y < state->press_rect_y ||
        (uint64_t)mouse_x >= state->press_rect_x + state->press_rect_w ||
        (uint64_t)mouse_y >= state->press_rect_y + state->press_rect_h) return;

    /* Keep a quick click visibly pressed for at least 60 ms before activation. */
    uint64_t minimum_press = tsc_ticks_per_second / 16u;
    uint64_t release_after = state->press_tsc + minimum_press;
    while (minimum_press && read_tsc() < release_after) __asm__ volatile ("pause");

    switch (control) {
        case PRESS_START:
            state->screen = state->screen == SCREEN_START_MENU
                ? (state->app >= 0 ? SCREEN_APP : SCREEN_DESKTOP) : SCREEN_START_MENU;
            break;
        case PRESS_TASKBAR:
            if (state->screen == SCREEN_APP && state->app == state->pressed_app &&
                (state->minimized_apps & (1u << state->pressed_app)) == 0)
                minimize_app(state, state->pressed_app);
            else
                open_app(state, state->pressed_app);
            break;
        case PRESS_MENU_APP:
            open_app(state, state->pressed_value);
            break;
        case PRESS_MENU_DISMISS:
            state->screen = state->app >= 0 ? SCREEN_APP : SCREEN_DESKTOP;
            break;
        case PRESS_WINDOW_CLOSE:
            if (state->pressed_app == APP_NOTEPAD && state->note_dirty)
                state->note_save_dialog = 1;
            else
                close_app(state, state->pressed_app);
            break;
        case PRESS_WINDOW_MINIMIZE:
            minimize_app(state, state->pressed_app);
            break;
        case PRESS_WINDOW_MAXIMIZE:
            toggle_maximize(s, state, state->pressed_app);
            break;
        case PRESS_CALCULATOR_KEY: {
            static const int keys[16] = {
                '7','8','9','/', '4','5','6','*',
                '1','2','3','-', 'c','0','=','+'
            };
            if (state->pressed_value >= 0 && state->pressed_value < 16)
                calculator_key(state, keys[state->pressed_value]);
            break;
        }
        case PRESS_NOTE_SAVE:
            note_save_and_close(state);
            break;
        case PRESS_NOTE_DISCARD:
            note_discard_and_close(state);
            break;
        case PRESS_NOTE_CANCEL:
            state->note_save_dialog = 0;
            break;
        case PRESS_DESKTOP_ICON: {
            uint64_t double_click_ticks = tsc_ticks_per_second / 2u;
            if (state->last_clicked_icon == state->pressed_app && double_click_ticks &&
                now - state->last_click_tsc <= double_click_ticks) {
                state->last_clicked_icon = -1;
                open_app(state, state->pressed_app);
            } else {
                state->last_clicked_icon = state->pressed_app;
                state->last_click_tsc = now;
            }
            break;
        }
        default:
            break;
    }
}

__attribute__((noreturn)) void _start(void) {
    struct limine_framebuffer_response *response = framebuffer_request.response;
    if (!LIMINE_BASE_REVISION_SUPPORTED(base_revision) || !response ||
        response->framebuffer_count == 0 || response->framebuffers == NULL) {
        for (;;) __asm__ volatile ("hlt");
    }
    struct limine_framebuffer *fb = response->framebuffers[0];
    if (!fb || !fb->address || fb->bpp != 32 || fb->memory_model != 1) {
        for (;;) __asm__ volatile ("hlt");
    }
    struct pixel_surface surface = {
        .pixels = (volatile uint32_t *)fb->address,
        .display_pixels = (volatile uint32_t *)fb->address,
        .width = fb->width,
        .height = fb->height,
        .pitch = fb->pitch,
        .display_pitch = fb->pitch,
        .r_size = fb->red_mask_size, .r_shift = fb->red_mask_shift,
        .g_size = fb->green_mask_size, .g_shift = fb->green_mask_shift,
        .b_size = fb->blue_mask_size, .b_shift = fb->blue_mask_shift
    };
    if (surface.width <= BACKBUFFER_MAX_WIDTH && surface.height <= BACKBUFFER_MAX_HEIGHT) {
        surface.pixels = backbuffer;
        surface.pitch = surface.width * sizeof(uint32_t);
        surface.buffered = 1;
    }
    boot_cmdline = (cmdline_request.response && cmdline_request.response->cmdline)
        ? cmdline_request.response->cmdline : "";
    static struct desktop_state desktop;
    desktop.screen = SCREEN_DESKTOP;
    desktop.selected = -1;
    desktop.app = -1;
    desktop.last_clicked_icon = -1;
    mouse_initialize(surface.width, surface.height);
    (void)rtc_refresh();
    render_scene(&surface, &desktop);
    present(&surface);
    cursor_show(&surface);
    uint64_t clock_frequency = (tsc_frequency_request.response != NULL)
        ? tsc_frequency_request.response->frequency : 0;
    tsc_ticks_per_second = clock_frequency;
    uint64_t clock_interval = clock_frequency / 4u;
    uint64_t input_interval = clock_frequency / 250u;
    uint64_t drag_redraw_interval = clock_frequency / 60u;
    uint64_t next_clock_update = read_tsc();
    uint64_t next_input_poll = next_clock_update;
    uint64_t next_drag_redraw = next_clock_update;
    for (;;) {
        uint64_t now = read_tsc();
        if (input_interval && now < next_input_poll) {
            __asm__ volatile ("pause");
            continue;
        }
        next_input_poll = now + input_interval;
        uint8_t old_buttons = previous_mouse_buttons;
        int pointer_changed = mouse_poll(&surface);
        if (pointer_changed) cursor_hide(&surface);
        int key = keyboard_poll();
        int dirty = 0;
        if (key) { handle_key(&surface, &desktop, key); dirty = 1; }
        if (pointer_changed) dirty = handle_mouse_motion(&surface, &desktop) || dirty;
        if ((mouse_buttons & 1u) && !(old_buttons & 1u)) {
            handle_mouse_press(&surface, &desktop);
            dirty = 1;
        }
        if (!(mouse_buttons & 1u) && (old_buttons & 1u)) {
            handle_mouse_release(&surface, &desktop);
            dirty = 1;
        }
        int clock_dirty = 0;
        if (clock_interval == 0 || now >= next_clock_update) {
            if (rtc_refresh()) clock_dirty = 1;
            next_clock_update = now + (clock_interval ? clock_interval : 1u);
        }
        int button_edge = ((mouse_buttons ^ old_buttons) & 1u) != 0;
        if (dirty && (desktop.dragging_window || desktop.selecting_box) &&
            pointer_changed && !key && !button_edge && now < next_drag_redraw) {
            dirty = 0;
        } else if (dirty) {
            render_scene(&surface, &desktop);
            present(&surface);
            next_drag_redraw = now + drag_redraw_interval;
        }
        if (clock_dirty && !dirty) {
            if (cursor_drawn) cursor_hide(&surface);
            draw_clock(&surface);
            uint64_t clock_x = clock_panel_x(&surface);
            uint64_t bar_y = surface.height > 44 ? surface.height - 44 : 0;
            present_rect(&surface, clock_x, bar_y + 4, 142, 36);
        }
        if (dirty || pointer_changed || clock_dirty) cursor_show(&surface);
        if (!dirty && !clock_dirty) {
            __asm__ volatile ("pause");
        }
    }
}
