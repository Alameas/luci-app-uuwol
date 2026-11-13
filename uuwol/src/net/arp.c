/* SPDX-License-Identifier: GPL-3.0-only */

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <linux/if_packet.h>

#include "arp.h"

struct uu_arp_frame {
	struct ether_header ethernet;
	struct ether_arp arp;
};

static int interface_address(const char *iface, struct in_addr *address)
{
	struct ifaddrs *addresses;
	struct ifaddrs *entry;

	if (getifaddrs(&addresses) < 0) return -1;
	for (entry = addresses; entry; entry = entry->ifa_next) {
		if (!entry->ifa_name || strcmp(entry->ifa_name, iface) ||
		    !entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET) {
			continue;
		}
		*address = ((struct sockaddr_in *)entry->ifa_addr)->sin_addr;
		freeifaddrs(addresses);
		return 0;
	}
	freeifaddrs(addresses);
	return -1;
}

static int interface_mac(int fd, const char *iface,
			 struct ether_addr *mac)
{
	struct ifreq request = { 0 };

	if (strlcpy(request.ifr_name, iface, sizeof(request.ifr_name)) >=
	    sizeof(request.ifr_name)) {
		return -1;
	}
	if (ioctl(fd, SIOCGIFHWADDR, &request) < 0) return -1;
	memcpy(mac->ether_addr_octet, request.ifr_hwaddr.sa_data,
	       sizeof(mac->ether_addr_octet));
	return 0;
}

static void build_request(const struct ether_addr *source_mac,
			  const struct in_addr *source_ip,
			  const struct in_addr *target_ip,
			  struct uu_arp_frame *request)
{
	memset(request, 0, sizeof(*request));
	memset(request->ethernet.ether_dhost, 0xff,
	       sizeof(request->ethernet.ether_dhost));
	memcpy(request->ethernet.ether_shost, source_mac->ether_addr_octet,
	       sizeof(request->ethernet.ether_shost));
	request->ethernet.ether_type = htons(ETHERTYPE_ARP);
	request->arp.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
	request->arp.ea_hdr.ar_pro = htons(ETH_P_IP);
	request->arp.ea_hdr.ar_hln = ETHER_ADDR_LEN;
	request->arp.ea_hdr.ar_pln = sizeof(source_ip->s_addr);
	request->arp.ea_hdr.ar_op = htons(ARPOP_REQUEST);
	memcpy(request->arp.arp_sha, source_mac->ether_addr_octet,
	       sizeof(request->arp.arp_sha));
	memcpy(request->arp.arp_spa, &source_ip->s_addr,
	       sizeof(request->arp.arp_spa));
	memcpy(request->arp.arp_tpa, &target_ip->s_addr,
	       sizeof(request->arp.arp_tpa));
}

static void match_reply(const struct ether_arp *reply,
			const struct uu_arp_target *targets, size_t count,
			bool *online)
{
	if (ntohs(reply->ea_hdr.ar_op) != ARPOP_REPLY) return;
	for (size_t i = 0; i < count; i++) {
		if (!memcmp(reply->arp_spa, &targets[i].ip.s_addr,
			    sizeof(reply->arp_spa)) &&
		    !memcmp(reply->arp_sha, targets[i].mac.ether_addr_octet,
			    sizeof(reply->arp_sha))) {
			online[i] = true;
			return;
		}
	}
}

static int arp_socket_open(const char *iface, int ifindex,
			   struct sockaddr_ll *address,
			   struct ether_addr *source_mac)
{
	int fd;

	fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
	if (fd < 0) return -1;
	address->sll_family = AF_PACKET;
	address->sll_protocol = htons(ETH_P_ARP);
	address->sll_ifindex = ifindex;
	if (bind(fd, (const struct sockaddr *)address, sizeof(*address)) < 0 ||
	    interface_mac(fd, iface, source_mac)) {
		close(fd);
		return -1;
	}
	address->sll_halen = ETHER_ADDR_LEN;
	memset(address->sll_addr, 0xff, ETHER_ADDR_LEN);
	return fd;
}

static int send_requests(int fd, const struct sockaddr_ll *address,
			 const struct ether_addr *source_mac,
			 const struct in_addr *source_ip,
			 const struct uu_arp_target *targets, size_t count)
{
	struct uu_arp_frame request;
	ssize_t sent;

	for (size_t i = 0; i < count; i++) {
		build_request(source_mac, source_ip, &targets[i].ip, &request);
		sent = sendto(fd, &request, sizeof(request), 0,
			      (const struct sockaddr *)address, sizeof(*address));
		if (sent != sizeof(request)) return -1;
	}
	return 0;
}

static int receive_replies(int fd, const struct uu_arp_target *targets,
			   size_t count, bool *online)
{
	struct pollfd pollfd = { .fd = fd, .events = POLLIN };
	unsigned char frame[ETH_FRAME_LEN];
	struct ether_header *header;
	struct ether_arp *reply;
	struct timespec start;
	ssize_t length;
	int remaining;
	int result;

	if (clock_gettime(CLOCK_MONOTONIC, &start)) return -1;
	for (;;) {
		struct timespec now;
		long elapsed;

		if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
		elapsed = (now.tv_sec - start.tv_sec) * 1000L +
			(now.tv_nsec - start.tv_nsec) / 1000000L;
		remaining = UU_ARP_TIMEOUT_MS - (int)elapsed;
		if (remaining <= 0) return 0;
		result = poll(&pollfd, 1, remaining);
		if (result < 0) return -1;
		if (!result) return 0;
		if (!(pollfd.revents & POLLIN)) continue;
		for (;;) {
			length = recv(fd, frame, sizeof(frame), MSG_DONTWAIT);
			if (length < 0) {
				if (errno == EINTR) continue;
				break;
			}
			if (!length) break;
			if (length < (ssize_t)(sizeof(*header) + sizeof(*reply))) continue;
			header = (struct ether_header *)frame;
			if (ntohs(header->ether_type) != ETHERTYPE_ARP) continue;
			reply = (struct ether_arp *)(frame + sizeof(*header));
			match_reply(reply, targets, count, online);
		}
	}
}

int uu_arp_probe_all(const char *iface, const struct uu_arp_target *targets,
			    size_t count, bool *online)
{
	struct sockaddr_ll address = { 0 };
	struct ether_addr source_mac;
	struct in_addr source_ip;
	int ifindex;
	int fd;

	if (!iface || !iface[0] || (!targets && count) || (!online && count)) {
		return -1;
	}
	if (!count) return 0;
	if (count > SIZE_MAX / sizeof(*online)) return -1;
	memset(online, 0, count * sizeof(*online));
	ifindex = if_nametoindex(iface);
	if (!ifindex || interface_address(iface, &source_ip)) return -1;
	fd = arp_socket_open(iface, ifindex, &address, &source_mac);
	if (fd < 0) return -1;
	if (send_requests(fd, &address, &source_mac, &source_ip, targets,
			  count)) {
		close(fd);
		return -1;
	}
	if (receive_replies(fd, targets, count, online)) {
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}
