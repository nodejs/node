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

#include <assert.h>

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"


/* Winsock extension functions (ipv4) */
LPFN_CONNECTEX pConnectEx;
LPFN_ACCEPTEX pAcceptEx;
LPFN_GETACCEPTEXSOCKADDRS pGetAcceptExSockAddrs;
LPFN_DISCONNECTEX pDisconnectEx;
LPFN_TRANSMITFILE pTransmitFile;

/* Winsock extension functions (ipv6) */
LPFN_CONNECTEX pConnectEx6;
LPFN_ACCEPTEX pAcceptEx6;
LPFN_GETACCEPTEXSOCKADDRS pGetAcceptExSockAddrs6;
LPFN_DISCONNECTEX pDisconnectEx6;
LPFN_TRANSMITFILE  pTransmitFile6;

/* Whether ipv6 is supported */
int uv_allow_ipv6;

/* Ip address used to bind to any port at any interface */
struct sockaddr_in uv_addr_ip4_any_;
struct sockaddr_in6 uv_addr_ip6_any_;


/*
 * Retrieves the pointer to a winsock extension function.
 */
static BOOL uv_get_extension_function(SOCKET socket, GUID guid,
    void **target) {
  DWORD result, bytes;

  result = WSAIoctl(socket,
                    SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guid,
                    sizeof(guid),
                    (void*)target,
                    sizeof(*target),
                    &bytes,
                    NULL,
                    NULL);

  if (result == SOCKET_ERROR) {
    *target = NULL;
    return FALSE;
  } else {
    return TRUE;
  }
}


void uv_winsock_init() {
  const GUID wsaid_connectex            = WSAID_CONNECTEX;
  const GUID wsaid_acceptex             = WSAID_ACCEPTEX;
  const GUID wsaid_getacceptexsockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
  const GUID wsaid_disconnectex         = WSAID_DISCONNECTEX;
  const GUID wsaid_transmitfile         = WSAID_TRANSMITFILE;

  WSADATA wsa_data;
  int errorno;
  SOCKET dummy;
  SOCKET dummy6;

  /* Initialize winsock */
  errorno = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (errorno != 0) {
    uv_fatal_error(errorno, "WSAStartup");
  }

  /* Set implicit binding address used by connectEx */
  uv_addr_ip4_any_ = uv_ip4_addr("0.0.0.0", 0);
  uv_addr_ip6_any_ = uv_ip6_addr("::", 0);

  /* Retrieve the needed winsock extension function pointers. */
  dummy = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (dummy == INVALID_SOCKET) {
    uv_fatal_error(WSAGetLastError(), "socket");
  }

  if (!uv_get_extension_function(dummy,
                            wsaid_connectex,
                            (void**)&pConnectEx) ||
      !uv_get_extension_function(dummy,
                            wsaid_acceptex,
                            (void**)&pAcceptEx) ||
      !uv_get_extension_function(dummy,
                            wsaid_getacceptexsockaddrs,
                            (void**)&pGetAcceptExSockAddrs) ||
      !uv_get_extension_function(dummy,
                            wsaid_disconnectex,
                            (void**)&pDisconnectEx) ||
      !uv_get_extension_function(dummy,
                            wsaid_transmitfile,
                            (void**)&pTransmitFile)) {
    uv_fatal_error(WSAGetLastError(),
                   "WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)");
  }

  if (closesocket(dummy) == SOCKET_ERROR) {
    uv_fatal_error(WSAGetLastError(), "closesocket");
  }

  /* optional IPv6 versions of winsock extension functions */
  dummy6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_IP);
  if (dummy6 != INVALID_SOCKET) {
    uv_allow_ipv6 = TRUE;

    if (!uv_get_extension_function(dummy6,
                              wsaid_connectex,
                              (void**)&pConnectEx6) ||
        !uv_get_extension_function(dummy6,
                              wsaid_acceptex,
                              (void**)&pAcceptEx6) ||
        !uv_get_extension_function(dummy6,
                              wsaid_getacceptexsockaddrs,
                              (void**)&pGetAcceptExSockAddrs6) ||
        !uv_get_extension_function(dummy6,
                              wsaid_disconnectex,
                              (void**)&pDisconnectEx6) ||
        !uv_get_extension_function(dummy6,
                              wsaid_transmitfile,
                              (void**)&pTransmitFile6)) {
      uv_allow_ipv6 = FALSE;
    }

    if (closesocket(dummy6) == SOCKET_ERROR) {
      uv_fatal_error(WSAGetLastError(), "closesocket");
    }
  }
}
