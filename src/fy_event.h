#ifndef __FY_EVENT_H__
#define __FY_EVENT_H__

#include <sys/types.h>
#include <sys/socket.h>

#include "fy_config.h"
#include "fy_alloc.h"

#define FY_EVIN   0x1
#define FY_EVOUT  0x2
#define FY_EVERR  0x4
#define FY_EVHUP  0x8

typedef struct fy_event_t fy_event;
typedef struct fy_event_loop_t fy_event_loop;

struct fy_event_t {
    void    *data;
    void    *conn;

    int (*handler)(struct fy_event_t *, void *);
};

struct fy_event_loop_t {
    int           stop;
    int           poll_fd;
    int           poll_timeout;
    size_t        poll_size;
    void         *poll_events;
    size_t        heap_size;    /* numbers of connections in poll */
    fy_event    **event_heap;

    void (*before_poll)(fy_event_loop *);
};

fy_event_loop *fy_create_event_loop(fy_pool_t *pool, size_t poll_size);

int fy_event_add(void *conn, void *loop, __uint32_t events);
int fy_event_mod(void *conn, void *loop, __uint32_t events);
int fy_event_del(void *conn, void *loop);

void fy_main_loop(fy_event_loop *loop);

#endif
