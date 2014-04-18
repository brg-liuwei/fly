#ifndef __FY_INFO_H__
#define __FY_INFO_H__

#include <sys/types.h>

typedef struct fy_info_t fy_info;

struct fy_info_t {
    int           req_type;
    int           res_type;
    int64_t       current;
    void        (*cln)(struct fy_info_t *);
};

fy_info *fy_info_create();

#endif

