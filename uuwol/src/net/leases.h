/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_NET_LEASES_H
#define UU_NET_LEASES_H

#include <netinet/ether.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

struct uu_lease {
	struct in_addr ip;
	struct ether_addr mac;
	char hostname[128];
	uint64_t expires;
};

struct uu_lease_snapshot {
	struct uu_lease *entries;
	size_t count;
};

void uu_lease_snapshot_free(struct uu_lease_snapshot *snapshot);
int uu_lease_snapshot_query(const char *iface,
				    struct uu_lease_snapshot *snapshot);

#endif
