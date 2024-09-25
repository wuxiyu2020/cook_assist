/**
 * Copyright (c) 2020 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#define LOG_VERSION "0.1.0"

typedef struct {
  va_list ap;
  const char *mod;  // u
  const char *fmt;
  const char *file;
  struct tm *time;
  void *udata;
  int line;
  int level;
} log_Event;

typedef void (*log_LogFn)(log_Event *ev);
typedef void (*log_LockFn)(bool lock, void *udata);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

// 源项目代码
// #define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
// #define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
// #define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
// #define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
// #define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
// #define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

// 加入module参数,兼容项目日志接口
#define LOGD(mod, fmt, ...) log_log(mod, LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)  // ##是为了当除了fmt,可变参数没有时,去掉fmt后面的逗号,不然你编译器会出错.而当有可变参数,##可加可不加,这是宏展开的规则,以便宏在有或没有可变参数时都能正常工作
#define LOGI(mod, fmt, ...) log_log(mod, LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGW(mod, fmt, ...) log_log(mod, LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGE(mod, fmt, ...) log_log(mod, LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGF(mod, fmt, ...) log_log(mod, LOG_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

const char* log_level_string(int level);
void log_set_lock(log_LockFn fn, void *udata);
void log_set_level(int level);
void log_set_quiet(bool enable);
int log_add_callback(log_LogFn fn, void *udata, int level);
int log_add_fp(FILE *fp, int level);

void log_log(const char *mod, int level, const char *file, int line, const char *fmt, ...);  // u

#endif