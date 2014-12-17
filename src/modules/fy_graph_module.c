#include "fy_graph_module.h"
#include "fy_conf.h"

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
    return 0;
}

static int fy_graph_task_submit(fy_task *task, void *request)
{
    fy_info     *info;
    fy_request  *r;

    r = (fy_request *)request;

    info = r->info;
    info->graph_duration = fy_graph_duration;
    info->graph_tracker = (char *)fy_graph_tracker;
    info->graph_adid  = (char *)fy_graph_adid;
    info->graph_addr = (char *)fy_graph_addr;
    fy_request_next_module(r);
    return 0;
}




