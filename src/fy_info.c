#include "fy_info.h"
#include "fy_time.h"
#include "fy_logger.h"
#include <stdlib.h>
#include <assert.h>

static void fy_info_cleanup(fy_info *info)
{
}

fy_info *fy_info_create()
{
    fy_info    *info;
    info = (fy_info *)calloc(1, sizeof(fy_info));
    info->current = fy_current();
    if (info != NULL) {
        info->cln = fy_info_cleanup;
    }    
    return info;
}
