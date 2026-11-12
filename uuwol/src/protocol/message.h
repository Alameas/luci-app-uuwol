/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_PROTOCOL_MESSAGE_H
#define UU_PROTOCOL_MESSAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct uu_message_device {
	bool has_ip;
	uint32_t ip;
	const char *mac;
	const char *hostname;
	const char *type;
	bool has_online;
	bool online;
};

struct uu_message_device_list {
	const char *mid;
	const struct uu_message_device *devices;
	size_t device_count;
	const char *router_type;
	const char *router_mac;
};

int uu_message_encode_echo(const char *mid, const char *text,
			   const char *router_type, const char *router_mac,
			   uint64_t seconds, uint64_t microseconds,
			   size_t max_frame, uint8_t **output, size_t *output_len);
int uu_message_encode_device_list(const struct uu_message_device_list *list,
					 uint64_t seconds, uint64_t microseconds,
					 size_t max_frame, uint8_t **output,
					 size_t *output_len);
int uu_message_encode_acc_reply(const char *mid,
					const struct uu_message_device *device,
					uint32_t result, size_t max_frame,
					uint8_t **output, size_t *output_len);

#endif
