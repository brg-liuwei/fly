#include "fy_conf.h"
#include "fy_logger.h"

#include <stdio.h>
#include <mxml.h>
#include <errno.h>
#include <assert.h>

static mxml_node_t *fy_conf_tree;
static mxml_node_t *fy_mod_root;

static int fy_conf_initialized = 0;

int fy_conf_init(const char *path)
{
    FILE   *fp;

    if (fy_conf_initialized == 1) {
        return 0;
    }
    if ((fp = fopen(path, "r")) == NULL) {
        printf("open conf file: %s error, errno: %d\n", path, errno);
        return -1;
    }
    fy_conf_tree = mxmlLoadFile(NULL, fp, MXML_TEXT_CALLBACK);
    fclose(fp);
    if (fy_conf_tree == NULL) {
        return -1;
    }
    fy_conf_initialized = 1;
    return 0;
}

void fy_conf_ok()
{
    if (fy_conf_tree != NULL) {
        mxmlDelete(fy_conf_tree);
        fy_conf_tree = NULL;
    }
}

int fy_module_conf_begin(fy_module *m)
{
    char *module_name;

    if (fy_conf_initialized == 0
            && fy_conf_init(FLY_DEFAULT_CONF_FILE) == -1)
    {
        return -1;
    }
    assert(fy_mod_root == NULL);
    if (m == NULL) {
        module_name = "Global";
    } else {
        module_name = m->module_name;
    }
    fy_mod_root = mxmlFindElement(fy_conf_tree, fy_conf_tree,
            module_name, NULL, NULL, MXML_DESCEND);
    if (fy_mod_root == NULL) {
        fy_log_error("cannot find module who named %s\n", module_name);
        return -1;
    }
    return 0;
}

int fy_module_conf_end(fy_module *m)
{
    assert(fy_mod_root != NULL);
    fy_mod_root = NULL;
    return 0;
}

const char *fy_conf_get_param(const char *key)
{
    int            whitespace;
    const char    *value;
    mxml_node_t   *node;

    value = NULL;
    whitespace = 0;
    node = mxmlFindElement(fy_mod_root, fy_mod_root, key,
            NULL, NULL, MXML_DESCEND);
    value = mxmlGetText(node, &whitespace);
    if (node == NULL || whitespace) {
        return NULL;
    }
    return value;
}
