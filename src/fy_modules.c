#include "fy_framework.h"

/* globe modules here */

extern fy_module null_module;
extern fy_task null_task;
extern fy_task *null_task_list[];

extern fy_module fy_fcgi_accept_module;
//extern fy_module fy_fcgi_getinfo_module;
extern fy_module fy_fcgi_layout_module;
extern fy_module fy_fcgi_finish_module;


#ifdef TEST_FRAMEWORK

#include <stdio.h>

int task_name_print(fy_task *task, void *request)
{
    //printf("====== start task_name_print %s =======\n", task->task_name);
    //printf("sub_task_size: %d\n", task->sub_task_size);
    //if (task != &null_task) {
    //    printf("belong to module: %s\n", task->module->module_name);
    //}
    //printf("====== end task_name_print %s =======\n", task->task_name);
    return 0;
}

int module_name_print(fy_module *m, void *request)
{
    //printf("------- start module_name_print -------\n");
    //printf("%s\n", m->module_name);
    //printf("module task size: %d\n", m->task_size);
    //printf("module task total: %d\n", m->task_total);
    //printf("------- end module_name_print -------\n\n");
    return 0;
}

static fy_task task3 = {
    "task 3",
    0,
    null_task_list,
    NULL,

    task_name_print
};

static fy_task task5 = {
    "task 5",
    0,
    null_task_list,
    NULL,

    task_name_print
};

static fy_task *task4_list[] = { &task5, &null_task };
static fy_task task4 = {
    "task 4",
    0,
    task4_list,
    NULL,

    task_name_print
};

static fy_task task6 = {
    "task 6",
    0,
    null_task_list,
    NULL,

    task_name_print
};

static fy_task *task2_list[] = { &task3, &task4, &task6, &null_task};
static fy_task task2 = {
    "task 2",
    0,
    task2_list,
    NULL,

    task_name_print
};

static fy_task task7 = {
    "task 7",
    0,
    null_task_list,
    NULL,

    task_name_print
};

static fy_task *task1_list[] = { &task2, &task7, &null_task };
static fy_task task1 = {
    "task 1",
    0,
    task1_list,
    NULL,

    task_name_print
};

static fy_task task8 = {
    "task 8",
    0,
    null_task_list,
    NULL,

    task_name_print
};

static fy_task *module1_task_list[] = { &task1, &task8, &null_task };

static fy_module module1 = {
    FY_MODULE_INIT("module 1", module1_task_list, module_name_print, NULL, NULL)
};

static fy_task *module2_task_list[] = { &task8, &task1, &null_task };

static fy_module module2 = {
    FY_MODULE_INIT("module 2", module2_task_list, module_name_print, NULL, NULL)
};


#endif

fy_module *fy_modules[] = {
    &fy_fcgi_accept_module,
    //&fy_fcgi_getinfo_module,
    &fy_fcgi_layout_module,
    &fy_fcgi_finish_module,
    &null_module
};

