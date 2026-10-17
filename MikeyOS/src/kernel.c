
#include <stdint.h>
#include <stddef.h>
extern const unsigned char _binary_image_rgb_start[];
extern const unsigned char _binary_image_rgb_end[];

#define REQUEST_START \
    0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf, \
    0x785c6ed015d3e316, 0x181e920a7852b9d9

#define REQUEST_END \
    0xadc0e0531bb10d03, 0x9572709f31764c62

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

static void draw_rgb_image(int x, int y) {
    const unsigned char *data = _binary_image_rgb_start;

    for (int py = 0; py < 1080; py++) {
        for (int px = 0; px < 1920; px++) {
            unsigned int i = (py * 1920 + px) * 3;

            unsigned char r = data[i];
            unsigned char g = data[i + 1];
            unsigned char b = data[i + 2];

            put_pixel(x + px, y + py, rgb(r, g, b));
        }
    }
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

void kernel_main(void) {
    if (!fb_request.response ||
        fb_request.response->framebuffer_count == 0)
        stop();

    fb = fb_request.response->framebuffers[0];

    if (!fb || !fb->address || fb->bpp != 32 ||
        fb->memory_model != 1)
        stop();

    clear_screen(rgb(12, 12, 28));

    draw_rgb_image(0, 0);

    stop();
}