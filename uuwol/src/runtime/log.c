/* SPDX-License-Identifier: GPL-3.0-only */

#include <syslog.h>

#include <libubox/ulog.h>

#include "log.h"

void uu_log_open(void)
{
	ulog_open(ULOG_SYSLOG, LOG_DAEMON, "uuwol");
	ulog_threshold(LOG_INFO);
}

void uu_log_close(void)
{
	ulog_close();
}
