#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ws2811.h"

#define DEFAULT_LED_COUNT 10
#define DEFAULT_LED_GPIO 18
#define DEFAULT_LED_DMA 10
#define DEFAULT_LED_INVERT 0
#define DEFAULT_LED_BRIGHTNESS 255
#define DEFAULT_STRIP_TYPE WS2811_STRIP_GRB

typedef enum {
    MODE_ALL,
    MODE_SKIP_ENDS,
    MODE_ALTERNATE,
    MODE_ALTERNATE_OFFSET
} led_mode_t;

typedef struct {
    int count;
    int gpio;
    int dma;
    int invert;
    int brightness;
    int strip_type;
    led_mode_t mode;
    int r;
    int g;
    int b;
    int turn_off;
    const char *mask;
    const char *only;
    const char *off_leds;
} led_options_t;

static ws2811_t ledstring;

static void usage(const char *prog)
{
    fprintf(stderr,
        "Uso:\n"
        "  %s R G B\n"
        "  %s --count N --rgb R G B [opciones]\n"
        "\n"
        "Opciones:\n"
        "  --config FILE              Lee configuracion key=value\n"
        "  --count N                  Cantidad fisica de LEDs\n"
        "  --gpio N                   GPIO de salida, default 18\n"
        "  --dma N                    Canal DMA, default 10\n"
        "  --brightness N             Brillo 0-255\n"
        "  --invert                   Invierte la salida\n"
        "  --strip TYPE               RGB, RBG, GRB, GBR, BRG, BGR, RGBW o GRBW\n"
        "  --rgb R G B                Color RGB 0-255\n"
        "  --hex RRGGBB               Color en hexadecimal\n"
        "  --off                      Apaga todos los LEDs\n"
        "  --mode MODE                all, skip-ends, alternate, alternate-offset\n"
        "  --mask 0101...             1 enciende, 0 apaga cada LED\n"
        "  --only LIST                Enciende solo indices/rangos, ej. 1,3,6-9\n"
        "  --off-leds LIST            Apaga indices/rangos despues del modo/mask\n",
        prog, prog);
}

static int parse_int(const char *text, int min, int max, const char *name, int *out)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (!text[0] || *end != '\0' || value < min || value > max) {
        fprintf(stderr, "%s fuera de rango (%d-%d): %s\n", name, min, max, text);
        return -1;
    }

    *out = (int)value;
    return 0;
}

static int parse_bool(const char *text, int *out)
{
    if (!strcmp(text, "1") || !strcmp(text, "true") || !strcmp(text, "yes") || !strcmp(text, "on")) {
        *out = 1;
        return 0;
    }
    if (!strcmp(text, "0") || !strcmp(text, "false") || !strcmp(text, "no") || !strcmp(text, "off")) {
        *out = 0;
        return 0;
    }
    return -1;
}

static int parse_mode(const char *text, led_mode_t *mode)
{
    if (!strcmp(text, "all")) {
        *mode = MODE_ALL;
    } else if (!strcmp(text, "skip-ends")) {
        *mode = MODE_SKIP_ENDS;
    } else if (!strcmp(text, "alternate")) {
        *mode = MODE_ALTERNATE;
    } else if (!strcmp(text, "alternate-offset")) {
        *mode = MODE_ALTERNATE_OFFSET;
    } else {
        fprintf(stderr, "Modo no soportado: %s\n", text);
        return -1;
    }
    return 0;
}

static int parse_strip_type(const char *text, int *strip_type)
{
    if (!strcmp(text, "RGB")) {
        *strip_type = WS2811_STRIP_RGB;
    } else if (!strcmp(text, "RBG")) {
        *strip_type = WS2811_STRIP_RBG;
    } else if (!strcmp(text, "GRB")) {
        *strip_type = WS2811_STRIP_GRB;
    } else if (!strcmp(text, "GBR")) {
        *strip_type = WS2811_STRIP_GBR;
    } else if (!strcmp(text, "BRG")) {
        *strip_type = WS2811_STRIP_BRG;
    } else if (!strcmp(text, "BGR")) {
        *strip_type = WS2811_STRIP_BGR;
    } else if (!strcmp(text, "RGBW")) {
        *strip_type = SK6812_STRIP_RGBW;
    } else if (!strcmp(text, "GRBW")) {
        *strip_type = SK6812_STRIP_GRBW;
    } else {
        fprintf(stderr, "Tipo de tira no soportado: %s\n", text);
        return -1;
    }
    return 0;
}

static int parse_hex_color(const char *text, led_options_t *opts)
{
    char *end = NULL;
    const char *hex = text;
    unsigned long value;

    if (hex[0] == '#') {
        hex++;
    }
    if (strlen(hex) != 6) {
        fprintf(stderr, "Color hex invalido: %s\n", text);
        return -1;
    }

    value = strtoul(hex, &end, 16);
    if (*end != '\0') {
        fprintf(stderr, "Color hex invalido: %s\n", text);
        return -1;
    }

    opts->r = (value >> 16) & 0xff;
    opts->g = (value >> 8) & 0xff;
    opts->b = value & 0xff;
    return 0;
}

static void trim(char *text)
{
    char *start = text;
    char *end;

    while (isspace((unsigned char)*start)) {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
}

static int apply_config_value(led_options_t *opts, const char *key, const char *value)
{
    if (!strcmp(key, "count")) {
        return parse_int(value, 1, 100000, "count", &opts->count);
    } else if (!strcmp(key, "gpio")) {
        return parse_int(value, 0, 31, "gpio", &opts->gpio);
    } else if (!strcmp(key, "dma")) {
        return parse_int(value, 0, 14, "dma", &opts->dma);
    } else if (!strcmp(key, "brightness")) {
        return parse_int(value, 0, 255, "brightness", &opts->brightness);
    } else if (!strcmp(key, "invert")) {
        return parse_bool(value, &opts->invert);
    } else if (!strcmp(key, "strip_type") || !strcmp(key, "strip")) {
        return parse_strip_type(value, &opts->strip_type);
    } else if (!strcmp(key, "mode")) {
        return parse_mode(value, &opts->mode);
    } else if (!strcmp(key, "mask")) {
        opts->mask = value;
    } else if (!strcmp(key, "only")) {
        opts->only = value;
    } else if (!strcmp(key, "off_leds") || !strcmp(key, "off-leds")) {
        opts->off_leds = value;
    }
    fprintf(stderr, "Clave de configuracion ignorada: %s\n", key);
    return 0;
}

static int load_config(led_options_t *opts, const char *path)
{
    FILE *file = fopen(path, "r");
    char line[512];
    static char config_values[16][256];
    static int config_value_count = 0;

    if (!file) {
        perror(path);
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        char *eq;
        char *key = line;
        char *value;

        trim(line);
        if (!line[0] || line[0] == '#') {
            continue;
        }

        eq = strchr(line, '=');
        if (!eq) {
            fprintf(stderr, "Linea de configuracion invalida: %s\n", line);
            fclose(file);
            return -1;
        }

        *eq = '\0';
        value = eq + 1;
        trim(key);
        trim(value);

        if (!strcmp(key, "mask") || !strcmp(key, "only") ||
            !strcmp(key, "off_leds") || !strcmp(key, "off-leds")) {
            if (config_value_count >= (int)(sizeof(config_values) / sizeof(config_values[0]))) {
                fprintf(stderr, "Demasiados valores de lista en config\n");
                fclose(file);
                return -1;
            }
            strncpy(config_values[config_value_count], value, sizeof(config_values[0]) - 1);
            config_values[config_value_count][sizeof(config_values[0]) - 1] = '\0';
            value = config_values[config_value_count++];
        }

        if (apply_config_value(opts, key, value) < 0) {
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

static int parse_range_list(const char *list, uint8_t *enabled, int count, int value)
{
    char *copy = malloc(strlen(list) + 1);
    char *token;

    if (!copy) {
        return -1;
    }

    strcpy(copy, list);
    for (token = strtok(copy, ","); token; token = strtok(NULL, ",")) {
        int start;
        int end;
        char *dash;

        trim(token);
        dash = strchr(token, '-');
        if (dash) {
            *dash = '\0';
            if (parse_int(token, 0, count - 1, "indice", &start) < 0 ||
                parse_int(dash + 1, 0, count - 1, "indice", &end) < 0) {
                free(copy);
                return -1;
            }
        } else {
            if (parse_int(token, 0, count - 1, "indice", &start) < 0) {
                free(copy);
                return -1;
            }
            end = start;
        }

        if (start > end) {
            fprintf(stderr, "Rango invalido: %d-%d\n", start, end);
            free(copy);
            return -1;
        }

        for (int i = start; i <= end; i++) {
            enabled[i] = (uint8_t)value;
        }
    }

    free(copy);
    return 0;
}

static int apply_selection(const led_options_t *opts, uint8_t *enabled)
{
    for (int i = 0; i < opts->count; i++) {
        switch (opts->mode) {
        case MODE_ALL:
            enabled[i] = 1;
            break;
        case MODE_SKIP_ENDS:
            enabled[i] = (i != 0 && i != opts->count - 1);
            break;
        case MODE_ALTERNATE:
            enabled[i] = (i % 2) == 0;
            break;
        case MODE_ALTERNATE_OFFSET:
            enabled[i] = (i % 2) == 1;
            break;
        }
    }

    if (opts->mask) {
        size_t mask_len = strlen(opts->mask);

        if (mask_len != (size_t)opts->count) {
            fprintf(stderr, "--mask debe tener %d caracteres\n", opts->count);
            return -1;
        }

        for (int i = 0; i < opts->count; i++) {
            if (opts->mask[i] == '1') {
                enabled[i] = 1;
            } else if (opts->mask[i] == '0') {
                enabled[i] = 0;
            } else {
                fprintf(stderr, "--mask solo acepta 0 y 1\n");
                return -1;
            }
        }
    }

    if (opts->only) {
        memset(enabled, 0, opts->count);
        if (parse_range_list(opts->only, enabled, opts->count, 1) < 0) {
            return -1;
        }
    }

    if (opts->off_leds && parse_range_list(opts->off_leds, enabled, opts->count, 0) < 0) {
        return -1;
    }

    return 0;
}

static uint32_t make_color(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void cleanup(int signum)
{
    (void)signum;

    if (ledstring.channel[0].leds) {
        for (int i = 0; i < ledstring.channel[0].count; i++) {
            ledstring.channel[0].leds[i] = 0;
        }
        ws2811_render(&ledstring);
    }

    ws2811_fini(&ledstring);
    _exit(0);
}

static void init_defaults(led_options_t *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->count = DEFAULT_LED_COUNT;
    opts->gpio = DEFAULT_LED_GPIO;
    opts->dma = DEFAULT_LED_DMA;
    opts->invert = DEFAULT_LED_INVERT;
    opts->brightness = DEFAULT_LED_BRIGHTNESS;
    opts->strip_type = DEFAULT_STRIP_TYPE;
    opts->mode = MODE_SKIP_ENDS;
}

static int parse_options(int argc, char *argv[], led_options_t *opts)
{
    enum {
        OPT_CONFIG = 1000,
        OPT_COUNT,
        OPT_GPIO,
        OPT_DMA,
        OPT_BRIGHTNESS,
        OPT_INVERT,
        OPT_STRIP,
        OPT_RGB,
        OPT_HEX,
        OPT_OFF,
        OPT_MODE,
        OPT_MASK,
        OPT_ONLY,
        OPT_OFF_LEDS,
        OPT_HELP
    };
    static const struct option long_options[] = {
        {"config", required_argument, 0, OPT_CONFIG},
        {"count", required_argument, 0, OPT_COUNT},
        {"gpio", required_argument, 0, OPT_GPIO},
        {"dma", required_argument, 0, OPT_DMA},
        {"brightness", required_argument, 0, OPT_BRIGHTNESS},
        {"invert", no_argument, 0, OPT_INVERT},
        {"strip", required_argument, 0, OPT_STRIP},
        {"rgb", required_argument, 0, OPT_RGB},
        {"hex", required_argument, 0, OPT_HEX},
        {"off", no_argument, 0, OPT_OFF},
        {"mode", required_argument, 0, OPT_MODE},
        {"mask", required_argument, 0, OPT_MASK},
        {"only", required_argument, 0, OPT_ONLY},
        {"off-leds", required_argument, 0, OPT_OFF_LEDS},
        {"help", no_argument, 0, OPT_HELP},
        {0, 0, 0, 0}
    };
    int opt;

    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp(argv[i], "--config") && load_config(opts, argv[i + 1]) < 0) {
            return -1;
        }
    }

    if (argc == 4 && argv[1][0] != '-') {
        if (parse_int(argv[1], 0, 255, "R", &opts->r) < 0 ||
            parse_int(argv[2], 0, 255, "G", &opts->g) < 0 ||
            parse_int(argv[3], 0, 255, "B", &opts->b) < 0) {
            return -1;
        }
        return 0;
    }

    optind = 1;
    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (opt) {
        case OPT_CONFIG:
            break;
        case OPT_COUNT:
            if (parse_int(optarg, 1, 100000, "count", &opts->count) < 0) return -1;
            break;
        case OPT_GPIO:
            if (parse_int(optarg, 0, 31, "gpio", &opts->gpio) < 0) return -1;
            break;
        case OPT_DMA:
            if (parse_int(optarg, 0, 14, "dma", &opts->dma) < 0) return -1;
            break;
        case OPT_BRIGHTNESS:
            if (parse_int(optarg, 0, 255, "brightness", &opts->brightness) < 0) return -1;
            break;
        case OPT_INVERT:
            opts->invert = 1;
            break;
        case OPT_STRIP:
            if (parse_strip_type(optarg, &opts->strip_type) < 0) return -1;
            break;
        case OPT_RGB:
            if (optind + 1 >= argc ||
                parse_int(optarg, 0, 255, "R", &opts->r) < 0 ||
                parse_int(argv[optind], 0, 255, "G", &opts->g) < 0 ||
                parse_int(argv[optind + 1], 0, 255, "B", &opts->b) < 0) {
                return -1;
            }
            optind += 2;
            break;
        case OPT_HEX:
            if (parse_hex_color(optarg, opts) < 0) return -1;
            break;
        case OPT_OFF:
            opts->turn_off = 1;
            break;
        case OPT_MODE:
            if (parse_mode(optarg, &opts->mode) < 0) return -1;
            break;
        case OPT_MASK:
            opts->mask = optarg;
            break;
        case OPT_ONLY:
            opts->only = optarg;
            break;
        case OPT_OFF_LEDS:
            opts->off_leds = optarg;
            break;
        case OPT_HELP:
            usage(argv[0]);
            exit(0);
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (optind != argc) {
        usage(argv[0]);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    led_options_t opts;
    uint8_t *enabled;
    uint32_t color;
    ws2811_return_t ret;

    init_defaults(&opts);
    if (parse_options(argc, argv, &opts) < 0) {
        usage(argv[0]);
        return 2;
    }

    memset(&ledstring, 0, sizeof(ledstring));
    ledstring.freq = WS2811_TARGET_FREQ;
    ledstring.dmanum = opts.dma;
    ledstring.channel[0].gpionum = opts.gpio;
    ledstring.channel[0].invert = opts.invert;
    ledstring.channel[0].count = opts.count;
    ledstring.channel[0].strip_type = opts.strip_type;
    ledstring.channel[0].brightness = (uint8_t)opts.brightness;

    enabled = calloc(opts.count, sizeof(*enabled));
    if (!enabled) {
        fprintf(stderr, "No hay memoria para %d LEDs\n", opts.count);
        return 1;
    }

    if (apply_selection(&opts, enabled) < 0) {
        free(enabled);
        return 2;
    }

    ret = ws2811_init(&ledstring);
    if (ret != WS2811_SUCCESS) {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
        free(enabled);
        return 1;
    }

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    color = opts.turn_off ? 0 : make_color((uint8_t)opts.r, (uint8_t)opts.g, (uint8_t)opts.b);
    for (int i = 0; i < opts.count; i++) {
        ledstring.channel[0].leds[i] = enabled[i] ? color : 0;
    }

    free(enabled);

    ret = ws2811_render(&ledstring);
    if (ret != WS2811_SUCCESS) {
        fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
        ws2811_fini(&ledstring);
        return 1;
    }

    ws2811_fini(&ledstring);
    return 0;
}
