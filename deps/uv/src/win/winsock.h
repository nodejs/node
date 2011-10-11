/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef UV_WIN_WINSOCK_H_
#define UV_WIN_WINSOCK_H_

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <windows.h>


/*
 * MinGW is missing these too
 */
#ifndef SO_UPDATE_CONNECT_CONTEXT
# define SO_UPDATE_CONNECT_CONTEXT 0x7010
#endif

#ifndef IPV6_V6ONLY
  #define IPV6_V6ONLY 27
#endif

/* Whether ipv6 is supported */
extern int uv_allow_ipv6;

BOOL uv_get_acceptex_function(SOCKET socket, LPFN_ACCEPTEX* target);
BOOL uv_get_connectex_function(SOCKET socket, LPFN_CONNECTEX* target);

/* Ip address used to bind to any port at any interface */
extern struct sockaddr_in uv_addr_ip4_any_;
extern struct sockaddr_in6 uv_addr_ip6_any_;

#endif /* UV_WIN_WINSOCK_H_ */
