/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdlib.h>

#include <protobuf-c/protobuf-c.h>

#include "../transport/frame.h"
#include "message.h"
#include "type.h"
#include "uu.pb-c.h"

static void fill_device(Uu__Device *target,
			const struct uu_message_device *source)
{
	*target = (Uu__Device)UU__DEVICE__INIT;
	if (source->has_ip) {
		target->has_ip = 1;
		target->ip = source->ip;
	}
	target->mac = (char *)source->mac;
	target->hostname = (char *)source->hostname;
	target->type = (char *)source->type;
	if (source->has_online) {
		target->has_online = 1;
		target->online = source->online;
	}
}

static int encode_message(uint32_t type, const ProtobufCMessage *message,
			  size_t max_frame, uint8_t **output, size_t *output_len)
{
	uint8_t *body;
	uint8_t *frame;
	size_t body_len;
	size_t frame_len;

	if (!message || !output || !output_len) return -1;

	body_len = protobuf_c_message_get_packed_size(message);
	body = malloc(body_len ? body_len : 1);
	if (!body) return -1;
	if (protobuf_c_message_pack(message, body) != body_len) {
		free(body);
		return -1;
	}
	if (uu_frame_encode(type, body, body_len, max_frame, &frame, &frame_len)) {
		free(body);
		return -1;
	}
	free(body);
	*output = frame;
	*output_len = frame_len;
	return 0;
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
	return encode_message(UU_MESSAGE_ECHO, &message.base, max_frame,
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

	if (!list || (!list->devices && list->device_count)) return -1;
	if (list->device_count > SIZE_MAX / sizeof(*devices) ||
	    list->device_count > SIZE_MAX / sizeof(*device_ptrs))
		return -1;
	devices = calloc(list->device_count, sizeof(*devices));
	device_ptrs = calloc(list->device_count, sizeof(*device_ptrs));
	if ((list->device_count && !devices) ||
	    (list->device_count && !device_ptrs)) {
		free(devices);
		free(device_ptrs);
		return -1;
	}
	for (size_t i = 0; i < list->device_count; i++) {
		fill_device(&devices[i], &list->devices[i]);
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

	int result = encode_message(UU_MESSAGE_DEVICE_LIST, &message.base,
				    max_frame, output, output_len);
	free(device_ptrs);
	free(devices);
	return result;
}

int uu_message_encode_acc_reply(const char *mid,
					const struct uu_message_device *device,
					uint32_t result, size_t max_frame,
					uint8_t **output, size_t *output_len)
{
	Uu__AccReply message = UU__ACC_REPLY__INIT;
	Uu__Device device_message;

	message.mid = (char *)mid;
	message.has_result = 1;
	message.result = result;
	if (device) {
		fill_device(&device_message, device);
		message.deviceinfo = &device_message;
	}
	return encode_message(UU_MESSAGE_ACC_REPLY, &message.base, max_frame,
			      output, output_len);
}
