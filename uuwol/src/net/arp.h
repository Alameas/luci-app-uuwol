/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_NET_ARP_H
#define UU_NET_ARP_H

#include <netinet/ether.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>

#define UU_ARP_TIMEOUT_MS 200

struct uu_arp_target {
	struct in_addr ip;
	struct ether_addr mac;
};

int uu_arp_probe_all(const char *iface, const struct uu_arp_target *targets,
			     size_t count, bool *online);

#endif
