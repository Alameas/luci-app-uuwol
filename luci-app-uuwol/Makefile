# SPDX-License-Identifier: GPL-3.0-only

include $(TOPDIR)/rules.mk

PKG_LICENSE:=GPL-3.0-only

LUCI_TITLE:=LuCI support for UU WOL
LUCI_DEPENDS:=+uuwol
LUCI_PKGARCH:=all

include $(TOPDIR)/feeds/luci/luci.mk

# call BuildPackage - OpenWrt buildroot signature
