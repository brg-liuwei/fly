#include "fy_fcgi_accept_module.h"
#include "fy_info.h"
#include "fy_event.h"
#include "fy_connection.h"
#include "fy_logger.h"
#include "fy_util.h"

#include <fastcgi.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern fy_pool_t *fy_mem_pool;
extern size_t fy_pagesize;

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_fcgi_accept_module_init(fy_module *, void *);
static int fy_fcgi_accept_task_submit(fy_task *, void *);

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
            fy_fcgi_accept_module_init, NULL)
};

static int fy_fcgi_accept_task_submit(fy_task *task, void *request)
{
    return 0;
}

static int fy_fcgi_accept_read_handler(fy_event *ev, void *ptr)
{
    fy_pool_t     *pool;
    fy_request    *request;
    FCGX_Request  *r;

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
    if (FCGX_InitRequest(r, FCGI_LISTENSOCK_FILENO, 0) != 0) {
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

static int fy_fcgi_accept_module_init(fy_module *module, void *ptr)
{
    fy_connection   *conn;

    FCGX_Init();

    module->pool = fy_mem_pool;
    if (module->pool == NULL) {
        return -1;
    }

    if ((conn = (fy_connection *)fy_pool_alloc(module->pool, sizeof(fy_connection))) == NULL) {
        fy_log_error("fy_fcgi_accept_module alloc conn error\n");
        return -1;
    }
    conn->conn_type = CONN_TCP_LSN;
    conn->fd = FCGI_LISTENSOCK_FILENO;
    conn->socklen = sizeof(struct sockaddr);
    getsockname(conn->fd, &conn->addr, &conn->socklen);
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
