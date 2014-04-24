#include "fy_fcgi_layout_module.h"
#include "fy_time.h"
#include "fy_util.h"
#include "fy_logger.h"

#include <string.h>
#include <assert.h>

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_fcgi_layout_task_submit(fy_task *, void *);
static int fy_fcgi_layout_module_init(fy_module *, void *);

static fy_task fy_fcgi_layout_task = {
    FY_TASK_INIT("fy_fcgi_layout_task", null_task_list, fy_fcgi_layout_task_submit)
};

static fy_task *fy_fcgi_layout_tasks[] = {
    &fy_fcgi_layout_task,
    &null_task
};

fy_module fy_fcgi_layout_module = {
    FY_MODULE_INIT("fy_fcgi_layout_module",
            fy_fcgi_layout_tasks,
            fy_fcgi_layout_module_init, NULL)
};

static int fy_fcgi_layout_module_init(fy_module *m, void *data){
    return 0;
}

static int fy_fcgi_layout_task_submit(fy_task *task, void *request)
{
    fy_request   *r;

    r = (fy_request *)request;
    assert(r != NULL);
    assert(r->info != NULL);

    FCGX_FPrintF(r->fcgi_request->out, "Status: 200 OK\r\nContent-type: text/html\r\nContent-Length: %d\r\n\r\n%s\r",
            strlen("hello world"), "hello world");
    fy_request_next_module(r);
    return 0;
}

