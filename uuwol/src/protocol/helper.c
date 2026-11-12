/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdlib.h>

#include <libubox/utils.h>

#include "../transport/frame.h"
#include "helper.h"
#include "type.h"
#include "uu.pb-c.h"

static const ProtobufCMessageDescriptor *const descriptors[] = {
	[UU_MESSAGE_ECHO] = &uu__echo__descriptor,
	[UU_MESSAGE_ECHO_REPLY] = &uu__echo_reply__descriptor,
	[UU_MESSAGE_CONNECT] = &uu__connect__descriptor,
	[UU_MESSAGE_ACC] = &uu__acc__descriptor,
	[UU_MESSAGE_ACC_REPLY] = &uu__acc_reply__descriptor,
	[UU_MESSAGE_STOP_ACC] = &uu__stop_acc__descriptor,
	[UU_MESSAGE_DEVICE_LIST] = &uu__device_list__descriptor,
	[UU_MESSAGE_CHECK_BOUND] = &uu__check_bound__descriptor,
	[UU_MESSAGE_CHECK_BOUND_REPLY] = &uu__check_bound_reply__descriptor,
	[UU_MESSAGE_WOL] = &uu__wol__descriptor,
	[UU_MESSAGE_WOL_REPLY] = &uu__wol_reply__descriptor,
};

const ProtobufCMessageDescriptor *uu_message_descriptor(uint32_t type)
{
	if (type >= ARRAY_SIZE(descriptors)) return NULL;
	return descriptors[type];
}

ProtobufCMessage *uu_message_unpack(uint32_t type, const void *body,
					    size_t body_length)
{
	const ProtobufCMessageDescriptor *descriptor;

	if (!body && body_length) return NULL;
	descriptor = uu_message_descriptor(type);
	if (!descriptor) return NULL;
	return protobuf_c_message_unpack(descriptor, NULL, body_length, body);
}

void uu_message_free(ProtobufCMessage *message)
{
	if (message) protobuf_c_message_free_unpacked(message, NULL);
}

int uu_message_encode(uint32_t type, const ProtobufCMessage *message,
				      size_t max_frame, uint8_t **output,
				      size_t *output_length)
{
	uint8_t *body;
	uint8_t *frame;
	size_t body_length;
	size_t frame_length;

	if (!message || !output || !output_length) return -1;
	body_length = protobuf_c_message_get_packed_size(message);
	body = malloc(body_length ? body_length : 1);
	if (!body) return -1;
	if (protobuf_c_message_pack(message, body) != body_length) {
		free(body);
		return -1;
	}
	if (uu_frame_encode(type, body, body_length, max_frame, &frame,
					&frame_length)) {
		free(body);
		return -1;
	}
	free(body);
	*output = frame;
	*output_length = frame_length;
	return 0;
}
