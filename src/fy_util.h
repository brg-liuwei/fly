#ifndef __FY_UTIL_H__
#define __FY_UTIL_H__

#include "fy_framework.h"

#include <sys/types.h>
#include <stdlib.h>
#include <time.h>

int fy_atoi(const char *ptr);
int fy_atoi_n(const char *ptr, size_t n);
size_t fy_url_decode(const char *e, size_t e_size, char *d, size_t d_size);
char *fy_fcgi_get_param(const char *key, fy_request *r);

#define FY_OS(haystack, needle, os) \
    do { \
        if (strstr(haystack, needle) != NULL) { \
            return os; \
        } \
    } while (0)

#define FY_BRW(haystack, needle, browser) \
    do { \
        if (strstr(haystack, needle) != NULL) { \
            return browser; \
        } \
    } while (0)

#endif
