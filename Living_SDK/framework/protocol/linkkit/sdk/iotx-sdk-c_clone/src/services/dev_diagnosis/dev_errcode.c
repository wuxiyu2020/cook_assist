/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifdef DEV_ERRCODE_ENABLE
#include "dev_diagnosis_log.h"
#include "dev_errcode.h"

static int last_suberror_code = 0;
static uint16_t last_error_code = 0;

#ifdef EN_COMBO_NET
extern char g_ble_state;
extern uint32_t breeze_post(uint8_t *buffer, uint32_t length);
#endif

void dev_errcode_module_init()
{
    int ret = 0;
    int errcode_len = sizeof(uint16_t);
    /* device errcode service init */
    ret = HAL_Kv_Get(DEV_ERRCODE_KEY, (void *)&last_error_code, &errcode_len);
    if (ret != 0)
    {
        diagnosis_info("no history err_code found");
        last_error_code = 0;
    }
}

int log_bucket_report_cached_log(void);

int dev_errcode_handle(const int sub_errcode, const char *state_message)
{
    uint16_t err_code = 0;
    char err_msg[DEV_ERRCODE_MSG_MAX_LEN + 8] = {0};
#ifdef EN_COMBO_NET
    uint8_t ble_rsp[DEV_ERRCODE_MSG_MAX_LEN + 8] = {0};
    uint8_t ble_rsp_idx = 0;
#endif

    // Ignore some state code
    if (sub_errcode == STATE_CODE_WIFI_CHAN_SCAN || sub_errcode == STATE_CODE_WIFI_ZCONFIG_DESTROY)
    {
        diagnosis_info("scode=0x%04X :%s", sub_errcode, state_message);
        return 0;
    }

    last_suberror_code = sub_errcode;

    if (sub_errcode < 0)
    {
        err_code = dev_errcode_sdk_filter(sub_errcode);
    }

    if (err_code > 0)
    {
        diagnosis_err("ecode=0x%04X sub ecode=-0x%04X :%s", err_code, -sub_errcode, state_message == NULL ? "no detail mesg" : state_message);
    }
    else
    {
        if (sub_errcode < 0) // This is sub error code
        {
            diagnosis_err("sub ecode=-0x%04X :%s", -sub_errcode, state_message == NULL ? "no detail mesg" : state_message);
        }
        else // This is state code
        {
            diagnosis_crit("scode=0x%04X :%s", sub_errcode, state_message == NULL ? "no detail mesg" : state_message);
        }
    }

    if (err_code > 0)
    {
#ifdef EN_COMBO_NET
        if (g_ble_state)
        {
            int is_report_log = 0;

            switch (err_code)
            {
            case DEV_ERRCODE_AP_DISCOVER_FAIL:
            case DEV_ERRCODE_AP_RSSI_TOO_LOW:
            case DEV_ERRCODE_AP_CONN_PASSWD_ERR:
            case DEV_ERRCODE_AP_GET_IP_FAIL:
            case DEV_ERRCODE_AP_CONN_LOCAL_NOTI_FAIL:
            case DEV_ERRCODE_AP_CONN_OTHERS_FAIL:

            case DEV_ERRCODE_HTTPS_DNS_FAIL:
            case DEV_ERRCODE_HTTPS_PREAUTH_TIMEOUT:
            case DEV_ERRCODE_HTTPS_DEVAUTH_FAIL:

            case DEV_ERRCODE_MQTT_INIT_FAIL:
            case DEV_ERRCODE_MQTT_CONN_TIMEOUT:
            case DEV_ERRCODE_MQTT_AUTH_FAIL:
            {
                #ifdef CONFIG_WIRELESS_LOG
                log_bucket_report_cached_log();
                #endif
                is_report_log = 1;
            }
            break;
            default:
                break;
            }

            ble_rsp[ble_rsp_idx++] = 0x01;                                         // Notify Code Type
            ble_rsp[ble_rsp_idx++] = 0x01;                                         // Notify Code Length
            ble_rsp[ble_rsp_idx++] = 0x02;                                         // Notify Code Value, 0x02-fail
            ble_rsp[ble_rsp_idx++] = 0x03;                                         // Notify SubErrcode Type
            ble_rsp[ble_rsp_idx++] = sizeof(err_code);                             // Notify SubErrcode Length
            memcpy(ble_rsp + ble_rsp_idx, (uint8_t *)&err_code, sizeof(err_code)); // Notify SubErrcode Value
            ble_rsp_idx += sizeof(err_code);

            if (is_report_log) // Waiting log report done
            {
                aos_msleep(5 * 1000);
            }

            // BZ_EBUSY=7
            while (7 == breeze_post(ble_rsp, ble_rsp_idx))
            {
                aos_msleep(20);
            }
        }
#endif

        if (err_code != last_error_code)
        {
            last_error_code = err_code;
            HAL_Snprintf(err_msg, DEV_ERRCODE_MSG_MAX_LEN + 8, "-0x%04x,%s", -sub_errcode, state_message == NULL ? "NULL" : state_message);
            dev_errcode_kv_set(err_code, err_msg);
        }
    }

    return 0;
}

uint16_t dev_errcode_sdk_filter(const int sub_errcode)
{
    uint16_t err_code = 0;
#ifdef DEV_STATEMACHINE_ENABLE
    dev_state_t dev_state = DEV_STATE_MAX;
    dev_state = dev_state_get();
#endif

    switch (sub_errcode)
    {
    case SUB_ERRCODE_USER_INPUT_NULL_POINTER:
    case SUB_ERRCODE_USER_INPUT_OUT_RANGE:
    case SUB_ERRCODE_USER_INPUT_PK:
    case SUB_ERRCODE_USER_INPUT_PS:
    case SUB_ERRCODE_USER_INPUT_DN:
    case SUB_ERRCODE_USER_INPUT_DS:
    case SUB_ERRCODE_SYS_DEPEND_MALLOC:
    case SUB_ERRCODE_SYS_DEPEND_MUTEX_CREATE:
    case SUB_ERRCODE_SYS_DEPEND_MUTEX_LOCK:
    case SUB_ERRCODE_SYS_DEPEND_MUTEX_UNLOCK:
    case SUB_ERRCODE_SYS_DEPEND_SEMAPHORE_CREATE:
    case SUB_ERRCODE_SYS_DEPEND_SEMAPHORE_WAIT:
    case SUB_ERRCODE_SYS_DEPEND_TIMER_CREATE:
    case SUB_ERRCODE_SYS_DEPEND_TIMER_START:
    case SUB_ERRCODE_SYS_DEPEND_TIMER_STOP:
    case SUB_ERRCODE_SYS_DEPEND_TIMER_DELETE:
    case SUB_ERRCODE_SYS_DEPEND_THREAD_CREATE:
    {
        err_code = DEV_ERRCODE_SYSTERM_ERR;
    }
    break;
    /* WIFI monitor state */

    /* AWSS state */
    // smart-config
    case STATE_CODE_WIFI_PROCESS_FRAME:
    case SUB_ERRCODE_WIFI_PASSWD_DECODE_FAILED:
    case SUB_ERRCODE_WIFI_CRC_ERROR:
    case SUB_ERRCODE_WIFI_BCAST_UNSUPPORT:
    {
        err_code = DEV_ERRCODE_SC_SSID_PWD_PARSE_ERR;
    }
    break;
    // dev-ap
    case SUB_ERRCODE_WIFI_DEV_AP_START_FAIL:
        err_code = DEV_ERRCODE_DA_DEV_AP_START_FAIL;
        break;
    case SUB_ERRCODE_WIFI_DEV_AP_RECV_IN_WRONG_STATE:
    case SUB_ERRCODE_WIFI_DEV_AP_SEND_PKT_FAIL:
        err_code = DEV_ERRCODE_DA_SSID_PWD_GET_TIMEOUT;
        break;
    case SUB_ERRCODE_WIFI_DEV_AP_RECV_PKT_INVALID:
        err_code = DEV_ERRCODE_DA_VERSION_ERR;
        break;
    case SUB_ERRCODE_WIFI_DEV_AP_PARSE_PKT_FAIL:
        err_code = DEV_ERRCODE_DA_PKT_CHECK_ERR;
        break;
    case SUB_ERRCODE_WIFI_DEV_AP_PASSWD_DECODE_FAILED:
        err_code = DEV_ERRCODE_DA_SSID_PWD_PARSE_ERR;
        break;
    case SUB_ERRCODE_WIFI_DEV_AP_CLOSE_FAIL:
        err_code = DEV_ERRCODE_DA_SWITCH_STA_FAIL;
        break;
    // zero-config
    // ble-config
    case SUB_ERRCODE_BLE_START_ADV_FAIL:
        err_code = DEV_ERRCODE_BA_BLE_ADV_START_FAIL;
        break;
    case SUB_ERRCODE_BLE_CONNECT_FAIL:
        err_code = DEV_ERRCODE_BA_BLE_CONN_FAIL;
        break;
    case SUB_ERRCODE_AWSS_SSID_PWD_PARSE_FAIL:
        err_code = DEV_ERRCODE_BA_SSID_PWD_PARSE_ERR;
        break;

    /* Router Connect */
    case SUB_ERRCODE_WIFI_AP_CONN_IP_GET_FAIL:
        err_code = DEV_ERRCODE_AP_GET_IP_FAIL;
        break;
    case SUB_ERRCODE_WIFI_SENT_CONNECTAP_NOTI_TIMEOUT:
        err_code = DEV_ERRCODE_AP_CONN_LOCAL_NOTI_FAIL;
        break;
    case SUB_ERRCODE_WIFI_AP_DISCOVER_FAIL:
        err_code = DEV_ERRCODE_AP_DISCOVER_FAIL;
        break;
    case SUB_ERRCODE_WIFI_AP_RSSI_TOO_LOW:
        err_code = DEV_ERRCODE_AP_RSSI_TOO_LOW;
        break;
    case SUB_ERRCODE_WIFI_AP_CONN_PASSWD_ERR:
        err_code = DEV_ERRCODE_AP_CONN_PASSWD_ERR;
        break;
    case SUB_ERRCODE_WIFI_AP_CONN_OTHERS_FAIL:
        err_code = DEV_ERRCODE_AP_CONN_OTHERS_FAIL;
        break;

    /* Cloud Connect */
    case SUB_ERRCODE_HTTP_NWK_INIT_FAIL:
        err_code = DEV_ERRCODE_HTTPS_INIT_FAIL;
        break;
    case SUB_ERRCODE_USER_INPUT_HTTP_DOMAIN:
    case SUB_ERRCODE_HTTP_PREAUTH_DNS_FAIL:
        err_code = DEV_ERRCODE_HTTPS_DNS_FAIL;
        break;
    case SUB_ERRCODE_HTTP_PREAUTH_TIMEOUT_FAIL:
        err_code = DEV_ERRCODE_HTTPS_PREAUTH_TIMEOUT;
        break;
    case SUB_ERRCODE_HTTP_PREAUTH_IDENT_AUTH_FAIL:
        err_code = DEV_ERRCODE_HTTPS_DEVAUTH_FAIL;
        break;
    case SUB_ERRCODE_USER_INPUT_MQTT_DOMAIN:
    case SUB_ERRCODE_MQTT_INIT_FAIL:
        err_code = DEV_ERRCODE_MQTT_INIT_FAIL;
        break;
    case SUB_ERRCODE_MQTT_CERT_VERIFY_FAIL:
    case SUB_ERRCODE_MQTT_CONNACK_VERSION_UNACCEPT:
    case SUB_ERRCODE_MQTT_CONNACK_IDENT_REJECT:
    case SUB_ERRCODE_MQTT_CONNACK_NOT_AUTHORIZED:
    case SUB_ERRCODE_MQTT_CONNACK_BAD_USERDATA:
    case SUB_ERRCODE_MQTT_CONNECT_UNKNOWN_FAIL:
        err_code = DEV_ERRCODE_MQTT_AUTH_FAIL;
        break;
    case SUB_ERRCODE_MQTT_NETWORK_CONNECT_ERROR:
    case SUB_ERRCODE_MQTT_CONNECT_PKT_SEND_FAIL:
    case SUB_ERRCODE_MQTT_CONNACK_SERVICE_NA:
        err_code = DEV_ERRCODE_MQTT_CONN_TIMEOUT;
        break;
    // dev bind
    case SUB_ERRCODE_BIND_COAP_INIT_FAIL:
        err_code = DEV_ERRCODE_COAP_INIT_FAIL;
        break;
    case SUB_ERRCODE_BIND_REPORT_TOKEN_TIMEOUT:
        err_code = DEV_ERRCODE_TOKEN_RPT_CLOUD_TIMEOUT;
        break;
    case SUB_ERRCODE_BIND_MQTT_MSG_INVALID:
        err_code = DEV_ERRCODE_TOKEN_RPT_CLOUD_ACK_ERR;
        break;
    case SUB_ERRCODE_BIND_COAP_MSG_INVALID:
        err_code = DEV_ERRCODE_TOKEN_GET_LOCAL_PKT_ERR;
        break;
    case SUB_ERRCODE_BIND_APP_GET_TOKEN_RESP_FAIL:
        err_code = DEV_ERRCODE_TOKEN_GET_LOCAL_RSP_ERR;
        break;
    default:
        break;
    }

    return err_code;
}

int dev_errcode_kv_set(uint16_t err_code, char *p_msg)
{
    int ret = 0;

    ret = HAL_Kv_Set(DEV_ERRCODE_KEY, (void *)&err_code, sizeof(err_code), 1);
    if (ret == 0)
    {
        ret = HAL_Kv_Set(DEV_ERRCODE_MSG_KEY, (void *)p_msg, strlen(p_msg), 1);
    }

    if (ret != 0)
    {
        diagnosis_err("err_code set fail(%d)", ret);
    }

    return ret;
}

int dev_errcode_kv_get(uint16_t *p_errcode, char *p_msg)
{
    int ret = 0;
    int errcode_len = sizeof(uint16_t);
    int msg_len = DEV_ERRCODE_MSG_MAX_LEN;
    ret = HAL_Kv_Get(DEV_ERRCODE_KEY, (void *)p_errcode, &errcode_len);
    if (ret == 0)
    {
        ret = HAL_Kv_Get(DEV_ERRCODE_MSG_KEY, (void *)p_msg, &msg_len);
    }
    else
    {
        diagnosis_err("get err_code failed(%d)!\r\n", ret);
    }
    return ret;
}

int dev_errcode_kv_del()
{
    int ret = 0;
    ret = HAL_Kv_Del(DEV_ERRCODE_KEY);
    if (ret == 0)
    {
        ret = HAL_Kv_Del(DEV_ERRCODE_MSG_KEY);
    }
    else
    {
        diagnosis_err("del err_code failed(%d)!\r\n", ret);
    }
    return ret;
}

#endif
