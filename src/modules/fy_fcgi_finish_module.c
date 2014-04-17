#include "fy_fcgi_finish_module.h"
#include "fy_logger.h"
#include <stdlib.h>
#include <assert.h>

extern fy_task *null_task_list[];
extern fy_task null_task;

static int fy_fcgi_finish_task_submit(fy_task *, void *);

static fy_task fy_fcgi_finish_task = {
    FY_TASK_INIT("fy_fcgi_finish_task", null_task_list, fy_fcgi_finish_task_submit)
};

static fy_task *fy_fcgi_finish_tasks[] = {
    &fy_fcgi_finish_task,
    &null_task
};

fy_module fy_fcgi_finish_module = {
    FY_MODULE_INIT("fy_fcgi_finish_module",
            fy_fcgi_finish_tasks,
            NULL, NULL, NULL)
};

static int fy_fcgi_finish_task_submit(fy_task *task, void *request)
{
    fy_request  *r;

    assert(request != NULL);

    r = (fy_request *)request;

#ifdef FY_DEBUG
    fy_log_fmt("fy_fcgi_finish_request fd: %p\n", r);
    fy_log_fmt("fy_fcgi_finish_request fd: %d\n", r->fcgi_request->ipcFd);
#endif

    FCGX_Finish_r(r->fcgi_request);

#ifdef FY_DEBUG
    fy_log_fmt("FCGX_Finish_r");
#endif

    if (r->cln != NULL) {
        r->cln(r);
    }
    free(r);
    return 0;
}

