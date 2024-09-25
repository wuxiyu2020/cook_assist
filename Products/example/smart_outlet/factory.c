/*
 *copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <aos/yloop.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "smart_outlet.h"
#include "vendor.h"
#include "device_state_manger.h"
#include "hal/wifi.h"
#include "factory.h"

#define FACTORY_TEST_AP_SSID "YOUR_SSID"
#define FACTORY_TEST_AP_PSW  "YOUR_PASSWORD"



int scan_factory_ap(void)
{
    /* scan wifi */
    int ret = 0;
    ap_scan_info_t scan_result;
    int ap_scan_result = -1;
    // start ap scanning for default 3 seconds
    memset(&scan_result, 0, sizeof(ap_scan_info_t));
    ap_scan_result = awss_apscan_process(NULL, FACTORY_TEST_AP_SSID, &scan_result);
    app_info("[FACTORY]scan factory AP result = %d", scan_result.found);
	if ( (ap_scan_result == 0) && (scan_result.found) ) {
        enter_factory_mode(scan_result.rssi);
    } else {
        ret = -1;
    }
    return ret;
}

int enter_factory_mode(int8_t rssi)
{
    #ifdef WIFI_PROVISION_ENABLED
    extern int awss_stop(void);
    awss_stop();
    #endif

#ifdef CONFIG_WIRELESS_LOG
    dump_device_state(STATE_CODE_ENTER_FACTORY_MODE, "factory mode");
#endif

    /* RSSI > -60dBm */
    if (rssi < -60) {
        app_info("[FACTORY]factory AP power < -60dbm");
        set_net_state(FACTORY_FAILED_1);
    } else {
        app_info("[FACTORY]meter calibrate begin");
        set_net_state(FACTORY_BEGIN);
    }

    return 0;
}

int exit_factory_mode()
{
    //need to reboot
}

