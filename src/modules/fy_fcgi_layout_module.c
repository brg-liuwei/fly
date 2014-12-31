#include "fy_fcgi_layout_module.h"
#include "fy_time.h"
#include "fy_util.h"
#include "fy_logger.h"

#include "jc_type.h"

#include <stdio.h>
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
    size_t       output_n;
    jc_json_t   *err_js;
    fy_request  *r;
    const char  *output;

    r = (fy_request *)request;
    assert(r != NULL);
    assert(r->info != NULL);

    if (r->info->json_rc != NULL) {
        output = jc_json_str_n(r->info->json_rc, &output_n);
        FCGX_FPrintF(r->fcgi_request->out, "Status: 200 OK\r\n"
                "Content-type: text/html\r\n"
                "Content-Length: %lu\r\n\r\n%s\r",
                output_n, output);
        fy_request_next_module(r);
        return 0;
    }

error:
    err_js = jc_json_create();
    assert(err_js != NULL);

    jc_json_add_num(err_js, "errorno", 1);
    jc_json_add_str(err_js, "errormsg", "internal error");
    jc_json_add_num(err_js, "expiredtime", fy_cur_sec());
    jc_json_add_json(err_js, "data", jc_json_create());
    output = jc_json_str_n(err_js, &output_n);
    FCGX_FPrintF(r->fcgi_request->out, "Status: 200 OK\r\n"
            "Content-type: text/html\r\n"
            "Content-Length: %lu\r\n\r\n%s\r",
            output_n, output);
    r->info->json_rc = err_js;
    fy_request_next_module(r);
    return 0;
}

