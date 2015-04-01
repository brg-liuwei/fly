#include "fy_connection.h"
#include "fy_event.h"
#include "fy_time.h"
#include "fy_logger.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

fy_conn_pool *fy_create_conn_pool(fy_pool_t *mem_pool, size_t pool_size, size_t max_load)
{
    int             n;
    fy_conn_pool  *pool;

    assert(mem_pool != NULL);

    if ((pool = (fy_conn_pool *)fy_pool_alloc(mem_pool, sizeof (fy_conn_pool))) == NULL) {
        return NULL;
    }
    if ((pool->conns = (fy_connection *)fy_pool_alloc(mem_pool,
                    pool_size * sizeof(fy_connection))) == NULL)
    {
        return NULL;
    }
    if ((pool->revents = (fy_event *)fy_pool_alloc(mem_pool,
                    pool_size * sizeof(fy_event))) == NULL)
    {
        return NULL;
    }
    if ((pool->wevents = (fy_event *)fy_pool_alloc(mem_pool, 
                    pool_size * sizeof(fy_event))) == NULL)
    {
        return NULL;
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

int fy_create_nonblocking_conn(fy_connection *conn, const char *remote_addr, int port)
{
    int    s, flag, opt;

    struct hostent      *phost;
    struct sockaddr_in   addr;

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fy_log_error("fy_create_nonblocking_conn socket errno: %d\n", errno);
        goto error;
    }

    /* set non-blocking */
    flag = fcntl(s, F_GETFL, 0);
    if (fcntl(s, F_SETFL, flag | O_NONBLOCK) == -1) {
        fy_log_error("fy_create_nonblocking_conn fcntl errno: %d\n", errno);
        goto error;
    }

    /* keep alive */
    opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == -1) {
        fy_log_error("fy_create_nonblocking_conn "
                "setsockopt keepalive errno: %d\n", errno);
        goto error;
    }

    /* fill in struct sockaddr_in */
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(remote_addr);

    if (conn->addr_text != remote_addr) {
        snprintf(conn->addr_text, ADDRTEXTLEN - 1, "%s", remote_addr);
    }
    conn->port = port;

    if (addr.sin_addr.s_addr == INADDR_NONE) {
        /* search ip of the domain name */
        /* TODO: cache the host ip addr */
        phost = (struct hostent *)gethostbyname(remote_addr);
        if (phost == NULL || phost->h_addr_list[0] == NULL) {
            fy_log_error("gethostbyname error, errno: %d, host: %s\n",
                    errno, remote_addr);
            goto error;
        }

        /* TODO: select a random h_addr_list[i] */
        //addr.sin_addr = *((struct in_addr *)phost->h_addr);
        addr.sin_addr = *((struct in_addr *)phost->h_addr_list[0]);
    }

    /* connecting */
    if (connect(s, (struct sockaddr *)(&addr), sizeof(addr)) == -1) {
        if (errno != EINPROGRESS) {
            fy_log_error("connect error, errno: %d, host: %s:%d\n",
                    errno, remote_addr, port);
            goto error;
        }
#ifdef FY_DEBUG
        fy_log_debug("connect remote addr %s:%d EINPROGRESS\n",
                remote_addr, port);
    } else {
        fy_log_debug("connect remote addr %s:%d seccessful\n",
                remote_addr, port);
#endif
    }

    /* fill conn */
    conn->conn_type = CONN_TCP_CLI;
    conn->fd = s;
    return 0;

error:
    if (s >= 0) {
        close(s);
    }
    return -1;
}

/*
 * conn_handler must push the repaired conn into the pool
 */
void fy_repair_conn_pool(fy_conn_pool *pool, void *ev_loop,
        int (*conn_handler)(fy_event *, void *),
        int (*read_handler)(fy_event *, void *))
{
    fy_connection *err_conn;

    assert(conn_handler);

    if (pool != NULL && pool->err_list != NULL) {
        err_conn = fy_pop_err_conn(pool);
        if (err_conn == NULL) {
            return;
        }
        if (fy_create_nonblocking_conn(err_conn, 
                    err_conn->addr_text, err_conn->port) == -1)
        {
            fy_push_err_conn(pool, err_conn);
            return;
        }
        err_conn->revent->handler = read_handler;
        err_conn->wevent->handler = conn_handler;
        if  (fy_event_add(err_conn, ev_loop, FY_EVOUT) == -1) {
            fy_log_error("fy_repair_conn_pool::fy_event_add errno: %d\n", errno);
            close(err_conn->fd);
            err_conn->fd = -1;
            fy_push_err_conn(pool, err_conn);
        }
    }
}

size_t fy_free_conns(fy_conn_pool *pool)
{
    return pool->free_size;
}

double fy_conn_usage_percent(fy_conn_pool *pool)
{
    return (pool->size - pool->free_size) * 100.0 / pool->size;
}
