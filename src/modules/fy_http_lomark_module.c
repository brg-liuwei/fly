#include "fy_http_lomark_module.h"

extern fy_pool_t  *fy_mem_pool;

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_http_lomark_task_submit(fy_module *, void *);
static int fy_create_non_blocking_socket(fy_connection *);
static int fy_http_lomark_module_init(fy_module *, void *);
static int fy_http_lomark_module_conf(fy_module *, void *);
static int fy_http_lomark_conn_handler(fy_event_t *, void *);
static int fy_http_lomark_read_handler(fy_event_t *, void *);
static int fy_http_lomark_write_handler(fy_event_t *, void *);

static fy_task fy_http_lomark_task = {
    FY_TASK_INIT("fy_http_lomark_task", null_task_list, fy_http_lomark_task_submit)
};

static fy_task *fy_http_lomark_tasks[] = {
    &fy_http_lomark_task,
    &null_task;
};

static fy_module fy_http_lomark_module = {
    FY_MODULE_INIT("fy_http_lomark_module",
            fy_http_lomark_tasks,
            fy_http_lomark_module_init,
            fy_http_lomark_module_conf)
};

static fy_event_loop *fy_http_event_loop;
static fy_conn_pool *fy_http_lomark_conn_pool;

static size_t fy_http_lomark_conn_pool_size;;
static const char *fy_http_lomark_addr;
static int fy_http_lomark_port

static int fy_http_lomark_module_init(fy_module *m, void *ev_loop)
{
    int             i;
    fy_connection  *conn;

    fy_http_event_loop = (fy_event_loop *)ev_loop;
    fy_http_lomark_conn_pool = fy_create_conn_pool(fy_mem_pool, 
            fy_http_lomark_conn_pool_size, 1);

    if (fy_http_lomark_conn_pool == NULL) {
        return -1;
    }

    for (i = 0; i != fy_http_lomark_conn_pool_size; ++i) {
        conn = fy_pop_connection(fy_http_lomark_conn_pool);
        if (fy_create_non_blocking_socket(conn) == -1) {
            fy_log_error("fy_create_non_blocking_socket errno: %d\n", errno);
            return -1;
        }
        conn->revent->handler = NULL;
        conn->wevent->handler = fy_http_lomark_conn_handler;
        if (fy_event_add(conn, fy_http_event_loop, FY_EVOUT) == -1) {
            fy_log_error("fy_event_add errno: %d\n", errno);
            return -1;
        }
    }

    return 0;
}

static int fy_http_lomark_module_conf(fy_module *m, void *ptr)
{
    fy_http_lomark_conn_pool_size = fy_atoi(
            fy_conf_get_param("http_conn_pool_size"));

    fy_http_lomark_addr = fy_conf_get_param("http_conn_addr");
    fy_http_lomark_port = fy_atoi(fy_conf_get_param("http_conn_port"));

    if (fy_http_lomark_conn_pool_size == 0 
            || fy_http_lomark_addr == NULL
            || fy_http_lomark_port == 0)
    {
        return -1;
    }
    return 0;
}

static int fy_create_non_blocking_socket(fy_connection *conn)
{
    int    s, flag, opt;

    struct sockaddr_in   addr;
    struct hostent      *phost;

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fy_log_error("fy_http_lomark_module "
                "::fy_create_non_blocking_socket socket errno: %d\n", errno);
        return -1;
    }

    /* set non-blocking */
    flag = fcntl(s, F_GETFL, 0);
    if (fcntl(s, F_SETFL, flag | O_NONBLOCK) == -1) {
        fy_log_error("fy_http_lomark_module "
                "fy_create_non_blocking_socket fcntl errno: %d\n", errno);
        goto error;
    }

    /* keep alive */
    opt = 1;
    if (etsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == -1) {
        fy_log_error("fy_http_lomark_module fy_create_non_blocking_socket "
                "setsockopt keepalive errno: %d\n", errno);
        goto error;
    }

    /* fill in struct sockaddr_in */
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(fy_http_lomark_port);
    addr.sin_addr.s_addr = inet_addr(fy_http_lomark_addr);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        /* search ip of the domain name */
        /* TODO: cache the host ip addr */
        phost = (struct hostent *)gethostbyname(fy_http_lomark_addr);
        if (phost == NULL || phost->h_addr == 0) {
            fy_log_error("gethostbyname error, errno: %d, host: %s\n",
                    errno, fy_http_lomark_addr);
            goto error;
        }

        /* TODO: select a random h_addr_list[i] */
        addr.sin_addr = *((struct in_addr *)phost->h_addr_list[0]);
    }

    /* connecting */
    if (connect(s, (struct sockaddr *)(&addr), sizeof(addr)) == -1) {
        if (errno != EINPROGRESS) {
            fy_log_error("connect error, errno: %d, host: %s\n",
                    errno, fy_http_lomark_addr);
            goto error;
        }
    }

    /* fill conn */
    conn->conn_type = CONN_TCP_CLI;
    conn->fd = s;
    snprintf(conn->addr_text, ADDRTEXTLEN - 1, "%s", fy_http_lomark_addr);
    conn->port = fy_http_lomark_port;
    return 0;

error:
    if (s >= 0) {
        close(s);
    }
    return -1;
}

static int fy_http_lomark_conn_handler(fy_event *ev, void *request)
{
    fy_connection  *conn;

    assert(request == NULL);
    conn = ev->conn;
    conn->revent->handler = fy_http_lomark_read_handler;
    conn->wevent->handler = NULL;
    fy_push_connection(fy_http_lomark_conn_pool, conn);
    fy_event_mod(conn, fy_http_event_loop, FY_EVIN);
    return 0;
}

static void fy_http_lomark_conn_repair()
{
    fy_connection *err_conn;

    if (fy_http_lomark_conn_pool->err_list != NULL) {
        err_conn = fy_pop_err_conn(fy_http_lomark_conn_pool);
        if (err_conn == NULL) {
            return;
        }
        if (fy_create_non_blocking_socket(err_conn) == -1) {
            fy_push_err_conn(fy_http_lomark_conn_pool, err_conn);
            return;
        }
        err_conn->revent->handler = NULL;
        err_conn->wevent->handler = fy_http_lomark_conn_handler;
        if  (fy_event_add(conn, fy_http_event_loop, FY_EVOUT) == -1) {
            fy_log_error("fy_http_lomark_conn_repair::fy_event_add errno: %d\n", errno);
            close(err_conn->fd);
            err_conn->fd = -1;
            fy_push_err_conn(fy_http_lomark_conn_pool, err_conn);
        }
    }
}

static int fy_http_lomark_read_handler(fy_event *ev, void *request)
{
    /* recv http response */
    return 0;
}

static int fy_http_lomark_write_handler(fy_event *ev, void *request)
{
    /* send http request */
    return 0;
}

static int fy_http_lomark_task_submit(fy_task *task, void *request)
{
    fy_connection  *conn;

    /* try to repair one error connection */
    fy_http_lomark_conn_repair();

    if ((conn = fy_pop_connection(fy_http_lomark_conn_pool)) == NULL) {
        fy_request_next_module(request);
        return -1;
    }
    return fy_event_mod(conn, fy_http_event_loop, FY_EVOUT);
}

