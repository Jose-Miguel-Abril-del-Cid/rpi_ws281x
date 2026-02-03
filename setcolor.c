#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include "ws2811.h"

// ====== AJUSTA ESTO ======
#define LED_COUNT   11          // <-- tu cantidad real de LEDs
#define LED_GPIO    18         // GPIO 18 (PWM)
#define LED_DMA     10
#define LED_INVERT  0
#define LED_BRIGHTNESS  255
// =========================

static ws2811_t ledstring =
{
    .freq = WS2811_TARGET_FREQ,
    .dmanum = LED_DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = LED_GPIO,
            .invert = LED_INVERT,
            .count = LED_COUNT,
            .strip_type = WS2811_STRIP_GRB,  // WS2812B típico
            .brightness = LED_BRIGHTNESS,
        }
    },
};

static void cleanup(int signum)
{
    for (int i = 0; i < LED_COUNT; i++) {
        ledstring.channel[0].leds[i] = 0;
    }
    ws2811_render(&ledstring);
    ws2811_fini(&ledstring);
    _exit(0);
}

static uint32_t make_color(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Uso: %s R G B (0-255)\n", argv[0]);
        return 2;
    }

    int r = atoi(argv[1]);
    int g = atoi(argv[2]);
    int b = atoi(argv[3]);

    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        fprintf(stderr, "RGB fuera de rango 0-255\n");
        return 2;
    }

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    ws2811_return_t ret = ws2811_init(&ledstring);
    if (ret != WS2811_SUCCESS) {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        return 1;
    }

    uint32_t c = make_color((uint8_t)r, (uint8_t)g, (uint8_t)b);
    for (int i = 0; i < LED_COUNT; i++) {
        ledstring.channel[0].leds[i] = c;
    }

    ret = ws2811_render(&ledstring);
    if (ret != WS2811_SUCCESS) {
        fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
        ws2811_fini(&ledstring);
        return 1;
    }

    ws2811_fini(&ledstring);
    return 0;
}
