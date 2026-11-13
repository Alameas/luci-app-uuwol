/* SPDX-License-Identifier: GPL-3.0-only */

#include <ctype.h>
#include <netinet/ether.h>
#include <stddef.h>

#include "helper.h"

int uu_format_lower_mac(const struct ether_addr *mac,
			       char buffer[UU_MAC_TEXT_LENGTH])
{
	if (!mac || !buffer) return -1;
	if (!ether_ntoa_r(mac, buffer)) return -1;
	for (size_t i = 0; buffer[i]; i++) {
		buffer[i] = (char)tolower((unsigned char)buffer[i]);
	}
	return 0;
}
