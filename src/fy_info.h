#ifndef __FY_INFO_H__
#define __FY_INFO_H__

#include "fy_alloc.h"

#include <sys/types.h>

typedef struct fy_info_t fy_info;

struct fy_info_t {
    int           req_type;
    int           res_type;
    int64_t       current;

    /* ==== for graph service ==== */
    int           graph_duration;
    char         *graph_tracker;
    char         *graph_adid;
    char         *graph_addr;
    /* ======= graph  end ======= */

    void        (*cln)(struct fy_info_t *);
};

fy_info *fy_info_create(fy_pool_t *pool);

#endif

