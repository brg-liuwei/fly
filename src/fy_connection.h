#ifndef __FY_CONNECTION_H__
#define __FY_CONNECTION_H__

#include "fy_event.h"

#include <sys/types.h>
#include <sys/socket.h>

typedef struct fy_conn_pool_t fy_conn_pool;
typedef struct fy_connection_t fy_connection;

typedef enum __fy_conn_type {
    CONN_UNSET = 0,
    CONN_TCP_LSN,
    CONN_TCP_CLI,
    CONN_TCP_SRV,
    CONN_UNIX_LSN,
    CONN_UNIX_CLI,
    CONN_UNIX_SRV
} fy_conn_type;

typedef enum fy_rpc_type {
    RPC_UNSET = 0,
    RPC_PA_CLI,
    RPC_UA_CLI,
    RPC_ADIDX_CLI,
    RPC_CTIDX_CLI,
    RPC_CTR_CLI,
    RPC_ADX_CLI
} fy_rpc_type;

#define ADDRTEXTLEN 256

struct fy_connection_t {
    fy_conn_type      conn_type;
    fy_rpc_type       rpc_type;
    int               fd;
    struct sockaddr   addr;
    socklen_t         socklen;
    char              addr_text[ADDRTEXTLEN];
    int               port;

    fy_event         *revent;
    fy_event         *wevent;

    fy_conn_pool     *pool;
    size_t            load;
    size_t            max_load;

    void             *request;
    void             *data;

    fy_connection    *next;
    fy_connection   **prev;
};

struct fy_conn_pool_t {
    size_t            size;
    size_t            free_size;

    fy_event         *revents;
    fy_event         *wevents;

    fy_connection    *conns;
    fy_connection    *free_list;
    fy_connection    *err_list;
};

fy_conn_pool *fy_create_conn_pool(size_t pool_size, size_t max_load);

void fy_push_connection(fy_conn_pool *pool, fy_connection *conn);
fy_connection *fy_pop_connection(fy_conn_pool *pool);

void fy_push_err_conn(fy_conn_pool *pool, fy_connection *conn);
fy_connection *fy_pop_err_conn(fy_conn_pool *pool);

#endif
