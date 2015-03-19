#include "fy_conf.h"
#include "fy_util.h"
#include "fy_time.h"
#include "fy_logger.h"
#include "fy_framework.h"

#include "jc_type.h"

#include <assert.h>

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_c_module_init(fy_module *, void *);
static int fy_c_module_conf(fy_module *, void *);
static int fy_c_task_submit(fy_task *, void *);

static fy_task fy_c_task = {
    FY_TASK_INIT("fy_c_task", null_task_list, fy_c_task_submit)
};

static fy_task *fy_c_tasks[] = {
    &fy_c_task,
    &null_task
};

fy_module fy_c_module = {
    FY_MODULE_INIT("fy_c_module",
            fy_c_tasks,
            fy_c_module_init,
            fy_c_module_conf)
};

static const char *fy_data;
static const char *fy_c_uri;

static int fy_c_module_init(fy_module *m, void *ptr)
{
    return 0;
}

static int fy_c_module_conf(fy_module *m, void *ptr)
{
    int size;

    size = fy_atoi(fy_conf_get_param("size"));
    if (size <= 0) {
        fy_log_error("c module, size error\n");
        return -1;
    }

    jc_json_t *jc = jc_json_create();
    jc_json_add_array(jc, "data");

    int i, percent;
    char key_name[64], key_url[64], key_percent[64], key_region[64];
    const char *name, *url, *regions;
    jc_json_t *elem;

    for (i = 0; i != size; ++i) {
        snprintf(key_name, 64, "name%d", i);
        snprintf(key_url, 64, "url%d", i);
        snprintf(key_percent, 64, "percent%d", i);
        snprintf(key_region, 64, "region%d", i);

        name = fy_conf_get_param(key_name);
        url = fy_conf_get_param(key_url);
        percent = fy_atoi(fy_conf_get_param(key_percent));
        regions = fy_conf_get_param(key_region);

        if (name == NULL || url == NULL 
                || percent < 0 || percent > 1000 
                || regions == NULL)
        {
            continue;
        }

        elem = jc_json_create();
        jc_json_add_str(elem, "name", name);
        jc_json_add_str(elem, "url", url);
        jc_json_add_num(elem, "percent", percent);
        jc_json_add_str(elem, "region", regions);

        jc_json_add_json(jc, "data", elem);
    }

    jc_json_add_str(jc, "errormsg", "ok");
    jc_json_add_num(jc, "errorno", 0);
    fy_data = jc_json_str(jc);

    if ((fy_c_uri = fy_conf_get_param("c_uri")) == NULL) {
        fy_log_error("c module, missing c_uri\n");
        return -1;
    }

    return 0;
}

static int fy_c_task_submit(fy_task *task, void *request)
{
    fy_info     *info;
    jc_json_t   *c_data;
    fy_request  *r;
    const char  *doc_uri, *phone_type;

    r = (fy_request *)request;

    doc_uri = fy_fcgi_get_param("DOCUMENT_URI", r);
    if (doc_uri == NULL || strcmp(doc_uri, fy_c_uri) != 0) {
        fy_request_next_module(r);
        return 0;
    }

    info = r->info;

    assert(info->json_rc == NULL);

    c_data = jc_json_parse(fy_data);
    jc_json_add_num(c_data, "expiredtime", fy_cur_sec() + 60);

    info->json_rc = c_data;
    fy_request_next_module(r);
    return 0;
}
