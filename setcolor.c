#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define LED_COUNT 11
#define DEV_PATH "/dev/ws281x_pwm"

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s R G B\n", argv[0]);
        return 2;
    }

    int r = atoi(argv[1]);
    int g = atoi(argv[2]);
    int b = atoi(argv[3]);

    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;

    int fd = open(DEV_PATH, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "open(%s): %s\n", DEV_PATH, strerror(errno));
        return 1;
    }

    unsigned char buf[LED_COUNT * 3];
    for (int i = 0; i < LED_COUNT; i++) {
        buf[i*3 + 0] = (unsigned char)r;
        buf[i*3 + 1] = (unsigned char)g;
        buf[i*3 + 2] = (unsigned char)b;
    }

    ssize_t n = write(fd, buf, sizeof(buf));
    if (n < 0) {
        fprintf(stderr, "write(%s): %s\n", DEV_PATH, strerror(errno));
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}
