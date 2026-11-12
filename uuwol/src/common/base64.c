/* SPDX-License-Identifier: GPL-3.0-only */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libubox/utils.h>

#include "base64.h"

int uu_base64_encode(const void *input, size_t input_length, char **output)
{
	size_t output_length;
	int result;

	if (!output || (!input && input_length)) return -1;
	*output = NULL;
	if (input_length > SIZE_MAX - 2 ||
	    (input_length + 2) / 3 > (SIZE_MAX - 1) / 4)
		return -1;
	output_length = B64_ENCODE_LEN(input_length);
	*output = malloc(output_length);
	if (!*output) return -1;
	result = b64_encode(input, input_length, *output, output_length);
	if (result < 0) {
		free(*output);
		*output = NULL;
		return -1;
	}
	return 0;
}

int uu_base64_decode(const char *input, uint8_t **output, size_t *output_length)
{
	size_t input_length;
	size_t buffer_length;
	uint8_t *buffer;
	int result;

	if (!input || !output || !output_length) return -1;
	*output = NULL;
	*output_length = 0;
	input_length = strlen(input);
	if (input_length > SIZE_MAX - 3 || input_length % 4) return -1;
	buffer_length = B64_DECODE_LEN(input_length);
	buffer = malloc(buffer_length ? buffer_length : 1);
	if (!buffer) return -1;
	result = b64_decode(input, buffer, buffer_length);
	if (result < 0) {
		free(buffer);
		return -1;
	}
	*output = buffer;
	*output_length = (size_t)result;
	return 0;
}
