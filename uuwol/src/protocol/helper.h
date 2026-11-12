/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_PROTOCOL_HELPER_H
#define UU_PROTOCOL_HELPER_H

#include <protobuf-c/protobuf-c.h>
#include <stddef.h>
#include <stdint.h>

const ProtobufCMessageDescriptor *uu_message_descriptor(uint32_t type);
ProtobufCMessage *uu_message_unpack(uint32_t type, const void *body,
					    size_t body_length);
void uu_message_free(ProtobufCMessage *message);
int uu_message_encode(uint32_t type, const ProtobufCMessage *message,
				      size_t max_frame, uint8_t **output,
				      size_t *output_length);

#endif
