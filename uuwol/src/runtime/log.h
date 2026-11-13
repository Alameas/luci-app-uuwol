/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_RUNTIME_LOG_H
#define UU_RUNTIME_LOG_H

#include <libubox/ulog.h>
#include <stdbool.h>

#define UU_LOG_INFO(...) ULOG_INFO(__VA_ARGS__)
#define UU_LOG_NOTICE(...) ULOG_NOTE(__VA_ARGS__)
#define UU_LOG_WARN(...) ULOG_WARN(__VA_ARGS__)
#define UU_LOG_ERR(...) ULOG_ERR(__VA_ARGS__)

void uu_log_open(void);
void uu_log_set_debug(bool debug);
void uu_log_close(void);

#endif
