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

