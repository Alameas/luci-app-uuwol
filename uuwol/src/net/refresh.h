/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_NET_REFRESH_H
#define UU_NET_REFRESH_H

#include <net/if.h>

#include <libubox/uloop.h>

#include "leases.h"

typedef int (*uu_lease_refresh_callback)(const struct uu_lease_snapshot *snapshot,
						 void *context);

struct uu_lease_refresher {
	struct uloop_timeout timer;
	char iface[IF_NAMESIZE];
	unsigned interval;
	struct uu_lease_snapshot snapshot;
	uu_lease_refresh_callback callback;
	void *context;
};

int uu_lease_refresher_init(struct uu_lease_refresher *refresher,
				    const char *iface, unsigned interval,
				    uu_lease_refresh_callback callback, void *context);
void uu_lease_refresher_start(struct uu_lease_refresher *refresher);
void uu_lease_refresher_stop(struct uu_lease_refresher *refresher);

#endif
