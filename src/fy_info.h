#ifndef __FY_INFO_H__
#define __FY_INFO_H__

#include "fy_alloc.h"

#include "jc_type.h"

#include <sys/types.h>

typedef struct fy_info_t fy_info;

struct fy_info_t {
    int           req_type;
    int           res_type;
    int64_t       current;

    int           graph_duration;
    char         *graph_tracker;
    char         *graph_adid;
    char         *graph_addr;

    char         *send_buf;
    char         *send_buf_rpos;
    char         *send_buf_wpos;
    char         *send_buf_end;

    char         *recv_buf;
    char         *recv_buf_rpos;
    char         *recv_buf_wpos;
    char         *recv_buf_end;

    int           recv_state;

    jc_json_t    *json_rc;

    void        (*cln)(struct fy_info_t *);
};

fy_info *fy_info_create(fy_pool_t *pool);

#endif

