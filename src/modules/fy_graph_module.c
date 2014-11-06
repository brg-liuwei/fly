#include "fy_graph_module.h"
#include "fy_conf.h"
#include "fy_logger.h"

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

static const char *fy_graph_addr;

static int fy_graph_module_init(fy_module *m, void *ptr)
{
    return 0;
}

static int fy_graph_module_conf(fy_module *m, void *ptr)
{
    fy_graph_addr = fy_conf_get_param("graph_addr");
    return 0;
}

static int fy_graph_task_submit(fy_task *task, void *request)
{
    fy_info     *info;
    fy_request  *r;

#ifdef FY_DEBUG
    fy_log_debug("fy_graph_task_submit\n");
#endif
    r = (fy_request *)request;
    info = r->info;
    info->graph_addr = (char *)fy_graph_addr;
    fy_request_next_module(r);
    return 0;
}




