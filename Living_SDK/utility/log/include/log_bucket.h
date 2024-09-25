/*
 * Copyright (C) 2022-2023 Alibaba Group Holding Limited
 */

#ifndef __LOG_BUCKET_H__
#define __LOG_BUCKET_H__

typedef enum _report_log_type_e
{
    REPROT_LOG_TYPE_CACHED,
    REPROT_LOG_TYPE_REAL,
    REPROT_LOG_TYPE_CACHED_AND_REAL,
    REPROT_LOG_TYPE_MAX
} report_log_type_e;

typedef enum _real_log_level_e
{
    REAL_LOG_LEVEL_FORCE,
    REAL_LOG_LEVEL_ERROR,
    REAL_LOG_LEVEL_WARNNING,
    REAL_LOG_LEVEL_INFO,
    REAL_LOG_LEVEL_MAX
} real_log_level_e;

#define LOG_REPORT_TLV_CACHED_LOG_TYPE (0x08)
#define LOG_REPORT_TLV_REAL_LOG_TYPE (0x09)
#define LOG_REPORT_TLV_COMMAND_TYPE (0x0A)

/**
 * @brief wireless log init
 *
 */
void log_wireless_init(void);

/**
 * @brief log write to  buf
 * @param[in] log_level: log level.
 * @param[in] mode: log mode.
 *
 */
int log_wireless_write(const char log_level, const char *mode, const long long log_time, const char *fmt, va_list *ap);

/**
 * @brief set log type by ble
 *
 * @retval  0 on success, non-zero on failure
 */
int log_bucket_set_log_type(report_log_type_e log_type);

/**
 * @brief set real log level by ble
 *
 * @retval  0 on success, non-zero on failure
 */
int log_bucket_set_real_log_level(real_log_level_e log_level);

/**
 * @brief start send real log by ble
 *
 * @retval  0 on success, non-zero on failure
 */
int log_bucket_send_real_start(void);

/**
 * @brief stop send real log by ble
 *
 * @retval  0 on success, non-zero on failure
 */
int log_bucket_send_real_stop(void);

#endif
