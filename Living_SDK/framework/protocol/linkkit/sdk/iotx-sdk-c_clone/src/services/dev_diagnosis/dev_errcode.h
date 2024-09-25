/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef __DEV_ERRCODE_H__
#define __DEV_ERRCODE_H__

#if defined(__cplusplus)
extern "C" {
#endif

#include "iot_export.h"

#ifdef DEV_ERRCODE_ENABLE
extern void dev_errcode_module_init(void);
extern int dev_errcode_handle(const int sub_errcode, const char *state_message);
extern uint16_t dev_errcode_sdk_filter(const int sub_errcode);
#endif

#if defined(__cplusplus)
}       /* extern "C" */
#endif
#endif  /* __DEV_ERRCODE_H__ */