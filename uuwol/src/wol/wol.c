/* SPDX-License-Identifier: GPL-3.0-only */

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/if_packet.h>

#include "../runtime/log.h"
#include "wol.h"

#define UU_WOL_PORT 9
#define UU_WOL_MAC_LENGTH 6
#define UU_WOL_REPEAT 16
#define UU_WOL_MAGIC_LENGTH \
	(UU_WOL_MAC_LENGTH + UU_WOL_MAC_LENGTH * UU_WOL_REPEAT)
#define UU_WOL_ETHERNET_HEADER_LENGTH 14
#define UU_WOL_ETHERTYPE 0x0842

static void build_magic_packet(const struct ether_addr *target,
				       uint8_t packet[UU_WOL_MAGIC_LENGTH])
{
	memset(packet, 0xff, UU_WOL_MAC_LENGTH);
	for (size_t i = 0; i < UU_WOL_REPEAT; i++) {
		memcpy(packet + UU_WOL_MAC_LENGTH + i * UU_WOL_MAC_LENGTH,
		       target->ether_addr_octet, UU_WOL_MAC_LENGTH);
	}
}

static int configure_udp_socket(int fd, const char *iface)
{
	int enabled = 1;

	if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, iface,
		       strlen(iface) + 1) < 0 ||
	    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &enabled,
		       sizeof(enabled)) < 0) {
		return -1;
	}
	return 0;
}

static int open_udp_socket(const char *iface)
{
	int fd;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return -1;
	if (configure_udp_socket(fd, iface) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static int send_udp_packet(const struct ether_addr *target, const char *iface,
			   const struct in_addr *destination)
{
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(UU_WOL_PORT),
		.sin_addr = *destination,
	};
	uint8_t packet[UU_WOL_MAGIC_LENGTH];
	ssize_t sent;
	int fd;

	fd = open_udp_socket(iface);
	if (fd < 0) return -1;
	build_magic_packet(target, packet);
	sent = sendto(fd, packet, sizeof(packet), 0,
		      (const struct sockaddr *)&address, sizeof(address));
	close(fd);
	return sent == (ssize_t)sizeof(packet) ? 0 : -1;
}

static int is_ipv4_interface(const struct ifaddrs *entry, const char *iface)
{
	return entry->ifa_name && !strcmp(entry->ifa_name, iface) &&
	       entry->ifa_addr && entry->ifa_netmask &&
	       entry->ifa_addr->sa_family == AF_INET &&
	       entry->ifa_netmask->sa_family == AF_INET;
}

static void calculate_broadcast(const struct sockaddr_in *address,
				const struct sockaddr_in *netmask,
				struct in_addr *broadcast)
{
	uint32_t host_address = ntohl(address->sin_addr.s_addr);
	uint32_t host_netmask = ntohl(netmask->sin_addr.s_addr);

	broadcast->s_addr = htonl((host_address & host_netmask) | ~host_netmask);
}

static int interface_broadcast(const char *iface, struct in_addr *broadcast)
{
	struct ifaddrs *addresses;
	struct ifaddrs *entry;

	if (getifaddrs(&addresses) < 0) return -1;
	for (entry = addresses; entry; entry = entry->ifa_next) {
		struct sockaddr_in *address;
		struct sockaddr_in *netmask;

		if (!is_ipv4_interface(entry, iface)) continue;
		address = (struct sockaddr_in *)entry->ifa_addr;
		netmask = (struct sockaddr_in *)entry->ifa_netmask;
		calculate_broadcast(address, netmask, broadcast);
		freeifaddrs(addresses);
		return 0;
	}
	freeifaddrs(addresses);
	return -1;
}

static int send_udp_limited(const struct ether_addr *target, const char *iface)
{
	struct in_addr destination;

	destination.s_addr = htonl(INADDR_BROADCAST);
	return send_udp_packet(target, iface, &destination);
}

static int send_udp_subnet(const struct ether_addr *target, const char *iface)
{
	struct in_addr destination;

	if (interface_broadcast(iface, &destination)) return -1;
	return send_udp_packet(target, iface, &destination);
}

static int read_interface_mac(int fd, const char *iface,
			       struct ether_addr *mac)
{
	struct ifreq request = { 0 };

	if (strlcpy(request.ifr_name, iface, sizeof(request.ifr_name)) >=
	    sizeof(request.ifr_name)) {
		return -1;
	}
	if (ioctl(fd, SIOCGIFHWADDR, &request) < 0) return -1;
	memcpy(mac->ether_addr_octet, request.ifr_hwaddr.sa_data,
	       UU_WOL_MAC_LENGTH);
	return 0;
}

static void build_ethernet_frame(const struct ether_addr *target,
				 const struct ether_addr *source,
				 uint8_t frame[UU_WOL_ETHERNET_HEADER_LENGTH +
					       UU_WOL_MAGIC_LENGTH])
{
	memset(frame, 0xff, 6);
	memcpy(frame + 6, source->ether_addr_octet, UU_WOL_MAC_LENGTH);
	frame[12] = UU_WOL_ETHERTYPE >> 8;
	frame[13] = UU_WOL_ETHERTYPE & 0xff;
	build_magic_packet(target, frame + UU_WOL_ETHERNET_HEADER_LENGTH);
}

static ssize_t send_ethernet_frame(int fd, int ifindex,
				   const uint8_t *frame, size_t length)
{
	struct sockaddr_ll address = { 0 };

	address.sll_family = AF_PACKET;
	address.sll_ifindex = ifindex;
	address.sll_halen = UU_WOL_MAC_LENGTH;
	memset(address.sll_addr, 0xff, UU_WOL_MAC_LENGTH);
	return sendto(fd, frame, length, 0,
		      (const struct sockaddr *)&address, sizeof(address));
}

static int send_raw_ethernet(const struct ether_addr *target, const char *iface)
{
	struct ether_addr source;
	uint8_t frame[UU_WOL_ETHERNET_HEADER_LENGTH + UU_WOL_MAGIC_LENGTH];
	ssize_t sent;
	int fd;
	int ifindex;

	ifindex = (int)if_nametoindex(iface);
	if (!ifindex) return -1;
	fd = socket(AF_PACKET, SOCK_RAW, htons(UU_WOL_ETHERTYPE));
	if (fd < 0) return -1;
	if (read_interface_mac(fd, iface, &source) < 0) {
		close(fd);
		return -1;
	}
	build_ethernet_frame(target, &source, frame);
	sent = send_ethernet_frame(fd, ifindex, frame, sizeof(frame));
	close(fd);
	return sent == (ssize_t)sizeof(frame) ? 0 : -1;
}

int uu_wol_send_all(const struct ether_addr *target, const char *iface,
			   struct uu_wol_result *result)
{
	struct uu_wol_result local;
	int success = 0;

	if (!target || !iface || !iface[0]) return -1;
	local.udp_limited = send_udp_limited(target, iface) == 0;
	UU_LOG_INFO("wol method=udp-limited result=%s\n",
		    local.udp_limited ? "ok" : "error");
	local.udp_subnet = send_udp_subnet(target, iface) == 0;
	UU_LOG_INFO("wol method=udp-subnet result=%s\n",
		    local.udp_subnet ? "ok" : "error");
	local.raw_ethernet = send_raw_ethernet(target, iface) == 0;
	UU_LOG_INFO("wol method=raw-ethernet result=%s\n",
		    local.raw_ethernet ? "ok" : "error");
	if (local.udp_limited || local.udp_subnet || local.raw_ethernet) success = 1;
	if (result) *result = local;
	return success ? 0 : -1;
}
