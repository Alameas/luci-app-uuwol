/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_COMMON_BASE64_H
#define UU_COMMON_BASE64_H

#include <stddef.h>
#include <stdint.h>

int uu_base64_encode(const void *input, size_t input_length, char **output);
int uu_base64_decode(const char *input, uint8_t **output, size_t *output_length);

#endif
