#include "fy_fcgi_accept_module.h"
#include "fy_info.h"
#include "fy_event.h"
#include "fy_connection.h"
#include "fy_logger.h"
#include "fy_util.h"
#include "fy_conf.h"

#include <fastcgi.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern fy_pool_t *fy_mem_pool;
extern size_t fy_pagesize;

extern fy_task *null_task_list[];
extern fy_task  null_task;

static const char *fy_unix_path;
static int fy_unix_fd;

static int fy_fcgi_accept_module_init(fy_module *, void *);
static int fy_fcgi_accept_module_conf(fy_module *, void *);
static int fy_fcgi_accept_task_submit(fy_task *, void *);

static int fy_fcgi_accept_module_init_tcp_listener(fy_module *, void *ptr);
static int fy_fcgi_accept_module_init_unix_listener(fy_module *, void *ptr);

static fy_task fy_fcgi_accept_task = {
    FY_TASK_INIT("fy_fcgi_accept_task", null_task_list, fy_fcgi_accept_task_submit)
};

static fy_task *fy_fcgi_accept_tasks[] = {
    &fy_fcgi_accept_task,
    &null_task
};

fy_module fy_fcgi_accept_module = {
    FY_MODULE_INIT("fy_fcgi_accept_module",
            fy_fcgi_accept_tasks,
            fy_fcgi_accept_module_init,
            fy_fcgi_accept_module_conf)
};

static int fy_fcgi_accept_task_submit(fy_task *task, void *request)
{
    return 0;
}

static int fy_fcgi_accept_read_handler(fy_event *ev, void *ptr)
{
    fy_pool_t      *pool;
    fy_request     *request;
    FCGX_Request   *r;
    fy_connection  *c;

    c = ev->conn;

    /* pool for fy_request */
    if ((pool = fy_pool_create(getpagesize())) == NULL) {
        fy_log_error("fy_fcgi_accept_module::read_handler::alloc pool error\n");
        return -1;
    }
    r = (FCGX_Request *)fy_pool_alloc(pool, sizeof(FCGX_Request));
    if (r == NULL) {
        fy_log_error("fy_fcgi_accept_module::read_handler::alloc r error\n");
        goto free1;
    }
    if (FCGX_InitRequest(r, c->fd, 0) != 0) {
        fy_log_error("fy_fcgi_accept_module::read_handler::FCGX_InitRequest error\n");
        goto free1;
    }
    if (FCGX_Accept_r(r) != 0) {
        fy_log_debug("fy_fcgi_accept_module::read_handler::FCGX_Accept_r errno: %d\n", errno);
        goto free1;
    }

#ifdef FY_DEBUG
    fy_log_debug("fy_fcgi_accept read_handler: accept fd: %d, listen fd: %d\n", r->ipcFd, r->listen_sock);
#endif

    request = fy_request_create(pool);
    if (request == NULL) {
        fy_log_debug("fy_fcgi_accept_module::read_handler::fy_request_create error\n");
        goto free2;
    }
    request->pool = pool;

    request->info = fy_info_create(pool);
    if (request->info == NULL) {
        fy_log_debug("fy_fcgi_accept_module::read_handler::fy_info_create error\n");
        goto free2;
    }

    request->fcgi_request = r;
    request->module = &fy_fcgi_accept_module;

#ifdef FY_DEBUG
    fy_log_debug("accept location: %s\n", fy_fcgi_get_param("DOCUMENT_URI", request));
#endif
    fy_request_next_module(request);

    return 0;

free2:
    if (request->cln != NULL) {
        request->cln(request);
    }
free1:
    fy_pool_destroy(pool);
    return -1;
}

static int fy_fcgi_accept_module_conf(fy_module *m, void *ptr)
{
    if ((fy_unix_path = fy_conf_get_param("accept_unix_path")) == NULL) {
        fy_log_error("accept_unix_path NULL\n");
        return -1;
    }
    return 0;
}

static int fy_fcgi_accept_module_init(fy_module *module, void *ptr)
{
    int rc;

    FCGX_Init();
    module->pool = fy_mem_pool;

    if (module->pool == NULL) {
        return -1;
    }

    rc = fy_fcgi_accept_module_init_tcp_listener(module, ptr);
    if (rc != 0) {
        return rc;
    }
    rc = fy_fcgi_accept_module_init_unix_listener(module, ptr);
    return rc;
}

static int fy_fcgi_accept_module_init_tcp_listener(fy_module *module, void *ptr)
{
    fy_connection   *conn;

    if ((conn = (fy_connection *)fy_pool_alloc(module->pool, sizeof(fy_connection))) == NULL) {
        fy_log_error("fy_fcgi_accept_module alloc conn error\n");
        return -1;
    }

    conn->conn_type = CONN_TCP_LSN;
    conn->fd = FCGI_LISTENSOCK_FILENO;
    conn->socklen = sizeof(struct sockaddr_in);
    getsockname(conn->fd, (struct sockaddr *)&conn->addr, &conn->socklen);
    gethostname(conn->addr_text, ADDRTEXTLEN);
    conn->pool = NULL;
    conn->revent = (fy_event *)fy_pool_alloc(module->pool, sizeof(fy_event));
    conn->wevent = (fy_event *)fy_pool_alloc(module->pool, sizeof(fy_event));
    if (conn->revent == NULL || conn->wevent == NULL) {
        fy_log_error("fy_fcgi_accept_module alloc event error\n");
        return -1;
    }

    conn->revent->conn = conn;
    conn->revent->data = NULL;
    conn->revent->handler = fy_fcgi_accept_read_handler;

    conn->wevent->conn = conn;
    conn->wevent->data = NULL;
    conn->wevent->handler = NULL;

    module->data = conn;

#ifdef FY_DEBUG
    fy_log_debug("fy_fcgi_accept_init hostname:%s\n", conn->addr_text);
    fy_log_debug("fy_fcgi_accept_init sin_addr:%s\n", 
            inet_ntoa(((struct sockaddr_in *)&conn->addr)->sin_addr));
#endif

    return fy_event_add(conn, ptr, FY_EVIN);
}

static int fy_fcgi_accept_module_init_unix_listener(fy_module *module, void *ptr)
{
    fy_connection   *conn;

    if ((conn = (fy_connection *)fy_pool_alloc(module->pool, sizeof(fy_connection))) == NULL) {
        fy_log_error("fy_fcgi_accept_module alloc conn error\n");
        return -1;
    }

    if ((fy_unix_fd = FCGX_OpenSocket(fy_unix_path, 4096)) == -1) {
        fy_log_error("fy_fcgi_accept_module open domain socket error\n");
        return -1;
    }

    if (chmod(fy_unix_path, 0666) != 0) {
        fy_log_error("fy_fcgi_accept_module %s chmod error: %d\n", fy_unix_path, errno);
        return -1;
    }

    conn->conn_type = CONN_UNIX_LSN;
    conn->fd = fy_unix_fd;
    conn->socklen = sizeof(struct sockaddr_un);
    getsockname(conn->fd, (struct sockaddr *)&conn->addr, &conn->socklen);
    gethostname(conn->addr_text, ADDRTEXTLEN);
    conn->pool = NULL;
    conn->revent = (fy_event *)fy_pool_alloc(module->pool, sizeof(fy_event));
    conn->wevent = (fy_event *)fy_pool_alloc(module->pool, sizeof(fy_event));
    if (conn->revent == NULL || conn->wevent == NULL) {
        fy_log_error("fy_fcgi_accept_module alloc event error\n");
        return -1;
    }

    conn->revent->conn = conn;
    conn->revent->data = NULL;
    conn->revent->handler = fy_fcgi_accept_read_handler;

    conn->wevent->conn = conn;
    conn->wevent->data = NULL;
    conn->wevent->handler = NULL;

    module->data = conn;

#ifdef FY_DEBUG
    fy_log_debug("fy_fcgi_accept_init hostname:%s\n", conn->addr_text);
    fy_log_debug("fy_fcgi_accept_init sin_addr:%s\n", 
            ((struct sockaddr_un *)&conn->addr)->sun_path);
#endif

    return fy_event_add(conn, ptr, FY_EVIN);
}
