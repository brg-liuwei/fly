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
    fy_request_next_module(r);
    return 0;
}

// static int fy_fcgi_layout_task_submit(fy_task *task, void *request)
// {
//     jc_json_t     *qt_js, *err_js, *qt_data;
//     fy_request    *r;
//     const char    *err_str, *display_str, *errmsg = "";
// 
//     r = (fy_request *)request;
//     assert(r != NULL);
//     assert(r->info != NULL);
// 
//     if (r->info->graph_addr == NULL) {
// 	errmsg = "graph addr NULL";
//         goto error;
//     }
// 
//     if (r->info->graph_adid == NULL) {
// 	errmsg = "adid NULL";
//         goto error;
//     }
// 
//     qt_js = jc_json_create();
//     assert(qt_js != NULL);
// 
//     jc_json_add_num(qt_js, "errorno", 0);
//     jc_json_add_str(qt_js, "errormsg", "ok");
//     jc_json_add_num(qt_js, "expiredtime", fy_cur_sec() + 60);
// 
//     qt_data = jc_json_create();
//     assert(qt_data != NULL);
// 
//     jc_json_add_str(qt_data, "image", r->info->graph_addr);
//     jc_json_add_str(qt_data, "tracker", r->info->graph_tracker);
//     jc_json_add_str(qt_data, "id", r->info->graph_adid);
//     jc_json_add_num(qt_data, "duration", r->info->graph_duration);
// 
//     /* tracker1 */
//     jc_json_t *tracker1 = jc_json_create();
//     jc_json_add_str(tracker1, "provider", "AdMaster");
//     jc_json_add_str(tracker1, "url", "http://tracker.admaster.cn");
//     jc_json_add_str(tracker1, "event_type", "display");
// 
//     /* tracker2 */
//     jc_json_t *tracker2 = jc_json_create();
//     jc_json_add_str(tracker2, "provider", "qingtingfm");
//     jc_json_add_str(tracker2, "url", "http://www.qingting.fm");
//     jc_json_add_str(tracker2, "event_type", "click");
// 
//     /* track3 */
//     jc_json_t *tracker3 = jc_json_create();
//     jc_json_add_str(tracker3, "provider", "someothers");
//     jc_json_add_str(tracker3, "url", "https://somebody.cn");
//     jc_json_add_str(tracker3, "event_type", "display");
// 
//     /* tracks */
//     jc_json_add_array(qt_data, "trackers");
//     jc_json_add_json(qt_data, "trackers", tracker1);
//     jc_json_add_json(qt_data, "trackers", tracker2);
//     jc_json_add_json(qt_data, "trackers", tracker3);
// 
//     jc_json_add_json(qt_js, "data", qt_data);
// 
//     display_str = jc_json_str(qt_js);
//     FCGX_FPrintF(r->fcgi_request->out, "Status: 200 OK\r\n"
//             "Content-type: text/html\r\n"
//             "Content-Length: %d\r\n\r\n%s\r",
//             strlen(display_str), display_str);
// 
//     jc_json_destroy(qt_js);
//     fy_request_next_module(r);
//     return 0;
// 
// error:
//     err_js = jc_json_create();
//     assert(err_js != NULL);
// 
//     jc_json_add_num(err_js, "errorno", 1);
//     jc_json_add_str(err_js, "errormsg", errmsg);
//     jc_json_add_num(err_js, "expiredtime", fy_cur_sec() + 60);
//     jc_json_add_json(err_js, "data", jc_json_create());
//     err_str = jc_json_str(err_js);
// 
//     FCGX_FPrintF(r->fcgi_request->out, "Status: 200 OK\r\n"
//             "Content-type: text/html\r\n"
//             "Content-Length: %d\r\n\r\n%s\r",
//             strlen(err_str), err_str);
// 
//     jc_json_destroy(err_js);
//     fy_request_next_module(r);
//     return 0;
// }
