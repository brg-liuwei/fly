#include "fy_framework.h"

/* globe modules here */

extern fy_module null_module;
extern fy_task null_task;
extern fy_task *null_task_list[];

extern fy_module fy_fcgi_accept_module;
//extern fy_module fy_graph_module;
//extern fy_module fy_c_module;
extern fy_module fy_api_adpos_module;
extern fy_module fy_api_adidx_module;
extern fy_module fy_api_adfilter_module;
extern fy_module fy_fcgi_layout_module;
//extern fy_module fy_http_count_module;
extern fy_module fy_api_adlog_module;
extern fy_module fy_fcgi_finish_module;

fy_module *fy_modules[] = {
    &fy_fcgi_accept_module,
    //&fy_graph_module,
    //&fy_c_module,
    &fy_api_adpos_module,
    &fy_api_adidx_module,
    &fy_api_adfilter_module,
    &fy_fcgi_layout_module,
    //&fy_http_count_module,
    &fy_api_adlog_module,
    &fy_fcgi_finish_module,
    &null_module
};

