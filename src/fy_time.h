#ifndef __FY_TIME_H__
#define __FY_TIME_H__

#include <pthread.h>
#include <sys/time.h>

#define FY_TIME_SLOTS 64
#define FY_TIME_SIZE sizeof("2011-11-11 11:11:11 111 ")
#define FY_TIME_SIZE_PART sizeof("2011-11-11 11:11:11")

struct fy_time_t {
    size_t   slot;
    time_t   sec;
    time_t   msec;
    time_t   cur;
};

static const char fy_time_fmt[] = "%04d-%02d-%02d %02d:%02d:%02d %03d ";
static char fy_time_str[FY_TIME_SLOTS][FY_TIME_SIZE];

static volatile struct fy_time_t fy_time;
static pthread_mutex_t fy_time_mutex;

void fy_time_init();
void fy_time_update();
time_t fy_cur_sec();
time_t fy_cur_msec();
time_t fy_current();
const char *fy_cur_time_str();

#endif
