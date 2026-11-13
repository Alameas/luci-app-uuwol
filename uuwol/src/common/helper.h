/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_COMMON_HELPER_H
#define UU_COMMON_HELPER_H

#include <netinet/ether.h>

#define UU_MAC_TEXT_LENGTH 18

int uu_format_lower_mac(const struct ether_addr *mac,
			       char buffer[UU_MAC_TEXT_LENGTH]);

#endif
