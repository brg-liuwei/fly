#include "fy_time.h"
#include "fy_util.h"
#include "fy_conf.h"
#include "fy_logger.h"
#include "fy_framework.h"

#include "jc_type.h"

#include <assert.h>
#include <fcgiapp.h>
#include <fcgi_config.h>

extern fy_task *null_task_list[];
extern fy_task  null_task;

static int fy_api_adlog_task_submit(fy_task *, void *);
static int fy_api_adlog_module_init(fy_module *, void *);
static int fy_api_adlog_module_conf(fy_module *, void *);

static fy_task fy_api_adlog_task = {
    FY_TASK_INIT("fy_api_adlog_task", null_task_list, fy_api_adlog_task_submit)
};

static fy_task *fy_api_adlog_tasks[] = {
    &fy_api_adlog_task,
    &null_task
};

fy_module fy_api_adlog_module = {
    FY_MODULE_INIT("fy_api_adlog_module",
            fy_api_adlog_tasks,
            fy_api_adlog_module_init,
            fy_api_adlog_module_conf)
};

static const char *fy_api_adlog_uri;

static int fy_api_adlog_module_init(fy_module *m, void *data) {
    return 0;
}

static int fy_api_adlog_module_conf(fy_module *m, void *ptr) {
    fy_api_adlog_uri = fy_conf_get_param("adlog_api_uri");
    if (fy_api_adlog_uri == NULL) {
        return -1;
    }
    return 0;
}

static const char *fy_get_phone_type(fy_request *r)
{
    const char *val;

    val = fy_fcgi_get_param("PHONETYPE", r);
    if (val == NULL) {
        return "";
    }
    if (strcasecmp(val, "ios") == 0) {
        return "ios";
    }
    if (strcasecmp(val, "android") == 0) {
        return "Android";
    }
    return val;
}

static const char *fy_get_region(fy_request *r)
{
    const char *val;
    val = fy_fcgi_get_param("REGION", r);
    if (val == NULL || strcasecmp("un", val) == 0) {
        return "";
    }
    return val;
}

static const char *fy_get_param_default(fy_request *r, const char *key, const char *def_val)
{
    const char *val;
    return (val = fy_fcgi_get_param(key, r)) ? val : def_val;
}

static int fy_api_adlog_task_submit(fy_task *task, void *request)
{
    char         adid[128];
    jc_val_t    *val;
    jc_json_t   *js;
    fy_request  *r;

    const char  *doc_uri;

    r = (fy_request *)request;
    assert(r != NULL);
    assert(r->info != NULL);

    doc_uri = fy_fcgi_get_param("DOCUMENT_URI", r);
    if (doc_uri == NULL || strcmp(fy_api_adlog_uri, doc_uri) != 0) {
        goto end;
    }

    js = r->info->json_rc;
    assert(js != NULL);

    val = jc_json_find(js, "errorno");
    assert(val != NULL);
    assert(val->type == JC_NUM);

    if (val->data.n != 0) {
        /* cannot found and ad */
        goto end;
    }

    val = jc_json_find(js, "data");
    if (val == NULL || val->type != JC_JSON) {
        goto end;
    }

    js = val->data.j;
    val = jc_json_find(js, "id");
    if (val == NULL || val->type != JC_STR) {
        fy_log_error("json id type error: %s\n", jc_json_str(js));
        goto end;
    }
    jc_str_copy(adid, val->data.s, 128);

    /* 广告投放引擎日志格式：
     * ----------------------- 通用字段 -----------------------
     * "Unix10位时间戳(1393257000)", "时区(+8)", "OS(Android/ios)",
     * "设备ID" ,"运营商(中国移动)", "App版本(4.3.5.1)",
     * "App渠道(应用宝)", "机型(Samsumg_br_FM_samsung_SCH-N719)", "OS版本(4.3)"
     * "个推ID(null)", "网络(null)", "经纬度(null)",
     * "国家(China)", "省(直接填region代码)", "城市(null)", 
     * "iOS推送ID(null)", "iOS推送证书(null)", "iOS日志版本",
     * "新增字段(null)", "package name(get_param("pkg"))",
     *
     * ----------------------- 自定义字段 -----------------------
     * "adid", "zoneid"
     * */
    fy_log_info(
            "\"%lu\"," /* unix timestamp */
            "\"+8\","  /* time zone */
            "\"%s\","  /* os(ios or Android) */
            "\"%s\","  /* device id */
            "\"\",\"\",\"\",\"\",\"\",\"\",\"\",\"\","
            "\"China\","
            "\"%s\","  /* region */
            "\"\",\"\",\"\",\"\",\"\","
            "\"%s\","   /* pkg name */
            "\"%s\","   /* adid */
            "\"%s\"\n"   /* zoneid */
            ,
            fy_cur_sec(),
            fy_get_phone_type(r),
            fy_get_param_default(r, "DEVICEID", ""),
            fy_get_region(r),
            fy_get_param_default(r, "PKG", ""),
            adid,
            fy_get_param_default(r, "ZONE", "")
            );
end:
    fy_request_next_module(r);
    return 0;
}
