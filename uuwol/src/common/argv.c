/* SPDX-License-Identifier: GPL-3.0-only */

#include <errno.h>
#include <getopt.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <stdlib.h>
#include <string.h>

#include "argv.h"

static int parse_refresh_interval(const char *value, unsigned *result)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(value, &end, 10);
	if (errno || !value[0] || *end || parsed < 3 || parsed > 60) {
		return -1;
	}

	*result = (unsigned)parsed;
	return 0;
}

int uu_cli_parse(int argc, char **argv, struct uu_cli *config)
{
	static const struct option options[] = {
		{ "mac", required_argument, NULL, 'm' },
		{ "iface", required_argument, NULL, 'i' },
		{ "model", required_argument, NULL, 'd' },
		{ "refresh-interval", required_argument, NULL, 'r' },
		{ NULL, 0, NULL, 0 },
	};
	struct ether_addr mac;
	int option;
	int has_mac = 0;
	int has_iface = 0;
	int has_model = 0;

	if (!config) return -1;
	*config = (struct uu_cli){ .refresh_interval = 20 };
	optind = 1;
	opterr = 0;

	while ((option = getopt_long(argc, argv, "m:i:d:r:", options, NULL)) != -1) {
		switch (option) {
		case 'm':
			if (!ether_aton_r(optarg, &mac)) return -1;
			config->mac = mac;
			has_mac = 1;
			break;
		case 'i':
			if (!optarg[0] || if_nametoindex(optarg) == 0 ||
			    strlcpy(config->iface, optarg, sizeof(config->iface)) >=
			    sizeof(config->iface)) {
				return -1;
			}
			has_iface = 1;
			break;
		case 'd':
			if (!optarg[0] || strlcpy(config->model, optarg,
						 sizeof(config->model)) >= sizeof(config->model)) {
				return -1;
			}
			has_model = 1;
			break;
		case 'r':
			if (parse_refresh_interval(optarg, &config->refresh_interval)) return -1;
			break;
		default:
			return -1;
		}
	}

	if (optind != argc) return -1;
	return has_mac && has_iface && has_model ? 0 : -1;
}
