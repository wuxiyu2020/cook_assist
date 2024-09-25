/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "breeze_export.h"

static void breeze_awss_init_helper(struct device_config *init,
                                    breeze_dev_info_t *dinfo,
                                    dev_status_changed_cb status_change_cb,
                                    set_dev_status_cb set_cb,
                                    get_dev_status_cb get_cb,
                                    apinfo_ready_cb apinfo_rx_cb,
                                    ota_dev_cb ota_cb)
{
    memset(init, 0, sizeof(struct device_config));
    init->status_changed_cb = status_change_cb;
    init->set_cb            = set_cb;
    init->get_cb            = get_cb;
    init->apinfo_cb         = apinfo_rx_cb;
    init->ota_cb            = ota_cb;

    init->product_id = dinfo->product_id;

    init->product_key_len = strlen(dinfo->product_key);
    memcpy(init->product_key, dinfo->product_key, init->product_key_len);

    init->product_secret_len = strlen(dinfo->product_secret);
    memcpy(init->product_secret, dinfo->product_secret, init->product_secret_len);

    memcpy(init->bd_adv_addr, dinfo->dev_adv_mac, BD_ADDR_LEN);

    /* device name may be NULL */
    if (dinfo->device_name != NULL) {
        init->device_key_len = strlen(dinfo->device_name);
        memcpy(init->device_name, dinfo->device_name, init->device_key_len);
    } else {
        init->device_key_len = 0;
    }

    /* device secret may be NULL */
    if (dinfo->device_secret != NULL) {
        init->device_secret_len = strlen(dinfo->device_secret);
        memcpy(init->device_secret, dinfo->device_secret, init->device_secret_len);
    } else {
        init->device_secret_len = 0;
    }
}

void breeze_awss_init(breeze_dev_info_t *info, 
                      dev_status_changed_cb status_change_cb,
                      set_dev_status_cb set_cb,
                      get_dev_status_cb get_cb,
                      apinfo_ready_cb apinfo_rx_cb,
                      ota_dev_cb ota_cb)
{
    int ret = 0;
    struct device_config brzinit;

    breeze_awss_init_helper(&brzinit, info, status_change_cb, set_cb, get_cb, apinfo_rx_cb, ota_cb);

    ret = breeze_start(&brzinit);
    if  (ret != 0) {
        BREEZE_ERR("breeze start fail:%d", ret);
        return;
    }
    BREEZE_TRACE("breeze start ok");
}

void breeze_awss_start()
{
#ifdef CONFIG_WIRELESS_LOG
    if (netmgr_has_valid_ap()) {
        char bind_flag[10];
        int  len = sizeof(bind_flag);

        memset(bind_flag, 0, len);
        HAL_Kv_Get("device.bind", bind_flag, &len);

        if (!strncmp(bind_flag, "y", 1)) {
            breeze_start_advertising(1, 0);
            return;
        }
    }
#endif

    return;
}

void breeze_awss_stop()
{
    breeze_end();
}
