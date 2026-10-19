#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
extern const unsigned char _binary_image_rgb_start[];
extern const unsigned char _binary_image_rgb_end[];

#define REQUEST_START \
    0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, \
    0x785c6ed015d3e316, 0x181e920a7852b9d9

#define REQUEST_END \
    0xadc0e0531bb10d03, 0x9572709f31764c62
#define VGA_ADDRESS 0xB8000

#define WHITE_ON_BLACK 0x0F
struct limine_framebuffer {
    void *address;
    uint64_t width, height, pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift;
    uint8_t green_mask_size, green_mask_shift;
    uint8_t blue_mask_size, blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;
    uint64_t mode_count;
    void *modes;
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

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t request_start[] = { REQUEST_START };

__attribute__((used, section(".limine_requests")))
static volatile uint64_t base_revision[] = {
    0xf9562b2d5c95a6c8,
    0x6a7b384944536bdc,
    3
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = {
        0xc7b1dd30df4c8b88,
        0x0a82e883a194f07b,
        0x9d5827dcd881dd75,
        0xa3148604f6fab11b
    },
    .revision = 0,
    .response = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t request_end[] = { REQUEST_END };
static struct limine_framebuffer *fb;

static void stop(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << fb->red_mask_shift)
         | ((uint32_t)g << fb->green_mask_shift)
         | ((uint32_t)b << fb->blue_mask_shift);
}

static void put_pixel(int x, int y, uint32_t color) {
    if (!fb || x < 0 || y < 0 ||
        (uint64_t)x >= fb->width ||
        (uint64_t)y >= fb->height)
        return;

    volatile uint32_t *row =
        (volatile uint32_t *)((uintptr_t)fb->address
                            + (uint64_t)y * fb->pitch);

    row[x] = color;
}

static void draw_block(int x, int y, int width, int height,
                       uint32_t color) {
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            put_pixel(x + dx, y + dy, color);
        }
    }
}

static void clear_screen(uint32_t color) {
    for (uint64_t y = 0; y < fb->height; y++) {
        for (uint64_t x = 0; x < fb->width; x++) {
            put_pixel((int)x, (int)y, color);
        }
    }
}
struct limine_terminal;

struct limine_terminal {
    struct limine_framebuffer *framebuffer;
    void (*write)(struct limine_terminal *,
                  const char *, uint64_t);
    void *write_ctx;
};

struct limine_terminal_response {
    uint64_t revision;
    uint64_t terminal_count;
    struct limine_terminal **terminals;
};

struct limine_terminal_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_terminal_response *response;
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_terminal_request terminal_request = {
    .id = {
        0xc8ac59310c2b0844,
        0xa68d0c7265d38878
    },
    .revision = 0,
    .response = 0
};

static struct limine_terminal *terminal;

static void kprint(const char *s) {
    if (terminal && terminal->write && s) {
        uint64_t len = 0;
        while (s[len])
            len++;
        terminal->write(terminal, s, len);
    }
}

static void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            char c[2] = {*fmt++, '\0'};
            kprint(c);
            continue;
        }

        fmt++;

        if (*fmt == '%') {
            kprint("%");
            fmt++;
        } else if (*fmt == 's') {
            const char *s = va_arg(args, const char *);
            kprint(s ? s : "(null)");
            fmt++;
        } else if (*fmt == 'c') {
            char c[2] = {(char)va_arg(args, int), '\0'};
            kprint(c);
            fmt++;
        } else if (*fmt == 'd' || *fmt == 'i') {
            int n = va_arg(args, int);
            unsigned int v;
            if (n < 0) {
                kprint("-");
                v = 0u - (unsigned int)n;
            } else {
                v = (unsigned int)n;
            }

            char buf[16];
            int i = 0;
            do {
                buf[i++] = '0' + v % 10;
                v /= 10;
            } while (v && i < 15);

            while (i)
            {
                char c[2] = {buf[--i], '\0'};
                kprint(c);
            }
            fmt++;
        } else if (*fmt == 'u' || *fmt == 'x') {
            unsigned int v = va_arg(args, unsigned int);
            unsigned int base = (*fmt == 'x') ? 16 : 10;
            const char *digits = "0123456789abcdef";
            char buf[16];
            int i = 0;

            do {
                buf[i++] = digits[v % base];
                v /= base;
            } while (v && i < 15);

            while (i) {
                char c[2] = {buf[--i], '\0'};
                kprint(c);
            }
            fmt++;
        } else {
            kprint("%");
            if (*fmt) {
                char c[2] = {*fmt++, '\0'};
                kprint(c);
            }
        }
    }

    va_end(args);
}

void kernel_main(void) {
    if (!terminal_request.response ||
        terminal_request.response->terminal_count == 0)

    terminal = terminal_request.response->terminals[0];

    if (!fb_request.response ||
        fb_request.response->framebuffer_count == 0)

    fb = fb_request.response->framebuffers[0];

    if (!fb || !fb->address || fb->bpp != 32 ||
        fb->memory_model != 1)

    kprintf("Resolution: %u x %u\n",
            (unsigned int)fb->width,
            (unsigned int)fb->height);

    draw_block(0, 0, 1920, 1080, rgb(78, 255, 128));
    draw_block(0, 0, 1920, 800, rgb(0, 255, 247));

    kprintf("Graphics initialized.\n");
    kprintf("Hello, MikeyOS!\n");
    kprintf("Kernel started successfully.\n");
    kprintf("Framebuffer initialization follows...\n");

    stop();
}
