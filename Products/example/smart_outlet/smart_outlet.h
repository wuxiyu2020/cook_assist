/*
 *copyright (C) 2019-2022 Alibaba Group Holding Limited
 */

#ifndef __SMART_OUTLET_METER_H__
#define __SMART_OUTLET_METER_H__

#include "aos/aos.h"
#include "iotx_log.h"

#define app_debug(...) log_debug("app", __VA_ARGS__)
#define app_info(...) log_info("app", __VA_ARGS__)
#define app_warn(...) log_warning("app", __VA_ARGS__)
#define app_err(...) log_err("app", __VA_ARGS__)
#define app_crit(...) log_crit("app", __VA_ARGS__)

typedef struct {
    uint8_t powerswitch;
    uint8_t all_powerstate;
} device_status_t;

typedef struct {
    int master_devid;
    int cloud_connected;
    int master_initialized;
    int bind_notified;
    device_status_t status;
} user_example_ctx_t;

user_example_ctx_t *user_example_get_ctx(void);
void user_post_powerstate(int powerstate);
void update_power_state(int state);
void example_free(void *ptr);

#endif
