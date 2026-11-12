/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdlib.h>
#include <string.h>

#include <libubox/utils.h>

#include "../common/base64.h"
#include "helper.h"
#include "message.h"
#include "type.h"
#include "uu.pb-c.h"

static char *encode_hostname(const char *source)
{
	char *encoded = NULL;
	char *target;
	size_t encoded_length;
	size_t source_length;

	if (!source) return NULL;
	source_length = strlen(source);
	if (!strncmp(source, "b64_", 4)) {
		return strdup(source);
	}
	if (uu_base64_encode(source, source_length, &encoded)) return NULL;
	encoded_length = strlen(encoded);
	if (encoded_length > SIZE_MAX - 5) {
		free(encoded);
		return NULL;
	}
	target = malloc(encoded_length + 5);
	if (!target) {
		free(encoded);
		return NULL;
	}
	memcpy(target, "b64_", 4);
	memcpy(target + 4, encoded, encoded_length + 1);
	free(encoded);
	return target;
}

static int fill_device(Uu__Device *target,
			const struct uu_message_device *source)
{
	*target = (Uu__Device)UU__DEVICE__INIT;
	if (!source) return -1;
	if (source->has_ip) {
		target->has_ip = 1;
		target->ip = source->ip;
	}
	target->mac = (char *)source->mac;
	target->hostname = encode_hostname(source->hostname);
	if (source->hostname && !target->hostname) return -1;
	target->type = (char *)source->type;
	if (source->has_online) {
		target->has_online = 1;
		target->online = source->online;
	}
	return 0;
}

static void free_devices(Uu__Device *devices, size_t count)
{
	for (size_t i = 0; i < count; i++) free(devices[i].hostname);
}

int uu_message_encode_echo(const char *mid, const char *text,
			   const char *router_type, const char *router_mac,
			   uint64_t seconds, uint64_t microseconds,
			   size_t max_frame, uint8_t **output, size_t *output_len)
{
	Uu__Echo message = UU__ECHO__INIT;
	Uu__Timestamp timestamp = UU__TIMESTAMP__INIT;

	message.mid = (char *)mid;
	message.text = (char *)text;
	message.router_type = (char *)router_type;
	message.router_mac = (char *)router_mac;
	timestamp.has_seconds = 1;
	timestamp.seconds = seconds;
	timestamp.has_microseconds = 1;
	timestamp.microseconds = microseconds;
	message.timestamp = &timestamp;
	return uu_message_encode(UU_MESSAGE_ECHO, &message.base, max_frame,
				 output, output_len);
}

int uu_message_encode_device_list(const struct uu_message_device_list *list,
					  uint64_t seconds, uint64_t microseconds,
					  size_t max_frame, uint8_t **output,
					  size_t *output_len)
{
	Uu__DeviceList message = UU__DEVICE_LIST__INIT;
	Uu__Timestamp timestamp = UU__TIMESTAMP__INIT;
	Uu__Device *devices;
	Uu__Device **device_ptrs;
	int result;

	if (!list || (!list->devices && list->device_count)) return -1;
	if (list->device_count > SIZE_MAX / sizeof(*devices) ||
	    list->device_count > SIZE_MAX / sizeof(*device_ptrs)) {
		return -1;
	}
	if (list->device_count) {
		devices = calloc_a(list->device_count * sizeof(*devices),
				   &device_ptrs,
				   list->device_count * sizeof(*device_ptrs));
		if (!devices) return -1;
	} else {
		devices = NULL;
		device_ptrs = NULL;
	}
	for (size_t i = 0; i < list->device_count; i++) {
		if (fill_device(&devices[i], &list->devices[i])) {
			free_devices(devices, list->device_count);
			free(devices);
			return -1;
		}
		device_ptrs[i] = &devices[i];
	}

	message.mid = (char *)list->mid;
	message.n_devices = list->device_count;
	message.devices = device_ptrs;
	message.router_type = (char *)list->router_type;
	message.router_mac = (char *)list->router_mac;
	timestamp.has_seconds = 1;
	timestamp.seconds = seconds;
	timestamp.has_microseconds = 1;
	timestamp.microseconds = microseconds;
	message.timestamp = &timestamp;
	result = uu_message_encode(UU_MESSAGE_DEVICE_LIST, &message.base,
					   max_frame, output, output_len);
	free_devices(devices, list->device_count);
	free(devices);
	return result;
}

int uu_message_encode_acc_reply(const char *mid,
				const struct uu_message_device *device,
				uint32_t result, size_t max_frame,
				uint8_t **output, size_t *output_len)
{
	Uu__AccReply message = UU__ACC_REPLY__INIT;
	Uu__Device device_message = UU__DEVICE__INIT;
	int encode_result;

	message.mid = (char *)mid;
	message.has_result = 1;
	message.result = result;
	if (device) {
		if (fill_device(&device_message, device)) return -1;
		message.deviceinfo = &device_message;
	}
	encode_result = uu_message_encode(UU_MESSAGE_ACC_REPLY, &message.base,
					   max_frame, output, output_len);
	free(device_message.hostname);
	return encode_result;
}

int uu_message_encode_check_bound(const struct uu_message_check_bound *bound,
					  size_t max_frame, uint8_t **output,
					  size_t *output_len)
{
	Uu__CheckBound message = UU__CHECK_BOUND__INIT;

	if (!bound) return -1;
	message.rid = (char *)bound->rid;
	message.router_type = (char *)bound->router_type;
	message.router_mac = (char *)bound->router_mac;
	message.model = (char *)bound->model;
	message.version = (char *)bound->version;
	return uu_message_encode(UU_MESSAGE_CHECK_BOUND, &message.base, max_frame,
				 output, output_len);
}

int uu_message_encode_connect(const struct uu_message_connect *connect,
				      size_t max_frame, uint8_t **output,
				      size_t *output_len)
{
	Uu__Connect message = UU__CONNECT__INIT;

	if (!connect) return -1;
	message.mid = (char *)connect->mid;
	message.type = (char *)connect->type;
	message.sn = (char *)connect->sn;
	message.version = (char *)connect->version;
	message.name = (char *)connect->name;
	message.model = (char *)connect->model;
	if (connect->has_firmware_version_code) {
		message.has_firmware_version_code = 1;
		message.firmware_version_code = connect->firmware_version_code;
	}
	message.firmware_version_name = (char *)connect->firmware_version_name;
	message.ssid = (char *)connect->ssid;
	message.device_type = (char *)connect->device_type;
	message.whoami = (char *)connect->whoami;
	message.mode_local = (char *)connect->mode_local;
	message.plugin_type = (char *)connect->plugin_type;
	message.real_sn = (char *)connect->real_sn;
	message.firmware_type = (char *)connect->firmware_type;
	message.router_info = (char *)connect->router_info;
	if (connect->has_wan_ip) {
		message.has_wan_ip = 1;
		message.wan_ip = connect->wan_ip;
	}
	if (connect->has_wan_port_speed) {
		message.has_wan_port_speed = 1;
		message.wan_port_speed = connect->wan_port_speed;
	}
	message.debug_whoami_ip = (char *)connect->debug_whoami_ip;
	message.ssid_5g = (char *)connect->ssid_5g;
	return uu_message_encode(UU_MESSAGE_CONNECT, &message.base, max_frame,
				 output, output_len);
}

int uu_message_encode_wol_reply(const char *field1, const char *field2,
					const char *field3,
					const struct uu_message_device *device,
					bool success, size_t max_frame,
					uint8_t **output, size_t *output_len)
{
	Uu__WolReply message = UU__WOL_REPLY__INIT;
	Uu__Device device_message = UU__DEVICE__INIT;
	int result;

	message.field1 = (char *)field1;
	message.field2 = (char *)field2;
	message.field3 = (char *)field3;
	message.has_field5 = 1;
	message.field5 = success;
	if (device) {
		if (fill_device(&device_message, device)) return -1;
		message.deviceinfo = &device_message;
	}
	result = uu_message_encode(UU_MESSAGE_WOL_REPLY, &message.base,
					max_frame, output, output_len);
	free(device_message.hostname);
	return result;
}
