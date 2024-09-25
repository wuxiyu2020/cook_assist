/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#ifndef __APP_ENTRY_H__
#define __APP_ENTRY_H__

#include <aos/aos.h>

/* WiFi Related definition */
#define MGMT_BEACON                  (0x80)
#define MGMT_PROBE_REQ               (0x40)
#define MGMT_PROBE_RESP              (0x50)
/* fc(2) + dur(2) + da(6) + sa(6) + bssid(6) + seq(2) */
#define MGMT_HDR_LEN                 (24)

/* Controller-DUT test command protocol */
#define CTL_PROTOCOL_VERSION         "1.0.0"

/* Common Command */
#define CMD_GET_INFO                 "GET_INFO"
#define CMD_GET_MAC                  "GET_MAC"
#define CMD_GET_IP                   "GET_IP"
#define CMD_GET_ENCRY_TYPE           "GET_ENCRY_TYPE"
#define CMD_GET_AWSS_TIMEOUT         "GET_AWSS_TIMEOUT"
#define CMD_SET_RAW_FRAME_TX         "SET_RAW_FRAME_TX"
    #define CMD_PARAM_PROBE_REQ      "PROBE_REQ"
    #define CMD_PARAM_PROBE_RESP     "PROBE_RESP"
    #define CMD_PARAM_DATA           "DATA"
    #define CMD_PARAM_BEACON         "BEACON"
    #define CMD_PARAM_ACTION         "ACTION"
#define CMD_SET_OPEN_RAW_RX          "SET_OPEN_RAW_RX"
#define CMD_SET_CLOSE_RAW_RX         "SET_CLOSE_RAW_RX"

#define CMD_GET_CHANSCAN_INTV        "CMD_GET_CHANSCAN_INTV"
#define CMD_SET_OPEN_MONITOR         "SET_OPEN_MONITOR"
#define CMD_SET_CLOSE_MONITOR        "SET_CLOSE_MONITOR"
#define CMD_SET_SWITCH_CHAN          "SET_SWITCH_CHAN"
#define CMD_SET_START_SCAN           "SET_START_SCAN"
#define CMD_SET_CONNECT_AP           "SET_CONNECT_AP"
#define CMD_SET_CLEAR_AP             "SET_CLEAR_AP"
#define CMD_GET_AP_INFO              "GET_AP_INFO"
#define CMD_GET_NET_STATUS           "GET_NET_STATUS"

/* Dev AP Command */
#define CMD_SET_OPEN_SOFT_AP         "SET_OPEN_SOFT_AP"
#define CMD_SET_CLOSE_SOFT_AP        "SET_CLOSE_SOFT_AP"

/* Response Status Code */
#define CMD_RESP_RUNNING             "RUNNING"                       // DUT received Command, start processing
#define CMD_RESP_INVALID             "INVALID"                       // DUT received Command, Command params error
#define CMD_RESP_ERROR               "ERROR"                         // DUT received Command, error occurred at processing
#define CMD_RESP_COMPLETE            "COMPLETE"                      // DUT received Command, and processing success
#define CMD_RESP_REPORT              "REPORT"                        // DUT after received Command, report data to controller

/* Test ErrCode */
#define E_DUT_CALL_FAIL              "CALL_FAIL"
#define E_DUT_CMD_NOT_FOUND          "CMD_NOT_FOUND"

typedef struct {
    int argc;
    char **argv;
}app_main_paras_t;

int linkkit_main(void *paras);
#endif
