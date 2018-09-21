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
#include <stdlib.h>

#include "uv.h"
#include "internal.h"


/* Whether there are any non-IFS LSPs stacked on TCP */
int uv_tcp_non_ifs_lsp_ipv4;
int uv_tcp_non_ifs_lsp_ipv6;

/* Ip address used to bind to any port at any interface */
struct sockaddr_in uv_addr_ip4_any_;
struct sockaddr_in6 uv_addr_ip6_any_;


/*
 * Retrieves the pointer to a winsock extension function.
 */
static BOOL uv_get_extension_function(SOCKET socket, GUID guid,
    void **target) {
  int result;
  DWORD bytes;

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


BOOL uv_get_acceptex_function(SOCKET socket, LPFN_ACCEPTEX* target) {
  const GUID wsaid_acceptex = WSAID_ACCEPTEX;
  return uv_get_extension_function(socket, wsaid_acceptex, (void**)target);
}


BOOL uv_get_connectex_function(SOCKET socket, LPFN_CONNECTEX* target) {
  const GUID wsaid_connectex = WSAID_CONNECTEX;
  return uv_get_extension_function(socket, wsaid_connectex, (void**)target);
}


static int error_means_no_support(DWORD error) {
  return error == WSAEPROTONOSUPPORT || error == WSAESOCKTNOSUPPORT ||
         error == WSAEPFNOSUPPORT || error == WSAEAFNOSUPPORT;
}


void uv_winsock_init(void) {
  WSADATA wsa_data;
  int errorno;
  SOCKET dummy;
  WSAPROTOCOL_INFOW protocol_info;
  int opt_len;

  /* Initialize winsock */
  errorno = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (errorno != 0) {
    uv_fatal_error(errorno, "WSAStartup");
  }

  /* Set implicit binding address used by connectEx */
  if (uv_ip4_addr("0.0.0.0", 0, &uv_addr_ip4_any_)) {
    abort();
  }

  if (uv_ip6_addr("::", 0, &uv_addr_ip6_any_)) {
    abort();
  }

  /* Detect non-IFS LSPs */
  dummy = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

  if (dummy != INVALID_SOCKET) {
    opt_len = (int) sizeof protocol_info;
    if (getsockopt(dummy,
                   SOL_SOCKET,
                   SO_PROTOCOL_INFOW,
                   (char*) &protocol_info,
                   &opt_len) == SOCKET_ERROR)
      uv_fatal_error(WSAGetLastError(), "getsockopt");

    if (!(protocol_info.dwServiceFlags1 & XP1_IFS_HANDLES))
      uv_tcp_non_ifs_lsp_ipv4 = 1;

    if (closesocket(dummy) == SOCKET_ERROR)
      uv_fatal_error(WSAGetLastError(), "closesocket");

  } else if (!error_means_no_support(WSAGetLastError())) {
    /* Any error other than "socket type not supported" is fatal. */
    uv_fatal_error(WSAGetLastError(), "socket");
  }

  /* Detect IPV6 support and non-IFS LSPs */
  dummy = socket(AF_INET6, SOCK_STREAM, IPPROTO_IP);

  if (dummy != INVALID_SOCKET) {
    opt_len = (int) sizeof protocol_info;
    if (getsockopt(dummy,
                   SOL_SOCKET,
                   SO_PROTOCOL_INFOW,
                   (char*) &protocol_info,
                   &opt_len) == SOCKET_ERROR)
      uv_fatal_error(WSAGetLastError(), "getsockopt");

    if (!(protocol_info.dwServiceFlags1 & XP1_IFS_HANDLES))
      uv_tcp_non_ifs_lsp_ipv6 = 1;

    if (closesocket(dummy) == SOCKET_ERROR)
      uv_fatal_error(WSAGetLastError(), "closesocket");

  } else if (!error_means_no_support(WSAGetLastError())) {
    /* Any error other than "socket type not supported" is fatal. */
    uv_fatal_error(WSAGetLastError(), "socket");
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
    case STATUS_QUOTA_EXCEEDED:
    case STATUS_TOO_MANY_PAGING_FILES:
    case STATUS_REMOTE_RESOURCES:
      return WSAENOBUFS;

    case STATUS_TOO_MANY_ADDRESSES:
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
    case STATUS_HOPLIMIT_EXCEEDED:
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

    case STATUS_CONFLICTING_ADDRESSES:
    case STATUS_INVALID_ADDRESS:
    case STATUS_INVALID_ADDRESS_COMPONENT:
      return WSAEADDRNOTAVAIL;

    case STATUS_NOT_SUPPORTED:
    case STATUS_NOT_IMPLEMENTED:
      return WSAEOPNOTSUPP;

    case STATUS_ACCESS_DENIED:
      return WSAEACCES;

    default:
      if ((status & (FACILITY_NTWIN32 << 16)) == (FACILITY_NTWIN32 << 16) &&
          (status & (ERROR_SEVERITY_ERROR | ERROR_SEVERITY_WARNING))) {
        /* It's a windows error that has been previously mapped to an ntstatus
         * code. */
        return (DWORD) (status & 0xffff);
      } else {
        /* The default fallback for unmappable ntstatus codes. */
        return WSAEINVAL;
      }
  }
}


/*
 * This function provides a workaround for a bug in the winsock implementation
 * of WSARecv. The problem is that when SetFileCompletionNotificationModes is
 * used to avoid IOCP notifications of completed reads, WSARecv does not
 * reliably indicate whether we can expect a completion package to be posted
 * when the receive buffer is smaller than the received datagram.
 *
 * However it is desirable to use SetFileCompletionNotificationModes because
 * it yields a massive performance increase.
 *
 * This function provides a workaround for that bug, but it only works for the
 * specific case that we need it for. E.g. it assumes that the "avoid iocp"
 * bit has been set, and supports only overlapped operation. It also requires
 * the user to use the default msafd driver, doesn't work when other LSPs are
 * stacked on top of it.
 */
int WSAAPI uv_wsarecv_workaround(SOCKET socket, WSABUF* buffers,
    DWORD buffer_count, DWORD* bytes, DWORD* flags, WSAOVERLAPPED *overlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion_routine) {
  NTSTATUS status;
  void* apc_context;
  IO_STATUS_BLOCK* iosb = (IO_STATUS_BLOCK*) &overlapped->Internal;
  AFD_RECV_INFO info;
  DWORD error;

  if (overlapped == NULL || completion_routine != NULL) {
    WSASetLastError(WSAEINVAL);
    return SOCKET_ERROR;
  }

  info.BufferArray = buffers;
  info.BufferCount = buffer_count;
  info.AfdFlags = AFD_OVERLAPPED;
  info.TdiFlags = TDI_RECEIVE_NORMAL;

  if (*flags & MSG_PEEK) {
    info.TdiFlags |= TDI_RECEIVE_PEEK;
  }

  if (*flags & MSG_PARTIAL) {
    info.TdiFlags |= TDI_RECEIVE_PARTIAL;
  }

  if (!((intptr_t) overlapped->hEvent & 1)) {
    apc_context = (void*) overlapped;
  } else {
    apc_context = NULL;
  }

  iosb->Status = STATUS_PENDING;
  iosb->Pointer = 0;

  status = pNtDeviceIoControlFile((HANDLE) socket,
                                  overlapped->hEvent,
                                  NULL,
                                  apc_context,
                                  iosb,
                                  IOCTL_AFD_RECEIVE,
                                  &info,
                                  sizeof(info),
                                  NULL,
                                  0);

  *flags = 0;
  *bytes = (DWORD) iosb->Information;

  switch (status) {
    case STATUS_SUCCESS:
      error = ERROR_SUCCESS;
      break;

    case STATUS_PENDING:
      error = WSA_IO_PENDING;
      break;

    case STATUS_BUFFER_OVERFLOW:
      error = WSAEMSGSIZE;
      break;

    case STATUS_RECEIVE_EXPEDITED:
      error = ERROR_SUCCESS;
      *flags = MSG_OOB;
      break;

    case STATUS_RECEIVE_PARTIAL_EXPEDITED:
      error = ERROR_SUCCESS;
      *flags = MSG_PARTIAL | MSG_OOB;
      break;

    case STATUS_RECEIVE_PARTIAL:
      error = ERROR_SUCCESS;
      *flags = MSG_PARTIAL;
      break;

    default:
      error = uv_ntstatus_to_winsock_error(status);
      break;
  }

  WSASetLastError(error);

  if (error == ERROR_SUCCESS) {
    return 0;
  } else {
    return SOCKET_ERROR;
  }
}


/* See description of uv_wsarecv_workaround. */
int WSAAPI uv_wsarecvfrom_workaround(SOCKET socket, WSABUF* buffers,
    DWORD buffer_count, DWORD* bytes, DWORD* flags, struct sockaddr* addr,
    int* addr_len, WSAOVERLAPPED *overlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion_routine) {
  NTSTATUS status;
  void* apc_context;
  IO_STATUS_BLOCK* iosb = (IO_STATUS_BLOCK*) &overlapped->Internal;
  AFD_RECV_DATAGRAM_INFO info;
  DWORD error;

  if (overlapped == NULL || addr == NULL || addr_len == NULL ||
      completion_routine != NULL) {
    WSASetLastError(WSAEINVAL);
    return SOCKET_ERROR;
  }

  info.BufferArray = buffers;
  info.BufferCount = buffer_count;
  info.AfdFlags = AFD_OVERLAPPED;
  info.TdiFlags = TDI_RECEIVE_NORMAL;
  info.Address = addr;
  info.AddressLength = addr_len;

  if (*flags & MSG_PEEK) {
    info.TdiFlags |= TDI_RECEIVE_PEEK;
  }

  if (*flags & MSG_PARTIAL) {
    info.TdiFlags |= TDI_RECEIVE_PARTIAL;
  }

  if (!((intptr_t) overlapped->hEvent & 1)) {
    apc_context = (void*) overlapped;
  } else {
    apc_context = NULL;
  }

  iosb->Status = STATUS_PENDING;
  iosb->Pointer = 0;

  status = pNtDeviceIoControlFile((HANDLE) socket,
                                  overlapped->hEvent,
                                  NULL,
                                  apc_context,
                                  iosb,
                                  IOCTL_AFD_RECEIVE_DATAGRAM,
                                  &info,
                                  sizeof(info),
                                  NULL,
                                  0);

  *flags = 0;
  *bytes = (DWORD) iosb->Information;

  switch (status) {
    case STATUS_SUCCESS:
      error = ERROR_SUCCESS;
      break;

    case STATUS_PENDING:
      error = WSA_IO_PENDING;
      break;

    case STATUS_BUFFER_OVERFLOW:
      error = WSAEMSGSIZE;
      break;

    case STATUS_RECEIVE_EXPEDITED:
      error = ERROR_SUCCESS;
      *flags = MSG_OOB;
      break;

    case STATUS_RECEIVE_PARTIAL_EXPEDITED:
      error = ERROR_SUCCESS;
      *flags = MSG_PARTIAL | MSG_OOB;
      break;

    case STATUS_RECEIVE_PARTIAL:
      error = ERROR_SUCCESS;
      *flags = MSG_PARTIAL;
      break;

    default:
      error = uv_ntstatus_to_winsock_error(status);
      break;
  }

  WSASetLastError(error);

  if (error == ERROR_SUCCESS) {
    return 0;
  } else {
    return SOCKET_ERROR;
  }
}


int WSAAPI uv_msafd_poll(SOCKET socket, AFD_POLL_INFO* info_in,
    AFD_POLL_INFO* info_out, OVERLAPPED* overlapped) {
  IO_STATUS_BLOCK iosb;
  IO_STATUS_BLOCK* iosb_ptr;
  HANDLE event = NULL;
  void* apc_context;
  NTSTATUS status;
  DWORD error;

  if (overlapped != NULL) {
    /* Overlapped operation. */
    iosb_ptr = (IO_STATUS_BLOCK*) &overlapped->Internal;
    event = overlapped->hEvent;

    /* Do not report iocp completion if hEvent is tagged. */
    if ((uintptr_t) event & 1) {
      event = (HANDLE)((uintptr_t) event & ~(uintptr_t) 1);
      apc_context = NULL;
    } else {
      apc_context = overlapped;
    }

  } else {
    /* Blocking operation. */
    iosb_ptr = &iosb;
    event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (event == NULL) {
      return SOCKET_ERROR;
    }
    apc_context = NULL;
  }

  iosb_ptr->Status = STATUS_PENDING;
  status = pNtDeviceIoControlFile((HANDLE) socket,
                                  event,
                                  NULL,
                                  apc_context,
                                  iosb_ptr,
                                  IOCTL_AFD_POLL,
                                  info_in,
                                  sizeof *info_in,
                                  info_out,
                                  sizeof *info_out);

  if (overlapped == NULL) {
    /* If this is a blocking operation, wait for the event to become signaled,
     * and then grab the real status from the io status block. */
    if (status == STATUS_PENDING) {
      DWORD r = WaitForSingleObject(event, INFINITE);

      if (r == WAIT_FAILED) {
        DWORD saved_error = GetLastError();
        CloseHandle(event);
        WSASetLastError(saved_error);
        return SOCKET_ERROR;
      }

      status = iosb.Status;
    }

    CloseHandle(event);
  }

  switch (status) {
    case STATUS_SUCCESS:
      error = ERROR_SUCCESS;
      break;

    case STATUS_PENDING:
      error = WSA_IO_PENDING;
      break;

    default:
      error = uv_ntstatus_to_winsock_error(status);
      break;
  }

  WSASetLastError(error);

  if (error == ERROR_SUCCESS) {
    return 0;
  } else {
    return SOCKET_ERROR;
  }
}

int uv__convert_to_localhost_if_unspecified(const struct sockaddr* addr,
                                            struct sockaddr_storage* storage) {
  struct sockaddr_in* dest4;
  struct sockaddr_in6* dest6;

  if (addr == NULL)
    return UV_EINVAL;

  switch (addr->sa_family) {
  case AF_INET:
    dest4 = (struct sockaddr_in*) storage;
    memcpy(dest4, addr, sizeof(*dest4));
    if (dest4->sin_addr.s_addr == 0)
      dest4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return 0;
  case AF_INET6:
    dest6 = (struct sockaddr_in6*) storage;
    memcpy(dest6, addr, sizeof(*dest6));
    if (memcmp(&dest6->sin6_addr,
               &uv_addr_ip6_any_.sin6_addr,
               sizeof(uv_addr_ip6_any_.sin6_addr)) == 0) {
      struct in6_addr init_sin6_addr = IN6ADDR_LOOPBACK_INIT;
      dest6->sin6_addr = init_sin6_addr;
    }
    return 0;
  default:
    return UV_EINVAL;
  }
}
