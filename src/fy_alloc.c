#include "fy_alloc.h"
#include <string.h>

#ifdef FY_HAVE_MALLOC_H
#include <malloc.h>
#elif defined FY_HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <unistd.h>
#include <assert.h>

#define FY_POOLMINSIZE 1024

struct fy_pool_data_s {
    char        *last;      /* last position of alloced data */
    char        *end;       /* end of current pool */
    short        fail;
    fy_pool_t   *next;
};

struct fy_pool_large_s {
    size_t             size;
    fy_pool_large_t   *next;
    void              *ptr;
};

struct fy_pool_s {
    fy_pool_data_t      data;
    size_t              max;     /* max data can alloc from pool */
    fy_pool_t          *current;
    fy_pool_large_t    *large;
    fy_pool_cln_t       cln;
};

size_t fy_pagesize;

static int fy_initialized = 0;

static void *fy_memalign(size_t boundary, size_t size);

#ifdef FY_HAVE_MALLOC_H
static void *fy_memalign(size_t boundary, size_t size)
{
    void  *ptr;
    ptr = memalign(boundary, size);
    if (ptr == NULL) {
        return NULL;
    }
    memset(ptr, 0, size);
    return ptr;
}

#elif defined FY_HAVE_STDLIB_H
static void *fy_memalign(size_t boundary, size_t size)
{
    int    rc;
    void  *ptr;

    rc = posix_memalign(&ptr, FY_MEMALIGN, size);
    if (rc != 0 || ptr == NULL) {
        return NULL;
    }
    memset(ptr, 0, size);
    return ptr;
}

#endif

static void fy_init()
{
    if (fy_initialized == 0) {
        fy_pagesize = (size_t)getpagesize();
        fy_initialized = 1;
    }
}

fy_pool_t *fy_pool_create(size_t size)
{
    fy_pool_t    *p;

    fy_init();

    if (size < FY_POOLMINSIZE) {
        size = FY_POOLMINSIZE;
    }
    p = (fy_pool_t *)fy_memalign(FY_MEMALIGN, size);
    if (p == NULL) {
        return NULL;
    }
    
    p->data.last = (char *)fy_align((char *)p + sizeof(fy_pool_t));
    p->data.end = (char *)p + size;
    p->data.fail = 0;
    p->data.next = NULL;

    p->max = size - fy_align(sizeof(fy_pool_t)) > fy_pagesize ? 
        fy_pagesize : size - fy_align(sizeof(fy_pool_t));

    p->current = p;
    p->large = NULL;
    p->cln = NULL;

    return p;
}

void fy_pool_destroy(fy_pool_t *p)
{
    fy_pool_large_t  *large;
    fy_pool_t        *cur;

    assert(p != NULL);

    if (p->cln) {
        p->cln(p);
    }
    for (large = p->large; large != NULL; large = large->next) {
        if (large->ptr) {
            free(large->ptr);
        }
    }
    for (cur = p; cur != NULL; /* void */ ) {
        p = cur->data.next;
        free(cur);
        cur = p;
    }
}

static void *fy_pool_alloc_large(fy_pool_t *p, size_t size)
{
    fy_pool_large_t *large;

    assert(size > p->max);

    if ((large = fy_pool_alloc(p, sizeof(fy_pool_large_t))) == NULL) {
        return NULL;
    }
    large->next = p->large;
    p->large = large;
    large->size = size;
    return large->ptr = (fy_pool_large_t *)fy_memalign(FY_MEMALIGN, size);
}

static void *fy_pool_alloc_block(fy_pool_t *pool, size_t size)
{
    void        *m;
    size_t       pool_size;
    fy_pool_t   *p;

    assert(pool != NULL);

    pool_size = (size_t)(pool->data.end - (char *)pool);

    p = (fy_pool_t *)fy_memalign(FY_MEMALIGN, pool_size);

    if (p == NULL) {
        return NULL;
    }
    
    p->data.last = (char *)fy_align((char *)p + sizeof(fy_pool_data_t));
    p->data.end = (char *)p + pool_size;
    p->data.fail = 0;
    p->data.next = NULL;

    assert(p->data.end - p->data.last >= size);

    pool = pool->current;
    while (pool->data.next != NULL) {
        pool = pool->data.next;
    }
    pool->data.next = p;

    m = p->data.last;
    p->data.last += size;
    return m;
}

void *fy_pool_alloc(fy_pool_t *pool, size_t size)
{
    void       *m;
    fy_pool_t  *p;

    assert(pool != NULL);

    size = fy_align(size);

    if (size > pool->max) {
        return fy_pool_alloc_large(pool, size);
    }

    p = pool->current;

    do {
        if (p->data.end - p->data.last > size) {
            m = p->data.last;
            p->data.last += size;
            return m;
        }
        if (p->data.fail++ > 5 && p->data.next != NULL) {
            pool->current = p->data.next;
        }
        p = p->data.next;
    } while (p != NULL);

    return fy_pool_alloc_block(pool, size);
}

