NAME := log

$(NAME)_TYPE := share
$(NAME)_MBINS_TYPE := share
$(NAME)_SOURCES    := log_bucket.c log.c

GLOBAL_CFLAGS += -DLOG_SIMPLE
#GLOBAL_CFLAGS += -DCONFIG_WIRELESS_LOG

GLOBAL_INCLUDES += include