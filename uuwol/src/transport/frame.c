/* SPDX-License-Identifier: GPL-3.0-only */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "frame.h"

static uint32_t read_be32(const uint8_t *data)
{
	return ((uint32_t)data[0] << 24) |
	       ((uint32_t)data[1] << 16) |
	       ((uint32_t)data[2] << 8) |
	       (uint32_t)data[3];
}

static void write_be32(uint8_t *data, uint32_t value)
{
	data[0] = (uint8_t)(value >> 24);
	data[1] = (uint8_t)(value >> 16);
	data[2] = (uint8_t)(value >> 8);
	data[3] = (uint8_t)value;
}

static int drain_frames(struct uu_frame_parser *parser,
				uu_frame_callback callback, void *ctx)
{
	while (parser->length >= 4) {
		uint32_t total_length = read_be32(parser->buffer);
		size_t frame_length;
		struct uu_frame frame;
		int result;

		if (total_length < 4 || total_length > parser->max_frame) {
			return -1;
		}

		frame_length = (size_t)total_length + 4;
		if (parser->length < frame_length) return 0;

		frame.type = read_be32(parser->buffer + 4);
		frame.body = parser->buffer + 8;
		frame.body_len = total_length - 4;
		result = callback ? callback(&frame, ctx) : 0;
		if (result < 0) return result;

		parser->length -= frame_length;
		if (parser->length) {
			memmove(parser->buffer, parser->buffer + frame_length, parser->length);
		}
	}

	return 0;
}

int uu_frame_parser_init(struct uu_frame_parser *parser, size_t max_frame)
{
	if (!parser || max_frame < 4 || max_frame > SIZE_MAX - 4) return -1;

	memset(parser, 0, sizeof(*parser));
	parser->capacity = max_frame + 4;
	parser->max_frame = max_frame;
	parser->buffer = malloc(parser->capacity);
	if (!parser->buffer) {
		memset(parser, 0, sizeof(*parser));
		return -1;
	}

	return 0;
}

void uu_frame_parser_free(struct uu_frame_parser *parser)
{
	if (!parser) return;

	free(parser->buffer);
	memset(parser, 0, sizeof(*parser));
}

int uu_frame_parser_feed(struct uu_frame_parser *parser,
				 const uint8_t *data, size_t length,
				 uu_frame_callback callback, void *ctx)
{
	if (!parser || !parser->buffer || (!data && length)) return -1;

	while (length) {
		size_t available;
		size_t copy;

		if (drain_frames(parser, callback, ctx) < 0) return -1;
		if (parser->length == parser->capacity) return -1;

		available = parser->capacity - parser->length;
		copy = length < available ? length : available;
		memcpy(parser->buffer + parser->length, data, copy);
		parser->length += copy;
		data += copy;
		length -= copy;
	}

	return drain_frames(parser, callback, ctx);
}

int uu_frame_encode(uint32_t type, const uint8_t *body, size_t body_len,
			    size_t max_frame, uint8_t **output, size_t *output_len)
{
	uint8_t *encoded;
	size_t total_length;

	if (!output || !output_len || (!body && body_len) || body_len > SIZE_MAX - 4) {
		return -1;
	}

	total_length = body_len + 4;
	if (total_length < 4 || total_length > max_frame || total_length > UINT32_MAX) {
		return -1;
	}
	if (total_length > SIZE_MAX - 4) return -1;

	encoded = malloc(total_length + 4);
	if (!encoded) return -1;

	write_be32(encoded, (uint32_t)total_length);
	write_be32(encoded + 4, type);
	if (body_len) memcpy(encoded + 8, body, body_len);

	*output = encoded;
	*output_len = total_length + 4;
	return 0;
}
