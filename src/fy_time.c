#include "fy_time.h"
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static void fy_time_update_signal(int signum)
{
    if (signum == SIGALRM) {
        fy_time_update();
        alarm(1);
    }
}

void fy_time_init()
{
    static int fy_time_initialized = 0;

    if (fy_time_initialized == 0) {
        pthread_mutex_init(&fy_time_mutex, NULL);
        fy_time_initialized = 1;
        fy_time_update();
        signal(SIGALRM, fy_time_update_signal);
        alarm(1);
    }
}

void fy_time_update()
{
    size_t          slot;
    time_t          msec;
    struct tm      *ptm, tm_buf;
    struct timeval  tv;

    if (pthread_mutex_trylock(&fy_time_mutex) != 0) {
        return;
    }

    gettimeofday(&tv, NULL);

    msec = (tv.tv_usec / 1000) % 1000;

    if (fy_time.sec == tv.tv_sec 
            && fy_time.msec == msec)
    {
        pthread_mutex_unlock(&fy_time_mutex);
        return;
    }

    slot = (fy_time.slot + 1) % FY_TIME_SLOTS;

    if (fy_time.sec == tv.tv_sec) {
        memcpy(fy_time_str[slot], fy_time_str[fy_time.slot], FY_TIME_SIZE_PART);
        sprintf(fy_time_str[slot] + FY_TIME_SIZE_PART, "%03ld ", msec);
    } else {
        ptm = localtime_r(&tv.tv_sec, &tm_buf);
        if (ptm == NULL) {
            /* TODO: log errno */
            pthread_mutex_unlock(&fy_time_mutex);
            return;
        }
        snprintf(fy_time_str[slot], FY_TIME_SIZE, fy_time_fmt,
                ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, msec);
        fy_time.hour = ptm->tm_hour;
    }

    /* memory barrier */
    __sync_synchronize();

    fy_time.sec = tv.tv_sec;
    fy_time.msec = msec;
    fy_time.cur = fy_time.sec * 1000 + fy_time.msec;
    fy_time.slot = slot;

    pthread_mutex_unlock(&fy_time_mutex);
}

inline int fy_cur_hour()
{
    return fy_time.hour;
}

inline time_t fy_cur_sec()
{
    return fy_time.sec;
}

inline time_t fy_cur_msec()
{
    return fy_time.msec;
}

inline time_t fy_current()
{
    return fy_time.cur;
}

inline const char *fy_cur_time_str()
{
    return fy_time_str[fy_time.slot];
}

