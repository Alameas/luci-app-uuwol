/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_SESSION_SESSION_H
#define UU_SESSION_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../net/leases.h"
#include "../transport/frame.h"
#include "../transport/tls.h"

#define UU_BUILD_CONTROL_HOST "rglg.uu.netease.com"
#define UU_BUILD_CONTROL_PORT 443
#define UU_SESSION_MAX_FRAME 0x200000
#define UU_SESSION_ECHO_INTERVAL_MS 10000
#define UU_SESSION_DEFAULT_TEXT "Hello world"
#define UU_SESSION_DEFAULT_CLIENT_TYPE "windows"
#define UU_SESSION_DEFAULT_ROUTER_INFO \
	"rsv=0,wan_if=wan,wan_mtu=1500,path_mtu=1500"

struct uu_session_config {
	const char *rid;
	const char *router_mac;
	const char *model;
	const char *hostname;
	const char *router_info;
	const char *ssid;
	const char *whoami;
	const char *iface;
};

struct uu_session {
	struct uu_session_config config;
	struct uu_tls tls;
	struct uu_frame_parser parser;
	struct uloop_timeout echo_timer;
	const struct uu_lease_snapshot *leases;
	uint64_t mid_counter;
	char current_mid[17];
	uint8_t *pending;
	size_t pending_length;
	size_t pending_offset;
	bool ready;
};

int uu_session_init(struct uu_session *session,
			    const struct uu_session_config *config);
int uu_session_start(struct uu_session *session);
void uu_session_update_leases(struct uu_session *session,
				      const struct uu_lease_snapshot *snapshot);
void uu_session_close(struct uu_session *session);
void uu_session_free(struct uu_session *session);

#endif
