/* SPDX-License-Identifier: GPL-3.0-only */

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <libubox/uloop.h>

#include "../common/argv.h"
#include "../net/refresh.h"
#include "../runtime/log.h"

int main(int argc, char **argv)
{
	struct uu_cli config;
	char mac[18];
	char hostname[256];
	struct uu_lease_refresher refresher;

	uu_log_open();
	if (uu_cli_parse(argc, argv, &config)) {
		UU_LOG_ERR("invalid startup arguments\n");
		uu_log_close();
		return 64;
	}

	if (gethostname(hostname, sizeof(hostname) - 1)) {
		UU_LOG_ERR("gethostname failed\n");
		uu_log_close();
		return 1;
	}
	hostname[sizeof(hostname) - 1] = '\0';
	if (uu_cli_mac_text(&config, mac, sizeof(mac))) {
		UU_LOG_ERR("could not format MAC\n");
		uu_log_close();
		return 1;
	}

	UU_LOG_NOTICE("started mac=%s iface=%s model=%s hostname=%s refresh=%u\n",
		      mac, config.iface, config.model, hostname, config.refresh_interval);
	if (uu_lease_refresher_init(&refresher, config.iface,
				    config.refresh_interval, NULL, NULL)) {
		UU_LOG_ERR("could not initialize dhcp refresher\n");
		uu_log_close();
		return 1;
	}

	uloop_init();
	uu_lease_refresher_start(&refresher);
	uloop_run();
	uu_lease_refresher_stop(&refresher);
	uloop_done();
	uu_log_close();
	return 0;
}
