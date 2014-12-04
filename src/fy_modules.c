#include "fy_framework.h"

/* globe modules here */

extern fy_module null_module;
extern fy_task null_task;
extern fy_task *null_task_list[];

extern fy_module fy_fcgi_accept_module;
extern fy_module fy_graph_module;
//extern fy_module fy_http_lomark_module;
extern fy_module fy_fcgi_layout_module;
extern fy_module fy_fcgi_finish_module;

fy_module *fy_modules[] = {
    &fy_fcgi_accept_module,
    &fy_graph_module,
    //&fy_http_lomark_module,
    &fy_fcgi_layout_module,
    &fy_fcgi_finish_module,
    &null_module
};

