#include "fy_util.h"
#include "fy_logger.h"
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
            hex2ch[h][l] = ((h - '0') << 4) + (l - 'a' + 10);
        }
        for (l = 'A'; l <= 'F'; ++l) {
            hex2ch[h][l] = ((h - '0') << 4) + (l - 'A' + 10);
        }
    }

    for (h = 'a'; h <= 'f'; ++h) {
        for (l = '0'; l <= '9'; ++l) {
            hex2ch[h][l] = ((h - 'a' + 10) << 4) + (l - '0');
        }
        for (l = 'a'; l <= 'f'; ++l) {
            hex2ch[h][l] = ((h - 'a' + 10) << 4) + (l - 'a' + 10);
        }
        for (l = 'A'; l <= 'F'; ++l) {
            hex2ch[h][l] = ((h - 'a' + 10) << 4) + (l - 'A' + 10);
        }
    }

    for (h = 'A'; h <= 'F'; ++h) {
        for (l = '0'; l <= '9'; ++l) {
            hex2ch[h][l] = ((h - 'A' + 10) << 4) + (l - '0');
        }
        for (l = 'a'; l <= 'f'; ++l) {
            hex2ch[h][l] = ((h - 'A' + 10) << 4) + (l - 'a' + 10);
        }
        for (l = 'A'; l <= 'F'; ++l) {
            hex2ch[h][l] = ((h - 'A' + 10) << 4) + (l - 'A' + 10);
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

int fy_hex_atoi_n(const char *ptr, size_t n)
{
    int      value;
    size_t   i;

    value = 0;
    if (ptr != NULL) {
        for (i = 0; i != n && ptr[i] != '\0'; ++i) {
            value *= 16;
            if (ptr[i] >= '0' && ptr[i] <= '9') {
                value += ptr[i] - '0';
            } else if (ptr[i] >= 'a' && ptr[i] <= 'f') {
                value += ptr[i] - 'a' + 10;
            } else if (ptr[i] >= 'A' && ptr[i] <= 'F') {
                value += ptr[i] - 'A' + 10;
            } else {
                return 0;
            }
        }
    }
    return value;
}

const char *fy_get_os(const char *p)
{
    if (p == NULL) {
        return "";
    }

#ifndef FY_OS
#define FY_OS(haystack, needle, os) \
    do { \
        if (strstr(haystack, needle) != NULL) { \
            return os; \
        } \
    } while (0)

    FY_OS(p, "Windows NT 6.2", "Windows8");
    FY_OS(p, "Windows NT 6.1", "Windows7");
    FY_OS(p, "Windows NT 6.0", "Vista");
    FY_OS(p, "Windows NT 5.1", "WindowsXP");
    FY_OS(p, "Windows NT 5.0", "Windows2000");
    FY_OS(p, "Windows NT 5.2", "WindowsServer2003");
    FY_OS(p, "Windows NT 4.0", "WindowsNT4");
    FY_OS(p, "Win 9x 4.9", "WindowsME");
    FY_OS(p, "Windows 98", "Windows98");
    FY_OS(p, "iPad", "iOS");
    FY_OS(p, "iPhone", "iOS");
    FY_OS(p, "Mac", "MacOS");
    FY_OS(p, "Android", "Android");
    FY_OS(p, "Google Web", "GoogleWebOS");
    FY_OS(p, "Linux", "Linux");
    return "";

#undef FY_OS
#endif
}

const char *fy_get_browser(const char *p)
{
    if (p == NULL) {
        return "";
    }

#ifndef FY_BRW
#define FY_BRW(haystack, needle, browser) \
    do { \
        if (strstr(haystack, needle) != NULL) { \
            return browser; \
        } \
    } while (0)

    FY_BRW(p, "360SE", "360SE");
    FY_BRW(p, "Chrome", "Chrome");
    FY_BRW(p, "Firefox", "Firefox");
    FY_BRW(p, "Mozilla", "Mozilla");
    FY_BRW(p, "Safari", "Safari");
    FY_BRW(p, "TencentTraveler", "TencentTraveler");
    FY_BRW(p, "Konqueror", "Konqueror");
    FY_BRW(p, "Navigator", "Navigator");
    FY_BRW(p, "Netscape", "Netscape");
    FY_BRW(p, "Opera", "Opera");
    FY_BRW(p, "IE 9", "IE9");
    FY_BRW(p, "IE 8", "IE8");
    FY_BRW(p, "IE 7", "IE7");
    FY_BRW(p, "IE 6", "IE6");
    FY_BRW(p, "IE 5", "IE5");
    FY_BRW(p, "IE 4", "IE4");
    FY_BRW(p, "IE 3", "IE3");
    FY_BRW(p, "SE 2.X MetaSr", "SE2.XMetaSr");
    FY_BRW(p, "TheWorld", "TheWorld");
    FY_BRW(p, "Maxthon", "Maxthon");
    FY_BRW(p, "MyIE2", "MyIE2");
    return "";

#undef FY_BRW
#endif
}

const char *fy_fcgi_get_param(const char *key, fy_request *r)
{
    const char   *p;

    p = FCGX_GetParam(key, r->fcgi_request->envp);
    if (p == NULL || p[0] == '\0') {
#ifdef FY_DEBUG
        fy_log_debug("%s = NULL\n", key);
#endif
        return NULL;
    }
#ifdef FY_DEBUG
    fy_log_debug("%s = %s\n", key, p);
#endif
    return p;
}

int fy_str_split(const char *haystack, const char *split, char **array, int array_size, int elem_len)
{
    int          array_len;
    char        *w;
    size_t       v_len;
    const char  *p, *pe;

    assert(haystack != NULL);

    p = haystack;
    w = (char *)array;
    for (array_len = 0;
            array_len < array_size && p != NULL;
            ++array_len, w += elem_len)
    {
        pe = strstr(p, split);
        if (pe == NULL) {
            v_len = strlen(p);
            if (v_len >= elem_len) {
                return -1;
            }
            memcpy(w, p, v_len);
            w[v_len] = '\0';
            return array_len + 1;
        }
        v_len = pe - p;
        if (v_len >= elem_len) {
            return -1;
        }
        memcpy(w, p, v_len);
        w[v_len] = '\0';
        p = pe + 1;
    }
    return array_len;
}
