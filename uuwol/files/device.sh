#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only

. /lib/functions.sh
. /lib/functions/system.sh

uu_device_mac() {
	local mac

	mac="$(uci -q get uuwol.config.mac)"
	if [ -z "$mac" ]; then
		mac="$(get_mac_label)"
		[ -n "$mac" ] || mac="$(macaddr_random)"
		[ -n "$mac" ] || return 1
		uci -q set "uuwol.config.mac=$mac" || return 1
		uci -q commit uuwol || return 1
	fi

	printf '%s\n' "$mac"
}

uu_device_iface() {
	local iface

	iface="$(ubus call network.interface.lan status 2>/dev/null | jsonfilter -e '@.l3_device' 2>/dev/null)"
	[ -n "$iface" ] || return 1
	printf '%s\n' "$iface"
}

uu_device_model() {
	local model

	model="$(ubus call system board 2>/dev/null | jsonfilter -e '@.model' 2>/dev/null)"
	[ -n "$model" ] || return 1
	printf '%s\n' "$model"
}
