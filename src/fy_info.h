#ifndef __FY_INFO_H__
#define __FY_INFO_H__

#include "fy_alloc.h"

#include <sys/types.h>

typedef struct fy_info_t fy_info;

struct fy_info_t {
    int           req_type;
    int           res_type;
    int64_t       current;

    /* ======= for lomark ad ======= */
    char         *send_buf;
    char         *send_buf_end;
    char         *send_buf_rpos;
    char         *send_buf_wpos;

    char         *recv_buf;
    char         *recv_buf_end;
    char         *recv_buf_rpos;
    char         *recv_buf_wpos;

    char         *lomark_json_str;
    /* ======= lomark end =======*/

    /* ======= for graph service */
    char         *graph_addr;
    /* ======= graph  end =======*/

    void        (*cln)(struct fy_info_t *);
};

fy_info *fy_info_create(fy_pool_t *pool);

#endif

