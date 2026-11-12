/* SPDX-License-Identifier: GPL-3.0-only */

#include <arpa/inet.h>
#include <errno.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/utils.h>
#include <netinet/ether.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../runtime/log.h"
#include "leases.h"

struct uu_lease_query {
	struct uu_lease_snapshot *snapshot;
};

static const struct blobmsg_policy response_policy[] = {
	{ .name = "dhcp_leases", .type = BLOBMSG_TYPE_ARRAY },
};

static const struct blobmsg_policy lease_policy[] = {
	{ .name = "ipaddr", .type = BLOBMSG_TYPE_STRING },
	{ .name = "macaddr", .type = BLOBMSG_TYPE_STRING },
	{ .name = "hostname", .type = BLOBMSG_TYPE_STRING },
	{ .name = "expires", .type = BLOBMSG_TYPE_UNSPEC },
};

void uu_lease_snapshot_free(struct uu_lease_snapshot *snapshot)
{
	if (!snapshot) return;
	free(snapshot->entries);
	snapshot->entries = NULL;
	snapshot->count = 0;
}

static int read_expiry(struct blob_attr *value, uint64_t *expires)
{
	if (!value || !expires) return -1;
	switch (blobmsg_type(value)) {
	case BLOBMSG_TYPE_INT32:
		*expires = blobmsg_get_u32((struct blob_attr *)value);
		return 0;
	case BLOBMSG_TYPE_INT64:
		*expires = blobmsg_get_u64((struct blob_attr *)value);
		return 0;
	default:
		return -1;
	}
}

static int append_lease(struct uu_lease_snapshot *snapshot,
			const struct blob_attr *lease)
{
	struct blob_attr *values[4] = { 0 };
	struct uu_lease *entry;
	struct ether_addr mac;
	struct in_addr ip;
	const char *ip_text;
	const char *mac_text;
	const char *hostname;

	blobmsg_parse_attr(lease_policy, ARRAY_SIZE(lease_policy), values,
			   (struct blob_attr *)lease);
	if (!values[0] || !values[1]) {
		UU_LOG_WARN("dhcp lease missing ipaddr or macaddr\n");
		return 0;
	}
	ip_text = blobmsg_get_string(values[0]);
	mac_text = blobmsg_get_string(values[1]);
	if (inet_pton(AF_INET, ip_text, &ip) != 1 ||
	    !ether_aton_r(mac_text, &mac)) {
		UU_LOG_WARN("dhcp lease invalid ipaddr=%s macaddr=%s\n",
			    ip_text, mac_text);
		return 0;
	}
	entry = reallocarray(snapshot->entries, snapshot->count + 1,
			     sizeof(*snapshot->entries));
	if (!entry) return -1;
	snapshot->entries = entry;
	entry = &snapshot->entries[snapshot->count];
	memset(entry, 0, sizeof(*entry));
	entry->ip = ip;
	entry->mac = mac;
	if (values[2]) {
		hostname = blobmsg_get_string(values[2]);
		strlcpy(entry->hostname, hostname, sizeof(entry->hostname));
	}
	if (values[3] && read_expiry(values[3], &entry->expires)) {
		UU_LOG_WARN("dhcp lease invalid expires\n");
	}
	snapshot->count++;
	return 0;
}

static void read_response(struct ubus_request *request, int type,
				 struct blob_attr *message)
{
	struct uu_lease_query *query = request->priv;
	struct blob_attr *values[1] = { 0 };
	struct blob_attr *lease;
	size_t remaining;

	(void)type;
	if (!message || !query || !query->snapshot) {
		return;
	}
	blobmsg_parse(response_policy, ARRAY_SIZE(response_policy), values,
		      blob_data(message),
			      blob_len(message));
	if (!values[0]) {
		UU_LOG_WARN("dhcp response missing dhcp_leases\n");
		return;
	}
	blobmsg_for_each_attr(lease, values[0], remaining) {
		if (blobmsg_type(lease) != BLOBMSG_TYPE_TABLE) continue;
		if (append_lease(query->snapshot, lease)) return;
	}
}

int uu_lease_snapshot_query(const char *iface,
				    struct uu_lease_snapshot *snapshot)
{
	struct uu_lease_query query;
	struct ubus_context *context;
	struct blob_buf request = { 0 };
	uint32_t object;
	int result;

	if (!snapshot) return -1;
	uu_lease_snapshot_free(snapshot);
	if (!iface || !iface[0]) return -1;
	query.snapshot = snapshot;
	context = ubus_connect(NULL);
	if (!context) {
		UU_LOG_ERR("dhcp ubus connect failed\n");
		return -1;
	}
	result = blob_buf_init(&request, 0);
	if (!result) result = ubus_lookup_id(context, "luci-rpc", &object);
	if (!result && blobmsg_add_u32(&request, "family", 4) < 0) {
		result = -1;
	}
	if (!result) {
		result = ubus_invoke(context, object, "getDHCPLeases", request.head,
				     read_response, &query, 2000);
	}
	blob_buf_free(&request);
	ubus_free(context);
	if (result) {
		UU_LOG_ERR("dhcp getDHCPLeases failed iface=%s code=%d\n",
			   iface, result);
		uu_lease_snapshot_free(snapshot);
		return -1;
	}
	UU_LOG_INFO("dhcp getDHCPLeases iface=%s count=%zu\n",
		    iface, snapshot->count);
	return 0;
}
