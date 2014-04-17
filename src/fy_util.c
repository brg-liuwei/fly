#include "fy_util.h"
#include <string.h>
#include <assert.h>

static int fy_util_init = 0;

static char hex2ch[256][256];

static void fy_util_initialize()
{
    char h, l;

    memset(hex2ch, 0, sizeof (hex2ch));

    for (h = '0'; h <= '9'; ++h) {
        for (l = '0'; l <= '9'; ++l) {
            hex2ch[h][l] = ((h - '0') << 4) + (l - '0');
        }
        for (l = 'a'; l <= 'f'; ++l) {
            hex2ch[h][l] = ((h - '0') << 4) + (l - 'a');
        }
        for (l = 'A'; l <= 'F'; ++l) {
            hex2ch[h][l] = ((h - '0') << 4) + (l - 'A');
        }
    }

    for (h = 'a'; h <= 'f'; ++h) {
        for (l = '0'; l <= '9'; ++l) {
            hex2ch[h][l] = ((h - 'a') << 4) + (l - '0');
        }
        for (l = 'a'; l <= 'f'; ++l) {
            hex2ch[h][l] = ((h - 'a') << 4) + (l - 'a');
        }
        for (l = 'A'; l <= 'F'; ++l) {
            hex2ch[h][l] = ((h - 'a') << 4) + (l - 'A');
        }
    }

    for (h = 'A'; h <= 'F'; ++h) {
        for (l = '0'; l <= '9'; ++l) {
            hex2ch[h][l] = ((h - 'A') << 4) + (l - '0');
        }
        for (l = 'a'; l <= 'f'; ++l) {
            hex2ch[h][l] = ((h - 'A') << 4) + (l - 'a');
        }
        for (l = 'A'; l <= 'F'; ++l) {
            hex2ch[h][l] = ((h - 'A') << 4) + (l - 'A');
        }
    }
    fy_util_init = 1;
}

size_t fy_url_decode(const char *e, size_t e_size, char *d, size_t d_size)
{
    size_t ee, dd;

    assert(e_size <= d_size);

    if (fy_util_init == 0) {
        fy_util_initialize();
    }

    for (ee = 0, dd = 0; ee < e_size; ++ee, ++dd) {
        if (e[ee] != '%') {
            d[dd] = e[ee];
            continue;
        }
        d[dd] = hex2ch[e[ee+1]][e[ee+2]];
        ee += 2;
    }

    d[dd] = '\0';
    return dd;
}

int fy_atoi(const char *ptr)
{
    return ptr == NULL ? 0 : atoi(ptr);
}

int fy_atoi_n(const char *ptr, size_t n)
{
    int      value;
    size_t   i;

    value = 0;
    if (ptr != NULL) {
        for (i = 0; i != n && ptr[i] != '\0'; ++i) {
            if (ptr[i] < '0' || ptr[i] > '9') {
                return 0;
            }
            value *= 10;
            value += ptr[i] - '0';
        }
    }
    return value;
}

