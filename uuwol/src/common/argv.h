/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_COMMON_ARGV_H
#define UU_COMMON_ARGV_H

#include <net/if.h>
#include <netinet/ether.h>
#include <stddef.h>

struct uu_cli {
	struct ether_addr mac;
	char iface[IF_NAMESIZE];
	char model[128];
	unsigned refresh_interval;
};

int uu_cli_parse(int argc, char **argv, struct uu_cli *config);
int uu_cli_mac_text(const struct uu_cli *config, char *buffer, size_t size);

#endif
