#include "fy_fcgi_getinfo_module.h"
#include "fy_framework.h"
#include "fy_info.h"
#include "fy_logger.h"
#include "fy_util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_fcgi_getinfo_task_submit(fy_task *task, void *request);

static fy_task fy_fcgi_getinfo_task = {
    FY_TASK_INIT("fy_fcgi_getinfo_task", null_task_list, fy_fcgi_getinfo_task_submit)
};

static fy_task *fy_fcgi_getinfo_tasks[] = {
    &fy_fcgi_getinfo_task,
    &null_task
};

fy_module fy_fcgi_getinfo_module = {
    FY_MODULE_INIT("fy_fcgi_getinfo_module",
            fy_fcgi_getinfo_tasks, NULL, NULL)
};

#define FY_OS(haystack, needle, os) \
    do { \
        if (strstr(haystack, needle) != NULL) { \
            return os; \
        } \
    } while (0)

static char *fy_get_os(const char *p)
{
    if (p == NULL) {
        return "";
    }

    FY_OS(p, "Windows NT 6.2", "Windows8");
    FY_OS(p, "Windows NT 6.1", "Windows7");
    FY_OS(p, "Windows NT 6.0", "Vista");
    FY_OS(p, "Windows NT 5.1", "WindowsXP");
    FY_OS(p, "Windows NT 5.0", "Windows2000");
    FY_OS(p, "Windows NT 5.2", "WindowsServer2003");
    FY_OS(p, "Windows NT 4.0", "WindowsNT4");
    FY_OS(p, "Win 9x 4.9", "WindowsME");
    FY_OS(p, "Windows 98", "Windows98");
    FY_OS(p, "iPad", "iOS");
    FY_OS(p, "iPhone", "iOS");
    FY_OS(p, "Mac", "MacOS");
    FY_OS(p, "Android", "Android");
    FY_OS(p, "Google Web", "GoogleWebOS");
    FY_OS(p, "Linux", "Linux");
    return "";
}

#define FY_BRW(haystack, needle, browser) \
    do { \
        if (strstr(haystack, needle) != NULL) { \
            return browser; \
        } \
    } while (0)

static char *fy_get_browser(const char *p)
{
    if (p == NULL) {
        return "";
    }

    FY_BRW(p, "360SE", "360SE");
    FY_BRW(p, "Chrome", "Chrome");
    FY_BRW(p, "Firefox", "Firefox");
    FY_BRW(p, "Mozilla", "Mozilla");
    FY_BRW(p, "Safari", "Safari");
    FY_BRW(p, "TencentTraveler", "TencentTraveler");
    FY_BRW(p, "Konqueror", "Konqueror");
    FY_BRW(p, "Navigator", "Navigator");
    FY_BRW(p, "Netscape", "Netscape");
    FY_BRW(p, "Opera", "Opera");
    FY_BRW(p, "IE 9", "IE9");
    FY_BRW(p, "IE 8", "IE8");
    FY_BRW(p, "IE 7", "IE7");
    FY_BRW(p, "IE 6", "IE6");
    FY_BRW(p, "IE 5", "IE5");
    FY_BRW(p, "IE 4", "IE4");
    FY_BRW(p, "IE 3", "IE3");
    FY_BRW(p, "SE 2.X MetaSr", "SE2.XMetaSr");
    FY_BRW(p, "TheWorld", "TheWorld");
    FY_BRW(p, "Maxthon", "Maxthon");
    FY_BRW(p, "MyIE2", "MyIE2");
    return "";
}

static char *fy_fcgi_get_param(const char *key, fy_request *r)
{
    char   *p;

    p = FCGX_GetParam(key, r->fcgi_request->envp);
    if (p == NULL || p[0] == '\0') {
#ifdef FY_DEBUG
        fy_log_debug("%s = NULL\n", key);
#endif
        return NULL;
    }
#ifdef FY_DEBUG
    fy_log_debug("%s = %s\n", key, p);
#endif
    return p;
}

static int fy_fcgi_getinfo_task_submit(fy_task *task, void *request)
{
#if FY_DEBUG
    fy_log_debug("get info\n");
#endif

    char          *arg;
    fy_request    *r;

    r = (fy_request *)request;

    assert(r != NULL);
    assert(r->info != NULL);

    /* request type */
    if ((arg = fy_fcgi_get_param("REQUEST_TYPE", r)) != NULL) {
        r->info->req_type = atoi(arg);
    }

    /* response type */
    if ((arg = fy_fcgi_get_param("RESPONSE_TYPE", r)) != NULL) {
        r->info->res_type = atoi(arg);
    }

    fy_request_next_module(r);
    return 0;
}

