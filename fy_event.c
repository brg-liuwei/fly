#include "fy_event.h"
#include "fy_time.h"
#include "fy_logger.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

fy_conn_pool *fy_create_conn_pool(size_t pool_size, size_t max_load)
{
    int             n;
    fy_conn_pool  *pool;

    if ((pool = (fy_conn_pool *)malloc(sizeof (fy_conn_pool))) == NULL) {
        return NULL;
    }
    if ((pool->conns = (fy_connection *)calloc(pool_size, sizeof(fy_connection))) == NULL) {
        goto free1;
    }
    if ((pool->revents = (fy_event *)calloc(pool_size, sizeof(fy_event))) == NULL) {
        goto free2;
    }
    if ((pool->wevents = (fy_event *)calloc(pool_size, sizeof(fy_event))) == NULL) {
        goto free3;
    }
    pool->size = pool_size;
    pool->free_size = pool_size;
    for (n = pool_size - 1, pool->free_list = NULL; n >= 0; --n) {
        pool->conns[n].fd = -1;
        pool->conns[n].pool = pool;

        pool->conns[n].revent = &pool->revents[n];
        pool->revents[n].conn = &pool->conns[n];

        pool->conns[n].wevent = &pool->wevents[n];
        pool->wevents[n].conn = &pool->conns[n];

        pool->conns[n].max_load = max_load;

        pool->conns[n].next = pool->free_list;
        pool->conns[n].prev = &pool->free_list;
        if (pool->free_list != NULL) {
            pool->free_list->prev = &pool->conns[n].next;
        }
        pool->free_list = &pool->conns[n];
    }
    pool->err_list = NULL;
    return pool;

free3:
    free(pool->revents);
free2:
    free(pool->conns);
free1:
    free(pool);
    return NULL;
}

fy_connection *fy_pop_connection(fy_conn_pool *pool)
{
    fy_connection *conn;

    assert(pool != NULL);
    if (pool->free_size == 0) {
        return NULL;
    }

    assert(pool->free_list != NULL);

    conn = pool->free_list;
    if (conn->next) {
        conn->next->prev = conn->prev;
    }
    pool->free_list = (fy_connection *)conn->next;

    conn->next = NULL;
    conn->prev = NULL;
    --pool->free_size;

    return conn;
}

void fy_push_connection(fy_conn_pool *pool, fy_connection *conn)
{
    assert(pool != NULL);
    assert(conn != NULL);
    assert(conn->next == NULL);
    assert(conn->prev == NULL);

    conn->request = NULL;
    conn->next = pool->free_list;
    if (conn->next) {
        conn->next->prev = &conn->next;
    }
    conn->prev = &pool->free_list;

    pool->free_list = conn;
    ++pool->free_size;
}

fy_connection *fy_pop_err_conn(fy_conn_pool *pool)
{
    fy_connection  *conn;

    conn = pool->err_list;
    if (conn != NULL) {
        pool->err_list = conn->next;
        if (pool->err_list) {
            pool->err_list->prev = conn->prev;
        }
        conn->next = NULL;
        conn->prev = NULL;
    }
    return conn;
}

void fy_push_err_conn(fy_conn_pool *pool, fy_connection *conn)
{
    assert(pool != NULL);
    assert(conn != NULL);

    /* if conn->prev != NULL, it means that the conn now is in the free_list,
     * we should remove it firstly */
    if (conn->prev != NULL) {
        --pool->free_size;
        *(conn->prev) = conn->next;
        if (conn->next) {
            conn->next->prev = conn->prev;
        }
    }

    conn->request = NULL;
    conn->fd = -1;
    conn->next = pool->err_list;
    if (pool->err_list) {
        pool->err_list->prev = &conn->next;
    }
    conn->prev = &pool->err_list;
    pool->err_list = conn;
}

fy_event_loop *fy_create_event_loop(size_t poll_size)
{
    fy_event_loop *ev_loop;

    if ((ev_loop = (fy_event_loop *)calloc(sizeof(fy_event_loop), 1)) == NULL) {
        return NULL;
    }
    if ((ev_loop->poll_fd = epoll_create(poll_size)) == -1) {
        printf("epoll_create errno: %d\n", errno);
        goto free1;
    }
    ev_loop->poll_size = poll_size;
    ev_loop->poll_timeout = -1;
    if ((ev_loop->ep_events = (ep_event *)calloc(poll_size, sizeof(ep_event))) == NULL) {
        goto free1;
    }
    if ((ev_loop->event_heap = (fy_event **)calloc(poll_size, sizeof(fy_event *))) == NULL) {
        goto free2;
    }
    ev_loop->heap_size = 0;

    return ev_loop;

free2:
    free(ev_loop->ep_events);
free1:
    free(ev_loop);
    return NULL;
}

static int fy_process_events(fy_event_loop *loop)
{
    int              i, nevs;
    fy_event       *rev, *wev;
    __uint32_t       ev_mask;
    fy_connection  *conn;

    assert(loop != NULL);

    nevs = epoll_wait(loop->poll_fd, loop->ep_events, loop->poll_size, loop->poll_timeout);

    if (nevs < 0) {
        switch (errno) {
            case EINTR:
                break;
            default:
                /* TODO log errno */
                break;
        }
    }

    for (i = 0; i < nevs; ++i) {
        conn = (fy_connection *)loop->ep_events[i].data.ptr;
        rev = conn->revent;
        wev = conn->wevent;
        ev_mask = loop->ep_events[i].events;        

        /* if some error happens without EPOLLIN and EPOLLOUT, we need to handle it */
        if ((ev_mask & (EPOLLERR | EPOLLHUP))
                && ~(ev_mask & (EPOLLIN | EPOLLOUT)))
        {
            ev_mask |= EPOLLIN;
        }

        /* handle the read event */
        if (ev_mask & EPOLLIN) {
            if (rev->handler
                    && rev->handler(rev, conn->request) < 0)
            {
                continue;
            }
        }

        /* handle the write event */
        if (ev_mask & EPOLLOUT) {
            if (wev->handler) {
                wev->handler(wev, conn->request);
            }
        }
    }

    return nevs;
}

int fy_event_add(void *conn, void *loop, __uint32_t events)
{
    fy_event_loop     *lp;
    fy_connection     *c;
    struct epoll_event  ev;

    lp = (fy_event_loop *)loop;
    c = (fy_connection *)conn;
    ev.events = events;
    ev.data.ptr = c;
    return epoll_ctl(lp->poll_fd, EPOLL_CTL_ADD, c->fd, &ev);
}

int fy_event_mod(void *conn, void *loop, __uint32_t events)
{
    fy_event_loop     *lp;
    fy_connection     *c;
    struct epoll_event  ev;

    lp = (fy_event_loop *)loop;
    c = (fy_connection *)conn;
    ev.events = events;
    ev.data.ptr = c;
    return epoll_ctl(lp->poll_fd, EPOLL_CTL_MOD, c->fd, &ev);
}

int fy_event_del(void *conn, void *loop)
{
    fy_event_loop     *lp;
    fy_connection     *c;

    lp = (fy_event_loop *)loop;
    c = (fy_connection *)conn;
    return epoll_ctl(lp->poll_fd, EPOLL_CTL_DEL, c->fd, NULL);
}

void fy_main_loop(fy_event_loop *loop)
{
    int nevs;

    assert(loop != NULL);

    while (loop->stop == 0) {
        fy_time_update();
        if (loop->before_poll) {
            loop->before_poll(loop);
        }
        nevs = fy_process_events(loop);
    }
}

