#include "fy_connection.h"
#include "fy_event.h"
#include "fy_logger.h"
#include "fy_time.h"
#include "fy_util.h"
#include "fy_conf.h"

#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern fy_pool_t  *fy_mem_pool;

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_http_count_module_conf(fy_module *, void *);
static int fy_http_count_module_init(fy_module *, void *);
static int fy_http_count_task_submit(fy_task *, void *);
static int fy_http_set_count_buf(fy_request *r, const char *key, const char *ip, int port);
static int fy_http_count_read_handler(fy_event *ev, void *request);
static int fy_http_count_write_handler(fy_event *ev, void *request);

static fy_task fy_http_count_task = {
    FY_TASK_INIT("fy_http_count_task", null_task_list, fy_http_count_task_submit)
};

static fy_task *fy_http_count_tasks[] = {
    &fy_http_count_task,
    &null_task,
};

fy_module fy_http_count_module = {
    FY_MODULE_INIT("fy_http_count_module",
            fy_http_count_tasks,
            fy_http_count_module_init,
            fy_http_count_module_conf)
};

#define FY_MAX_COUNT_SERVER 256
#ifndef FY_MAX_IP_LEN
#define FY_MAX_IP_LEN sizeof("255.255.255.255:99999")
#endif

static fy_event_loop *fy_http_count_event_loop;

static size_t fy_http_count_server_n;
static char fy_http_count_server_addrs[FY_MAX_COUNT_SERVER][FY_MAX_IP_LEN];

static char fy_http_count_server_ips[FY_MAX_COUNT_SERVER][FY_MAX_IP_LEN];
static int fy_http_count_server_ports[FY_MAX_COUNT_SERVER];

static int fy_tair_area;
const static char *fy_http_count_uri;

static int fy_http_count_module_conf(fy_module *m, void *ptr)
{
    const char *servers;
    const char *tair_area_str;

    tair_area_str = fy_conf_get_param("tair_area");
    if (tair_area_str == NULL) {
        fy_log_error("no tair_area_str\n");
        return -1;
    }

    fy_tair_area = fy_atoi(tair_area_str);
    if (fy_tair_area == 0
            && strcmp(tair_area_str, "0") != 0)
    {
        fy_log_error("tair_area_str format error\n");
        return -1;
    }

    fy_http_count_uri = fy_conf_get_param("count_uri");

    servers = fy_conf_get_param("count_server_addr");

    if (servers == NULL) {
        fy_log_error("no count_server_addr\n");
        return -1;
    }

    fy_http_count_server_n = fy_str_split(servers, ",", (char **)fy_http_count_server_addrs, FY_MAX_COUNT_SERVER, FY_MAX_IP_LEN);

    return fy_http_count_server_n <= 0 ? -1 : 0;
}

static int fy_http_count_module_init(fy_module *m, void *ev_loop)
{
    int       port;
    char     *s, *p;
    size_t    i;

    fy_http_count_event_loop = ev_loop;

    for (i = 0; i != fy_http_count_server_n; ++i) {
        s = fy_http_count_server_addrs[i];
        p = strstr(s, ":");
        if (p == NULL) {
            fy_log_error("inet addr format error: \n", s);
            return -1;
        }
        if ((port = fy_atoi(p+1)) == 0) {
            fy_log_error("inet addr port error: \n", s);
            return -1;
        }
        memcpy(fy_http_count_server_ips[i], s, p - s);
        if (strlen(fy_http_count_server_ips[i]) != p - s) {
            fy_log_error("inet addr copy error: \n", s);
            return -1;
        }
        fy_http_count_server_ports[i] = port;
    }
    return 0;
}

static int fy_http_set_count_buf(fy_request *r, const char *key, const char *ip, int port)
{
    char  *w, *e;

    const size_t n = 1024;

    if ((r->info->send_buf = fy_pool_alloc(r->pool, n)) == NULL) {
        return -1;
    }
    r->info->send_buf_rpos = r->info->send_buf_wpos = r->info->send_buf;
    r->info->send_buf_end = r->info->send_buf + n;
    w = r->info->send_buf_wpos;
    e = r->info->send_buf_end;
    w += snprintf(w, e - w, "GET /tair_incr?key=%s&area=%d&count=1 HTTP/1.1\r\nHost: %s:%d\r\n\r\n", key, fy_tair_area, ip, port);
    if (w > e) {
        w = e;
    }
    r->info->send_buf_wpos = w;
    return 0;
}

static int fy_http_count_task_submit(fy_task *task, void *request)
{
    size_t          i;
    const char     *doc_uri;
    fy_request     *r;
    fy_connection  *conn;

    assert(request != NULL);

    r = (fy_request *)request;

    doc_uri = fy_fcgi_get_param("DOCUMENT_URI", r);
    if (doc_uri == NULL || strcmp(doc_uri, fy_http_count_uri) != 0) {
        fy_request_next_module(r);
        return -1;
    }

    if (r->info->delivery_adid == NULL) {
        /* no ad to delivery */
        fy_request_next_module(r);
        return 0;
    }

    i = rand() % fy_http_count_server_n;
    if (fy_http_set_count_buf(r, r->info->delivery_adid,
                fy_http_count_server_ips[i],
                fy_http_count_server_ports[i]) == -1)
    {
        fy_request_next_module(r);
        return -1;
    }

    conn = (fy_connection *)fy_pool_alloc(r->pool, sizeof(fy_connection));
    conn->revent = (fy_event *)fy_pool_alloc(r->pool, sizeof(fy_event));
    conn->wevent = (fy_event *)fy_pool_alloc(r->pool, sizeof(fy_event));

    if (fy_create_nonblocking_conn(conn, fy_http_count_server_ips[i], fy_http_count_server_ports[i]) == -1) {
        fy_request_next_module(r);
        return -1;
    }
    conn->revent->handler = fy_http_count_read_handler;
    conn->wevent->handler = fy_http_count_write_handler;

    fy_event_mod(conn, fy_http_count_event_loop, FY_EVOUT | FY_EVIN);
    return 0;
}

static int fy_http_count_read_handler(fy_event *ev, void *request)
{
    fy_connection  *c;

    c = ev->conn;
    if (c->fd != -1) {
        close(c->fd);
        c->fd = -1;
    }
    return 0;
}

static int fy_http_count_write_handler(fy_event *ev, void *request)
{
    ssize_t         s_size;
    fy_request     *r;
    fy_connection  *c;

    r = (fy_request *)request;
    c = ev->conn;

    s_size = send(c->fd, r->info->send_buf_rpos,
            r->info->send_buf_wpos - r->info->send_buf_rpos, 0);

    if (s_size == -1) {
        if (errno != EAGAIN) {
            close(c->fd);
            c->fd = -1;
            fy_request_next_module(r);
        } else {
            fy_event_mod(ev->conn, fy_http_count_event_loop, FY_EVOUT);
            return 0;
        }
    }

    if (s_size != r->info->send_buf_wpos - r->info->send_buf_rpos) {
        r->info->send_buf_wpos += s_size;
        fy_event_mod(ev->conn, fy_http_count_event_loop, FY_EVOUT);
        return 0;
    }

    /* just send HTTP request to tair HTTP server, do NOT need to read resp data */
    close(c->fd);
    fy_request_next_module(r);
    return 0;
}
