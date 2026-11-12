/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_WOL_WOL_H
#define UU_WOL_WOL_H

#include <netinet/ether.h>
#include <stdbool.h>

struct uu_wol_result {
	bool udp_limited;
	bool udp_subnet;
	bool raw_ethernet;
};

int uu_wol_send_all(const struct ether_addr *target, const char *iface,
			   struct uu_wol_result *result);

#endif
