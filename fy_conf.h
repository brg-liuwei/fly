#ifndef __FY_CONF_H__
#define __FY_CONF_H__

#include "fy_framework.h"

#include <mxml.h>

int fy_conf_init(const char *path);
int fy_module_conf_begin(fy_module *m);
int fy_module_conf_end(fy_module *m);
const char *fy_conf_get_param(const char *key);

#endif
