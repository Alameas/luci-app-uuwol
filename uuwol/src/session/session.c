/* SPDX-License-Identifier: GPL-3.0-only */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ether.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libubox/utils.h>

#include "../common/helper.h"
#include "../protocol/dump.h"
#include "../protocol/helper.h"
#include "../protocol/message.h"
#include "../protocol/type.h"
#include "../protocol/uu.pb-c.h"
#include "../identity/build.h"
#include "../runtime/log.h"
#include "../wol/wol.h"
#include "session.h"

static int session_send(struct uu_session *session, uint8_t *frame,
			 size_t frame_length);

static int session_send_check_bound(struct uu_session *session)
{
	struct uu_message_check_bound bound = {
		.rid = session->config.rid,
		.router_type = UU_BUILD_ROUTER_TYPE,
		.router_mac = session->config.router_mac,
		.model = session->config.model,
		.version = UU_BUILD_VERSION,
	};
	uint8_t *frame;
	size_t frame_length;

	if (uu_message_encode_check_bound(&bound, UU_SESSION_MAX_FRAME, &frame,
					 &frame_length))
		return -1;
	return session_send(session, frame, frame_length);
}

static int session_send_connect(struct uu_session *session)
{
	struct uu_message_connect connect = {
		.mid = session->config.mid,
		.type = UU_BUILD_ROUTER_TYPE,
		.sn = session->config.router_mac,
		.version = UU_BUILD_VERSION,
		.name = session->config.hostname,
		.model = session->config.model,
		.firmware_version_name = "",
		.ssid = session->config.ssid,
		.device_type = NULL,
		.whoami = session->config.whoami,
		.mode_local = "",
		.plugin_type = UU_BUILD_PLUGIN_TYPE,
		.real_sn = "",
		.firmware_type = "",
		.router_info = session->config.router_info,
		.debug_whoami_ip = "",
		.ssid_5g = "",
	};
	uint8_t *frame;
	size_t frame_length;

	if (uu_message_encode_connect(&connect, UU_SESSION_MAX_FRAME, &frame,
					&frame_length))
		return -1;
	return session_send(session, frame, frame_length);
}

static int session_send_echo(struct uu_session *session)
{
	struct timespec now;
	uint8_t *frame;
	size_t frame_length;

	if (clock_gettime(CLOCK_REALTIME, &now)) return -1;
	if (uu_message_encode_echo(session->config.mid, UU_SESSION_DEFAULT_TEXT,
				   UU_BUILD_ROUTER_TYPE,
				   session->config.router_mac,
				   (uint64_t)now.tv_sec,
				   (uint64_t)now.tv_nsec / 1000,
				   UU_SESSION_MAX_FRAME, &frame, &frame_length))
		return -1;
	return session_send(session, frame, frame_length);
}

static int session_send_device_list(struct uu_session *session)
{
	struct uu_message_device *devices;
	char (*mac_text)[UU_MAC_TEXT_LENGTH];
	struct uu_message_device_list list;
	uint8_t *frame;
	size_t frame_length;
	struct timespec now;
	int result;

	if (!session->leases) return 0;
	if (session->leases->count > SIZE_MAX / sizeof(*devices) ||
	    session->leases->count > SIZE_MAX / sizeof(*mac_text))
		return -1;
	if (session->leases->count) {
		devices = calloc_a(session->leases->count * sizeof(*devices),
				   &mac_text,
				   session->leases->count * sizeof(*mac_text));
		if (!devices) return -1;
	} else {
		devices = NULL;
		mac_text = NULL;
	}
	for (size_t i = 0; i < session->leases->count; i++) {
		if (uu_format_lower_mac(&session->leases->entries[i].mac, mac_text[i])) {
			free(devices);
			return -1;
		}
		devices[i].has_ip = true;
		devices[i].ip = ntohl(session->leases->entries[i].ip.s_addr);
		devices[i].mac = mac_text[i];
		devices[i].hostname = session->leases->entries[i].hostname;
		devices[i].type = UU_SESSION_DEFAULT_CLIENT_TYPE;
		devices[i].has_online = true;
		devices[i].online = false;
	}
	if (clock_gettime(CLOCK_REALTIME, &now)) {
		free(devices);
		return -1;
	}
	list.mid = session->config.mid;
	list.devices = devices;
	list.device_count = session->leases->count;
	list.router_type = UU_BUILD_ROUTER_TYPE;
	list.router_mac = session->config.router_mac;
	result = uu_message_encode_device_list(&list, (uint64_t)now.tv_sec,
						(uint64_t)now.tv_nsec / 1000,
						UU_SESSION_MAX_FRAME, &frame,
						&frame_length);
	free(devices);
	if (result) return -1;
	return session_send(session, frame, frame_length);
}

static int session_flush(struct uu_session *session)
{
	while (session->pending_offset < session->pending_length) {
		ssize_t written = uu_tls_write(
			session->tls.state == UU_TLS_STATE_OPEN ? &session->tls : NULL,
			session->pending + session->pending_offset,
			session->pending_length - session->pending_offset);
		if (written == -EAGAIN) return 0;
		if (written <= 0) return -1;
		session->pending_offset += (size_t)written;
	}
	free(session->pending);
	session->pending = NULL;
	session->pending_length = 0;
	session->pending_offset = 0;
	return 0;
}

static uint32_t session_frame_type(const uint8_t *frame)
{
	return ((uint32_t)frame[4] << 24) |
	       ((uint32_t)frame[5] << 16) |
	       ((uint32_t)frame[6] << 8) |
	       (uint32_t)frame[7];
}

static int session_send(struct uu_session *session, uint8_t *frame,
			 size_t frame_length)
{
	uint8_t *pending;

	if (!session || !frame || !frame_length) {
		free(frame);
		return -1;
	}
	if (frame_length < 8) {
		free(frame);
		return -1;
	}
	if (session->pending_offset == session->pending_length) {
		free(session->pending);
		session->pending = NULL;
		session->pending_length = 0;
		session->pending_offset = 0;
	}
	if (frame_length > SIZE_MAX - session->pending_length) {
		free(frame);
		return -1;
	}
	pending = realloc(session->pending,
			  session->pending_length + frame_length);
	if (!pending) {
		free(frame);
		return -1;
	}
	session->pending = pending;
	memcpy(session->pending + session->pending_length, frame, frame_length);
	session->pending_length += frame_length;
	uu_message_dump(session_frame_type(frame), "out", frame + 8,
			frame_length - 8);
	free(frame);
	return session_flush(session);
}

static void session_copy_device(struct uu_message_device *target,
					const Uu__Device *source)
{
	memset(target, 0, sizeof(*target));
	if (!source) return;
	target->has_ip = source->has_ip != 0;
	target->ip = source->ip;
	target->mac = source->mac;
	target->hostname = source->hostname;
	target->type = source->type;
	target->has_online = source->has_online != 0;
	target->online = source->online != 0;
}

static int session_handle_acc(struct uu_session *session,
				      const uint8_t *body, size_t body_length)
{
	Uu__Acc *message;
	struct uu_message_device device;
	uint8_t *frame;
	size_t frame_length;
	int result;

	message = (Uu__Acc *)uu_message_unpack(UU_MESSAGE_ACC, body, body_length);
	if (!message) return -1;
	session_copy_device(&device, message->deviceinfo);
	result = uu_message_encode_acc_reply(message->mid,
					     message->deviceinfo ? &device : NULL,
					     0, UU_SESSION_MAX_FRAME, &frame,
					     &frame_length);
	uu_message_free(&message->base);
	if (result) return -1;
	return session_send(session, frame, frame_length);
}

static int session_handle_wol(struct uu_session *session,
				      const uint8_t *body, size_t body_length)
{
	Uu__Wol *message;
	struct uu_message_device device;
	struct uu_wol_result wol_result;
	struct ether_addr target;
	uint8_t *frame;
	size_t frame_length;
	bool success = false;
	int result;

	message = (Uu__Wol *)uu_message_unpack(UU_MESSAGE_WOL, body, body_length);
	if (!message) return -1;
	session_copy_device(&device, message->deviceinfo);
	if (message->deviceinfo && device.mac &&
	    ether_aton_r(device.mac, &target)) {
		success = uu_wol_send_all(&target, session->config.iface,
					  &wol_result) == 0;
	} else {
		UU_LOG_WARN("wol target has invalid mac\n");
	}
	result = uu_message_encode_wol_reply(message->mid, NULL, NULL,
					     message->deviceinfo ? &device : NULL,
					     success, UU_SESSION_MAX_FRAME, &frame,
					     &frame_length);
	uu_message_free(&message->base);
	if (result) return -1;
	return session_send(session, frame, frame_length);
}

static int session_frame(const struct uu_frame *frame, void *context)
{
	struct uu_session *session = context;

	uu_message_dump(frame->type, "in", frame->body, frame->body_len);
	switch (frame->type) {
	case UU_MESSAGE_CONNECT_REPLY:
		if (!session->ready) {
			session->ready = true;
			if (session_send_device_list(session)) UU_LOG_WARN("initial device list failed\n");
			uloop_timeout_set(&session->echo_timer,
					  UU_SESSION_ECHO_INTERVAL_MS);
		}
		return 0;
	case UU_MESSAGE_ACC:
		return session_handle_acc(session, frame->body, frame->body_len);
	case UU_MESSAGE_WOL:
		return session_handle_wol(session, frame->body, frame->body_len);
	default:
		return 0;
	}
}

static void session_echo_timeout(struct uloop_timeout *timeout)
{
	struct uu_session *session = container_of(timeout, struct uu_session,
							  echo_timer);

	if (session->ready && session_send_echo(session)) UU_LOG_WARN("echo send failed\n");
	uloop_timeout_set(&session->echo_timer, UU_SESSION_ECHO_INTERVAL_MS);
}

static void session_tls_event(enum uu_tls_event event, void *context)
{
	struct uu_session *session = context;
	uint8_t buffer[16384];
	ssize_t length;

	switch (event) {
	case UU_TLS_EVENT_CONNECTED:
		UU_LOG_NOTICE("tls control connected\n");
		if (session_send_check_bound(session) || session_send_connect(session)) {
			UU_LOG_ERR("session identity send failed\n");
		}
		break;
	case UU_TLS_EVENT_READABLE:
		for (;;) {
			length = uu_tls_read(&session->tls, buffer, sizeof(buffer));
			if (length == -EAGAIN) break;
			if (length <= 0) {
				uu_session_close(session);
				break;
			}
			if (uu_frame_parser_feed(&session->parser, buffer,
						(size_t)length, session_frame, session)) {
				UU_LOG_ERR("session frame parse failed\n");
				uu_session_close(session);
				break;
			}
		}
		break;
	case UU_TLS_EVENT_WRITABLE:
		if (session_flush(session)) uu_session_close(session);
		break;
	case UU_TLS_EVENT_CLOSED:
	case UU_TLS_EVENT_ERROR:
		UU_LOG_WARN("tls control closed event=%d\n", event);
		session->ready = false;
		break;
	}
}

int uu_session_init(struct uu_session *session,
			    const struct uu_session_config *config)
{
	if (!session || !config || !config->mid || !config->router_mac || !config->model ||
	    !config->iface || !config->whoami)
		return -1;
	memset(session, 0, sizeof(*session));
	session->config = *config;
	if (!session->config.ssid) session->config.ssid = session->config.hostname;
	if (!session->config.router_info) {
		session->config.router_info = UU_SESSION_DEFAULT_ROUTER_INFO;
	}
	if (uu_frame_parser_init(&session->parser, UU_SESSION_MAX_FRAME)) return -1;
	if (uu_tls_init(&session->tls, session_tls_event, session)) {
		uu_frame_parser_free(&session->parser);
		return -1;
	}
	session->echo_timer.cb = session_echo_timeout;
	return 0;
}

int uu_session_start(struct uu_session *session)
{
	if (!session) return -1;
	return uu_tls_connect(&session->tls, UU_BUILD_CONTROL_HOST,
				      UU_BUILD_CONTROL_PORT);
}

void uu_session_update_leases(struct uu_session *session,
				      const struct uu_lease_snapshot *snapshot)
{
	if (!session) return;
	session->leases = snapshot;
	if (session->ready && session_send_device_list(session)) UU_LOG_WARN("device list refresh failed\n");
}

void uu_session_close(struct uu_session *session)
{
	if (!session) return;
	uloop_timeout_cancel(&session->echo_timer);
	uu_tls_close(&session->tls);
	session->ready = false;
	free(session->pending);
	session->pending = NULL;
	session->pending_length = 0;
	session->pending_offset = 0;
}

void uu_session_free(struct uu_session *session)
{
	if (!session) return;
	uu_session_close(session);
	uu_tls_free(&session->tls);
	uu_frame_parser_free(&session->parser);
}
