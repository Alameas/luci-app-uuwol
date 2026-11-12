# SPDX-License-Identifier: GPL-3.0-only

include $(TOPDIR)/rules.mk

PKG_NAME:=uuwol
PKG_VERSION:=0.1.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-3.0-only

UU_VERSION?=v14.11.2
UU_ROUTER_TYPE?=h3c
UU_PLUGIN_TYPE?=h3c-nx30pro
UU_CONTROL_HOST?=rglg.uu.netease.com
UU_CONTROL_PORT?=443
UU_CA_FILE?=/etc/ssl/certs/ca-certificates.crt

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

TARGET_CPPFLAGS += -D_GNU_SOURCE \
	-DUU_VERSION=\"$(UU_VERSION)\" \
	-DUU_ROUTER_TYPE=\"$(UU_ROUTER_TYPE)\" \
	-DUU_PLUGIN_TYPE=\"$(UU_PLUGIN_TYPE)\" \
	-DUU_CONTROL_HOST=\"$(UU_CONTROL_HOST)\" \
	-DUU_CONTROL_PORT=$(UU_CONTROL_PORT) \
	-DUU_TLS_CA_FILE=\"$(UU_CA_FILE)\"

UU_SOURCES:=$(wildcard $(CURDIR)/src/*/*.c) $(wildcard $(CURDIR)/src/*/*/*.c)

define Package/uuwol
  SECTION:=net
  CATEGORY:=Network
  DEPENDS:=+libubox +libubus +libuci +libmbedtls +libcurl +libjson-c +libprotobuf-c +rpcd-mod-luci
  TITLE:=UU Wake on LAN client
  URL:=https://uu.163.com
endef

define Package/uuwol/description
  Native OpenWrt UU Wake on LAN client.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(CURDIR)/src $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -std=c11 -Wall -Wextra \
		-I$(CURDIR)/src -I$(PKG_BUILD_DIR)/src \
		$(TARGET_LDFLAGS) -o $(PKG_BUILD_DIR)/uuwol \
		$(UU_SOURCES) \
		-lubox -lubus -luci -lmbedtls -lmbedx509 -lmbedcrypto -lcurl \
		-ljson-c -lprotobuf-c
endef

define Package/uuwol/conffiles
/etc/config/uuwol
endef

define Package/uuwol/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/usr/share/uuwol
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uuwol $(1)/usr/sbin/uuwol
	$(INSTALL_BIN) $(CURDIR)/files/device.sh $(1)/usr/share/uuwol/device.sh
	$(INSTALL_CONF) $(CURDIR)/files/uuwol.config $(1)/etc/config/uuwol
	$(INSTALL_BIN) $(CURDIR)/files/uuwol.init $(1)/etc/init.d/uuwol
endef

$(eval $(call BuildPackage,uuwol))
