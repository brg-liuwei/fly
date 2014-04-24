#ifndef __FY_FRAMEWORK_H__
#define __FY_FRAMEWORK_H__

#include "fy_info.h"
#include "fy_alloc.h"

#include <unistd.h>
#include <sys/types.h>
#include <fcgiapp.h>
#include <fcgi_config.h>

enum fy_task_status {
    ready = 0,
    running,
    compeleted
};

typedef struct fy_task_t fy_task;
typedef struct fy_module_t fy_module;
typedef struct fy_request_t fy_request;

#define FY_TASK_INIT(name, sub_tasks, submit_handler) \
    name, 0, sub_tasks, NULL, submit_handler

struct fy_task_t {
    char             *task_name;
    size_t            sub_task_size;
    fy_task         **sub_tasks;
    fy_module        *module;

    int             (*task_submit)(fy_task *, void *);
};

#define FY_MODULE_INIT(name, task_list, init_handler, conf_handler) \
    name, 0, 0, task_list, NULL, NULL, NULL, init_handler, conf_handler

struct fy_module_t {
    char                  *module_name;
    size_t                 task_total;
    size_t                 task_size;
    fy_task              **tasks;
    fy_pool_t             *pool;
    struct fy_module_t    *next;
    void                  *data;

    int (*module_init)(fy_module *, void *);
    int (*module_conf)(fy_module *, void *);
};

struct fy_request_t {
    size_t         task_completes;
    fy_pool_t     *pool;
    fy_module     *module;
    FCGX_Request  *fcgi_request;

    fy_info       *info;
    void         (*cln)(struct fy_request_t *);
};

void fy_module_init(void *data);
void fy_module_display();
void fy_module_display_no_null();

fy_request *fy_request_create(fy_pool_t *pool);
void fy_request_next_module(fy_request *);
void fy_submit_subtask(fy_task *, fy_request *);

#endif
