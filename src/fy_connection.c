#include "fy_connection.h"
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


