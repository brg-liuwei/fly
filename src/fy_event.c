#include "fy_event.h"
#include "fy_connection.h"
#include "fy_time.h"
#include "fy_logger.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_EPOLL
    #include <epoll.h>
#elif defined HAVE_KQUEUE
    #include <sys/types.h>
    #include <sys/time.h>
    #include <sys/event.h>
#endif

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

    ev_loop->poll_events = (void *)calloc(poll_size,
#ifdef HAVE_EPOLL
            sizeof(struct epoll_event)
#elif defined HAVE_KQUEUE
            sizeof(struct kevent)
#endif
            );
    if (ev_loop->poll_events == NULL) {
        goto free1;
    }
    if ((ev_loop->event_heap = (fy_event **)calloc(poll_size, sizeof(fy_event *))) == NULL) {
        goto free2;
    }
    ev_loop->heap_size = 0;

    return ev_loop;

free2:
    free(ev_loop->poll_events);
free1:
    free(ev_loop);
    return NULL;
}

static int fy_process_events(fy_event_loop *loop)
{
    int                  i, nevs;
    fy_event            *rev, *wev;
    __uint32_t           ev_mask;
    fy_connection       *conn;

#ifdef HAVE_EPOLL
    struct epoll_event  *evs;
#elif defined HAVE_KQUEUE
    struct kevent       *evs;
#endif

    assert(loop != NULL);

#ifdef HAVE_EPOLL
    nevs = epoll_wait(loop->poll_fd, (struct epoll_event *)loop->poll_events,
            loop->poll_size, loop->poll_timeout);

#elif defined HAVE_KQUEUE
    struct timespec timeout, *tp;
    if (loop->poll_timeout != -1) {
        tp = &timeout;
        tp->tv_sec = loop->poll_timeout / 1000;
        tp->tv_nsec = (loop->poll_timeout % 1000) * 1000L * 1000L;
    } else {
        tp = NULL;
    }
    nevs = kevent(loop->pool_fd, NULL, 0, (struct kevent *)loop->poll_events,
            loop->poll_size, tp);

#endif

    if (nevs < 0) {
        switch (errno) {
            case EINTR:
                break;
            default:
                fy_log_error("Mutil IO returns %d events, errno: %d\n", nevs, errno);
                break;
        }
    }

    evs = loop->poll_events;

    for (i = 0; i < nevs; ++i) {

        /* if some error happens without FY_EVIN and FV_EVOUT, we need to handle it */
#ifdef HAVE_EPOLL
        conn = (fy_connection *)evs[i].data.ptr;

        if ((evs[i].events & (EPOLLERR | EPOLLHUP))
                || (evs[i].events & EPOLLIN))
        {
            ev_mask = FY_EVIN;
        } else if (evs[i].events & EPOLLOUT) {
            ev_mask = FY_EVOUT;
        }

#elif HAVE_KQUEUE
        conn = (fy_connection *)evs[i].udata;

        if ((evs[i].flags & EV_ERROR)
                || (evs[i].filter & EVFILT_READ))
        {
            ev_mask = FY_EVIN;
        } else if (evs[i].filter & EVFILT_WRITE) {
            ev_mask = FY_EVOUT;
        }
#endif

        rev = conn->revent;
        wev = conn->wevent;

        /* handle the read event */
        if (ev_mask & FY_EVIN) {
            if (rev->handler
                    && rev->handler(rev, conn->request) < 0)
            {
                continue;
            }
        }

        /* handle the write event */
        if (ev_mask & FY_EVOUT) {
            if (wev->handler) {
                wev->handler(wev, conn->request);
            }
        }
    }

    return nevs;
}

static int fy_event(void *conn, void *loop, __uint32_t events, int op)
{
    fy_event_loop     *lp;
    fy_connection     *c;

    lp = (fy_event_loop *)loop;
    c = (fy_connection *)conn;

#ifdef HAVE_EPOLL
    struct epoll_event  ev;

    ev.events = EPOLLIN;
    if (events & FY_EVOUT) {
        ev.events |= EPOLLOUT;
    }
    ev.data.ptr = c;
    if (op == 1) {
        return epoll_ctl(lp->poll_fd, EPOLL_CTL_ADD, c->fd, &ev);
    } else {
        return epoll_ctl(lp->poll_fd, EPOLL_CTL_MOD, c->fd, &ev);
    }

#elif HAVE_KQUEUE
    struct kevent  ke;

    if (events & FY_EVOUT) {
        EV_SET(&ke, c->fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        if (kevent(lp->poll_fd, &ke, 1, NULL, 0, NULL) == -1) {
            return -1;
        }
    }

    if ((events & FY_EVIN) || ~(events & FY_EVOUT)) {
        EV_SET(&ke, c->fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(lp->poll_fd, &ke, 1, NULL, 0, NULL) == -1) {
            return -1;
        }
    }
    return 0;
#endif
}

int fy_event_add(void *conn, void *loop, __uint32_t events)
{
    fy_event(conn, loop, events, 1);
}

int fy_event_mod(void *conn, void *loop, __uint32_t events)
{
    fy_event(conn, loop, events, 0);
}

int fy_event_del(void *conn, void *loop)
{
    fy_event_loop     *lp;
    fy_connection     *c;

    lp = (fy_event_loop *)loop;
    c = (fy_connection *)conn;

#ifdef HAVE_EPOLL
    return epoll_ctl(lp->poll_fd, EPOLL_CTL_DEL, c->fd, NULL);

#elif defined HAVE_KQUEUE
    kevent   ke;
    EV_SET(&ke, c->fd, EVFILT_READ | EVFILT_WRITE,
            EV_DELETE, 0, 0, NULL);
    return kevent(lp->poll_fd, &ke, 1, NULL, 0, NULL);
#endif
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

