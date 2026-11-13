/* SPDX-License-Identifier: GPL-3.0-only */

#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <libubox/list.h>
#include <libubox/ustream.h>
#include <libubox/utils.h>

#include "../identity/build.h"
#include "../runtime/log.h"
#include "router.h"

struct uu_router_client {
	struct list_head list;
	struct uu_router_server *server;
	struct ustream_fd stream;
	bool response_started;
};

static void client_destroy(struct uu_router_client *client)
{
	int fd;

	if (!client) return;
	fd = client->stream.fd.fd;
	if (client->server->client_count) client->server->client_count--;
	list_del(&client->list);
	ustream_free(&client->stream.stream);
	if (fd >= 0) close(fd);
	free(client);
}

static bool request_is_router(const char *request)
{
	return !strncmp(request, "GET /router ", sizeof("GET /router ") - 1);
}

static bool request_is_complete(const char *request)
{
	return strstr(request, "\r\n\r\n") || strstr(request, "\n\n");
}

static int client_send(struct uu_router_client *client, int status,
			       const char *reason, const char *body)
{
	client->response_started = true;
	if (ustream_printf(&client->stream.stream,
			   "HTTP/1.1 %d %s\r\nConnection: close\r\n\r\n%s",
			   status, reason, body ? body : "") < 0) {
		return -1;
	}
	if (!ustream_pending_data(&client->stream.stream, true)) {
		client_destroy(client);
	}
	return 0;
}

static int client_send_router(struct uu_router_client *client)
{
	char body[256];
	int length;

	length = snprintf(body, sizeof(body), "type:%s\nsn:%s\n",
			  UU_BUILD_ROUTER_TYPE,
			  client->server->router_mac);
	if (length < 0 || (size_t)length >= sizeof(body)) return -1;
	return client_send(client, 200, "OK", body);
}

static void client_read(struct ustream *stream, int bytes)
{
	struct uu_router_client *client;
	char *request;

	(void)bytes;
	client = container_of(stream, struct uu_router_client, stream.stream);
	if (client->response_started) return;
	if (ustream_pending_data(stream, false) >= UU_LOCAL_ROUTER_MAX_REQUEST) {
		if (client_send(client, 400, "Bad Request", NULL)) client_destroy(client);
		return;
	}
	request = ustream_get_read_buf(stream, NULL);
	if (!request || !request_is_complete(request)) return;
	if (request_is_router(request)) {
		if (client_send_router(client)) client_destroy(client);
		return;
	}
	if (client_send(client, 404, "NOT FOUND", NULL)) client_destroy(client);
}

static void client_write(struct ustream *stream, int bytes)
{
	struct uu_router_client *client;

	(void)bytes;
	client = container_of(stream, struct uu_router_client, stream.stream);
	if (client->response_started && !ustream_pending_data(stream, true)) {
		client_destroy(client);
	}
}

static void client_state(struct ustream *stream)
{
	struct uu_router_client *client;

	client = container_of(stream, struct uu_router_client, stream.stream);
	if (stream->write_error || !stream->eof ||
	    !ustream_pending_data(stream, true) || !client->response_started) {
		client_destroy(client);
	}
}

static void listener_accept(struct uloop_fd *fd, unsigned int events)
{
	struct uu_router_server *server;
	struct uu_router_client *client;
	int client_fd;

	server = container_of(fd, struct uu_router_server, listener);
	if (events & ULOOP_ERROR_CB) UU_LOG_WARN("local router listener error\n");
	while (1) {
		client_fd = accept(fd->fd, NULL, NULL);
		if (client_fd < 0) {
			if (errno == EINTR) continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK) return;
			UU_LOG_WARN("local router accept failed: %s\n", strerror(errno));
			return;
		}
		if (server->client_count >= UU_LOCAL_ROUTER_MAX_CLIENTS) {
			UU_LOG_WARN("local router client limit reached\n");
			close(client_fd);
			continue;
		}
		client = calloc(1, sizeof(*client));
		if (!client) {
			close(client_fd);
			return;
		}
		client->server = server;
		server->client_count++;
		INIT_LIST_HEAD(&client->list);
		list_add(&client->list, &server->clients);
		client->stream.stream.r.buffer_len = UU_LOCAL_ROUTER_MAX_REQUEST;
		ustream_fd_init(&client->stream, client_fd);
		client->stream.stream.string_data = true;
		client->stream.stream.notify_read = client_read;
		client->stream.stream.notify_write = client_write;
		client->stream.stream.notify_state = client_state;
	}
}

int uu_router_server_init(struct uu_router_server *server,
				  const char *router_mac, const char *iface)
{
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(UU_LOCAL_ROUTER_PORT),
		.sin_addr = { .s_addr = htonl(INADDR_ANY) },
	};
	int enabled = 1;
	int fd;

	if (!server) return -1;
	*server = (struct uu_router_server){
		.listener = { .fd = -1 },
	};
	INIT_LIST_HEAD(&server->clients);
	if (!router_mac || !router_mac[0] || !iface || !iface[0]) return -1;
	if (strlcpy(server->router_mac, router_mac,
			    sizeof(server->router_mac)) >= sizeof(server->router_mac)) {
		return -1;
	}
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		UU_LOG_ERR("local router socket failed: %s\n", strerror(errno));
		return -1;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) ||
	    setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, iface,
		       strlen(iface) + 1) ||
	    bind(fd, (struct sockaddr *)&address, sizeof(address)) ||
	    listen(fd, UU_LOCAL_ROUTER_MAX_CLIENTS)) {
		UU_LOG_ERR("local router bind failed on %s: %s\n", iface,
			   strerror(errno));
		close(fd);
		return -1;
	}
	server->listener.fd = fd;
	server->listener.cb = listener_accept;
	return 0;
}

int uu_router_server_start(struct uu_router_server *server)
{
	if (!server || server->listener.fd < 0) return -1;
	if (uloop_fd_add(&server->listener, ULOOP_READ | ULOOP_ERROR_CB)) {
		UU_LOG_ERR("local router listener registration failed\n");
		return -1;
	}
	UU_LOG_NOTICE("local router endpoint listening on %u type=%s\n",
		      UU_LOCAL_ROUTER_PORT, UU_BUILD_ROUTER_TYPE);
	return 0;
}

void uu_router_server_stop(struct uu_router_server *server)
{
	struct uu_router_client *client;
	struct uu_router_client *next;

	if (!server) return;
	if (server->listener.registered) uloop_fd_delete(&server->listener);
	if (server->listener.fd >= 0) {
		close(server->listener.fd);
		server->listener.fd = -1;
	}
	list_for_each_entry_safe(client, next, &server->clients, list) {
		client_destroy(client);
	}
}

void uu_router_server_free(struct uu_router_server *server)
{
	uu_router_server_stop(server);
}
