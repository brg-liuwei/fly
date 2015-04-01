#ifndef __FY_ESTIMATE_H__
#define __FY_ESTIMATE_H__

#include "fy_connection.h"
#include "fy_framework.h"
#include "fy_info.h"
#include "fy_logger.h"

#include <sys/time.h>

void fy_conn_estimate(fy_conn_pool *pool, fy_module *module);
void fy_request_estimate_begin(fy_request *r);
void fy_request_estimate_end(fy_request *r);

#endif
