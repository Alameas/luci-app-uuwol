/* SPDX-License-Identifier: GPL-3.0-only */

#include <curl/curl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/log.h"
#include "whoami.h"

struct uu_whoami_buffer {
	char *data;
	size_t length;
};

static size_t write_body(char *data, size_t size, size_t count, void *context)
{
	struct uu_whoami_buffer *buffer = context;
	char *expanded;
	size_t bytes;

	if (size && count > SIZE_MAX / size) return 0;
	bytes = size * count;
	if (bytes > SIZE_MAX - buffer->length - 1) return 0;
	expanded = realloc(buffer->data, buffer->length + bytes + 1);
	if (!expanded) return 0;
	memcpy(expanded + buffer->length, data, bytes);
	buffer->length += bytes;
	expanded[buffer->length] = '\0';
	buffer->data = expanded;
	return bytes;
}

void uu_whoami_response_free(struct uu_whoami_response *response)
{
	if (!response) return;
	free(response->body);
	response->body = NULL;
	response->length = 0;
	response->status = 0;
}

int uu_whoami_fetch(unsigned timeout_ms, struct uu_whoami_response *response)
{
	struct uu_whoami_buffer buffer = { 0 };
	struct curl_slist *headers = NULL;
	struct curl_slist *updated_headers;
	CURL *curl = NULL;
	CURLcode code;
	int result = -1;

	if (!response || !timeout_ms) {
		return -1;
	}
	uu_whoami_response_free(response);
	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return -1;
	curl = curl_easy_init();
	if (!curl) goto cleanup;
	updated_headers = curl_slist_append(headers, "x-auth-product: uu");
	if (!updated_headers) goto cleanup;
	headers = updated_headers;
	{
		char *token_header;

		if (asprintf(&token_header, "x-auth-token: %s", UU_WHOAMI_TOKEN) < 0) {
			goto cleanup;
		}
		updated_headers = curl_slist_append(headers, token_header);
		free(token_header);
		if (!updated_headers) goto cleanup;
		headers = updated_headers;
	}
	curl_easy_setopt(curl, CURLOPT_URL, UU_WHOAMI_URL);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)timeout_ms);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	code = curl_easy_perform(curl);
	if (code != CURLE_OK) {
		UU_LOG_ERR("whoami request failed: %s\n", curl_easy_strerror(code));
		goto cleanup;
	}
	if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status) !=
	    CURLE_OK) {
		goto cleanup;
	}
	response->body = buffer.data;
	response->length = buffer.length;
	buffer.data = NULL;
	if (response->status < 200 || response->status >= 300) {
		UU_LOG_ERR("whoami http status=%ld length=%zu\n",
			   response->status, response->length);
		goto cleanup;
	}
	UU_LOG_INFO("whoami http status=%ld length=%zu\n",
		    response->status, response->length);
	result = 0;

cleanup:
	free(buffer.data);
	curl_slist_free_all(headers);
	if (curl) curl_easy_cleanup(curl);
	curl_global_cleanup();
	return result;
}
