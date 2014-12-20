#include "fy_logger.h"
#include "fy_time.h"
#include "fy_util.h"
#include "fy_conf.h"

#include "jc_type.h"

#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_adfilter_task_submit(fy_task *, void *);
static int fy_adfilter_module_init(fy_module *, void *);
static int fy_adfilter_module_conf(fy_module *, void *);

static fy_task fy_adfilter_task = {
    FY_TASK_INIT("fy_adfilter_task", null_task_list, fy_adfilter_task_submit)
};

static fy_task *fy_adfilter_tasks[] = {
    &fy_adfilter_task,
    &null_task,
};

fy_module fy_api_adfilter_module = {
    FY_MODULE_INIT("fy_api_adfilter_module",
            fy_adfilter_tasks,
            fy_adfilter_module_init,
            fy_adfilter_module_conf)
};

static const char *fy_api_adfilter_uri;

static int fy_adfilter_module_conf(fy_module *m, void *ptr)
{
    if ((fy_api_adfilter_uri = fy_conf_get_param("adfilter_api_uri")) == NULL) {
        fy_log_error("adfilter conf error\n");
        return -1;
    }
    return 0;
}

static int fy_adfilter_module_init(fy_module *module, void *ptr)
{
    srand((unsigned)fy_current());
    return 0;
}

static int fy_adfilter_task_submit(fy_task *task, void *request)
{
    int             randidx;
    size_t          size;
    jc_val_t       *jval;
    jc_json_t      *new_json;
    jc_array_t     *jarray;
    fy_request     *r;
    const char     *doc_uri;

    assert(request != NULL);

    r = (fy_request *)request;

    assert(r->info != NULL);

    doc_uri = fy_fcgi_get_param("DOCUMENT_URI", r);
    if (doc_uri == NULL || strcmp(doc_uri, fy_api_adfilter_uri) != 0) {
        goto end;
    }

    if (r->info->json_rc == NULL) {
        goto end;
    }

    if ((jval = jc_json_find(r->info->json_rc, "data")) == NULL) {
        fy_log_error("cannot find data in json_str: %s\n", jc_json_str(r->info->json_rc));
        goto end;
    }

    if (jval->type != JC_ARRAY) {
        goto end;
    }

    jarray = jval->data.a;
    size = jc_array_size(jarray);
    if (size == 0) {
        goto end;
    }

    randidx = rand() % size;
    jval = jc_array_get(jarray, randidx);
    if (jval->type != JC_JSON) {
        goto end;
    }

    new_json = jc_json_parse(jc_json_str(jval->data.j));
    jc_json_destroy(r->info->json_rc);
    r->info->json_rc = new_json;

end:
    fy_request_next_module(r);
    return 0;
}