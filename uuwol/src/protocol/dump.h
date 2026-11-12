/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_PROTOCOL_DUMP_H
#define UU_PROTOCOL_DUMP_H

#include <stddef.h>
#include <stdint.h>

void uu_message_dump(uint32_t type, const char *direction,
			     const uint8_t *body, size_t body_len);

#endif
