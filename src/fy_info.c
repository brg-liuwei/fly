#include "fy_info.h"
#include "fy_time.h"
#include "fy_logger.h"
#include <stdlib.h>
#include <assert.h>

static void fy_info_cleanup(fy_info *info)
{
    if (info->json_rc != NULL) {
        jc_json_destroy(info->json_rc);
        info->json_rc = NULL;
    }
}

fy_info *fy_info_create(fy_pool_t *pool)
{
    fy_info    *info;

    assert(pool != NULL);
    info = (fy_info *)fy_pool_alloc(pool, sizeof(fy_info));
    info->current = fy_current();
    info->json_rc = NULL;
    if (info != NULL) {
        info->cln = fy_info_cleanup;
    }
    return info;
}
