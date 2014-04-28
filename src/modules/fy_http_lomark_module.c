#include "fy_http_lomark_module.h"
#include "fy_connection.h"
#include "fy_event.h"
#include "fy_logger.h"
#include "fy_time.h"
#include "fy_util.h"
#include "fy_conf.h"
#include "fy_md5.h"

#include <stdarg.h>
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

static int fy_http_lomark_task_submit(fy_task *, void *);
static int fy_create_non_blocking_socket(fy_connection *);
static int fy_http_lomark_module_init(fy_module *, void *);
static int fy_http_lomark_module_conf(fy_module *, void *);
static int fy_http_lomark_conn_handler(fy_event *, void *);
static int fy_http_lomark_read_handler(fy_event *, void *);
static int fy_http_lomark_write_handler(fy_event *, void *);

static fy_task fy_http_lomark_task = {
    FY_TASK_INIT("fy_http_lomark_task", null_task_list, fy_http_lomark_task_submit)
};

static fy_task *fy_http_lomark_tasks[] = {
    &fy_http_lomark_task,
    &null_task
};

fy_module fy_http_lomark_module = {
    FY_MODULE_INIT("fy_http_lomark_module",
            fy_http_lomark_tasks,
            fy_http_lomark_module_init,
            fy_http_lomark_module_conf)
};

static fy_event_loop *fy_http_event_loop;
static fy_conn_pool *fy_http_lomark_conn_pool;

static size_t fy_http_lomark_conn_pool_size;;
static const char *fy_http_lomark_addr;
static const char *fy_http_lomark_location;
static int fy_http_lomark_port;
static const char *fy_http_lomark_key;
static const char *fy_http_lomark_secret_key;

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
        conn->revent->handler = fy_http_lomark_read_handler;
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
    fy_http_lomark_location = fy_conf_get_param("http_conn_location");
    fy_http_lomark_port = fy_atoi(fy_conf_get_param("http_conn_port"));
    fy_http_lomark_key = fy_conf_get_param("lomark_key");
    fy_http_lomark_secret_key = fy_conf_get_param("lomark_secret_key");

    if (fy_http_lomark_conn_pool_size == 0 
            || fy_http_lomark_addr == NULL
            || fy_http_lomark_location == NULL
            || fy_http_lomark_port == 0
            || fy_http_lomark_key == NULL)
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
    if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) == -1) {
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
        fy_log_debug("connect lomark EINPROGRESS\n");
    } else {
        fy_log_debug("connect lomark seccessful\n");
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
    fy_log_debug("lomark conn ok\n");
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

    fy_log_debug("lomark conn repair\n");
    if (fy_http_lomark_conn_pool->err_list != NULL) {
        err_conn = fy_pop_err_conn(fy_http_lomark_conn_pool);
        if (err_conn == NULL) {
            return;
        }
        if (fy_create_non_blocking_socket(err_conn) == -1) {
            fy_push_err_conn(fy_http_lomark_conn_pool, err_conn);
            return;
        }
        err_conn->revent->handler = fy_http_lomark_read_handler;
        err_conn->wevent->handler = fy_http_lomark_conn_handler;
        if  (fy_event_add(err_conn, fy_http_event_loop, FY_EVOUT) == -1) {
            fy_log_error("fy_http_lomark_conn_repair::fy_event_add errno: %d\n", errno);
            close(err_conn->fd);
            err_conn->fd = -1;
            fy_push_err_conn(fy_http_lomark_conn_pool, err_conn);
        }
    }
}

#define FY_BUF_APPEND(p, limit, fmt, ...) do { \
    len = snprintf(p, limit, fmt, ##__VA_ARGS__); \
    limit -= len; \
    p += len; \
} while (0)

static int fy_http_lomark_set_send_buf(fy_request *r)
{
    char        *w, *sign_buf_p, *param;
    char         sign_buf[2048];
    size_t       n, sign_buf_n, len;
    time_t       cur_sec;
    u_char       md5[17];
    fy_md5_ctx   md5_ctx;

    assert(r->info->send_buf == NULL);

    if ((r->info->send_buf = fy_pool_alloc(r->pool, 2048)) == NULL) {
        return -1;
    }
    r->info->send_buf_rpos = r->info->send_buf_wpos = r->info->send_buf;
    r->info->send_buf_end = r->info->send_buf + 2048;

    w = r->info->send_buf_wpos;
    n = r->info->send_buf_end - w;

    /* GET */
    FY_BUF_APPEND(w, n, "GET ");

    /* location */
    FY_BUF_APPEND(w, n, "%s ", fy_http_lomark_location);

    if (w >= r->info->send_buf_end) {
        fy_log_error("buf limit\n");
        return -1;
    }

    sign_buf_p = sign_buf;
    sign_buf_n = sizeof(sign_buf);

    /* get param with dict sequence */

    /* adnum: lomark一次请求返回的广告数量，默认值为10 */
    FY_BUF_APPEND(w, n, "?adnum=1");
    FY_BUF_APPEND(sign_buf_p, sign_buf_n, "adnum=1");
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* adspacetype: 1,Banner; 2,插屏; 3,全屏 */
    FY_BUF_APPEND(w, n, "&adspacetype=3");
    FY_BUF_APPEND(sign_buf_p, sign_buf_n, "adspacetype=3");
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* ah: 广告高 */
    param = fy_fcgi_get_param("AH", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&ah=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "ah=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    } else {
        /* default value */
        FY_BUF_APPEND(w, n, "&ah=960");
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "ah=960");
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* appid */
    FY_BUF_APPEND(w, n, "&appid=qingtingfm");
    FY_BUF_APPEND(sign_buf_p, sign_buf_n, "appid=qingtingfm");
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* appname: 暂时不填 */

    /* aw: 广告宽 */
    param = fy_fcgi_get_param("AW", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&aw=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "aw=%s", param);
    } else {
        /* default value */
        FY_BUF_APPEND(w, n, "&aw=720");
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "aw=720");
    }
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* category: 1:工具; 2:游戏; 3:阅读; 4:娱乐; 5:商旅; 6:财经
     *           7:汽车; 8:女性; 9:新闻; 10:生活; 11:体育; 12:其他
     */
    FY_BUF_APPEND(w, n, "&category=1");
    FY_BUF_APPEND(sign_buf_p, sign_buf_n, "category=1");
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* client: 1:Android, 2:iOS, 3:Windows Phone */
    param = fy_fcgi_get_param("CLIENT", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&client=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "client=%s", param);
    } else {
        /* default value */
        FY_BUF_APPEND(w, n, "&client=4");
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "client=4");
    }
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* devicenum: 设备型号*/
    param = fy_fcgi_get_param("DEVICENUM", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&devicenum=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "devicenum=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* devicetype: 设备类型, 1:电话; 2:平板电脑*/
    param = fy_fcgi_get_param("DEVICETYPE", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&devicetype=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "devicetype=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* format: 返回的结果类型: json, xml */
    FY_BUF_APPEND(w, n, "&format=json");
    FY_BUF_APPEND(sign_buf_p, sign_buf_n, "format=json");
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* ip: 设备ip地址 */
    param = fy_fcgi_get_param("IP", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&ip=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "ip=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* key: lomark所分配的key，测试key为1000 */
    FY_BUF_APPEND(w, n, "&key=%s", fy_http_lomark_key);
    FY_BUF_APPEND(sign_buf_p, sign_buf_n, "key=%s", fy_http_lomark_key);
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* lat: 设备所在的纬度*/
    param = fy_fcgi_get_param("LAT", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&lat=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "lat=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* long: 设备所在的经度*/
    param = fy_fcgi_get_param("LONG", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&long=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "long=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* net: 设备所在的网络类型 */
    param = fy_fcgi_get_param("NET", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&net=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "net=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* operator:设备运营商: 1,移动; 2,联通; 3,电信; 4,其他 */
    param = fy_fcgi_get_param("OPERATOR", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&operator=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "operator=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* os_version:设备操作系统版本 */
    param = fy_fcgi_get_param("OS_VERSION", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&os_version=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "os_version=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* ph:设备屏幕高度 */
    param = fy_fcgi_get_param("PH", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&ph=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "ph=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* pw:设备屏幕宽度 */
    param = fy_fcgi_get_param("PW", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&pw=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "pw=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* sdkversion: SDK版本号 */
    param = fy_fcgi_get_param("SDKVERSION", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&sdkversion=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "sdkversion=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* timestamp: 请求发送的时间 */
    cur_sec = fy_cur_sec();
    FY_BUF_APPEND(w, n, "&timestamp=%ld", cur_sec);
    FY_BUF_APPEND(sign_buf_p, sign_buf_n, "timestamp=%ld", cur_sec);
    if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* ua:用户代理 */
    param = fy_fcgi_get_param("UA", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&ua=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "ua=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* uuid: 设备唯一标识 */
    param = fy_fcgi_get_param("UUID", r);
    if (param != NULL) {
        FY_BUF_APPEND(w, n, "&uuid=%s", param);
        FY_BUF_APPEND(sign_buf_p, sign_buf_n, "uuid=%s", param);
        if (w >= r->info->send_buf_end || sign_buf_p - &sign_buf[0] >= 2048) {
            fy_log_error("buf limit\n");
            return -1;
        }
    }

    /* append secret key */
    FY_BUF_APPEND(sign_buf_p, sign_buf_n, "%s", fy_http_lomark_secret_key);
    if (sign_buf_p - &sign_buf[0] >= 2048) {
        fy_log_error("buf limit\n");
        return -1;
    }

    /* md5 */
    fy_md5_init(&md5_ctx);
    fy_md5_update(&md5_ctx, sign_buf, sign_buf_n);
    fy_md5_final(md5, &md5_ctx);
    md5[16] = '\0';

    FY_BUF_APPEND(w, n, "&sign=%s", md5);
    if (w >= r->info->send_buf_end) {
        fy_log_error("buf limit\n");
        return -1;
    }

    r->info->send_buf_wpos = w;
    r->info->send_buf_rpos = r->info->send_buf;

    return 0;
}

static int fy_http_lomark_read_handler(fy_event *ev, void *request)
{
    /* recv http response */
    fy_connection   *c;

    fy_log_debug("http lomark read handler, request: %p\n", request);
    c = ev->conn;
    if (request == NULL) {
        close(c->fd);
        fy_push_err_conn(fy_http_lomark_conn_pool, c);
        return -1;
    }
    fy_request_next_module(request);
    return 0;
}

static int fy_http_lomark_write_handler(fy_event *ev, void *request)
{
    /* send http request */
    fy_request *r;

    fy_log_debug("http lomark write handler\n");
    r = (fy_request *)request;
    *r->info->send_buf_wpos = '\0';
    fy_log_debug("send_buf: %s\n", r->info->send_buf_rpos);
    fy_request_next_module(r);
    return 0;
}

static int fy_http_lomark_task_submit(fy_task *task, void *request)
{
    fy_connection  *conn;

    assert(request != NULL);

    fy_log_debug("fy_http_lomark_task_submit\n");
    /* try to repair one error connection */
    fy_http_lomark_conn_repair();

    if ((conn = fy_pop_connection(fy_http_lomark_conn_pool)) == NULL) {
        fy_log_debug("http lomark conn pool empty\n");
        fy_request_next_module(request);
        return -1;
    }

    /* get param and generate data package in request */
    if (fy_http_lomark_set_send_buf(request) == -1) {
        fy_request_next_module(request);
        return -1;
    }
    return fy_event_mod(conn, fy_http_event_loop, FY_EVOUT);
}






