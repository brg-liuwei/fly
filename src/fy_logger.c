#include "fy_logger.h"
#include "fy_time.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define FYLOGMAXLINE 4096

static int fy_debug_log_fd;
static int fy_info_log_fd;
static int fy_err_log_fd;

int fy_log_init(fy_log_type type, const char *path)
{
    int *p;

    assert(path != NULL);

    switch (type) {
        case FY_LOG_DEBUG:
            p = &fy_debug_log_fd;
            break;
        case FY_LOG_INFO:
            p = &fy_info_log_fd;
            break;
        case FY_LOG_ERROR:
            p = &fy_err_log_fd;
            break;
        default:
            printf("%s:%d init log type error, type = %d\n",
                    __FILE__, __LINE__, type);
            return -1;
    }
    *p = open(path, O_APPEND | O_CREAT | O_WRONLY, 0644);
    return *p;
}

void __fy_log_fmt(fy_log_type type, const char *file, size_t line, const char *fmt, ...)
{
    int      n;
    int      log_fd;
    char     content[FYLOGMAXLINE];
    va_list  ap;

    if (type == FY_LOG_ERROR) {
        n = snprintf(content, FYLOGMAXLINE, "%s%s:%ld ", fy_cur_time_str(), file, line);
    } else {
        n = snprintf(content, FYLOGMAXLINE, "%s", fy_cur_time_str());
    }

    va_start(ap, fmt);
    n += vsnprintf(content + n, FYLOGMAXLINE - n, fmt, ap);
    va_end(ap);

    switch (type) {
        case FY_LOG_DEBUG:
            log_fd = fy_debug_log_fd;
            break;
        case FY_LOG_INFO:
            log_fd = fy_info_log_fd;
            break;
        case FY_LOG_ERROR:
            log_fd = fy_err_log_fd;
            break;
        default:
            __fy_log_fmt(FY_LOG_ERROR, __FILE__, __LINE__, "error log type = %d\n", type);
            abort();
    }
    write(log_fd, content, n);
}

