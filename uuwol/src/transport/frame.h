/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_TRANSPORT_FRAME_H
#define UU_TRANSPORT_FRAME_H

#include <stddef.h>
#include <stdint.h>

struct uu_frame {
	uint32_t type;
	const uint8_t *body;
	size_t body_len;
};

typedef int (*uu_frame_callback)(const struct uu_frame *frame, void *ctx);

struct uu_frame_parser {
	uint8_t *buffer;
	size_t length;
	size_t capacity;
	size_t max_frame;
};

int uu_frame_parser_init(struct uu_frame_parser *parser, size_t max_frame);
void uu_frame_parser_free(struct uu_frame_parser *parser);
int uu_frame_parser_feed(struct uu_frame_parser *parser,
				 const uint8_t *data, size_t length,
				 uu_frame_callback callback, void *ctx);
int uu_frame_encode(uint32_t type, const uint8_t *body, size_t body_len,
			    size_t max_frame, uint8_t **output, size_t *output_len);

#endif
