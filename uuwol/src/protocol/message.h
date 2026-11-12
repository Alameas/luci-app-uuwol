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

struct uu_message_check_bound {
	const char *rid;
	const char *router_type;
	const char *router_mac;
	const char *model;
	const char *version;
};

struct uu_message_connect {
	const char *mid;
	const char *type;
	const char *sn;
	const char *version;
	const char *name;
	const char *model;
	bool has_firmware_version_code;
	uint32_t firmware_version_code;
	const char *firmware_version_name;
	const char *ssid;
	const char *device_type;
	const char *whoami;
	const char *mode_local;
	const char *plugin_type;
	const char *real_sn;
	const char *firmware_type;
	const char *router_info;
	bool has_wan_ip;
	uint32_t wan_ip;
	bool has_wan_port_speed;
	uint32_t wan_port_speed;
	const char *debug_whoami_ip;
	const char *ssid_5g;
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
int uu_message_encode_check_bound(const struct uu_message_check_bound *bound,
					  size_t max_frame, uint8_t **output,
					  size_t *output_len);
int uu_message_encode_connect(const struct uu_message_connect *connect,
				      size_t max_frame, uint8_t **output,
				      size_t *output_len);
int uu_message_encode_wol_reply(const char *field1, const char *field2,
					const char *field3,
					const struct uu_message_device *device,
					bool success, size_t max_frame,
					uint8_t **output, size_t *output_len);

#endif
