#ifndef __FY_UTIL_H__
#define __FY_UTIL_H__

#include <sys/types.h>
#include <stdlib.h>
#include <time.h>

int fy_atoi(const char *ptr);
int fy_atoi_n(const char *ptr, size_t n);
size_t fy_url_decode(const char *e, size_t e_size, char *d, size_t d_size);

#endif
