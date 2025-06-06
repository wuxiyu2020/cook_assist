/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */
#ifdef EN_COMBO_NET
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "aos/kernel.h"
#include <aos/yloop.h>
#include "cJSON.h"
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "breeze_export.h"
#include "smart_outlet.h"
#include "combo_net.h"
#include "device_state_manger.h"

extern uint8_t *os_wifi_get_mac(uint8_t mac[6]);
extern int vendor_get_product_key(char *product_key, int *len);
extern int vendor_get_product_secret(char *product_secret, int *len);
extern int vendor_get_device_name(char *device_name, int *len);
extern int vendor_get_device_secret(char *device_secret, int *len);
extern int vendor_get_product_id(uint32_t *pid);
extern int ieee80211_is_beacon(uint16_t fc);
extern int ieee80211_is_probe_resp(uint16_t fc);
extern int ieee80211_get_bssid(uint8_t *in, uint8_t *mac);
extern int ieee80211_get_ssid(uint8_t *beacon_frame, uint16_t frame_len, uint8_t *ssid);

#define HAL_IEEE80211_IS_BEACON        ieee80211_is_beacon
#define HAL_IEEE80211_IS_PROBE_RESP    ieee80211_is_probe_resp
#define HAL_IEEE80211_GET_SSID         ieee80211_get_ssid
#define HAL_IEEE80211_GET_BSSID        ieee80211_get_bssid
#define HAL_AWSS_OPEN_MONITOR          HAL_Awss_Open_Monitor
#define HAL_AWSS_CLOSE_MONITOR         HAL_Awss_Close_Monitor
#define HAL_AWSS_SWITCH_CHANNEL        HAL_Awss_Switch_Channel


struct ieee80211_hdr {
    uint16_t frame_control;
    uint16_t duration_id;
    uint8_t addr1[ETH_ALEN];
    uint8_t addr2[ETH_ALEN];
    uint8_t addr3[ETH_ALEN];
    uint16_t seq_ctrl;
    uint8_t addr4[ETH_ALEN];
};

typedef struct combo_rx_s {
    uint8_t *p_rx_buf;
    uint32_t rx_len;
    int rx_msg_id;
} combo_rx_t;

typedef struct combo_tx_s {
    int tx_msg_id;
    uint8_t tx_doing;
} combo_tx_t;

typedef struct combo_wifi_state_s {
    uint8_t ap_conn;
    uint8_t cloud_conn;
    uint8_t awss_run;
} combo_wifi_state_t;

typedef struct combo_user_bind_s {
    uint8_t bind_state;
} combo_user_bind_t;

uint8_t g_ble_state = 0;

static breeze_apinfo_t apinfo;
static char monitor_got_bssid = 0;
static netmgr_ap_config_t config;

static combo_wifi_state_t g_combo_wifi = { COMBO_AP_CONN_UNINIT, 0, 0 };

static combo_user_bind_t g_combo_bind = { 0 };

#ifdef COMBO_AWSS_SILENT_ADV
static uint8_t g_silent_adv_support = 1;
#else
static uint8_t g_silent_adv_support = 0;
#endif

static breeze_apinfo_t g_apinfo;
static aos_sem_t wifi_connect_sem;

/* Device info(five elements) will be used by ble breeze */
/* ProductKey, ProductSecret, DeviceName, DeviceSecret, ProductID */
char g_combo_pk[PRODUCT_KEY_LEN + 1] = { 0 };
char g_combo_ps[PRODUCT_SECRET_LEN + 1] = { 0 };
char g_combo_dn[DEVICE_NAME_LEN + 1] = { 0 };
char g_combo_ds[DEVICE_SECRET_LEN + 1] = { 0 };
uint32_t g_combo_pid = 0;
uint16_t last_errsubcode = 0;

/*callback*/
static uint16_t aiot_get_subcode(void)
{
    return last_errsubcode;
}

static void combo_status_change_cb(breeze_event_t event)
{
   switch (event) {
        case CONNECTED:
            app_info("BLE Connected");
            g_ble_state = 1;
            break;

        case DISCONNECTED:
            app_info("BLE Disconnected");
            g_ble_state = 0;
            aos_post_event(EV_BZ_COMBO, COMBO_EVT_CODE_RESTART_ADV, 0);
            break;

        case AUTHENTICATED:
            app_info("BLE Authenticated");
            g_combo_bind.bind_state = 0;
            break;

        case TX_DONE:
            app_info("BLE Payload TX Done");
            break;
        default:
            break;
    }
}

static void combo_set_dev_status_cb(uint8_t *buffer, uint32_t length)
{
    if (NULL == buffer || 0 == length) {
        return;
    }
    app_info("BLE_SET:%.*s", length, buffer);
}

static void combo_get_dev_status_cb(uint8_t *buffer, uint32_t length)
{
    if (NULL == buffer || 0 == length) {
        return;
    }
    app_info("BLE_QUE:%.*s", length, buffer);
}

static void combo_apinfo_rx_cb(breeze_apinfo_t * ap)
{
    if (!ap) {
        app_info("combo apinfo rx null!!");
        return;
    }

    memcpy(&apinfo, ap, sizeof(apinfo));
    aos_post_event(EV_BZ_COMBO, COMBO_EVT_CODE_AP_INFO, (unsigned long)&apinfo);
}

static int combo_80211_frame_callback(char *buf, int length, enum AWSS_LINK_TYPE link_type, int with_fcs, signed char rssi)
{
    uint8_t ssid[MAX_SSID_LEN] = {0}, bssid[ETH_ALEN] = {0};
    struct ieee80211_hdr *hdr;
    int fc;
    int ret = -1;

    if (link_type != AWSS_LINK_TYPE_NONE) {
        return -1;
    }
    hdr = (struct ieee80211_hdr *)buf;

    fc = hdr->frame_control;
    if (!HAL_IEEE80211_IS_BEACON(fc) && !HAL_IEEE80211_IS_PROBE_RESP(fc)) {
        return -1;
    }
    ret = HAL_IEEE80211_GET_BSSID((uint8_t *)hdr, bssid);
    if (ret < 0) {
        return -1;
    }
    if (memcmp(config.bssid, bssid, ETH_ALEN) != 0) {
        return -1;
    }
    ret = HAL_IEEE80211_GET_SSID((uint8_t *)hdr, length, ssid);
    if (ret < 0) {
        return -1;
    }
    monitor_got_bssid = 1;
    strncpy(config.ssid, ssid, sizeof(config.ssid) - 1);
    return 0;
}

static char combo_is_str_asii(char *str)
{
    for (uint32_t i = 0; str[i] != '\0'; ++i) {
        if ((uint8_t) str[i] > 0x7F) {
            return 0;
        }
    }
    return 1;
}

int ms_cnt = 0;
static void combo_connect_ap(breeze_apinfo_t * info)
{
    uint8_t ap_connected = 0;
    ap_scan_info_t scan_result;
    int ap_scan_result = -1;
    int rssi = 0;

    memset(&config, 0, sizeof(netmgr_ap_config_t));
    if (!info) {
        app_info("combo connect ap info null!!");
        return;
    }

    strncpy(config.ssid, info->ssid, sizeof(config.ssid) - 1);
    strncpy(config.pwd, info->pw, sizeof(config.pwd) - 1);
    memcpy(config.bssid, info->bssid, ETH_ALEN);
    // if awss is running, should stop it because awss info got
    if (g_combo_wifi.awss_run) {
        // for combo device, ble_awss and smart_config awss mode can exist simultaneously
        device_stop_awss();
    }
    if (info->apptoken_len > 0) {
        // if AppToken get, set for binding
        awss_set_token(info->apptoken, info->token_type);
    }
    // get region information
    if (info->region_type == REGION_TYPE_ID) {
        app_info("info->region_id: %d", info->region_id);
        iotx_guider_set_dynamic_region(info->region_id);
    } else if (info->region_type == REGION_TYPE_MQTTURL) {
        app_info("info->region_mqtturl: %s", info->region_mqtturl);
        iotx_guider_set_dynamic_mqtt_url(info->region_mqtturl);
    } else {
        app_info("REGION TYPE not supported");
        iotx_guider_set_dynamic_region(IOTX_CLOUD_REGION_INVALID);
    }
    if (!combo_is_str_asii(config.ssid)) {
        uint64_t pre_chn_time = HAL_UptimeMs();
        uint64_t cur_chn_time = pre_chn_time;
        uint8_t channel = 6;

        app_info("SSID not asicci encode");
        monitor_got_bssid = 0;
        ms_cnt = 0;
        apscan_reset_scan_chan();
        HAL_AWSS_OPEN_MONITOR(combo_80211_frame_callback);
        HAL_AWSS_SWITCH_CHANNEL(channel, 0, NULL);
        last_errsubcode = 0;
        app_info(" last_errsubcode %x", last_errsubcode);
        while (ms_cnt < 10000) {
            if (!monitor_got_bssid) {
                cur_chn_time = HAL_UptimeMs();
                if (cur_chn_time - pre_chn_time > 250) {
                    channel = apscan_next_scan_chan();
                    app_info("next chan(%d)", channel);
                    HAL_AWSS_SWITCH_CHANNEL(channel, 0, NULL);
                    pre_chn_time = cur_chn_time;
                }
                aos_msleep(50);
                ms_cnt += 50;
            } else {
                break;
            }
        }
        HAL_AWSS_CLOSE_MONITOR();
    }

    // Save valid ap info and start to connect AP
    aiot_ais_report_awss_status(AIS_AWSS_STATUS_PROGRESS_REPORT, AIS_AWSS_PROGRESS_CONNECT_AP_START);
    netmgr_set_ap_config(&config);
    hal_wifi_suspend_station(NULL);
    netmgr_reconnect_wifi();
    ms_cnt = 0;
    while (ms_cnt < 45000) {
        // set connect ap timeout
        if (netmgr_get_ip_state() == false) {
            aos_msleep(500);
            ms_cnt += 500;
            app_info("wait ms_cnt(%d)", ms_cnt);
            if(ms_cnt==10000) {
                char* info = "{\"v\":\"%s\",\"t\":\"%d\",\"s\":\"%s\",\"r\":\"%d\",\"p\":\"%s%d\"}";
                int len = strlen(info) + strlen(aos_get_app_version()) + strlen(config.ssid) + 30;
                // uint32_t t = HAL_UptimeMs();
                unsigned char sign[16] = {0};
                unsigned char out[33] = {0};
                #include "utils_md5.h"
                if (strlen(config.pwd) > 0) {
                    utils_md5((unsigned char *)config.pwd, strlen(config.pwd), sign);
                    utils_md5_hexstr(sign, out);
                    LITE_hexbuf_convert(sign, out, 16, 1);
                    out[10] = 0;
                    out[11] = strlen((unsigned char *)config.pwd);
                }
                #if (defined (TG7100CEVB))
                wifi_mgmr_rssi_get(&rssi);
                #endif
                char* data = (char*)aos_malloc(len);
                if(data) {
                    memset(data, 0, len);
                    HAL_Snprintf(data, len, info, aos_get_app_version(),(uint32_t)HAL_UptimeMs(),config.ssid, rssi, out, out[11]);
                    app_info("----%s", data);
                    int payloadlen = strlen(data);
                    int i = 0;
                    for(i = payloadlen; i >= 0; i--) {
                        data[i+5] = data[i];
                    }
                    data[0] = 0x01;
                    data[1] = 0x01;
                    data[2] = 0x07;
                    data[3] = 0x07;
                    data[4] = payloadlen;

                    int ret = 0;
                    do{
                      ret = breeze_post(data, payloadlen+5);
                      aos_msleep(10);
                    } while (ret != 0);
                    aos_free(data);
                }
            }
            if (netmgr_get_ip_state() == true) {
                app_info("AP connected");
                ap_connected = 1;
                break;
            }
        } else {
            app_info("AP connected");
            ap_connected = 1;
            break;
        }
    }
    ms_cnt = 0;
    if (!ap_connected) {
        uint16_t err_code = 0;

        // if AP connect fail, should inform the module to suspend station
        // to avoid module always reconnect and block Upper Layer running
        hal_wifi_suspend_station(NULL);
        #if (defined (TG7100CEVB))
        int s_code = 0;
        wifi_mgmr_status_code_get(&s_code);
            switch (s_code)
            {
            case 0:
                err_code = 0xC4E3; // AP connect fail(Authentication fail or Association fail or AP exeption)
                break;
            case 12:
                err_code = 0xC4E0; // 50400
                break;
            case 6:
            case 8:
            case 9:
            case 10:
            case 17:
            case 18:
                err_code = 0x0100 | s_code; // or 50403?
                break;

            default:
                err_code = 0x0100 | s_code;
                break;
            }
            wifi_mgmr_rssi_get(&rssi);
            if (rssi < -70)
                err_code = 0xC4E1; // 50401: rssi too low
        #else
        // if ap connect failed in specified timeout, rescan and analyse fail reason
        memset(&scan_result, 0, sizeof(ap_scan_info_t));
        app_info("start awss_apscan_process");
        ap_scan_result = awss_apscan_process(NULL, config.ssid, &scan_result);
        app_info("stop awss_apscan_process %x", last_errsubcode);
        if ( (ap_scan_result == 0) && (scan_result.found) ) {
            if (scan_result.rssi < -70) {
                err_code = 0xC4E1; // rssi too low
            } else {
                if(last_errsubcode == 0)
                    err_code = 0xC4E3; // AP connect fail(Authentication fail or Association fail or AP exeption)
                else
                    err_code = last_errsubcode;
            }
        } else {
            err_code = 0xC4E0; // AP not found
        }
        #endif

        if(g_ble_state){
            uint8_t ble_rsp[DEV_ERRCODE_MSG_MAX_LEN + 8] = {0};
            uint8_t ble_rsp_idx = 0;
            ble_rsp[ble_rsp_idx++] = 0x01;                          // Notify Code Type
            ble_rsp[ble_rsp_idx++] = 0x01;                          // Notify Code Length
            ble_rsp[ble_rsp_idx++] = 0x02;                          // Notify Code Value, 0x02-fail
            ble_rsp[ble_rsp_idx++] = 0x03;                          // Notify SubErrcode Type
            ble_rsp[ble_rsp_idx++] = sizeof(err_code);              // Notify SubErrcode Length
            memcpy(ble_rsp + ble_rsp_idx, (uint8_t *)&err_code, sizeof(err_code));  // Notify SubErrcode Value
            ble_rsp_idx += sizeof(err_code);

            int ret = 0;
            do{
                ret = breeze_post(ble_rsp, ble_rsp_idx);
                aos_msleep(20);
            } while (ret != 0);
        }

        combo_set_ap_state(COMBO_AP_DISCONNECTED);
        netmgr_clear_ap_config();

        /* always restart awss if connect ap failed */
        device_start_awss(0);
        last_errsubcode = 0;
    }
}

void wifi_connect_handler(void *arg)
{
    while(1) {
        aos_sem_wait(&wifi_connect_sem, AOS_WAIT_FOREVER);
        if (g_combo_wifi.awss_run) {
            combo_connect_ap(&g_apinfo);
        }
    }
}

static void combo_restart_ble_adv()
{
    int32_t valid_ap = 0;
    valid_ap = netmgr_has_valid_ap();

    if (!g_ble_state) {
        if (!g_combo_wifi.cloud_conn) {
            if ((valid_ap && (g_combo_wifi.ap_conn == COMBO_AP_DISCONNECTED)) || !valid_ap) {
                // combo device ap is not configed
                if (g_combo_wifi.awss_run) {
                    // in awss mode, should advertising and wait for wifi config
                    // If device is not advertising, it's a redundant operation
#if (!defined (TG7100CEVB))
                    breeze_stop_advertising();
                    aos_msleep(300); // wait for adv stop
#endif
                    breeze_start_advertising(g_combo_bind.bind_state, COMBO_AWSS_NORMAL);
                } else {
                    if (g_silent_adv_support) {
                        breeze_start_advertising(g_combo_bind.bind_state, COMBO_AWSS_SILENT);
                    } else {
                        breeze_stop_advertising();
                    }
                }
            } else {
                // Stop ble adv when ap is configed
                breeze_stop_advertising();
            }
        }
    }
}

static void combo_service_evt_handler(input_event_t * event, void *priv_data)
{
    if (event->type != EV_BZ_COMBO) {
        return;
    }

    if (event->code == COMBO_EVT_CODE_AP_INFO) {
        memcpy((void *)&g_apinfo, (void *)event->value, sizeof(breeze_apinfo_t));
        aos_sem_signal(&wifi_connect_sem);
    } else if (event->code == COMBO_EVT_CODE_RESTART_ADV) {
        combo_restart_ble_adv();
    } else {
        app_err("Unknown combo event");
    }
}

static int reverse_byte_array(uint8_t *byte_array, int byte_len) {
    uint8_t temp = 0;
    int i = 0;
    if (!byte_array) {
        app_err("reverse_mac invalid params!");
        return -1;
    }
    for (i = 0; i < (byte_len / 2); i++) {
        temp = byte_array[i];
        byte_array[i] = byte_array[byte_len - i - 1];
        byte_array[byte_len - i - 1] = temp;
    }
    return 0;
}

int combo_net_init()
{
    breeze_dev_info_t dinfo = { 0 };
    int len;

    len = sizeof(g_combo_pk);
    vendor_get_product_key(g_combo_pk, &len);

    len = sizeof(g_combo_ps);
    vendor_get_product_secret(g_combo_ps, &len);

    len = sizeof(g_combo_dn);
    vendor_get_device_name(g_combo_dn, &len);

    len = sizeof(g_combo_ds);
    vendor_get_device_secret(g_combo_ds, &len);

    vendor_get_product_id(&g_combo_pid);

    aos_register_event_filter(EV_BZ_COMBO, combo_service_evt_handler, NULL);
#if (defined (TG7100CEVB))
    aos_task_new("wifi_connect_task", wifi_connect_handler, NULL, 2048 + 1024);
#else
    aos_task_new("wifi_connect_task", wifi_connect_handler, NULL, 2048);
#endif
    aos_sem_new(&wifi_connect_sem, 0);

    aiot_set_wirelesslog_code_cb(aiot_get_subcode);

    if ((strlen(g_combo_pk) > 0) && (strlen(g_combo_ps) > 0)
            && (strlen(g_combo_dn) > 0) && (strlen(g_combo_ds) > 0) && g_combo_pid > 0) {
        uint8_t combo_adv_mac[6] = {0};
        os_wifi_get_mac(combo_adv_mac);
        // Set wifi mac to breeze awss adv, to notify app this is a wifi dev
        // Awss use wifi module device info.
        // Only use BT mac when use breeze and other bluetooth communication functions
        reverse_byte_array(combo_adv_mac, 6);
        dinfo.product_id = g_combo_pid;
        dinfo.product_key = g_combo_pk;
        dinfo.product_secret = g_combo_ps;
        dinfo.device_name = g_combo_dn;
        dinfo.device_secret = g_combo_ds;
        dinfo.dev_adv_mac = combo_adv_mac;
        breeze_awss_init(&dinfo,
                         combo_status_change_cb,
                         combo_set_dev_status_cb,
                         combo_get_dev_status_cb,
                         combo_apinfo_rx_cb,
                         NULL);
        breeze_awss_start();
    } else {
        app_info("combo device info not set!");
    }
    return 0;
}

int combo_net_deinit()
{
    breeze_awss_stop();
    return 0;
}

uint8_t combo_ble_conn_state(void)
{
    return g_ble_state;
}

void combo_set_cloud_state(uint8_t cloud_connected)
{
#ifdef DEV_STATEMACHINE_ENABLE
    dev_awss_state_set(AWSS_PATTERN_BLE_CONFIG, AWSS_STATE_STOP);
#endif

    g_combo_wifi.cloud_conn = cloud_connected;
    if (!g_ble_state) {
        aos_post_event(EV_BZ_COMBO, COMBO_EVT_CODE_RESTART_ADV, 0);
    }
}

void combo_set_ap_state(uint8_t ap_connected)
{
    g_combo_wifi.ap_conn = ap_connected;
    if (!g_ble_state) {
        aos_post_event(EV_BZ_COMBO, COMBO_EVT_CODE_RESTART_ADV, 0);
    }
}

void combo_set_awss_state(uint8_t awss_running)
{
    g_combo_wifi.awss_run = awss_running;
    if (!g_ble_state) {
        aos_post_event(EV_BZ_COMBO, COMBO_EVT_CODE_RESTART_ADV, 0);
    }
}

void combo_ap_conn_notify(void)
{
    uint8_t rsp[] = {0x01, 0x01, 0x01};
    if (g_ble_state) {

        int ret = 0;
        do{
            ret = breeze_post(rsp, sizeof(rsp));
            aos_msleep(10);
        } while (ret != 0);
    }
}

void combo_token_report_notify(void)
{
    uint8_t rsp[] = { 0x01, 0x01, 0x03 };
    if (g_ble_state) {
        int ret = 0;
        do{
            ret = breeze_post(rsp, sizeof(rsp));
            aos_msleep(10);
        } while (ret != 0);
    }
}

int32_t aiot_ais_report_log(const char *fmt, ...)
{
    char buff[50] = {0x05, 0x00};
    va_list args;
    int32_t res = STATE_SUCCESS;

    if (g_ble_state == 0) {
        return -1;
    }

    va_start(args, fmt);
    HAL_Vsnprintf(buff + 2, sizeof(buff) - 2, fmt, args);
    va_end(args);

    buff[1] = strlen(buff + 2);

    BREEZE_DEBUG("aiot_ais_report_log");
    return breeze_post(buff, strlen(buff));
}

#endif
