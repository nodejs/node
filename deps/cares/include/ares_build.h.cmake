#ifndef __CARES_BUILD_H
#define __CARES_BUILD_H
/*
 * Copyright (C) The c-ares project and its contributors
 * SPDX-License-Identifier: MIT
 */

#define CARES_TYPEOF_ARES_SOCKLEN_T @CARES_TYPEOF_ARES_SOCKLEN_T@
#define CARES_TYPEOF_ARES_SSIZE_T @CARES_TYPEOF_ARES_SSIZE_T@

/* Prefix names with CARES_ to make sure they don't conflict with other config.h
 * files.  We need to include some dependent headers that may be system specific
 * for C-Ares */
#cmakedefine CARES_HAVE_SYS_TYPES_H
#cmakedefine CARES_HAVE_SYS_SOCKET_H
#cmakedefine CARES_HAVE_SYS_SELECT_H
#cmakedefine CARES_HAVE_WINDOWS_H
#cmakedefine CARES_HAVE_WS2TCPIP_H
#cmakedefine CARES_HAVE_WINSOCK2_H
#cmakedefine CARES_HAVE_ARPA_NAMESER_H
#cmakedefine CARES_HAVE_ARPA_NAMESER_COMPAT_H

#ifdef CARES_HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#ifdef CARES_HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif

#ifdef CARES_HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#ifdef CARES_HAVE_WINSOCK2_H
#  include <winsock2.h>
#endif

#ifdef CARES_HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
#endif

#ifdef CARES_HAVE_WINDOWS_H
#  include <windows.h>
#endif

#endif /* __CARES_BUILD_H */
