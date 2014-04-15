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

static int fy_fcgi_userinfo_task_submit(fy_task *, void *);
static int fy_fcgi_pageinfo_task_submit(fy_task *, void *);

static fy_task fy_fcgi_userinfo_task = {
    FY_TASK_INIT("fy_fcgi_userinfo_task", null_task_list, fy_fcgi_userinfo_task_submit)
};

static fy_task fy_fcgi_pageinfo_task = {
    FY_TASK_INIT("fy_fcgi_pageinfo_task", null_task_list, fy_fcgi_pageinfo_task_submit)
};

static fy_task *fy_fcgi_getinfo_tasks[] = {
    &fy_fcgi_userinfo_task,
    &fy_fcgi_pageinfo_task,
    &null_task
};

fy_module fy_fcgi_getinfo_module = {
    FY_MODULE_INIT("fy_fcgi_getinfo_module",
            fy_fcgi_getinfo_tasks,
            NULL, NULL, NULL)
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
        fy_log_fmt("%s = NULL\n", key);
#endif
        return NULL;
    }
#ifdef FY_DEBUG
    fy_log_fmt("%s = %s\n", key, p);
#endif
    return p;
}

static void fy_user_cleanup(fy_user *u)
{
    assert(u != NULL);

    if (u->freq != NULL) {
        free(u->freq);
        u->freq = NULL;
        u->freq_size = 0;
    }
    if (u->os != NULL) {
        free(u->os);
        u->os = NULL;
    }
    
    if (u->ua_vec != NULL) {
        free(u->ua_vec);
        u->ua_vec = NULL;
        u->ua_vec_size = 0;
    }
}

static int fy_fcgi_userinfo_task_submit(fy_task *task, void *request)
{
    fy_user      *u;
    const char    *p;
    fy_request   *r;
    size_t         e_size;

    r = (fy_request *)request;

    assert(r != NULL);
    assert(r->info != NULL);

#ifdef FY_DEBUG
    fy_log_fmt("fy_fcgi_userinfo_task_submit  invoked request = %p\n", r);
    fy_log_fmt("get user info\n");
#endif
    if ((r->info->usr = (fy_user *)calloc(1, sizeof(fy_user))) == NULL) {
        return -1;
    }

    u = r->info->usr;
    u->cln = fy_user_cleanup;

    /* get fcgi param of user */
    u->proxy = fy_atoi(fy_fcgi_get_param("USER_PROXY", r));

    /* YYID */
    u->yyid = fy_fcgi_get_param("YYID", r);

    /* SUV */
    u->suv = fy_fcgi_get_param("SUV", r);

    /* DEVICE */
    u->dev = fy_fcgi_get_param("DEVICEID", r);

    /* IP */
    u->ip = fy_fcgi_get_param("REAL_IP", r);

    /* REGION */
    u->region_num = fy_fcgi_get_param("AD_LOC_LAB", r);
    u->region_cn = fy_fcgi_get_param("AD_LOC", r);

    /* AGENT */
    u->agent = fy_fcgi_get_param("USER_AGENT", r);
    
    char *tmp_os = fy_get_os(u->agent);
    if ((u->os = (char *)calloc(strlen(tmp_os) + 1, sizeof(char))) != NULL) {
        strcpy(u->os, tmp_os);
    }

#ifdef FY_DEBUG
    fy_log_fmt("OS = %s\n", u->os);
#endif

    u->browser = fy_get_browser(u->agent);
#ifdef FY_DEBUG
    fy_log_fmt("BROWSER = %s\n", u->browser);
#endif
    u->lang = "";

    /* retargetting */
    u->retarget = fy_fcgi_get_param("RTG", r);

    /* FREQ info */
    if ((p = fy_fcgi_get_param("FREQUENCY", r)) != NULL) {
        e_size = strlen(p) + 1;
        if ((u->freq = (char *)calloc(e_size, sizeof(char))) != NULL) {
            u->freq_size = fy_url_decode(p, e_size, u->freq, e_size);
        }
    }

    /* invoke next module */
    fy_request_next_module(r);

    return 0;
}

static int fy_fcgi_pageinfo_task_submit(fy_task *task, void *request)
{
#if FY_DEBUG
    fy_log_fmt("get page info\n");
#endif

    char           *arg;
    fy_page       *p;
    fy_request    *r;

    r = (fy_request *)request;

    assert(r != NULL);
    assert(r->info != NULL);

    if ((r->info->pg = (fy_page *)calloc(1, sizeof(fy_page))) == NULL) {
        return -1;
    }

    p = r->info->pg;
    p->cln = fy_page_cleanup;

    /* get page id */
    p->id = fy_fcgi_get_param("PAGE_ID", r);

    /* get page type */
    if ((arg = fy_fcgi_get_param("PAGE_TYPE", r)) != NULL) {
        p->pg_type = (fy_page_type)atoi(arg);
        if (p->pg_type > PAGE_OTHER) {
            p->pg_type = PAGE_OTHER;
        }
    }

    /* get space position */
    if ((arg = fy_fcgi_get_param("SPACE_POSITION", r)) != NULL) {
        p->ad_pos = (fy_ad_pos)atoi(arg);
        if (p->ad_pos > OTHER_VIEW) {
            p->ad_pos = OTHER_VIEW;
        }
    }

    /* get url of HTTP GET */
    //测试版本使用GETURL,实际使用GET_URL
    p->get_url = fy_fcgi_get_param("GET_URL", r);
    if (p->get_url != NULL && (arg = strstr(p->get_url, "?")) != NULL) {
        *arg = '\0';
    }

    /* get ref_url */
    p->ref_url = fy_fcgi_get_param("REF_URL", r);

    /* request type */
    if ((arg = fy_fcgi_get_param("REQUEST_TYPE", r)) != NULL) {
        p->req_type = (fy_req_type)atoi(arg);
        if (p->req_type > REQ_OTHER) {
            p->req_type = REQ_OTHER;
        }
    }

    /* response type */
    if ((arg = fy_fcgi_get_param("RESPONSE_TYPE", r)) != NULL) {
        p->res_type = (fy_res_type)atoi(arg);
        if (p->res_type > RES_OTHER) {
            p->res_type = RES_OTHER;
        }
    }

    /* get content params */
    p->fy_ct_text.color_bg = fy_fcgi_get_param("COLOR_BG", r);
    p->fy_ct_text.color_border = fy_fcgi_get_param("COLOR_BORDER", r);
    p->fy_ct_text.color_fg_title = fy_fcgi_get_param("COLOR_FG_TITLE", r);
    p->fy_ct_text.color_fg_text = fy_fcgi_get_param("COLOR_FG_TEXT", r);
    p->fy_ct_text.color_fg_link = fy_fcgi_get_param("COLOR_FG_LINK", r);

    p->fy_ct_text.font_size = fy_atoi(fy_fcgi_get_param("FONT_SIZE", r));
    p->fy_ct_text.text_size = fy_atoi(fy_fcgi_get_param("TEXT_LEN", r));
    p->fy_ct_text.ad_num = fy_atoi(fy_fcgi_get_param("TEXT_NUM", r));

    p->fy_ct_text.width = fy_atoi(fy_fcgi_get_param("CONTENTTMP_WIDTH", r));
    p->fy_ct_text.height = fy_atoi(fy_fcgi_get_param("CONTENTTMP_HEIGHT", r));

    p->fy_ct_text.tp_name = fy_fcgi_get_param("CONTENTTMP_NAME", r);

    fy_request_next_module(r);
    return 0;
}

