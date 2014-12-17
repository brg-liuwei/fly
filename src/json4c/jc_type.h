#ifndef __JC_TYPE_H__
#define __JC_TYPE_H__

#include <sys/types.h>
#include <stdint.h>

#include "jc_alloc.h"

typedef short                jc_bool_t;
typedef double               jc_num_t;
typedef struct jc_str_s      jc_str_t;
typedef struct jc_array_s    jc_array_t;
typedef struct jc_json_s     jc_json_t;

typedef struct jc_str_s      jc_key_t;

typedef struct jc_val_s      jc_val_t;

typedef enum __jc_type_t {
    JC_BOOL = 0,
    JC_NUM,
    JC_STR,
    JC_ARRAY,
    JC_JSON,
    JC_NULL
} jc_type_t;

struct jc_val_s {
    jc_type_t         type;
    union {
        jc_bool_t     b;
        jc_num_t      n;
        jc_str_t     *s;
        jc_array_t   *a;
        jc_json_t    *j;
    } data;
};

struct jc_str_s {
    size_t   size;     /* size of str */
    size_t   free;     /* free space */
    char     body[];   /* str */
};

struct jc_array_s {
    size_t      size;     /* length of array */
    size_t      free;     /* free size of array */
    jc_val_t  **value;
};

struct jc_json_s {
    size_t       size;     /* size of keys and values */
    size_t       free;     /* free size of keys and values */
    jc_key_t   **keys;     /* keys of json */
    jc_val_t   **vals;     /* values of json */
    jc_pool_t   *pool;     /* mem pool of json */
};

/* json create and delete functions */
jc_json_t *jc_json_create();
jc_json_t *jc_json_parse(const char *json_str);
void jc_json_destroy(jc_json_t *js);

/* json add kv functions */
int jc_json_add_bool(jc_json_t *js, const char *key, int bool_val);
int jc_json_add_num(jc_json_t *js, const char *key, double val);
int jc_json_add_str(jc_json_t *js, const char *key, const char *val);
int jc_json_add_array(jc_json_t *js, const char *key);
int jc_json_add_json(jc_json_t *js, const char *key, jc_json_t *sub_js);
int jc_json_add_null(jc_json_t *js, const char *key);

/* json find function */
jc_val_t *jc_json_find(jc_json_t *js, const char *key);

/* json to string function */
const char *jc_json_str(jc_json_t *js);
const char *jc_json_str_n(jc_json_t *js, size_t *len);

#endif

