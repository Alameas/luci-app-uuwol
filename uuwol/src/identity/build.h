/* SPDX-License-Identifier: GPL-3.0-only */

#ifndef UU_IDENTITY_BUILD_H
#define UU_IDENTITY_BUILD_H

#ifndef UU_VERSION
#error UU_VERSION must be defined by the package Makefile
#endif

#ifndef UU_ROUTER_TYPE
#error UU_ROUTER_TYPE must be defined by the package Makefile
#endif

#ifndef UU_PLUGIN_TYPE
#error UU_PLUGIN_TYPE must be defined by the package Makefile
#endif

#define UU_BUILD_VERSION UU_VERSION
#define UU_BUILD_ROUTER_TYPE UU_ROUTER_TYPE
#define UU_BUILD_PLUGIN_TYPE UU_PLUGIN_TYPE

#endif
