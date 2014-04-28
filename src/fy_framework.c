#include "fy_framework.h"
#include "fy_time.h"
#include "fy_logger.h"
#include "fy_event.h"
#include "fy_time.h"
#include "fy_conf.h"
#include "fy_util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

fy_task null_task = {
    FY_TASK_INIT("null task", NULL, NULL)
};

fy_task *null_task_list[] = { &null_task };

fy_module null_module = {
    FY_MODULE_INIT("null module", null_task_list, NULL, NULL)
};

extern fy_module *fy_modules[];

static size_t fy_module_size;

static void fy_task_init(fy_task *task, fy_module *module);
static void fy_task_display(fy_task *task, size_t level);

static void fy_request_run(fy_request *r);

static int fy_check_mode = 0;

static const char *fy_debug_log_path;
static const char *fy_info_log_path;
static const char *fy_error_log_path;
static size_t fy_poll_size;

fy_pool_t *fy_mem_pool;

int main(int argc, char *argv[])
{
    int opt;
    int run_loop;

    run_loop = 1;

    printf("fly\n");
    while ((opt = getopt(argc, argv, "th?")) != -1) {
        run_loop = 0;
        switch (opt) {
            case 't':
                fy_check_mode = 1;
                break;
            case 'h':
            case '?':
                printf("Usage: \n");
                printf("%s -t  -----  print relationship of modules of tasks\n", argv[0]);
                printf("to start the service, use fastcgi-spawn %s args...\n", argv[0]);
            default:
                break;
        }
    }

    if ((fy_mem_pool = fy_pool_create(getpagesize())) == NULL) {
        printf("init global pool error\n");
        exit(-1);
    }
    if (fy_conf_init(FLY_DEFAULT_CONF_FILE) == -1) {
        printf("init conf file error\n");
        exit(-1);
    }

    /* get global config */
    fy_module_conf_begin(NULL);
    fy_debug_log_path = fy_conf_get_param("debug_log_path");
    fy_info_log_path = fy_conf_get_param("info_log_path");
    fy_error_log_path = fy_conf_get_param("error_log_path");
    fy_poll_size = (size_t)fy_atoi(fy_conf_get_param("poll_size"));
    fy_module_conf_end(NULL);

    if (fy_log_init(FY_LOG_DEBUG, fy_debug_log_path) == -1) {
        exit(-1);
    }
    if (fy_log_init(FY_LOG_INFO, fy_info_log_path) == -1) {
        exit(-1);
    }
    if (fy_log_init(FY_LOG_ERROR, fy_error_log_path) == -1) {
        exit(-1);
    }

    fy_time_init();

    fy_event_loop *loop;
    if ((loop = fy_create_event_loop(fy_mem_pool, fy_poll_size)) == NULL) {
        fy_log_error("%s:%d fy_create_event_loop error, pool size = %ld\n",
                __FILE__, __LINE__, fy_poll_size);
        return -1;
    }

    fy_module_init(loop);
    
    if (fy_check_mode) {
        fy_module_display_no_null();
        printf("\n");
        return 0;
    }

    if (run_loop) {
        fy_main_loop(loop);
    }

    fy_pool_destroy(fy_mem_pool);
    return 0;
}

static void fy_task_init(fy_task *task, fy_module *module)
{
    size_t  sub;

    assert(task != NULL);

    task->module = module;
    for (sub = 0; task->sub_tasks[sub] != &null_task; ++sub) {
        fy_task_init(task->sub_tasks[sub], module);
    }
    task->sub_task_size = sub;
    ++module->task_total;
}

void fy_module_init(void *data)
{
    size_t      t_idx;
    fy_module  *module, *pre_module;

    for (fy_module_size = 0, pre_module = NULL; 
            fy_modules[fy_module_size] != &null_module; 
            ++fy_module_size)
    {
        module = fy_modules[fy_module_size];
        assert(module != NULL);
        assert(module->tasks != NULL);

        if (pre_module != NULL) {
            pre_module->next = module;
        }
        for (t_idx = 0; module->tasks[t_idx] != &null_task; ++t_idx) {
            fy_task_init(module->tasks[t_idx], module);
        }
        module->task_size = t_idx;
        module->next = NULL;

        /* module conf */
        if (module->module_conf) {

            if (fy_module_conf_begin(module) == -1) {
                fy_log_error("fy_module_conf_begin of module: %s failed\n",
                        module->module_name);
                exit(-1);
            }

            if (module->module_conf(module, NULL) < 0) {
                fy_log_error("module: %s conf failed\n", module->module_name);
                exit(-1);
            }

            fy_module_conf_end(module);
        }

        /* module init */
        if (module->module_init) {
            if (module->module_init(module, data) < 0
                    && !fy_check_mode)
            {
                fy_log_error("module: %s init failed\n", module->module_name);
                exit(-1);
            }
        }
        pre_module = module;
    }
}

static void fy_task_display_no_null(char *pre, size_t pre_size,
        size_t pre_max, fy_task *task, fy_task *next_task)
{
    size_t i;

    if (pre_max < pre_size + 4) {
        printf("Out of bounder\n");
        return;
    }
    printf("%s|\n", pre);
    printf("%s|--%s\n", pre, task->task_name);
    pre[pre_size] = '|';
    pre[pre_size + 1] = ' ';
    pre[pre_size + 2] = ' ';
    pre[pre_size + 3] = '\0';
    for  (i = 0; i != task->sub_task_size; ++i) {
        if (next_task == &null_task) {
            pre[pre_size] = ' ';
        } else {
            pre[pre_size] = '|';
        }
        fy_task_display_no_null(pre, pre_size + 3, pre_max, task->sub_tasks[i], task->sub_tasks[i+1]);
    }
    /* recover the context */
    pre[pre_size] = '\0';
}

static void fy_task_display(fy_task *task, size_t level)
{
    size_t  i;

    for (i = 0; i < level; ++i) {
        printf("|   ");
    }
    printf("|\n");
    for (i = 0; i < level; ++i) {
        printf("|   ");
    }
    printf("|---%s", task->task_name);
    if (task->module != NULL) {
        printf(" (%s)\n", task->module->module_name);
    } else {
        printf("\n");
    }
    if (task != &null_task) {
        for (i = 0; i <= task->sub_task_size; ++i) {
            fy_task_display(task->sub_tasks[i], level + 1);
        }
    }
}

void fy_module_display_no_null()
{
    char     pre[1024];
    size_t   i, j;

    for (i = 0; i < fy_module_size; ++i) {
        printf("\n%s\n", fy_modules[i]->module_name);
        for (j = 0; j < fy_modules[i]->task_size; ++j) {
            pre[0] = '\0';
            fy_task_display_no_null(pre, 0, 1023, fy_modules[i]->tasks[j], fy_modules[i]->tasks[j+1]);
        }
    }
}

void fy_module_display()
{
    size_t   i, j;

    for (i = 0; i <= fy_module_size; ++i) {
        printf("\n%s\n", fy_modules[i]->module_name);
        for (j = 0; j <= fy_modules[i]->task_size; ++j) {
            fy_task_display(fy_modules[i]->tasks[j], 0);
        }
    }
}

static void fy_request_cleanup(fy_request *r)
{
    r->module = NULL;
    r->fcgi_request = NULL;
    r->task_completes = 0;

    if (r->info != NULL) {
        r->info->cln(r->info);
        r->info = NULL;
    }
}

fy_request *fy_request_create(fy_pool_t *pool)
{
    fy_request   *r;

    assert(pool != NULL);
    r = (fy_request *)fy_pool_alloc(pool, sizeof(fy_request));
    if (r != NULL) {
        r->cln = fy_request_cleanup;
    }
    return r;
}

static void fy_request_run(fy_request *r)
{
    size_t  i, task_size;

    assert(r != NULL);
    if (r->module != NULL) {
        task_size = r->module->task_size;
        for (i = 0; i != task_size; ++i) {
            if (r->module->tasks[i]->task_submit != NULL) {
                r->module->tasks[i]->task_submit(r->module->tasks[i], r);
            }
        }
    }
}

void fy_request_next_module(fy_request *r)
{
    assert(r != NULL);

    if (r->module != NULL) {
        if (++r->task_completes == r->module->task_total) {
            r->module = r->module->next;
            r->task_completes = 0;
            fy_request_run(r);
        }
    }
}

void fy_submit_subtask(fy_task *task, fy_request *r)
{
    size_t i;

    assert(r != NULL);
    assert(task != NULL);

    ++r->task_completes;
    for (i = 0; i != task->sub_task_size; ++i) {
        task->sub_tasks[i]->task_submit(task->sub_tasks[i], r);
    }
}

