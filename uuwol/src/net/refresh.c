/* SPDX-License-Identifier: GPL-3.0-only */

#include <string.h>

#include "../runtime/log.h"
#include "refresh.h"

static void refresh_timeout(struct uloop_timeout *timeout)
{
	struct uu_lease_refresher *refresher = container_of(timeout,
									 struct uu_lease_refresher, timer);
	int result;

	result = uu_lease_snapshot_query(refresher->iface, &refresher->snapshot);
	if (result) {
		UU_LOG_WARN("dhcp lease refresh failed; sending empty device list\n");
	}
	if (refresher->callback &&
	    refresher->callback(&refresher->snapshot, refresher->context)) {
		UU_LOG_WARN("dhcp lease refresh callback failed\n");
	}
	uloop_timeout_set(&refresher->timer, refresher->interval * 1000);
}

int uu_lease_refresher_init(struct uu_lease_refresher *refresher,
				    const char *iface, unsigned interval,
				    uu_lease_refresh_callback callback, void *context)
{
	if (!refresher || !iface || !iface[0] || interval < 3 || interval > 60) {
		return -1;
	}
	memset(refresher, 0, sizeof(*refresher));
	if (strlcpy(refresher->iface, iface, sizeof(refresher->iface)) >=
	    sizeof(refresher->iface)) {
		return -1;
	}
	refresher->interval = interval;
	refresher->callback = callback;
	refresher->context = context;
	refresher->timer.cb = refresh_timeout;
	return 0;
}

void uu_lease_refresher_start(struct uu_lease_refresher *refresher)
{
	if (!refresher) return;
	uloop_timeout_set(&refresher->timer, 0);
}

void uu_lease_refresher_stop(struct uu_lease_refresher *refresher)
{
	if (!refresher) return;
	uloop_timeout_cancel(&refresher->timer);
	uu_lease_snapshot_free(&refresher->snapshot);
}
