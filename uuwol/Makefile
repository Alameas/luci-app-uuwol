# SPDX-License-Identifier: GPL-3.0-only

include $(TOPDIR)/rules.mk

PKG_NAME:=uuwol
PKG_VERSION:=0.1.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-3.0-only

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

UU_SOURCES:=$(wildcard $(CURDIR)/src/*/*.c) $(wildcard $(CURDIR)/src/*/*/*.c)

define Package/uuwol
  SECTION:=net
  CATEGORY:=Network
  DEPENDS:=+libubox +libubus +libuci +libjson-c +libprotobuf-c
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
		-lubox -lubus -luci -ljson-c -lprotobuf-c
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
