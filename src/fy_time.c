#include "fy_time.h"
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static int fy_time_need_update = 0;

static int fy_trylock(int32_t *lock)
{
    /* lock == 0: freed; lock == 1: locked */
    return __sync_val_compare_and_swap(lock, 0, 1);
}

static int fy_unlock(int32_t *lock)
{
    if (*lock != 1) {
        return -1;
    }
    return __sync_val_compare_and_swap(lock, 1, 0);
}

static void fy_time_update_signal(int signum)
{
    if (signum == SIGALRM) {
        fy_time_need_update = 1;
        alarm(1);
    }
}

void fy_time_init()
{
    static int fy_time_initialized = 0;

    if (fy_time_initialized == 0) {
        fy_time_lock = 0;
        fy_time_initialized = 1;
        fy_time_need_update = 1;
        fy_time_update();
        signal(SIGALRM, fy_time_update_signal);
        alarm(1);
    }
}

void fy_time_update()
{
    size_t          slot;
    time_t          msec;
    struct tm      *ptm, buf;
    struct timeval  tv;

    if (fy_time_need_update == 0) {
        return;
    }

    fy_time_need_update = 0;

    if (fy_trylock(&fy_time_lock) != 0) {
        return;
    }

    gettimeofday(&tv, NULL);

    msec = (tv.tv_usec / 1000) % 1000;

    if (fy_time.sec == tv.tv_sec 
            && fy_time.msec == msec)
    {
        fy_unlock(&fy_time_lock);
        return;
    }

    slot = (fy_time.slot + 1) % FY_TIME_SLOTS;

    if (fy_time.sec == tv.tv_sec) {
        memcpy(fy_time_str[slot], fy_time_str[fy_time.slot], FY_TIME_SIZE_PART);
        sprintf(fy_time_str[slot] + FY_TIME_SIZE_PART, "%03ld ", msec);
    } else {
        ptm = localtime_r(&tv.tv_sec, &buf);
        if (ptm == NULL) {
            /* TODO: log errno */
            fy_unlock(&fy_time_lock);
            return;
        }
        snprintf(fy_time_str[slot], FY_TIME_SIZE, fy_time_fmt,
                ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec, msec);
    }

    /* memory barrier */
    __sync_synchronize();

    fy_time.sec = tv.tv_sec;
    fy_time.msec = msec;
    fy_time.cur = fy_time.sec * 1000 + fy_time.msec;
    fy_time.slot = slot;

    fy_unlock(&fy_time_lock);
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

