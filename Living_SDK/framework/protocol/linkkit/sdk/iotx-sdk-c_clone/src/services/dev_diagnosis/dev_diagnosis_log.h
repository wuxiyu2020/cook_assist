/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef __DEV_DIAGNOSIS_LOG_H__
#define __DEV_DIAGNOSIS_LOG_H__

#include "iotx_log.h"

#define diagnosis_flow(...)      log_flow("diag", __VA_ARGS__)
#define diagnosis_debug(...)     log_debug("diag", __VA_ARGS__)
#define diagnosis_info(...)      log_info("diag", __VA_ARGS__)
#define diagnosis_warn(...)      log_warning("diag", __VA_ARGS__)
#define diagnosis_err(...)       log_err("diag", __VA_ARGS__)
#define diagnosis_crit(...)      log_crit("diag", __VA_ARGS__)
#define diagnosis_emerg(...)     log_emerg("diag", __VA_ARGS__)
#define diagnosis_trace(...)     log_crit("diag", __VA_ARGS__)

#endif
