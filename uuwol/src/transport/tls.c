/* SPDX-License-Identifier: GPL-3.0-only */

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>

#include "../runtime/log.h"
#include "tls.h"

static void tls_event(struct uu_tls *tls, enum uu_tls_event event)
{
	if (tls->callback) tls->callback(event, tls->callback_context);
}

static void tls_log_error(const char *operation, int result)
{
	char text[256];

	mbedtls_strerror(result, text, sizeof(text));
	UU_LOG_ERR("tls %s failed: -0x%04x %s\n", operation, -result, text);
}

static void tls_close_socket(struct uu_tls *tls)
{
	if (tls->watcher.registered) uloop_fd_delete(&tls->watcher);
	mbedtls_net_close(&tls->net);
	tls->watcher.fd = -1;
}

static void tls_stop(struct uu_tls *tls, enum uu_tls_event event)
{
	tls_close_socket(tls);
	tls->state = UU_TLS_STATE_CLOSED;
	tls_event(tls, event);
}

static void tls_fail(struct uu_tls *tls)
{
	tls_stop(tls, UU_TLS_EVENT_ERROR);
}

static void tls_watch(struct uu_tls *tls, unsigned flags)
{
	if (tls->net.fd < 0) return;
	tls->watcher.flags = flags | ULOOP_ERROR_CB;
	if (uloop_fd_add(&tls->watcher, tls->watcher.flags)) {
		UU_LOG_ERR("tls watcher registration failed\n");
	}
}

static int tls_prepare_ssl(struct uu_tls *tls)
{
	int result;

	if (tls->ssl_ready) {
		result = mbedtls_ssl_session_reset(&tls->ssl);
	} else {
		result = mbedtls_ssl_setup(&tls->ssl, &tls->config);
		if (!result) tls->ssl_ready = true;
	}
	if (result) return result;
	result = mbedtls_ssl_set_hostname(&tls->ssl, tls->hostname);
	if (result) return result;
	mbedtls_ssl_set_bio(&tls->ssl, &tls->net,
			    mbedtls_net_send, mbedtls_net_recv, NULL);
	return 0;
}

static bool tls_want_io(struct uu_tls *tls, int result)
{
	if (result == MBEDTLS_ERR_SSL_WANT_READ) {
		tls_watch(tls, ULOOP_READ);
		return true;
	}
	if (result == MBEDTLS_ERR_SSL_WANT_WRITE) {
		tls_watch(tls, ULOOP_WRITE);
		return true;
	}
	return false;
}

static ssize_t tls_io_error(struct uu_tls *tls, const char *operation,
			    int result)
{
	if (tls_want_io(tls, result)) return -EAGAIN;
	tls_log_error(operation, result);
	tls_fail(tls);
	return -1;
}

static int tls_handshake(struct uu_tls *tls)
{
	uint32_t verify_result;
	int result;

	result = mbedtls_ssl_handshake(&tls->ssl);
	if (!result) {
		verify_result = mbedtls_ssl_get_verify_result(&tls->ssl);
		if (verify_result) {
			UU_LOG_ERR("tls certificate verification failed: 0x%08x\n",
				   verify_result);
			tls_fail(tls);
			return -1;
		}
		tls->state = UU_TLS_STATE_OPEN;
		tls_watch(tls, ULOOP_READ | ULOOP_WRITE);
		tls_event(tls, UU_TLS_EVENT_CONNECTED);
		return 0;
	}
	if (tls_want_io(tls, result)) return 0;
	tls_log_error("handshake", result);
	tls_fail(tls);
	return -1;
}

static int tls_finish_tcp_connect(struct uu_tls *tls)
{
	int error = 0;
	socklen_t length = sizeof(error);
	int result;

	if (getsockopt(tls->net.fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
		error = errno;
	}
	if (error) {
		UU_LOG_ERR("tls tcp connect failed: %s\n", strerror(error));
		tls_fail(tls);
		return -1;
	}
	result = tls_prepare_ssl(tls);
	if (result) {
		tls_log_error("ssl setup", result);
		tls_fail(tls);
		return -1;
	}
	tls->state = UU_TLS_STATE_HANDSHAKE;
	return tls_handshake(tls);
}

static void tls_fd_callback(struct uloop_fd *watcher, unsigned events)
{
	struct uu_tls *tls = container_of(watcher, struct uu_tls, watcher);

	if (events & ULOOP_ERROR_CB) {
		tls_fail(tls);
		return;
	}
	if (tls->state == UU_TLS_STATE_CONNECTING) {
		if (tls_finish_tcp_connect(tls)) return;
		if (tls->state != UU_TLS_STATE_OPEN) return;
	}
	if (tls->state == UU_TLS_STATE_HANDSHAKE) {
		if (tls_handshake(tls)) return;
		if (tls->state != UU_TLS_STATE_OPEN) return;
	}
	if (tls->state != UU_TLS_STATE_OPEN) return;
	if (events & ULOOP_READ) tls_event(tls, UU_TLS_EVENT_READABLE);
	if (events & ULOOP_WRITE) tls_event(tls, UU_TLS_EVENT_WRITABLE);
}

int uu_tls_init(struct uu_tls *tls, uu_tls_callback callback, void *context)
{
	const unsigned char personalization[] = "uuwol";
	int result;

	if (!tls) return -1;
	memset(tls, 0, sizeof(*tls));
	mbedtls_ssl_config_init(&tls->config);
	mbedtls_ssl_init(&tls->ssl);
	mbedtls_ctr_drbg_init(&tls->drbg);
	mbedtls_entropy_init(&tls->entropy);
	mbedtls_x509_crt_init(&tls->ca);
	mbedtls_net_init(&tls->net);
	tls->state = UU_TLS_STATE_CLOSED;
	tls->callback = callback;
	tls->callback_context = context;
	tls->watcher.fd = -1;
	tls->watcher.cb = tls_fd_callback;
	result = mbedtls_ctr_drbg_seed(&tls->drbg, mbedtls_entropy_func,
				      &tls->entropy, personalization,
				      sizeof(personalization) - 1);
	if (result) goto error;
	result = mbedtls_x509_crt_parse_file(&tls->ca, UU_TLS_CA_FILE);
	if (result < 0) goto error;
	if (result > 0) {
		UU_LOG_WARN("tls CA bundle contains %d invalid certificates\n", result);
	}
	result = mbedtls_ssl_config_defaults(&tls->config,
					     MBEDTLS_SSL_IS_CLIENT,
					     MBEDTLS_SSL_TRANSPORT_STREAM,
					     MBEDTLS_SSL_PRESET_DEFAULT);
	if (result) goto error;
	mbedtls_ssl_conf_authmode(&tls->config, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&tls->config, &tls->ca, NULL);
	mbedtls_ssl_conf_rng(&tls->config, mbedtls_ctr_drbg_random,
				    &tls->drbg);
	return 0;

error:
	tls_log_error("initialization", result);
	uu_tls_free(tls);
	return -1;
}

int uu_tls_connect(struct uu_tls *tls, const char *hostname, unsigned port)
{
	struct addrinfo hints = { 0 };
	struct addrinfo *addresses;
	struct addrinfo *address;
	char port_text[6];
	int result;

	if (!tls || !hostname || !hostname[0] || port > 65535 ||
	    tls->state != UU_TLS_STATE_CLOSED)
		return -1;
	if (strlcpy(tls->hostname, hostname, sizeof(tls->hostname)) >=
	    sizeof(tls->hostname)) {
		return -1;
	}
	snprintf(port_text, sizeof(port_text), "%u", port);
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_UNSPEC;
	result = getaddrinfo(hostname, port_text, &hints, &addresses);
	if (result) return -1;
	for (address = addresses; address; address = address->ai_next) {
		mbedtls_net_context net;

		mbedtls_net_init(&net);
		net.fd = socket(address->ai_family, address->ai_socktype,
				address->ai_protocol);
		if (net.fd < 0) continue;
		if (mbedtls_net_set_nonblock(&net)) {
			mbedtls_net_close(&net);
			continue;
		}
		result = connect(net.fd, address->ai_addr, address->ai_addrlen);
		if (result < 0 && errno != EINPROGRESS) {
			mbedtls_net_close(&net);
			continue;
		}
		tls->net = net;
		tls->watcher.fd = tls->net.fd;
		tls->watcher.registered = false;
		tls->state = result == 0 ? UU_TLS_STATE_HANDSHAKE :
			UU_TLS_STATE_CONNECTING;
		if (tls->state == UU_TLS_STATE_HANDSHAKE) {
			tls_watch(tls, ULOOP_READ);
			freeaddrinfo(addresses);
			return tls_finish_tcp_connect(tls);
		}
		tls_watch(tls, ULOOP_WRITE);
		freeaddrinfo(addresses);
		return 0;
	}
	freeaddrinfo(addresses);
	return -1;
}

void uu_tls_close(struct uu_tls *tls)
{
	if (!tls) return;
	if (tls->watcher.registered) uloop_fd_delete(&tls->watcher);
	if (tls->ssl_ready) {
		if (tls->state == UU_TLS_STATE_OPEN) {
			mbedtls_ssl_close_notify(&tls->ssl);
		}
		mbedtls_ssl_session_reset(&tls->ssl);
	}
	tls_close_socket(tls);
	tls->state = UU_TLS_STATE_CLOSED;
}

void uu_tls_free(struct uu_tls *tls)
{
	if (!tls) return;
	uu_tls_close(tls);
	mbedtls_ssl_free(&tls->ssl);
	mbedtls_ssl_config_free(&tls->config);
	mbedtls_x509_crt_free(&tls->ca);
	mbedtls_ctr_drbg_free(&tls->drbg);
	mbedtls_entropy_free(&tls->entropy);
	tls->ssl_ready = false;
}

ssize_t uu_tls_read(struct uu_tls *tls, void *buffer, size_t length)
{
	int result;

	if (!tls || tls->state != UU_TLS_STATE_OPEN || (!buffer && length) ||
	    length > INT_MAX) {
		return -1;
	}
	result = mbedtls_ssl_read(&tls->ssl, buffer, length);
	if (result > 0) return result;
	if (result == 0 || result == MBEDTLS_ERR_SSL_CONN_EOF ||
	    result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
		tls_stop(tls, UU_TLS_EVENT_CLOSED);
		return 0;
	}
	return tls_io_error(tls, "read", result);
}

ssize_t uu_tls_write(struct uu_tls *tls, const void *buffer, size_t length)
{
	int result;

	if (!tls || tls->state != UU_TLS_STATE_OPEN || (!buffer && length) ||
	    length > INT_MAX) {
		return -1;
	}
	result = mbedtls_ssl_write(&tls->ssl, buffer, length);
	if (result > 0) return result;
	return tls_io_error(tls, "write", result);
}

enum uu_tls_state uu_tls_state_get(const struct uu_tls *tls)
{
	return tls ? tls->state : UU_TLS_STATE_CLOSED;
}
