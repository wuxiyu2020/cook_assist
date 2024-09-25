/*
 * Copyright (C) 2022-2023 Alibaba Group Holding Limited
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <aos/aos.h>
#include <breeze_export.h>
#include "log_bucket.h"
#include "ble_service.h"

#ifdef CONFIG_WIRELESS_LOG

#define BUCKET_NUM (5)
#define BUCKET_SIZE (1024)
#define MSG_QUEUE_COUNT 2
#define LOG_WAIT_TIME 1000
#define LOG_PACKET_SIZE (BZ_FRAME_SIZE_MAX - 5)
#define REAL_LOGBUF_SIZE LOG_PACKET_SIZE

typedef struct _log_bucket_s
{
    bool is_used;
    uint16_t write_pos;
    uint8_t bucket[BUCKET_SIZE];
} log_bucket_t;

typedef struct _log_cached_info_s
{
    uint8_t is_sending_cached_log_flag;
    log_bucket_t *log_buckets[BUCKET_NUM];
} log_cached_info_t;

typedef struct _log_real_info_s
{
    aos_queue_t g_cmd_msg_queue;
    uint8_t *g_cmd_msg_queue_buff;
    uint8_t *real_log_buf;
    uint8_t is_sending_real_log_flag;
} log_real_info_t;

static log_cached_info_t log_cached_info = {
    .is_sending_cached_log_flag = 0,
    .log_buckets = {NULL}};

static log_real_info_t log_real_info = {
    .is_sending_real_log_flag = 0};
static uint8_t current_bucket = 0;

static report_log_type_e report_log_type = REPROT_LOG_TYPE_MAX;
static real_log_level_e real_log_level = REAL_LOG_LEVEL_INFO;
static bool real_log_reporting = false;

static void print_cached_log(void)
{
    uint8_t index = 0;
    uint8_t length = BUCKET_NUM;

    index = (current_bucket + 1) % BUCKET_NUM;

    printf("log start==================\r\n");
    while (length--)
    {
        if (log_cached_info.log_buckets[index]->is_used && log_cached_info.log_buckets[index]->write_pos > 0)
        {
            for (uint16_t i = 0; i < log_cached_info.log_buckets[index]->write_pos; i++)
            {
                printf("%c", log_cached_info.log_buckets[index]->bucket[i]);
            }
        }
        index++;
        index = index % BUCKET_NUM;
    }
    printf("log end==================\r\n");
}

static void log_send_cached_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    print_cached_log();
}

static struct cli_command log_cmds[] = {
    {.name = "log",
     .help = "log cached",
     .function = log_send_cached_cmd}};

static int init_msg_queue(void)
{
    log_real_info.g_cmd_msg_queue_buff = (uint8_t *)HAL_Malloc(MSG_QUEUE_COUNT * REAL_LOGBUF_SIZE);
    if (NULL == log_real_info.g_cmd_msg_queue_buff)
    {
        return -1;
    }

    if (0 != aos_queue_new(&log_real_info.g_cmd_msg_queue, log_real_info.g_cmd_msg_queue_buff,
                           MSG_QUEUE_COUNT * REAL_LOGBUF_SIZE, REAL_LOGBUF_SIZE))
    {
        return -1;
    }

    return 0;
}

static int send_msg_to_queue(uint8_t *cmd_msg, int size)
{
    int ret = -1;

    if (cmd_msg == NULL)
    {
        printf("ptr is NULL");
        return ret;
    }
    if (real_log_reporting)
    {
        return 0;
    }
    ret = aos_queue_send(&log_real_info.g_cmd_msg_queue, cmd_msg, size);
    if (0 != ret)
    {
        printf("queue send failed:%d", ret);
    }
    return ret;
}

static int log_bucket_init(void)
{
    /* init msg queue for real log*/
    if (0 != init_msg_queue())
    {
        return -1;
    }
    memset(log_cached_info.log_buckets, 0, sizeof(log_cached_info.log_buckets));
    for (uint8_t i = 0; i < BUCKET_NUM; i++)
    {
        log_cached_info.log_buckets[i] = (log_bucket_t *)HAL_Malloc(sizeof(log_bucket_t));
        memset(log_cached_info.log_buckets[i], 0, sizeof(log_bucket_t));
        log_cached_info.log_buckets[i]->is_used = false;
    }

    log_real_info.real_log_buf = (uint8_t *)HAL_Malloc(sizeof(uint8_t) * REAL_LOGBUF_SIZE);
    memset(log_real_info.real_log_buf, 0, sizeof(uint8_t) * REAL_LOGBUF_SIZE);
    log_cached_info.log_buckets[0]->is_used = true;

    return 0;
}

static int log_bucket_write(const char log_level, const char *mode, const long long log_time, const char *fmt, va_list *ap)
{
    uint16_t writed_len = 0;
    uint8_t writed_head_len = 0;
    uint8_t writed_tail_len = 0;

    if (fmt == NULL || ap == NULL)
    {
        printf("error fmt is NULL\r\n");
        return -1;
    }
    if (log_cached_info.log_buckets[current_bucket] == NULL)
    {
        printf("error log_buckets is NULL\r\n");
        return -1;
    }

    if (log_level != 'F' && log_level != 'E' && log_level != 'W')
    {
        return 0;
    }

    /* write log head*/
    writed_head_len = HAL_Snprintf((char *)(log_cached_info.log_buckets[current_bucket]->bucket + log_cached_info.log_buckets[current_bucket]->write_pos),
                                   BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos, "[%06d]<%c>(%s) ", (unsigned)log_time, log_level, mode);
    if (writed_head_len < 0)
    {
        printf("log snprintf failed\r\n");
        return -1;
    }
    /*log_bucket space is not enough*/
    if (writed_head_len + 1 > BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos)
    {
        printf("log bucket(%d) full write len:%d room:%d\r\n", current_bucket, writed_head_len, BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos);
        current_bucket++;
        if (current_bucket >= BUCKET_NUM)
        {
            current_bucket = 0;
        }
        log_cached_info.log_buckets[current_bucket]->write_pos = 0;
        log_cached_info.log_buckets[current_bucket]->is_used = true;
        HAL_Snprintf((char *)(log_cached_info.log_buckets[current_bucket]->bucket + log_cached_info.log_buckets[current_bucket]->write_pos),
                     BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos, "[%06d]<%c>(%s) ", (unsigned)(log_time), log_level, mode);
    }
    log_cached_info.log_buckets[current_bucket]->write_pos += writed_head_len;
    /* write log context*/
    writed_len = HAL_Vsnprintf((char *)(log_cached_info.log_buckets[current_bucket]->bucket + log_cached_info.log_buckets[current_bucket]->write_pos), BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos, fmt, *ap);
    if (writed_len < 0)
    {
        printf("log vsnprintf failed\r\n");
        return -1;
    }
    if (writed_len + 1 > BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos)
    {
        printf("log bucket(%d) full write len:%d room:%d\r\n", current_bucket, writed_len, BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos);
        current_bucket++;
        if (current_bucket >= BUCKET_NUM)
        {
            current_bucket = 0;
        }
        log_cached_info.log_buckets[current_bucket]->write_pos = 0;
        log_cached_info.log_buckets[current_bucket]->is_used = true;
        HAL_Vsnprintf((char *)(log_cached_info.log_buckets[current_bucket]->bucket + log_cached_info.log_buckets[current_bucket]->write_pos),
                      BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos, fmt, *ap);
    }
    log_cached_info.log_buckets[current_bucket]->write_pos += writed_len;
    /*write log tail '\r\n'*/
    writed_tail_len = HAL_Snprintf((char *)(log_cached_info.log_buckets[current_bucket]->bucket + log_cached_info.log_buckets[current_bucket]->write_pos),
                                   BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos, "\r\n");
    if (writed_tail_len + 1 > BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos)
    {
        printf("log bucket(%d) full write len:%d room:%d\r\n", current_bucket, writed_len, BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos);
        current_bucket = (current_bucket + 1) % BUCKET_NUM;
        log_cached_info.log_buckets[current_bucket]->write_pos = 0;
        log_cached_info.log_buckets[current_bucket]->is_used = true;
        HAL_Snprintf((char *)(log_cached_info.log_buckets[current_bucket]->bucket + log_cached_info.log_buckets[current_bucket]->write_pos),
                     BUCKET_SIZE - log_cached_info.log_buckets[current_bucket]->write_pos, "\r\n");
    }

    log_cached_info.log_buckets[current_bucket]->write_pos += writed_tail_len;

    // printf("write bucket:%d len:%d\r\n", current_bucket, writed_head_len + writed_len + writed_tail_len);

    return 0;
}

static int real_log_write(const char log_level, const char *mode, const long long log_time, const char *fmt, va_list *ap)
{
    uint16_t reallog_len = 0;
    uint8_t reallog_head_len = 0;

    if (true != ble_ais_conn_enable_state() || log_real_info.is_sending_real_log_flag != 1)
    {
        return -1;
    }

    switch (real_log_level)
    {
    case REAL_LOG_LEVEL_INFO:
    {
        if (log_level == 'D')
        {
            return 0;
        }
    }
    break;
    case REAL_LOG_LEVEL_WARNNING:
    {
        if (log_level == 'D' || log_level == 'I')
        {
            return 0;
        }
    }
    break;
    case REAL_LOG_LEVEL_ERROR:
    {
        if (log_level == 'D' || log_level == 'I' || log_level == 'W')
        {
            return 0;
        }
    }
    break;

    default:
        break;
    }

    reallog_head_len = HAL_Snprintf((char *)log_real_info.real_log_buf, REAL_LOGBUF_SIZE, "[%06d]<%c>(%s) ", (unsigned)log_time, log_level, mode);
    reallog_len = HAL_Vsnprintf((char *)(log_real_info.real_log_buf + reallog_head_len), REAL_LOGBUF_SIZE - reallog_head_len - 1, fmt, *ap);
    if (reallog_head_len + reallog_len + 2 < REAL_LOGBUF_SIZE - 1)
    {
        HAL_Snprintf((char *)(log_real_info.real_log_buf + reallog_head_len + reallog_len), REAL_LOGBUF_SIZE - reallog_head_len - reallog_len, "\r\n");
        send_msg_to_queue(log_real_info.real_log_buf, reallog_head_len + reallog_len + 2);
    }
    else
    {
        send_msg_to_queue(log_real_info.real_log_buf, REAL_LOGBUF_SIZE);
    }
    return 0;
}

int log_wireless_write(const char log_level, const char *mode, const long long log_time, const char *fmt, va_list *ap)
{
    if (log_cached_info.is_sending_cached_log_flag == 0)
    {
        log_bucket_write(log_level, mode, log_time, fmt, ap);
    }

    if (log_real_info.is_sending_real_log_flag == 1)
    {
        real_log_write(log_level, mode, log_time, fmt, ap);
    }
}

int log_bucket_set_log_type(report_log_type_e log_type)
{
    report_log_type = log_type;

    printf("set log type:%d\r\n", log_type);

    if (log_type == REPROT_LOG_TYPE_REAL)
    {
        log_real_info.is_sending_real_log_flag = 1;
        printf("report real log start\r\n");
    }
    else if (log_type == REPROT_LOG_TYPE_CACHED_AND_REAL)
    {
        log_real_info.is_sending_real_log_flag = 0;
    }

    return 0;
}

int log_bucket_set_real_log_level(real_log_level_e log_level)
{
    real_log_level = log_level;

    printf("set real log level:%d\r\n", log_level);

    return 0;
}

static int log_bucket_report_packet(uint8_t type, uint8_t *data, uint32_t len)
{
    uint32_t ret = 0;
    uint8_t *report_buff = (uint8_t *)HAL_Malloc(len + 2);

    if (report_buff)
    {
        report_buff[0] = type;
        report_buff[1] = len;
        memcpy(&report_buff[2], data, len);

        while (1)
        {
            if (type == LOG_REPORT_TLV_CACHED_LOG_TYPE)
            {
                ret = breeze_logpost(report_buff, len + 2);
            }
            else
            {
                real_log_reporting = true;
                ret = breeze_logpost_fast(report_buff, len + 2);
                real_log_reporting = false;
            }

            if (ret == 7) // tx busy
            {
                aos_msleep(20);
                continue;
            }

            if (ret != 0)
            {
                printf("log report fail:%d\r\n", ret);
            }
            else
            {
                printf("log report len:%d ok\r\n", len);
            }

            break;
        }

        HAL_Free(report_buff);
    }
    else
    {
        printf("log report malloc size:%d fail\r\n", len + 2);
    }

    return 0;
}

int log_bucket_report(uint8_t type, uint8_t *data, uint32_t len)
{
    uint16_t need_send_log_len = len;
    uint16_t have_send_len = 0;

    while (need_send_log_len > 0)
    {
        if (need_send_log_len > LOG_PACKET_SIZE)
        {
            log_bucket_report_packet(type, data + have_send_len, LOG_PACKET_SIZE);
            have_send_len += LOG_PACKET_SIZE;
            need_send_log_len -= LOG_PACKET_SIZE;
        }
        else
        {
            log_bucket_report_packet(type, data + have_send_len, need_send_log_len);
            break;
        }
    }

    return 0;
}

static int log_bucket_send_cached(void)
{
    uint8_t index = 0;
    uint8_t length = BUCKET_NUM;

    index = (current_bucket + 1) % BUCKET_NUM;

    print_cached_log();

    while (length--)
    {
        if (true != ble_ais_conn_enable_state())
        {
            return -1;
        }

        if (log_cached_info.log_buckets[index]->is_used && log_cached_info.log_buckets[index]->write_pos > 0)
        {
            uint16_t need_send_log_len = log_cached_info.log_buckets[index]->write_pos;
            uint16_t have_send_len = 0;

            printf("cache log report index:%d len:%d\r\n", index, log_cached_info.log_buckets[index]->write_pos);
            log_bucket_report(LOG_REPORT_TLV_CACHED_LOG_TYPE, log_cached_info.log_buckets[index]->bucket, log_cached_info.log_buckets[index]->write_pos);
        }

        // Resume this log bucket
        memset(log_cached_info.log_buckets[index], 0, sizeof(log_bucket_t));
        log_cached_info.log_buckets[index]->is_used = false;

        index++;
        index = index % BUCKET_NUM;
    }

    log_cached_info.log_buckets[0]->is_used = true;

    return 0;
}

static void log_wireless_task(void *argv)
{
    uint32_t ret = 0;
    uint32_t recv_len = 0;
    uint8_t *data = NULL;

    data = (uint8_t *)HAL_Malloc(sizeof(uint8_t) * REAL_LOGBUF_SIZE);

    if (data == NULL)
    {
        printf("log no mem\r\n");
        return;
    }

    memset(data, 0, sizeof(uint8_t) * REAL_LOGBUF_SIZE);

    while (true)
    {
        if (!aos_queue_recv(&log_real_info.g_cmd_msg_queue, LOG_WAIT_TIME, data, &recv_len) && (recv_len > 0))
        {
            printf("report real log len:%d\r\n", recv_len);
            log_bucket_report(LOG_REPORT_TLV_REAL_LOG_TYPE, data, recv_len);
            memset(data, 0, sizeof(uint8_t) * REAL_LOGBUF_SIZE);
        }

        if (report_log_type == REPROT_LOG_TYPE_CACHED || report_log_type == REPROT_LOG_TYPE_CACHED_AND_REAL) /*send cached log*/
        {
            log_cached_info.is_sending_cached_log_flag = 1;
            ret = log_bucket_send_cached();
            if (ret != 0)
            {
                printf("report cached log fail:%d\r\n", ret);
            }
            log_cached_info.is_sending_cached_log_flag = 0;

            if (report_log_type == REPROT_LOG_TYPE_CACHED_AND_REAL)
            {
                printf("report real log start ...\r\n");
                log_real_info.is_sending_real_log_flag = 1; // Reprot cached log done then send real log
            }

            report_log_type = REPROT_LOG_TYPE_MAX; // resume log type
        }
    }
}

void log_wireless_init(void)
{
    aos_task_t task_wireless_log;

    log_bucket_init();
    aos_cli_register_commands(&log_cmds[0], sizeof(log_cmds) / sizeof(log_cmds[0]));
    aos_task_new_ext(&task_wireless_log, "wireless log process", log_wireless_task, NULL, 2048, AOS_DEFAULT_APP_PRI - 1);
}

int log_bucket_send_real_stop(void)
{
    log_real_info.is_sending_real_log_flag = 0;
    printf("real log stop\r\n");
    return 0;
}

int log_bucket_report_cached_log(void)
{
    if (dev_awss_state_get(AWSS_PATTERN_BLE_CONFIG) == AWSS_STATE_START)
    {
        log_bucket_set_log_type(REPROT_LOG_TYPE_CACHED);
    }
    else
    {
        printf("dev_awss_state_get(AWSS_PATTERN_BLE_CONFIG):%d\r\n", dev_awss_state_get(AWSS_PATTERN_BLE_CONFIG));
        log_bucket_set_log_type(REPROT_LOG_TYPE_CACHED);
    }

    return 0;
}

#endif
