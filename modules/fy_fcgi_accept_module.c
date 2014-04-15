#include "fy_fcgi_accept_module.h"
#include "fy_info.h"
#include "fy_event.h"
#include "fy_logger.h"

#include <fastcgi.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
            fy_fcgi_accept_module_init, 
            NULL, NULL)
};

static int fy_fcgi_accept_task_submit(fy_task *task, void *request)
{
    return 0;
}

static int fy_fcgi_accept_read_handler(fy_event *ev, void *ptr)
{
    FCGX_Request  *r;
    fy_request   *request;

#ifdef FY_DEBUG
    int                  i;
    struct fcgi_params  *p;
#endif

    r = (FCGX_Request *)calloc(1, sizeof(FCGX_Request));
    if (r == NULL) {
        /* TODO: log error */
        goto free1;
    }
    if (FCGX_InitRequest(r, FCGI_LISTENSOCK_FILENO, 0) != 0) {
        /* TODO: log error */
        goto free1;
        return 0;
    }
    if (FCGX_Accept_r(r) != 0) {
        /* TODO: log error */
        goto free1;
    }

#ifdef FY_DEBUG
    fy_log_fmt("fy_fcgi_accept read_handler: accept fd: %d, listen fd: %d\n", r->ipcFd, r->listen_sock);
#endif

    request = fy_request_create();
    if (request == NULL) {
        /* TODO: log error */
        goto free1;
    }

    request->info = fy_info_create();
    if (request->info == NULL) {
        /* TODO: log error */
        goto free2;
    }

    request->fcgi_request = r;
    request->module = &fy_fcgi_accept_module;

    fy_request_next_module(request);

    return 0;

free2:
    if (request->cln != NULL) {
        request->cln(request);
    }
    free(request);
free1:
    free(r);
    return -1;
}

static int fy_fcgi_accept_module_init(fy_module *module, void *ptr)
{
    fy_connection   *conn;

    FCGX_Init();

    if ((conn = (fy_connection *)calloc(1, sizeof(fy_connection))) == NULL) {
        fy_log_fmt("fy_fcgi_accept_module calloc conn error\n");
        return -1;
    }
    conn->conn_type = CONN_TCP_LSN;
    conn->rpc_type = RPC_UNSET;
    conn->fd = FCGI_LISTENSOCK_FILENO;
    conn->socklen = sizeof(struct sockaddr);
    getsockname(conn->fd, &conn->addr, &conn->socklen);
    gethostname(conn->addr_text, ADDRTEXTLEN);
    conn->pool = NULL;
    conn->revent = (fy_event *)calloc(1, sizeof(fy_event));
    conn->wevent = (fy_event *)calloc(1, sizeof(fy_event));
    if (conn->revent == NULL || conn->wevent == NULL) {
        fy_log_fmt("fy_fcgi_accept_module calloc event error\n");
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
    fy_log_fmt("fy_fcgi_accept_init hostname:%s\n", conn->addr_text);
    fy_log_fmt("fy_fcgi_accept_init sin_addr:%s\n", 
            inet_ntoa(((struct sockaddr_in *)&conn->addr)->sin_addr));
#endif

    return fy_event_add(conn, ptr, EPOLLIN);
} 
