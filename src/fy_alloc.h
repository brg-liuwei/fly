#ifndef __FY_ALLOC_H__
#define __FY_ALLOC_H__

/* 
 * Thanks for Igor Sysoev. 
 * I followed your memory pool of nginx 
 * */

#include "fy_config.h"

#ifdef FY_HAVE_SYS_TYPES_H
# include <sys/types.h>
#else
typedef unsigned long size_t;
#endif

#define FY_MEMALIGN sizeof(unsigned long)
#define fy_align(x) \
    (((size_t)(x) + (FY_MEMALIGN - 1)) & ~(FY_MEMALIGN - 1))

typedef struct fy_pool_data_s  fy_pool_data_t;
typedef struct fy_pool_large_s fy_pool_large_t;
typedef struct fy_pool_s       fy_pool_t;

typedef void (*fy_pool_cln_t)(void *);

fy_pool_t *fy_pool_create(size_t  size);
void fy_pool_destroy(fy_pool_t *pool);
void *fy_pool_alloc(fy_pool_t *pool, size_t size);

#endif

