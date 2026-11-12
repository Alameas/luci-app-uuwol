/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_IDENTITY_WHOAMI_H
#define UU_IDENTITY_WHOAMI_H

#include <stddef.h>

#define UU_WHOAMI_URL "https://whoami.nie.netease.com/v1"
#define UU_WHOAMI_TOKEN "token.PrdkAfGROQQ9"

struct uu_whoami_response {
	char *body;
	size_t length;
	long status;
};

void uu_whoami_response_free(struct uu_whoami_response *response);
int uu_whoami_fetch(unsigned timeout_ms, struct uu_whoami_response *response);

#endif
