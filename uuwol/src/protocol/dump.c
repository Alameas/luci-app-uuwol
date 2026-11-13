/* SPDX-License-Identifier: GPL-3.0-only */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <protobuf-c/protobuf-c.h>

#include "../runtime/log.h"
#include "dump.h"
#include "helper.h"
#include "type.h"

static const char *message_name(uint32_t type)
{
	switch (type) {
	case UU_MESSAGE_ECHO: return "Echo";
	case UU_MESSAGE_ECHO_REPLY: return "EchoReply";
	case UU_MESSAGE_CONNECT: return "Connect";
	case UU_MESSAGE_CONNECT_REPLY: return "ConnectReply";
	case UU_MESSAGE_DEVICE_LIST: return "DeviceList";
	case UU_MESSAGE_ACC: return "Acc";
	case UU_MESSAGE_ACC_REPLY: return "AccReply";
	case UU_MESSAGE_STOP_ACC: return "StopAcc";
	case UU_MESSAGE_LOG: return "Log";
	case UU_MESSAGE_MAC_LIST: return "MacList";
	case UU_MESSAGE_BOUND_USER: return "BoundUser";
	case UU_MESSAGE_EXTENSION17: return "Extension17";
	case UU_MESSAGE_CONNECT_REQUEST: return "ConnectRequest";
	case UU_MESSAGE_SERVER_PING: return "ServerPing";
	case UU_MESSAGE_CHECK_BOUND: return "CheckBound";
	case UU_MESSAGE_CHECK_BOUND_REPLY: return "CheckBoundReply";
	case UU_MESSAGE_WOL: return "Wol";
	case UU_MESSAGE_WOL_REPLY: return "WolReply";
	case UU_MESSAGE_REMOTE_LINK_CONFIG: return "RemoteLinkConfig";
	default: return "Unknown";
	}
}

static void log_hex(const char *prefix, const uint8_t *data, size_t length)
{
	static const char digits[] = "0123456789abcdef";
	char text[3 * 32 + 4];
	size_t limit = length < 32 ? length : 32;
	size_t offset = 0;

	for (size_t i = 0; i < limit; i++) {
		text[offset++] = digits[data[i] >> 4];
		text[offset++] = digits[data[i] & 0x0f];
		if (i + 1 < limit) text[offset++] = ':';
	}
	if (length > limit) {
		text[offset++] = ':';
		text[offset++] = '.';
		text[offset++] = '.';
		text[offset++] = '.';
	}
	text[offset] = '\0';

	UU_LOG_INFO("%s%s len=%zu\n", prefix, text, length);
}

static int read_varint(const uint8_t *data, size_t length, size_t *offset,
			       uint64_t *value)
{
	uint64_t result = 0;

	for (unsigned shift = 0; shift < 64 && *offset < length; shift += 7) {
		uint8_t byte = data[(*offset)++];

		result |= (uint64_t)(byte & 0x7f) << shift;
		if (!(byte & 0x80)) {
			*value = result;
			return 0;
		}
	}

	return -1;
}

static void dump_wire_fields(const uint8_t *body, size_t body_len)
{
	size_t offset = 0;

	while (offset < body_len) {
		size_t field_offset = offset;
		uint64_t key;
		uint64_t value;
		uint64_t field_length;
		unsigned field_number;
		unsigned wire_type;

		if (read_varint(body, body_len, &offset, &key)) {
			UU_LOG_INFO("wire offset=%zu invalid-varint\n", field_offset);
			return;
		}
		field_number = (unsigned)(key >> 3);
		wire_type = (unsigned)(key & 7);
		if (!field_number) {
			UU_LOG_INFO("wire offset=%zu invalid-field-number\n", field_offset);
			return;
		}

		switch (wire_type) {
		case 0:
			if (read_varint(body, body_len, &offset, &value)) {
				UU_LOG_INFO("wire field=%u wire=%u truncated\n",
					    field_number, wire_type);
				return;
			}
			UU_LOG_INFO("wire field=%u wire=%u value=%" PRIu64 "\n",
				    field_number, wire_type, value);
			break;
		case 1:
			if (body_len - offset < 8) {
				UU_LOG_INFO("wire field=%u wire=%u truncated\n",
					    field_number, wire_type);
				return;
			}
			log_hex("wire fixed64=", body + offset, 8);
			offset += 8;
			break;
		case 2:
			if (read_varint(body, body_len, &offset, &field_length) ||
			    field_length > body_len - offset) {
				UU_LOG_INFO("wire field=%u wire=%u truncated\n",
					    field_number, wire_type);
				return;
			}
			UU_LOG_INFO("wire field=%u wire=%u len=%" PRIu64 "\n",
				    field_number, wire_type, field_length);
			log_hex("wire bytes=", body + offset, (size_t)field_length);
			offset += (size_t)field_length;
			break;
		case 5:
			if (body_len - offset < 4) {
				UU_LOG_INFO("wire field=%u wire=%u truncated\n",
					    field_number, wire_type);
				return;
			}
			log_hex("wire fixed32=", body + offset, 4);
			offset += 4;
			break;
		default:
			UU_LOG_INFO("wire field=%u wire=%u unsupported\n",
				    field_number, wire_type);
			return;
		}
	}
}

static int field_count(const ProtobufCMessage *message,
			       const ProtobufCFieldDescriptor *field, size_t *count)
{
	const uint8_t *base = (const uint8_t *)message;

	if (field->label == PROTOBUF_C_LABEL_REPEATED) {
		*count = *(const size_t *)(base + field->quantifier_offset);
		return *count != 0;
	}
	if (field->label == PROTOBUF_C_LABEL_OPTIONAL ||
	    field->label == PROTOBUF_C_LABEL_REQUIRED) {
		if (*(const protobuf_c_boolean *)(base + field->quantifier_offset)) return 1;
		return 0;
	}
	*count = 1;
	return 1;
}

static void dump_message_fields(const ProtobufCMessage *message, unsigned depth);

static void dump_scalar(const ProtobufCFieldDescriptor *field,
			const void *storage, size_t index)
{
	switch (field->type) {
	case PROTOBUF_C_TYPE_INT32:
	case PROTOBUF_C_TYPE_SINT32:
	case PROTOBUF_C_TYPE_SFIXED32:
		UU_LOG_INFO("field=%s value=%" PRId32 "\n", field->name,
			    ((const int32_t *)storage)[index]);
		break;
	case PROTOBUF_C_TYPE_INT64:
	case PROTOBUF_C_TYPE_SINT64:
	case PROTOBUF_C_TYPE_SFIXED64:
		UU_LOG_INFO("field=%s value=%" PRId64 "\n", field->name,
			    ((const int64_t *)storage)[index]);
		break;
	case PROTOBUF_C_TYPE_UINT32:
	case PROTOBUF_C_TYPE_FIXED32:
		UU_LOG_INFO("field=%s value=%" PRIu32 "\n", field->name,
			    ((const uint32_t *)storage)[index]);
		break;
	case PROTOBUF_C_TYPE_UINT64:
	case PROTOBUF_C_TYPE_FIXED64:
		UU_LOG_INFO("field=%s value=%" PRIu64 "\n", field->name,
			    ((const uint64_t *)storage)[index]);
		break;
	case PROTOBUF_C_TYPE_BOOL:
		UU_LOG_INFO("field=%s value=%s\n", field->name,
			    ((const protobuf_c_boolean *)storage)[index] ? "true" : "false");
		break;
	case PROTOBUF_C_TYPE_ENUM:
		UU_LOG_INFO("field=%s value=%d\n", field->name,
			    ((const int *)storage)[index]);
		break;
	case PROTOBUF_C_TYPE_FLOAT:
		UU_LOG_INFO("field=%s value=%g\n", field->name,
			    ((const float *)storage)[index]);
		break;
	case PROTOBUF_C_TYPE_DOUBLE:
		UU_LOG_INFO("field=%s value=%g\n", field->name,
			    ((const double *)storage)[index]);
		break;
	default:
		break;
	}
}

static void dump_field(const ProtobufCFieldDescriptor *field,
			       const void *storage, size_t index, unsigned depth)
{
	switch (field->type) {
	case PROTOBUF_C_TYPE_STRING: {
		const char *const *values = storage;
		UU_LOG_INFO("field=%s value=%s\n", field->name,
			    values[index] ? values[index] : "<null>");
		break;
	}
	case PROTOBUF_C_TYPE_BYTES: {
		const ProtobufCBinaryData *values = storage;
		char prefix[64];
		snprintf(prefix, sizeof(prefix), "field=%s bytes=", field->name);
		log_hex(prefix, values[index].data, values[index].len);
		break;
	}
	case PROTOBUF_C_TYPE_MESSAGE: {
		const ProtobufCMessage *const *values = storage;
		UU_LOG_INFO("field=%s message=%s depth=%u\n", field->name,
			    values[index] ? values[index]->descriptor->short_name : "<null>", depth);
		if (values[index] && depth < 4) dump_message_fields(values[index], depth + 1);
		break;
	}
	default:
		dump_scalar(field, storage, index);
		break;
	}
}

static void dump_message_fields(const ProtobufCMessage *message, unsigned depth)
{
	const ProtobufCMessageDescriptor *descriptor = message->descriptor;
	const uint8_t *base = (const uint8_t *)message;

	for (unsigned i = 0; i < descriptor->n_fields; i++) {
		const ProtobufCFieldDescriptor *field = &descriptor->fields[i];
		size_t count = 0;

		if (!field_count(message, field, &count)) continue;
		if (field->label == PROTOBUF_C_LABEL_REPEATED) {
			const void *storage = *(const void *const *)(base + field->offset);
			UU_LOG_INFO("field=%s repeated=%zu\n", field->name, count);
			for (size_t j = 0; j < count; j++) {
				dump_field(field, storage, j, depth);
			}
		} else {
			dump_field(field, base + field->offset, 0, depth);
		}
	}

	for (unsigned i = 0; i < message->n_unknown_fields; i++) {
		const ProtobufCMessageUnknownField *field = &message->unknown_fields[i];
		char prefix[64];

		snprintf(prefix, sizeof(prefix), "unknown field=%u wire=%u bytes=",
			 field->tag, field->wire_type);
		log_hex(prefix, field->data, field->len);
	}
}

void uu_message_dump(uint32_t type, const char *direction,
			     const uint8_t *body, size_t body_len)
{
	const ProtobufCMessageDescriptor *descriptor = uu_message_descriptor(type);
	ProtobufCMessage *message;

	UU_LOG_INFO("message direction=%s type=%u name=%s body_len=%zu\n",
		    direction ? direction : "?", type, message_name(type), body_len);
	if (!descriptor) {
		dump_wire_fields(body, body_len);
		return;
	}

	message = uu_message_unpack(type, body, body_len);
	if (!message) {
		UU_LOG_INFO("message type=%u parse=failed\n", type);
		dump_wire_fields(body, body_len);
		return;
	}
	dump_message_fields(message, 0);
	uu_message_free(message);
}
