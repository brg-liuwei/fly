#include "fy_graph_module.h"
#include "fy_conf.h"
#include "fy_logger.h"
#include "fy_util.h"
#include "fy_time.h"

#include "jc_type.h"

#include <assert.h>

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_graph_module_init(fy_module *, void *);
static int fy_graph_module_conf(fy_module *, void *);
static int fy_graph_task_submit(fy_task *, void *);

static fy_task fy_graph_task = {
    FY_TASK_INIT("fy_graph_task", null_task_list, fy_graph_task_submit)
};

static fy_task *fy_graph_tasks[] = {
    &fy_graph_task,
    &null_task
};

fy_module fy_graph_module = {
    FY_MODULE_INIT("fy_graph_module",
            fy_graph_tasks,
            fy_graph_module_init,
            fy_graph_module_conf)
};

static int fy_graph_duration;
static const char *fy_graph_tracker;
static const char *fy_graph_adid;
static const char *fy_graph_addr;

static const char *fy_graph_uri;

static int fy_graph_module_init(fy_module *m, void *ptr)
{
    return 0;
}

static int fy_graph_module_conf(fy_module *m, void *ptr)
{
    const char *ch_duration;

    ch_duration = fy_conf_get_param("duration");
    if (ch_duration == NULL
            || (fy_graph_duration = atoi(ch_duration)) == 0)
    {
        fy_graph_duration = 2; //default value
    }
    if ((fy_graph_tracker = fy_conf_get_param("tracker")) == NULL) {
        fy_graph_tracker = "";
    }
    fy_graph_adid = fy_conf_get_param("adid");
    fy_graph_addr = fy_conf_get_param("graph_addr");
    if ((fy_graph_uri = fy_conf_get_param("graph_uri")) == NULL) {
        fy_log_error("graph module, missing graph uri\n");
        return -1;
    }
    return 0;
}

static int fy_graph_task_submit(fy_task *task, void *request)
{
    fy_info     *info;
    jc_json_t   *graph_data;
    fy_request  *r;
    const char  *doc_uri;

    r = (fy_request *)request;

    doc_uri = fy_fcgi_get_param("DOCUMENT_URI", r);
    if (doc_uri == NULL || strcmp(doc_uri, fy_graph_uri) != 0) {
        fy_request_next_module(r);
        return 0;
    }

    info = r->info;

    assert(info->json_rc == NULL);

    if (fy_graph_addr == NULL) {
        goto error;
    }

    graph_data = jc_json_create();
    jc_json_add_num(graph_data, "duration", fy_graph_duration);
    jc_json_add_str(graph_data, "tracker", fy_graph_tracker);
    jc_json_add_str(graph_data, "id", fy_graph_adid);
    jc_json_add_str(graph_data, "image", fy_graph_addr);

    info->json_rc = jc_json_create();
    jc_json_add_json(info->json_rc, "data", graph_data);
    jc_json_add_num(info->json_rc, "expiredtime", fy_cur_sec() + 60);
    jc_json_add_num(info->json_rc, "errorno", 0);
    jc_json_add_str(info->json_rc, "errormsg", "ok");

    fy_request_next_module(r);
    return 0;

error:
    info->json_rc = jc_json_create();
    jc_json_add_json(info->json_rc, "data", jc_json_create());
    jc_json_add_num(info->json_rc, "expiredtime", fy_cur_sec() + 60);
    jc_json_add_num(info->json_rc, "errorno", 1);
    jc_json_add_str(info->json_rc, "errormsg", "no loading page ad");

    fy_request_next_module(r);
    return 0;
}




