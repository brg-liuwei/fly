#ifndef __FY_LOGGER_H__
#define __FY_LOGGER_H__

#include <stdarg.h>
#include <sys/types.h>

typedef enum __fy_log_type {
    FY_LOG_DEBUG = 0,
    FY_LOG_INFO,
    FY_LOG_ERROR,
} fy_log_type;

int fy_log_init(fy_log_type type, const char *path);
void __fy_log_fmt(fy_log_type type, const char *file, size_t line, const char *fmt, ...);

#define fy_log_debug(fmt, ...) __fy_log_fmt(FY_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define fy_log_info(fmt, ...) __fy_log_fmt(FY_LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define fy_log_error(fmt, ...) __fy_log_fmt(FY_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
