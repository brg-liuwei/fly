#include "fy_connection.h"
#include "fy_event.h"
#include "fy_logger.h"
#include "fy_time.h"
#include "fy_util.h"
#include "fy_conf.h"

#include "jc_type.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>

extern fy_pool_t  *fy_mem_pool;

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_adpos_task_submit(fy_task *, void *);
static int fy_adpos_module_init(fy_module *, void *);
static int fy_adpos_module_conf(fy_module *, void *);

static int fy_adpos_conn_handler(fy_event *ev, void *request);
static int fy_adpos_read_handler(fy_event *ev, void *request);
static int fy_adpos_write_handler(fy_event *ev, void *request);

typedef struct {
    int32_t magic;
    int32_t size;
    char    data[];
} fy_adpos_req_hdr;

static fy_task fy_adpos_task = {
    FY_TASK_INIT("fy_adpos_task", null_task_list, fy_adpos_task_submit)
};

static fy_task *fy_adpos_tasks[] = {
    &fy_adpos_task,
    &null_task,
};

fy_module fy_api_adpos_module = {
    FY_MODULE_INIT("fy_api_adpos_module",
            fy_adpos_tasks,
            fy_adpos_module_init,
            fy_adpos_module_conf)
};

static fy_event_loop *fy_adpos_event_loop;

static fy_conn_pool *fy_adpos_conn_pool;
static int fy_adpos_conn_pool_size;
static const char *fy_adpos_server_addr;
static int fy_adpos_server_port;

static const char *fy_api_adpos_uri;

static int fy_adpos_module_conf(fy_module *m, void *ptr)
{
    fy_adpos_conn_pool_size = fy_atoi(fy_conf_get_param("adpos_conn_pool_size"));
    fy_adpos_server_addr = fy_conf_get_param("adpos_server_addr");
    fy_adpos_server_port = fy_atoi(fy_conf_get_param("adpos_server_port"));
    fy_api_adpos_uri = fy_conf_get_param("adpos_api_uri");
    if (fy_adpos_conn_pool_size == 0
            || fy_adpos_server_addr == NULL
            || fy_adpos_server_port == 0)
    {
        fy_log_error("adpos conf error\n");
        return -1;
    }
    return 0;
}

static int fy_adpos_module_init(fy_module *module, void *ev_loop)
{
    int             i;
    fy_connection  *conn;

    fy_adpos_event_loop = (fy_event_loop *)ev_loop;
    fy_adpos_conn_pool = fy_create_conn_pool(fy_mem_pool,
            fy_adpos_conn_pool_size, 1);

    if (fy_adpos_conn_pool == NULL) {
        fy_log_error("fy_adpos_module create conn pool error\n");
        return -1;
    }

    for (i = 0; i != fy_adpos_conn_pool_size; ++i) {
        conn = fy_pop_connection(fy_adpos_conn_pool);
        if (fy_create_nonblocking_conn(conn, 
                    fy_adpos_server_addr, fy_adpos_server_port) == -1)
        {
            fy_log_error("fy_adpos_module: fy_create_nonblocking_conn errno: %d\n", errno);
            return -1;
        }
        conn->revent->handler = fy_adpos_read_handler;
        conn->wevent->handler = fy_adpos_conn_handler;
        if (fy_event_add(conn, fy_adpos_event_loop, FY_EVOUT) == -1) {
            fy_log_error("fy_adpos_module: fy_event_add errno: %d\n", errno);
            return -1;
        }
    }

    return 0;
}

static int fy_adpos_conn_handler(fy_event *ev, void *request)
{
    fy_connection *conn;

    assert(request == NULL);

    conn = ev->conn;
    conn->revent->handler = fy_adpos_read_handler;
    conn->wevent->handler = NULL;

    fy_push_connection(fy_adpos_conn_pool, conn);
    fy_event_mod(conn, fy_adpos_event_loop, FY_EVIN);

    return 0;
}

static int fy_adpos_read_handler(fy_event *ev, void *request)
{
    fy_request     *r;
    fy_connection  *c;

    r = (fy_request *)request;
    c = ev->conn;

    if (r == NULL) {
        if (c->fd != -1) {
            close(c->fd);
            c->fd = -1;
        }
        fy_push_err_conn(fy_adpos_conn_pool, c);
        fy_repair_conn_pool(fy_adpos_conn_pool, fy_adpos_event_loop,
                fy_adpos_conn_handler, fy_adpos_read_handler);
        return -1;
    }

    // TODO: read data and update cache
    return 0;
}

#define FY_BUF_APPEND(p, limit, fmt, ...) do { \
    int n = snprintf(p, limit, fmt, ##__VA_ARGS__); \
    if (n > limit) { \
        p += limit; \
        limit = 0; \
    } else { \
        p += n; \
        limit -= n; \
    } \
} while (0)

#define fy_cpymem(dst, src, n) (memcpy((void *)dst, (void *)src, n) + n)

static int fy_adpos_write_ready(void *request)
{
    const int         send_buf_size = 64;
    fy_request       *r;
    fy_adpos_req_hdr  reqhdr;

    assert(request != NULL);

    r = (fy_request *)request;
    if ((r->info->send_buf = fy_pool_alloc(r->pool, send_buf_size)) == NULL) {
        return -1;
    }
    r->info->send_buf_rpos = r->info->send_buf_wpos = r->info->send_buf;
    r->info->send_buf_end = r->info->send_buf + send_buf_size;

    reqhdr.magic = 0xCAFE;
    reqhdr.size = sizeof("GETZONE"); // just for test

    r->info->send_buf_wpos = fy_cpymem(r->info->send_buf_wpos, &reqhdr, sizeof(reqhdr));
    r->info->send_buf_wpos = fy_cpymem(r->info->send_buf_wpos, "GETZONE", sizeof("GETZONE"));
    return 0;
}

static int fy_adpos_write_handler(fy_event *ev, void *request)
{
    ssize_t         s_size;
    fy_request     *r;
    fy_connection  *c;

    if (fy_adpos_write_ready(request) == -1) {
        fy_request_next_module(request);
        return 0;
    }

    r = (fy_request *)request;
    c = ev->conn;

    s_size = send(c->fd, r->info->send_buf_rpos,
            r->info->send_buf_wpos - r->info->send_buf_rpos, 0);

    if (s_size == -1) {
        if (errno != EAGAIN) {
            close(c->fd);
            c->fd = -1;
            fy_push_err_conn(fy_adpos_conn_pool, c);
            fy_request_next_module(r);
        } else {
            fy_event_mod(ev->conn, fy_adpos_event_loop, FY_EVOUT);
        }
        return -1;
    }

    if (s_size != r->info->send_buf_wpos - r->info->send_buf_rpos) {
        r->info->send_buf_wpos += s_size;
    }

    fy_event_mod(ev->conn, fy_adpos_event_loop, FY_EVOUT);
    return 0;
}

static int fy_adpos_task_submit(fy_task *task, void *request)
{
    int             i, api_version;
    jc_json_t      *out, *sub;
    const char     *doc_uri;
    fy_request     *r;
    fy_connection  *conn;

    assert(request != NULL);

    r = (fy_request *)request;

    fy_repair_conn_pool(fy_adpos_conn_pool, fy_adpos_event_loop,
            fy_adpos_conn_handler, fy_adpos_read_handler);

    doc_uri = fy_fcgi_get_param("DOCUMENT_URI", r);
    if (doc_uri == NULL && strcmp(doc_uri, fy_api_adpos_uri) != 0) {
        fy_request_next_module(r);
        return 0;
    }

    api_version = fy_atoi(fy_fcgi_get_param("API_VERSION", r));

    out = jc_json_create();

    // for test
    for (i = 0; i != 3; ++i) {
        sub = jc_json_create();
        jc_json_add_num(sub, "posid", i);
        jc_json_add_str(sub, "posdesc", "pos_desc");
        jc_json_add_json(out, "adpos", sub);
    }

    switch(api_version) {
        case 5: // TODO: return V5 adpos list
            break;
        case 6: // TODO: return V6 adpos list
            break;
        default: // TODO: return version error
            break;
    }

    r->info->json_rc = out;
    fy_request_next_module(request);
    return 0;
}












