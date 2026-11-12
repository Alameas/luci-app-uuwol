/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

#include <libubox/uloop.h>

#include "../common/argv.h"
#include "../identity/whoami.h"
#include "../net/refresh.h"
#include "../runtime/log.h"
#include "../session/session.h"

static int update_session_leases(const struct uu_lease_snapshot *snapshot,
					 void *context)
{
	struct uu_session *session = context;

	uu_session_update_leases(session, snapshot);
	return 0;
}

static int read_hostname(char *hostname, size_t size)
{
	if (!hostname || size < 2 || gethostname(hostname, size - 1)) return -1;
	hostname[size - 1] = '\0';
	return 0;
}

struct uu_app {
	struct uu_lease_refresher refresher;
	struct uu_whoami_response whoami;
	struct uu_session session;
	bool refresher_ready;
	bool session_ready;
	bool uloop_ready;
};

static void app_cleanup(struct uu_app *app)
{
	if (!app) return;
	if (app->refresher_ready) uu_lease_refresher_stop(&app->refresher);
	if (app->session_ready) uu_session_free(&app->session);
	uu_whoami_response_free(&app->whoami);
	if (app->uloop_ready) uloop_done();
	uu_log_close();
}

int main(int argc, char **argv)
{
	struct uu_cli config = { 0 };
	struct uu_app app = { 0 };
	char mac[18];
	char hostname[256];
	struct uu_session_config session_config;
	int result = 1;

	uu_log_open();
	if (uu_cli_parse(argc, argv, &config)) {
		UU_LOG_ERR("invalid startup arguments\n");
		result = 64;
		goto cleanup;
	}

	if (read_hostname(hostname, sizeof(hostname))) {
		UU_LOG_ERR("gethostname failed\n");
		goto cleanup;
	}
	if (uu_cli_mac_text(&config, mac, sizeof(mac))) {
		UU_LOG_ERR("could not format MAC\n");
		goto cleanup;
	}

	UU_LOG_NOTICE("started mac=%s iface=%s model=%s hostname=%s refresh=%u\n",
		      mac, config.iface, config.model, hostname, config.refresh_interval);
	if (uu_whoami_fetch(10000, &app.whoami)) {
		UU_LOG_ERR("whoami request failed\n");
		goto cleanup;
	}
	session_config = (struct uu_session_config){
		.rid = "0000000000000000",
		.mid = "0000000000000000",
		.router_mac = mac,
		.model = config.model,
		.hostname = hostname,
		.ssid = hostname,
		.whoami = app.whoami.body,
		.iface = config.iface,
	};
	if (uu_session_init(&app.session, &session_config)) {
		UU_LOG_ERR("could not initialize control session\n");
		goto cleanup;
	}
	app.session_ready = true;
	if (uu_lease_refresher_init(&app.refresher, config.iface,
				    config.refresh_interval, update_session_leases,
				    &app.session)) {
		UU_LOG_ERR("could not initialize dhcp refresher\n");
		goto cleanup;
	}
	app.refresher_ready = true;

	if (uloop_init()) {
		UU_LOG_ERR("could not initialize event loop\n");
		goto cleanup;
	}
	app.uloop_ready = true;
	if (uu_session_start(&app.session)) {
		UU_LOG_ERR("could not start control session\n");
		goto cleanup;
	}
	uu_lease_refresher_start(&app.refresher);
	uloop_run();
	result = 0;

cleanup:
	app_cleanup(&app);
	return result;
}
