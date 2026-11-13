/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_LOCAL_ROUTER_H
#define UU_LOCAL_ROUTER_H

#include <libubox/list.h>
#include <libubox/uloop.h>

#define UU_LOCAL_ROUTER_PORT 16363
#define UU_LOCAL_ROUTER_MAX_REQUEST 1024
#define UU_LOCAL_ROUTER_MAX_CLIENTS 2
#define UU_LOCAL_ROUTER_MAC_LENGTH 18

struct uu_router_server {
	struct uloop_fd listener;
	struct list_head clients;
	unsigned client_count;
	char router_mac[UU_LOCAL_ROUTER_MAC_LENGTH];
};

int uu_router_server_init(struct uu_router_server *server,
				  const char *router_mac, const char *iface);
int uu_router_server_start(struct uu_router_server *server);
void uu_router_server_stop(struct uu_router_server *server);
void uu_router_server_free(struct uu_router_server *server);

#endif
