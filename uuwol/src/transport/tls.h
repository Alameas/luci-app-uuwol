/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_TRANSPORT_TLS_H
#define UU_TRANSPORT_TLS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <libubox/uloop.h>

enum uu_tls_event {
	UU_TLS_EVENT_CONNECTED,
	UU_TLS_EVENT_READABLE,
	UU_TLS_EVENT_WRITABLE,
	UU_TLS_EVENT_CLOSED,
	UU_TLS_EVENT_ERROR,
};

enum uu_tls_state {
	UU_TLS_STATE_CLOSED,
	UU_TLS_STATE_CONNECTING,
	UU_TLS_STATE_HANDSHAKE,
	UU_TLS_STATE_OPEN,
};

typedef void (*uu_tls_callback)(enum uu_tls_event event, void *context);

struct uu_tls {
	mbedtls_ssl_config config;
	mbedtls_ssl_context ssl;
	mbedtls_ctr_drbg_context drbg;
	mbedtls_entropy_context entropy;
	mbedtls_x509_crt ca;
	mbedtls_net_context net;
	struct uloop_fd watcher;
	char hostname[256];
	bool ssl_ready;
	enum uu_tls_state state;
	uu_tls_callback callback;
	void *callback_context;
};

#ifndef UU_TLS_CA_FILE
#define UU_TLS_CA_FILE "/etc/ssl/certs/ca-certificates.crt"
#endif

int uu_tls_init(struct uu_tls *tls, uu_tls_callback callback, void *context);
int uu_tls_connect(struct uu_tls *tls, const char *hostname, unsigned port);
void uu_tls_close(struct uu_tls *tls);
void uu_tls_free(struct uu_tls *tls);
ssize_t uu_tls_read(struct uu_tls *tls, void *buffer, size_t length);
ssize_t uu_tls_write(struct uu_tls *tls, const void *buffer, size_t length);
enum uu_tls_state uu_tls_state_get(const struct uu_tls *tls);

#endif
