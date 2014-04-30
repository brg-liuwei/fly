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
    fy_request   *r;
    jc_val_t     *val;
    jc_json_t    *lomark_js, *qt_js, *qt_data;

    const char err_js[] = "{\"errorno\":\"1\",\"errormsg\":\"no ad return\",\"expiredtime\":1398502624,\"data\":{}}";

    r = (fy_request *)request;
    assert(r != NULL);
    assert(r->info != NULL);

    if (r->info->lomark_json_str == NULL) {
        goto no_ad;
    }
    lomark_js = jc_json_parse(r->info->lomark_json_str);
    if (lomark_js == NULL) {
        fy_log_error("layout module parse json error: \n%s\n", r->info->lomark_json_str);
        goto no_ad;
    }
    val = jc_json_find(lomark_js, "status");
    if (val == NULL || val->type != JC_STR 
            || strncmp("100", val->data.s->body, val->data.s->size) != 0)
    {
        fy_log_error("layout module, lomark return json illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    /* get data */
    /*
     * {"status":"100","msg":"成功","data":{"ad":[{"aid":"503","ts":"1396253418","sessionid":"b35ca15128ba460ca943ed0fa61b3463","creative":{"cid":"503","ts":"1396253418","displayinfo":{"type":"1","img":"http://rm.lomark.cn/Upload/AdMaterials/5a87fac11ffb4d399e3b05e07d44fba3_640X1136.jpg","schema":"E37B7DAA-11DF-4D29-B709-51BF5B73D338"},"clkinfos":[{"type":"1","schema":"E37B7DAA-11DF-4D29-B709-51BF5B73D338","url":"http://api.lomark.cn/v2/clk/http%3A%3A%2Fsite.lomark.cn%2Fbyd%2Findex.html=b35ca15128ba460ca943ed0fa61b3463=json=10=1398743107"}],"trackers":{"clicks":[{"schema":"E37B7DAA-11DF-4D29-B709-51BF5B73D338","urls":["http://api.lomark.cn/v2/cbclk/b35ca15128ba460ca943ed0fa61b3463=json=10=1398743107"]}],"display":{"urls":["http://api.lomark.cn/v2/cbimp/b35ca15128ba460ca943ed0fa61b3463=json=10=1398743107"]}}}}]}}
     * */

    val = jc_json_find(lomark_js, "data");
    if (val == NULL || val->type != JC_JSON || val->data.j == NULL) {
        fy_log_error("data->layout module, lomark return json illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    jc_json_t  *data;
    data = val->data.j;
    val = jc_json_find(data, "ad");
    if (val == NULL || val->type != JC_ARRAY 
            || val->data.a == NULL || val->data.a->size != 1)
    {
        fy_log_error("ad->layout module, lomark return json illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    val = val->data.a->value[0];  /* val: {"aid":"503","ts":...} */
    if (val == NULL || val->type != JC_JSON || val->data.j == NULL) {
        fy_log_error("ad json->layout module, lomark return json illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    data = val->data.j;
    val = jc_json_find(data, "aid");
    if (val == NULL || val->type != JC_STR) {
        fy_log_error("aid->layout module, no aid number:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    const char *aid;
    aid = val->data.s->body;

    jc_json_t *creative;
    val = jc_json_find(data, "creative");
    if (val == NULL || val->type != JC_JSON || val->data.j == NULL) {
        fy_log_error("creative->layout module, lomark return json illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }
    creative = val->data.j;

    /* display info */
    val = jc_json_find(creative, "displayinfo");
    if (val == NULL || val->type != JC_JSON || val->data.j == NULL) {
        fy_log_error("displayinfo->layout module, lomark return json illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    jc_json_t *display;
    display = val->data.j;
    val = jc_json_find(display, "type");
    if (val == NULL || val->type != JC_STR || val->data.s->body[0] != '1') {
        fy_log_error("displayinfo->layout module, type illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }
    val = jc_json_find(display, "img");
    if (val == NULL || val->type != JC_STR) {
        fy_log_error("displayinfo->layout module, cannot find img:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    const char *img_addr;
    img_addr = val->data.s->body;
#ifdef FY_DEBUG
    fy_log_debug("img addr: %s\n", img_addr);
#endif

    /* trackers */
    val = jc_json_find(creative, "trackers");
    if (val == NULL || val->type != JC_JSON || val->data.j == NULL) {
        fy_log_error("trackers->layout module, no trackers in creative:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    val = jc_json_find(val->data.j, "display");
    if (val == NULL || val->type != JC_JSON || val->data.j == NULL) {
        fy_log_error("trackers display->layout module, no display in trackers:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    val = jc_json_find(val->data.j, "urls");
    if (val == NULL || val->type != JC_ARRAY || val->data.a->size != 1) {
        fy_log_error("trackers display->layout module, urls in display illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    val = val->data.a->value[0];
    if (val == NULL || val->type != JC_STR) {
        fy_log_error("trackers display urls->layout module, urls str illegal:\n%s\n", r->info->lomark_json_str);
        goto free_lomark_js;
    }

    const char *tracker_addr;
    tracker_addr = val->data.s->body;

    if ((qt_js = jc_json_create()) == NULL) {
        goto free_lomark_js;
    }
    if ((qt_data = jc_json_create()) == NULL) {
        jc_json_destroy(qt_js);
        goto free_lomark_js;
    }

    jc_json_add_str(qt_js, "errorno", "0");
    jc_json_add_str(qt_js, "errormsg", "ok");
    jc_json_add_num(qt_js, "expiredtime", (double)fy_cur_sec());

    jc_json_add_str(qt_data, "image", img_addr);
    jc_json_add_str(qt_data, "tracker", tracker_addr);
    jc_json_add_str(qt_data, "id", aid);

    jc_json_add_json(qt_js, "data", qt_data);

    const char *ret_str;

    ret_str = jc_json_str(qt_js);

    FCGX_FPrintF(r->fcgi_request->out, "Status: 200 OK\r\n"
            "Content-type: text/html\r\n"
            "Content-Length: %d\r\n\r\n%s\r",
            strlen(ret_str), ret_str);

    jc_json_destroy(lomark_js);
    jc_json_destroy(qt_js);

    fy_request_next_module(r);
    return 0;

free_lomark_js:
    jc_json_destroy(lomark_js);
no_ad:
    FCGX_FPrintF(r->fcgi_request->out, "Status: 200 OK\r\n"
            "Content-type: text/html\r\n"
            "Content-Length: %d\r\n\r\n%s\r",
            sizeof(err_js), err_js);
    fy_request_next_module(r);
    return 0;

}




