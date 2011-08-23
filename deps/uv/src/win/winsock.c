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


int uv_ntstatus_to_winsock_error(NTSTATUS status) {
  switch (status) {
    case STATUS_SUCCESS:
      return ERROR_SUCCESS;

    case STATUS_PENDING:
      return ERROR_IO_PENDING;

    case STATUS_INVALID_HANDLE:
    case STATUS_OBJECT_TYPE_MISMATCH:
      return WSAENOTSOCK;

    case STATUS_INSUFFICIENT_RESOURCES:
    case STATUS_PAGEFILE_QUOTA:
    case STATUS_COMMITMENT_LIMIT:
    case STATUS_WORKING_SET_QUOTA:
    case STATUS_NO_MEMORY:
    case STATUS_CONFLICTING_ADDRESSES:
    case STATUS_QUOTA_EXCEEDED:
    case STATUS_TOO_MANY_PAGING_FILES:
    case STATUS_REMOTE_RESOURCES:
    case STATUS_TOO_MANY_ADDRESSES:
      return WSAENOBUFS;

    case STATUS_SHARING_VIOLATION:
    case STATUS_ADDRESS_ALREADY_EXISTS:
      return WSAEADDRINUSE;

    case STATUS_LINK_TIMEOUT:
    case STATUS_IO_TIMEOUT:
    case STATUS_TIMEOUT:
      return WSAETIMEDOUT;

    case STATUS_GRACEFUL_DISCONNECT:
      return WSAEDISCON;

    case STATUS_REMOTE_DISCONNECT:
    case STATUS_CONNECTION_RESET:
    case STATUS_LINK_FAILED:
    case STATUS_CONNECTION_DISCONNECTED:
    case STATUS_PORT_UNREACHABLE:
      return WSAECONNRESET;

    case STATUS_LOCAL_DISCONNECT:
    case STATUS_TRANSACTION_ABORTED:
    case STATUS_CONNECTION_ABORTED:
      return WSAECONNABORTED;

    case STATUS_BAD_NETWORK_PATH:
    case STATUS_NETWORK_UNREACHABLE:
    case STATUS_PROTOCOL_UNREACHABLE:
      return WSAENETUNREACH;

    case STATUS_HOST_UNREACHABLE:
      return WSAEHOSTUNREACH;

    case STATUS_CANCELLED:
    case STATUS_REQUEST_ABORTED:
      return WSAEINTR;

    case STATUS_BUFFER_OVERFLOW:
    case STATUS_INVALID_BUFFER_SIZE:
      return WSAEMSGSIZE;

    case STATUS_BUFFER_TOO_SMALL:
    case STATUS_ACCESS_VIOLATION:
      return WSAEFAULT;

    case STATUS_DEVICE_NOT_READY:
    case STATUS_REQUEST_NOT_ACCEPTED:
      return WSAEWOULDBLOCK;

    case STATUS_INVALID_NETWORK_RESPONSE:
    case STATUS_NETWORK_BUSY:
    case STATUS_NO_SUCH_DEVICE:
    case STATUS_NO_SUCH_FILE:
    case STATUS_OBJECT_PATH_NOT_FOUND:
    case STATUS_OBJECT_NAME_NOT_FOUND:
    case STATUS_UNEXPECTED_NETWORK_ERROR:
      return WSAENETDOWN;

    case STATUS_INVALID_CONNECTION:
      return WSAENOTCONN;

    case STATUS_REMOTE_NOT_LISTENING:
    case STATUS_CONNECTION_REFUSED:
      return WSAECONNREFUSED;

    case STATUS_PIPE_DISCONNECTED:
      return WSAESHUTDOWN;

    case STATUS_INVALID_ADDRESS:
    case STATUS_INVALID_ADDRESS_COMPONENT:
      return WSAEADDRNOTAVAIL;

    case STATUS_NOT_SUPPORTED:
    case STATUS_NOT_IMPLEMENTED:
      return WSAEOPNOTSUPP;

    case STATUS_ACCESS_DENIED:
      return WSAEACCES;

    default:
      if (status & ((FACILITY_NTWIN32 << 16) | ERROR_SEVERITY_ERROR)) {
        /* It's a windows error that has been previously mapped to an */
        /* ntstatus code. */
        return (DWORD) (status & 0xffff);
      } else {
        /* The default fallback for unmappable ntstatus codes. */
        return WSAEINVAL;
      }
  }
}
