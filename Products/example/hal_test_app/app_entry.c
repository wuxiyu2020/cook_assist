/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
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
#include "app_entry.h"
#include "aos/kv.h"

#ifdef CSP_LINUXHOST
#include <signal.h>
#endif

#include <k_api.h>

#define PROBE_REQ_FIX_LEN                      (47)
#define PROBE_REQ_SA_POS                       (10)
#define PROBE_REQ_OUI_LEN_POS                  (43)
#define FRAME_FCS_LEN                          (4)
const uint8_t probe_req_test[PROBE_REQ_FIX_LEN] = {
    0x40, 0x00,  // mgnt type, frame control
    0x00, 0x00,  // duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // DA
    0x28, 0xC2, 0xDD, 0x61, 0x68, 0x83,  // SA, to be replaced with wifi mac
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // BSSID
    0xC0, 0x79,  // seq
    0x00, 0x00,  // hide ssid,
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x8C, 0x92, 0x98, 0xA4,  // supported rates
    0x32, 0x04, 0xB0, 0x48, 0x60, 0x6C,  // extended supported rates
    0xDD, 0x0A, 0xD8, 0x96, 0xE0   // OUI info
};

const uint8_t frame_fcs[FRAME_FCS_LEN] = {
    0x3F, 0x84, 0x10, 0x9E  // FCS
};

struct ieee80211_header {
    uint16_t frame_control;
    uint16_t duration_id;
    uint8_t addr1[ETH_ALEN];
    uint8_t addr2[ETH_ALEN];
    uint8_t addr3[ETH_ALEN];
    uint16_t seq_ctrl;
    uint8_t addr4[ETH_ALEN];
};

static void wifi_raw_frame_rx_callback(uint8_t *buffer, int length, signed char rssi, int buffer_type)
{
    uint8_t type = buffer[0], eid;
    uint8_t need_find_ie = 0;            // 0 - no need find ie, 1 - need find ie
    int len = 0;
    const uint8_t *ie = NULL;
    if (buffer_type) {
        // ie has been filtered and found by lower layer
        ie = buffer;
    } else {
        // ie should be pased here
        switch (type) {
            case MGMT_BEACON:
                buffer += MGMT_HDR_LEN + 12;  // hdr(24) + 12(timestamp, beacon_interval, cap)
                length -= MGMT_HDR_LEN + 12;
                eid = buffer[0];
                len = buffer[1];
                if (eid != 0) {
                    //awss_warn("error eid, should be 0!");
                    return;
                }
                // skip ssid
                buffer += 2;
                buffer += len;
                length -= len;
                need_find_ie = 1;
                break;
            case MGMT_PROBE_REQ:
                buffer += MGMT_HDR_LEN;
                length -= MGMT_HDR_LEN;
                need_find_ie = 1;
                break;
            default:
                break;
        }
    }
}

static int wifi_monitor_callback(char *buf, int length, enum AWSS_LINK_TYPE link_type, int with_fcs, signed char rssi)
{
    uint8_t ssid[HAL_MAX_SSID_LEN] = {0}, bssid[ETH_ALEN] = {0};
    uint16_t fc;
    uint8_t channel;
    uint8_t auth, pairwise_cipher, group_cipher;
    int ret;
    struct ieee80211_header *hdr;

    /* remove FCS filed */
    if (with_fcs) {
        length -= 4;
    }
    /* useless, will be removed */
    if (ieee80211_is_invalid_pkg(buf, length)) {
        return -1;
    }
    /* link type transfer for supporting linux system. */
    hdr = (struct ieee80211_header *)zconfig_remove_link_header((uint8_t **)&buf, &length, link_type);
    if (length <= 0) {
        return -1;
    }

    /* search ssid and bssid in management frame */
    fc = hdr->frame_control;
    if (!ieee80211_is_beacon(fc) && !ieee80211_is_probe_resp(fc)) {
        return -1;
    }
    ret = ieee80211_get_bssid((uint8_t *)hdr, bssid);
    if (ret < 0) {
        return -1;
    }
    ret = ieee80211_get_ssid((uint8_t *)hdr, length, ssid);
    if (ret < 0) {
        return -1;
    }

    rssi = rssi > 0 ? rssi - 256 : rssi;

    // designated ap found, get ap detail information
    channel = cfg80211_get_bss_channel((uint8_t *)hdr, length);
    cfg80211_get_cipher_info((uint8_t *)hdr, length, &auth,
                             &pairwise_cipher, &group_cipher);
    return 0;
}

static int wifi_scan_callback(const char ssid[HAL_MAX_SSID_LEN],
                        const uint8_t bssid[ETH_ALEN],
                        enum AWSS_AUTH_TYPE auth,
                        enum AWSS_ENC_TYPE encry,
                        uint8_t channel, signed char rssi,
                        int last_ap)
{
#if 0
#define ONE_AP_INFO_LEN_MAX           (141)
    static char *aplist = NULL;
    static int msg_len = 0;

    if (aplist == NULL) {
        aplist = HAL_Malloc(WIFI_APINFO_LIST_LEN);
        if (aplist == NULL) {
            return SHUB_ERR;
		}

        msg_len = 0;
        msg_len += snprintf(aplist + msg_len, WIFI_APINFO_LIST_LEN - msg_len - 1, "{\"awssVer\":%s, \"wifiList\":[", AWSS_VER);
    }

    if ((ssid != NULL) && (ssid[0] != '\0')) {
        uint8_t bssid_connected[ETH_ALEN] = {0};
        char *other_apinfo = HAL_Malloc(64);
        char *encode_ssid = HAL_Malloc(OS_MAX_SSID_LEN * 2 + 1);
        int ssid_len = strlen(ssid);
        ssid_len = ssid_len > OS_MAX_SSID_LEN - 1 ? OS_MAX_SSID_LEN - 1 : ssid_len;

        os_wifi_get_ap_info(NULL, NULL, bssid_connected);

        if (other_apinfo && encode_ssid) {
            if (memcmp(bssid_connected, bssid, ETH_ALEN) == 0) {
                snprintf(other_apinfo, 64 - 1, "\"auth\":\"%d\",\"connected\":\"1\"", auth);
            } else {
                snprintf(other_apinfo, 64 - 1, "\"auth\":\"%d\"", auth);
            }
            if (zconfig_is_utf8(ssid, ssid_len)) {
                strncpy(encode_ssid, (const char *)ssid, ssid_len);
                msg_len += snprintf(aplist + msg_len, WIFI_APINFO_LIST_LEN - msg_len - 1,
                                    "{\"ssid\":\"%s\",\"bssid\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"rssi\":\"%d\",%s},",
                                    encode_ssid, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                                    rssi > 0 ? rssi - 256 : rssi, other_apinfo);
            } else {
                utils_hex_to_str((uint8_t *)ssid, ssid_len, encode_ssid, OS_MAX_SSID_LEN * 2);
                msg_len += snprintf(aplist + msg_len, WIFI_APINFO_LIST_LEN - msg_len - 1,
                                    "{\"xssid\":\"%s\",\"bssid\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"rssi\":\"%d\",%s},",
                                    encode_ssid, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                                    rssi > 0 ? rssi - 256 : rssi, other_apinfo);
            }
        }

        if (other_apinfo) {
            os_free(other_apinfo);
        }
        if (encode_ssid) {
            os_free(encode_ssid);
        }
    }
    awss_debug("last_ap:%u\r\n", last_ap);

    if (last_ap || WIFI_APINFO_LIST_LEN < msg_len + ONE_AP_INFO_LEN_MAX + strlen(AWSS_ACK_FMT)) {
        if (last_ap) {
            AWSS_UPDATE_STATIS(AWSS_STATIS_PAP_IDX, AWSS_STATIS_TYPE_SCAN_STOP);
		}
        if (aplist[msg_len - 1] == ',') {
            msg_len--;    /* eating the last ',' */
		}
        msg_len += snprintf(aplist + msg_len, WIFI_APINFO_LIST_LEN - msg_len - 1, "]}");

        uint32_t tlen = DEV_SIMPLE_ACK_LEN + msg_len;
        msg_len = 0;
        char *msg_aplist = HAL_Malloc(tlen + 1);
        if (!msg_aplist) {
            os_free(aplist);
            aplist = NULL;
            return SHUB_ERR;
        }

        snprintf(msg_aplist, tlen, AWSS_ACK_FMT, g_req_msg_id, 200, aplist);
        os_free(aplist);
        aplist = NULL;

        scan_list_t *list = (scan_list_t *)HAL_Malloc(sizeof(scan_list_t));
        if (!list) {
            awss_debug("scan list fail\n");
            os_free(msg_aplist);
            return SHUB_ERR;
        }
        list->data = msg_aplist;
        HAL_MutexLock(g_scan_mutex);
        list_add(&list->entry, &g_scan_list);
        HAL_MutexUnlock(g_scan_mutex);

        if (last_ap) {
            if (scan_tx_wifilist_timer == NULL) {
                scan_tx_wifilist_timer = HAL_Timer_Create("wifilist", (void (*)(void *))wifimgr_scan_tx_wifilist, NULL);
            }
            HAL_Timer_Stop(scan_tx_wifilist_timer);
            HAL_Timer_Start(scan_tx_wifilist_timer, 1);
        }
        awss_debug("sending message to app: %s\n", msg_aplist);
    }
#endif
    return 0;
}

static void handle_at_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    int ret = 0;
    const char *cmdtype = argc > 1 ? argv[1] : "";
    const char *param_a = argc > 2 ? argv[2] : "";
    const char *param_b = argc > 3 ? argv[3] : "";
    const char *param_c = argc > 4 ? argv[4] : "";
    const char *param_d = argc > 5 ? argv[5] : "";

    aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_RUNNING);

    if (strcmp(cmdtype, CMD_GET_INFO) == 0) {
        //aos_msleep(200);
        aos_cli_printf("%s %s %s %s\r\n", cmdtype, CMD_RESP_COMPLETE, CTL_PROTOCOL_VERSION, aos_get_app_version());
    } else if (strcmp(cmdtype, CMD_GET_MAC) == 0) {
        char mac_str[HAL_MAC_LEN] = {0};
        HAL_Wifi_Get_Mac(mac_str);
        aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_COMPLETE, mac_str);
    } else if (strcmp(cmdtype, CMD_GET_IP) == 0) {
        char ip_addr[NETWORK_ADDR_LEN + 1] = {0};
        HAL_Wifi_Get_IP(ip_addr, 0);
        aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_COMPLETE, ip_addr);
    } else if (strcmp(cmdtype, CMD_GET_ENCRY_TYPE) == 0) {
        aos_cli_printf("%s %s %d %d\r\n", cmdtype, CMD_RESP_COMPLETE, HAL_Awss_Get_Encrypt_Type(), HAL_Awss_Get_Conn_Encrypt_Type());
    } else if (strcmp(cmdtype, CMD_GET_AWSS_TIMEOUT) == 0) {
        aos_cli_printf("%s %s %d\r\n", cmdtype, CMD_RESP_COMPLETE, HAL_Awss_Get_Timeout_Interval_Ms());
    } else if (strcmp(cmdtype, CMD_SET_RAW_FRAME_TX) == 0) {
        uint16_t frame_len = 0;
        if (strlen(param_b) > 0) {
            frame_len = (uint16_t)strtoul(param_b, NULL, 0);
        }
        if ((strcmp(param_a, CMD_PARAM_PROBE_REQ) == 0) && (frame_len > 0)) {
            uint16_t buf_idx = 0;
            uint8_t temp_data = 0x01;
            uint8_t *tx_probe_req = HAL_Malloc(frame_len + PROBE_REQ_FIX_LEN + FRAME_FCS_LEN);
            memcpy(tx_probe_req, probe_req_test, PROBE_REQ_FIX_LEN);
            tx_probe_req[PROBE_REQ_OUI_LEN_POS] = frame_len + 3;
            // update probe request frame src mac
            os_wifi_get_mac(tx_probe_req + PROBE_REQ_SA_POS);
            for (buf_idx = PROBE_REQ_FIX_LEN; buf_idx < frame_len; buf_idx++) {
                tx_probe_req[buf_idx] = temp_data;
                temp_data++;
            }
            // joint FCS, TODO
            memcpy(&tx_probe_req[frame_len + PROBE_REQ_FIX_LEN], frame_fcs, FRAME_FCS_LEN);
            ret = HAL_Wifi_Send_80211_Raw_Frame(FRAME_PROBE_REQ, tx_probe_req, frame_len + PROBE_REQ_FIX_LEN + FRAME_FCS_LEN);
            if (ret == 0) {
                aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
            } else {
                aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CALL_FAIL);
            }
        } else if (strcmp(param_a, CMD_PARAM_DATA) == 0) {
            //FRAME_DATA
        } else {
            aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_INVALID);
        }
    } else if (strcmp(cmdtype, CMD_SET_OPEN_RAW_RX) == 0) {
        uint8_t alibaba_oui[3] = {0xD8, 0x96, 0xE0};
        ret = HAL_Wifi_Enable_Mgmt_Frame_Filter(FRAME_BEACON_MASK | FRAME_PROBE_REQ_MASK, (uint8_t *)alibaba_oui, 
                                            wifi_raw_frame_rx_callback);
        if (ret == 0) {
            aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
        } else {
            aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CALL_FAIL);
        }
    } else if (strcmp(cmdtype, CMD_SET_CLOSE_RAW_RX) == 0) {
        uint8_t alibaba_oui[3] = {0xD8, 0x96, 0xE0};
        ret = HAL_Wifi_Enable_Mgmt_Frame_Filter(FRAME_BEACON_MASK | FRAME_PROBE_REQ_MASK, (uint8_t *)alibaba_oui, NULL);
        if (ret == 0) {
            aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
        } else {
            aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CALL_FAIL);
        }
    } else if (strcmp(cmdtype, CMD_GET_CHANSCAN_INTV) == 0) {
        aos_cli_printf("%s %s %d\r\n", cmdtype, CMD_RESP_COMPLETE, HAL_Awss_Get_Channelscan_Interval_Ms());
    } else if (strcmp(cmdtype, CMD_SET_OPEN_MONITOR) == 0) {
        HAL_Awss_Open_Monitor(wifi_monitor_callback);
        aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
    } else if (strcmp(cmdtype, CMD_SET_CLOSE_MONITOR) == 0) {
        HAL_Awss_Close_Monitor();
        aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
    } else if (strcmp(cmdtype, CMD_SET_SWITCH_CHAN) == 0) {
        uint8_t channel = 0;
        if (strlen(param_b) > 0) {
            channel = (uint8_t)strtoul(param_b, NULL, 0);
            HAL_Awss_Switch_Channel(channel, 0, NULL);
            aos_cli_printf("%s %s %d\r\n", cmdtype, CMD_RESP_COMPLETE, channel);
        } else {
            aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_INVALID);
        }
    } else if (strcmp(cmdtype, CMD_SET_START_SCAN) == 0) {
        ret = HAL_Wifi_Scan(wifi_scan_callback);
        if (ret == 0) {
            aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
        } else {
            aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CALL_FAIL);
        }
    } else if (strcmp(cmdtype, CMD_SET_CONNECT_AP) == 0) {
        if ((strlen(param_a) == 0) || (strlen(param_b) == 0) || (strlen(param_c) == 0)) {
            aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_INVALID);
        } else {
            uint32_t conn_to = (uint32_t)strtoul(param_a, NULL, 0);
            if (strlen(param_d) > 0) {
                uint8_t bssid[ETH_ALEN] = {0};
                os_wifi_str2mac(param_d, (char *)bssid);
                ret = HAL_Awss_Connect_Ap(conn_to, param_b, param_c, 0, 0, bssid, 0);
            } else {
                ret = HAL_Awss_Connect_Ap(conn_to, param_b, param_c, 0, 0, NULL, 0);
            }
            if (ret == 0) {
                aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
            } else {
                aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CALL_FAIL);
            }
        }
    } else if (strcmp(cmdtype, CMD_SET_CLEAR_AP) == 0) {
        iotx_sdk_reset_local();
        aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
        HAL_Reboot();
    } else if (strcmp(cmdtype, CMD_GET_AP_INFO) == 0) {
        char ssid[HAL_MAX_SSID_LEN] = {0};
        char passwd[HAL_MAX_PASSWD_LEN] = {0};
        uint8_t bssid[ETH_ALEN] = {0};
        ret = HAL_Wifi_Get_Ap_Info(ssid, passwd, bssid);
        if (ret == 0) {
            aos_cli_printf("%s %s %s %s %02X:%02X:%02X:%02X:%02X:%02X\r\n", cmdtype, CMD_RESP_COMPLETE, ssid, passwd, 
                            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
        } else {
            aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CALL_FAIL);
        }
    } else if (strcmp(cmdtype, CMD_GET_NET_STATUS) == 0) {
        aos_cli_printf("%s %s %d\r\n", cmdtype, CMD_RESP_COMPLETE, HAL_Sys_Net_Is_Ready());
    } else if (strcmp(cmdtype, CMD_SET_OPEN_SOFT_AP) == 0) {
        if ((strlen(param_a) == 0) || (strlen(param_b) == 0) || (strlen(param_c) == 0) || (strlen(param_d) == 0)) {
            aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_INVALID);
        } else {
            uint32_t beacon_interval = (uint32_t)strtoul(param_c, NULL, 0);
            uint8_t hide_ssid = (uint8_t)strtoul(param_d, NULL, 0);
            ret = HAL_Awss_Open_Ap(param_a, param_b, (int)beacon_interval, (int)hide_ssid);
            if (ret == 0) {
                aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
            } else {
                aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CALL_FAIL);
            }
        }
    } else if (strcmp(cmdtype, CMD_SET_CLOSE_SOFT_AP) == 0) {
        ret = HAL_Awss_Close_Ap();
        if (ret == 0) {
            aos_cli_printf("%s %s\r\n", cmdtype, CMD_RESP_COMPLETE);
        } else {
            aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CALL_FAIL);
        }
    } else {
        aos_cli_printf("%s %s %s\r\n", cmdtype, CMD_RESP_ERROR, E_DUT_CMD_NOT_FOUND);
    }
    return;
}

static struct cli_command atcmd = {
    "AT",
    "AT <COMMAND> [ARG1 ... ARGX]",
    handle_at_cmd
};

int application_start(int argc, char **argv)
{
    aos_cli_register_command(&atcmd);

    aos_loop_run();

    return 0;
}