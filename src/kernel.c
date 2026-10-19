#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

extern const unsigned char _binary_image_rgb_start[];
extern const unsigned char _binary_image_rgb_end[];

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define WHITE_ON_BLACK 0x0F

// Generic Framebuffer Structure
struct framebuffer {
    void *address;
    uint64_t width, height, pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift;
    uint8_t green_mask_size, green_mask_shift;
    uint8_t blue_mask_size, blue_mask_shift;
};

static struct framebuffer *fb = NULL;
static uint16_t *vga_buffer = (uint16_t *)VGA_ADDRESS;
static int vga_col = 0;
static int vga_row = 0;

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

static void draw_block(int x, int y, int width, int height, uint32_t color) {
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

// Basic VGA Text Mode Print Implementation
static void kprint(const char *s) {
    if (!s) return;
    while (*s) {
        if (*s == '\n') {
            vga_col = 0;
            vga_row++;
        } else {
            const size_t index = vga_row * VGA_WIDTH + vga_col;
            vga_buffer[index] = (uint16_t)((WHITE_ON_BLACK << 8) | (unsigned char)*s);
            vga_col++;
            if (vga_col >= VGA_WIDTH) {
                vga_col = 0;
                vga_row++;
            }
        }
        s++;
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

            while (i) {
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
    // Note: Initialize your `fb` structure here using your alternative bootloader 
    // or architecture-specific setup (e.g., Multiboot info structure).

    if (fb) {
        kprintf("Resolution: %u x %u\n",
                (unsigned int)fb->width,
                (unsigned int)fb->height);

        draw_block(0, 0, 1920, 1080, rgb(78, 255, 128));
        draw_block(0, 0, 1920, 800, rgb(0, 255, 247));
        kprintf("Graphics initialized.\n");
    }

    kprintf("Hello, MikeyOS!\n");
    kprintf("Kernel started successfully.\n");
    kprintf("Framebuffer initialization follows...\n");

    stop();
}