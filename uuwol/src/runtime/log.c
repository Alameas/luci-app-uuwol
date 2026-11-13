/* SPDX-License-Identifier: GPL-3.0-only */

#include <syslog.h>

#include <libubox/ulog.h>

#include "log.h"

void uu_log_open(void)
{
	ulog_open(ULOG_SYSLOG, LOG_DAEMON, "uuwol");
	uu_log_set_debug(false);
}

void uu_log_set_debug(bool debug)
{
	ulog_threshold(debug ? LOG_INFO : LOG_NOTICE);
}

void uu_log_close(void)
{
	ulog_close();
}
