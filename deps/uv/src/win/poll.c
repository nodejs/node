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
#include <io.h>

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "req-inl.h"


static const GUID uv_msafd_provider_ids[UV_MSAFD_PROVIDER_COUNT] = {
  {0xe70f1aa0, 0xab8b, 0x11cf,
      {0x8c, 0xa3, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}},
  {0xf9eab0c0, 0x26d4, 0x11d0,
      {0xbb, 0xbf, 0x00, 0xaa, 0x00, 0x6c, 0x34, 0xe4}},
  {0x9fc48064, 0x7298, 0x43e4,
      {0xb7, 0xbd, 0x18, 0x1f, 0x20, 0x89, 0x79, 0x2a}},
  {0xa00943d9, 0x9c2e, 0x4633,
      {0x9b, 0x59, 0x00, 0x57, 0xa3, 0x16, 0x09, 0x94}}
};

typedef struct uv_single_fd_set_s {
  unsigned int fd_count;
  SOCKET fd_array[1];
} uv_single_fd_set_t;


static OVERLAPPED overlapped_dummy_;
static uv_once_t overlapped_dummy_init_guard_ = UV_ONCE_INIT;

static AFD_POLL_INFO afd_poll_info_dummy_;


static void uv__init_overlapped_dummy(void) {
  HANDLE event;

  event = CreateEvent(NULL, TRUE, TRUE, NULL);
  if (event == NULL)
    uv_fatal_error(GetLastError(), "CreateEvent");

  memset(&overlapped_dummy_, 0, sizeof overlapped_dummy_);
  overlapped_dummy_.hEvent = (HANDLE) ((uintptr_t) event | 1);
}


static OVERLAPPED* uv__get_overlapped_dummy(void) {
  uv_once(&overlapped_dummy_init_guard_, uv__init_overlapped_dummy);
  return &overlapped_dummy_;
}


static AFD_POLL_INFO* uv__get_afd_poll_info_dummy(void) {
  return &afd_poll_info_dummy_;
}


static void uv__fast_poll_submit_poll_req(uv_loop_t* loop, uv_poll_t* handle) {
  uv_req_t* req;
  AFD_POLL_INFO* afd_poll_info;
  int result;

  /* Find a yet unsubmitted req to submit. */
  if (handle->submitted_events_1 == 0) {
    req = &handle->poll_req_1;
    afd_poll_info = &handle->afd_poll_info_1;
    handle->submitted_events_1 = handle->events;
    handle->mask_events_1 = 0;
    handle->mask_events_2 = handle->events;
  } else if (handle->submitted_events_2 == 0) {
    req = &handle->poll_req_2;
    afd_poll_info = &handle->afd_poll_info_2;
    handle->submitted_events_2 = handle->events;
    handle->mask_events_1 = handle->events;
    handle->mask_events_2 = 0;
  } else {
    /* Just wait until there's an unsubmitted req. This will happen almost
     * immediately as one of the 2 outstanding requests is about to return.
     * When this happens, uv__fast_poll_process_poll_req will be called, and
     * the pending events, if needed, will be processed in a subsequent
     * request. */
    return;
  }

  /* Setting Exclusive to TRUE makes the other poll request return if there is
   * any. */
  afd_poll_info->Exclusive = TRUE;
  afd_poll_info->NumberOfHandles = 1;
  afd_poll_info->Timeout.QuadPart = INT64_MAX;
  afd_poll_info->Handles[0].Handle = (HANDLE) handle->socket;
  afd_poll_info->Handles[0].Status = 0;
  afd_poll_info->Handles[0].Events = 0;

  if (handle->events & UV_READABLE) {
    afd_poll_info->Handles[0].Events |= AFD_POLL_RECEIVE |
        AFD_POLL_DISCONNECT | AFD_POLL_ACCEPT | AFD_POLL_ABORT;
  } else {
    if (handle->events & UV_DISCONNECT) {
      afd_poll_info->Handles[0].Events |= AFD_POLL_DISCONNECT;
    }
  }
  if (handle->events & UV_WRITABLE) {
    afd_poll_info->Handles[0].Events |= AFD_POLL_SEND | AFD_POLL_CONNECT_FAIL;
  }

  memset(&req->u.io.overlapped, 0, sizeof req->u.io.overlapped);

  result = uv__msafd_poll((SOCKET) handle->peer_socket,
                          afd_poll_info,
                          afd_poll_info,
                          &req->u.io.overlapped);
  if (result != 0 && WSAGetLastError() != WSA_IO_PENDING) {
    /* Queue this req, reporting an error. */
    SET_REQ_ERROR(req, WSAGetLastError());
    uv__insert_pending_req(loop, req);
  }
}


static void uv__fast_poll_process_poll_req(uv_loop_t* loop, uv_poll_t* handle,
    uv_req_t* req) {
  unsigned char mask_events;
  AFD_POLL_INFO* afd_poll_info;

  if (req == &handle->poll_req_1) {
    afd_poll_info = &handle->afd_poll_info_1;
    handle->submitted_events_1 = 0;
    mask_events = handle->mask_events_1;
  } else if (req == &handle->poll_req_2) {
    afd_poll_info = &handle->afd_poll_info_2;
    handle->submitted_events_2 = 0;
    mask_events = handle->mask_events_2;
  } else {
    assert(0);
    return;
  }

  /* Report an error unless the select was just interrupted. */
  if (!REQ_SUCCESS(req)) {
    DWORD error = GET_REQ_SOCK_ERROR(req);
    if (error != WSAEINTR && handle->events != 0) {
      handle->events = 0; /* Stop the watcher */
      handle->poll_cb(handle, uv_translate_sys_error(error), 0);
    }

  } else if (afd_poll_info->NumberOfHandles >= 1) {
    unsigned char events = 0;

    if ((afd_poll_info->Handles[0].Events & (AFD_POLL_RECEIVE |
        AFD_POLL_DISCONNECT | AFD_POLL_ACCEPT | AFD_POLL_ABORT)) != 0) {
      events |= UV_READABLE;
      if ((afd_poll_info->Handles[0].Events & AFD_POLL_DISCONNECT) != 0) {
        events |= UV_DISCONNECT;
      }
    }
    if ((afd_poll_info->Handles[0].Events & (AFD_POLL_SEND |
        AFD_POLL_CONNECT_FAIL)) != 0) {
      events |= UV_WRITABLE;
    }

    events &= handle->events & ~mask_events;

    if (afd_poll_info->Handles[0].Events & AFD_POLL_LOCAL_CLOSE) {
      /* Stop polling. */
      handle->events = 0;
      if (uv__is_active(handle))
        uv__handle_stop(handle);
    }

    if (events != 0) {
      handle->poll_cb(handle, 0, events);
    }
  }

  if ((handle->events & ~(handle->submitted_events_1 |
      handle->submitted_events_2)) != 0) {
    uv__fast_poll_submit_poll_req(loop, handle);
  } else if ((handle->flags & UV_HANDLE_CLOSING) &&
             handle->submitted_events_1 == 0 &&
             handle->submitted_events_2 == 0) {
    uv__want_endgame(loop, (uv_handle_t*) handle);
  }
}


static SOCKET uv__fast_poll_create_peer_socket(HANDLE iocp,
    WSAPROTOCOL_INFOW* protocol_info) {
  SOCKET sock = 0;

  sock = WSASocketW(protocol_info->iAddressFamily,
                    protocol_info->iSocketType,
                    protocol_info->iProtocol,
                    protocol_info,
                    0,
                    WSA_FLAG_OVERLAPPED);
  if (sock == INVALID_SOCKET) {
    return INVALID_SOCKET;
  }

  if (!SetHandleInformation((HANDLE) sock, HANDLE_FLAG_INHERIT, 0)) {
    goto error;
  };

  if (CreateIoCompletionPort((HANDLE) sock,
                             iocp,
                             (ULONG_PTR) sock,
                             0) == NULL) {
    goto error;
  }

  return sock;

 error:
  closesocket(sock);
  return INVALID_SOCKET;
}


static SOCKET uv__fast_poll_get_peer_socket(uv_loop_t* loop,
    WSAPROTOCOL_INFOW* protocol_info) {
  int index, i;
  SOCKET peer_socket;

  index = -1;
  for (i = 0; (size_t) i < ARRAY_SIZE(uv_msafd_provider_ids); i++) {
    if (memcmp((void*) &protocol_info->ProviderId,
               (void*) &uv_msafd_provider_ids[i],
               sizeof protocol_info->ProviderId) == 0) {
      index = i;
    }
  }

  /* Check if the protocol uses an msafd socket. */
  if (index < 0) {
    return INVALID_SOCKET;
  }

  /* If we didn't (try) to create a peer socket yet, try to make one. Don't try
   * again if the peer socket creation failed earlier for the same protocol. */
  peer_socket = loop->poll_peer_sockets[index];
  if (peer_socket == 0) {
    peer_socket = uv__fast_poll_create_peer_socket(loop->iocp, protocol_info);
    loop->poll_peer_sockets[index] = peer_socket;
  }

  return peer_socket;
}


static DWORD WINAPI uv__slow_poll_thread_proc(void* arg) {
  uv_req_t* req = (uv_req_t*) arg;
  uv_poll_t* handle = (uv_poll_t*) req->data;
  unsigned char reported_events;
  int r;
  uv_single_fd_set_t rfds, wfds, efds;
  struct timeval timeout;

  assert(handle->type == UV_POLL);
  assert(req->type == UV_POLL_REQ);

  if (handle->events & UV_READABLE) {
    rfds.fd_count = 1;
    rfds.fd_array[0] = handle->socket;
  } else {
    rfds.fd_count = 0;
  }

  if (handle->events & UV_WRITABLE) {
    wfds.fd_count = 1;
    wfds.fd_array[0] = handle->socket;
    efds.fd_count = 1;
    efds.fd_array[0] = handle->socket;
  } else {
    wfds.fd_count = 0;
    efds.fd_count = 0;
  }

  /* Make the select() time out after 3 minutes. If select() hangs because the
   * user closed the socket, we will at least not hang indefinitely. */
  timeout.tv_sec = 3 * 60;
  timeout.tv_usec = 0;

  r = select(1, (fd_set*) &rfds, (fd_set*) &wfds, (fd_set*) &efds, &timeout);
  if (r == SOCKET_ERROR) {
    /* Queue this req, reporting an error. */
    SET_REQ_ERROR(&handle->poll_req_1, WSAGetLastError());
    POST_COMPLETION_FOR_REQ(handle->loop, req);
    return 0;
  }

  reported_events = 0;

  if (r > 0) {
    if (rfds.fd_count > 0) {
      assert(rfds.fd_count == 1);
      assert(rfds.fd_array[0] == handle->socket);
      reported_events |= UV_READABLE;
    }

    if (wfds.fd_count > 0) {
      assert(wfds.fd_count == 1);
      assert(wfds.fd_array[0] == handle->socket);
      reported_events |= UV_WRITABLE;
    } else if (efds.fd_count > 0) {
      assert(efds.fd_count == 1);
      assert(efds.fd_array[0] == handle->socket);
      reported_events |= UV_WRITABLE;
    }
  }

  SET_REQ_SUCCESS(req);
  req->u.io.overlapped.InternalHigh = (DWORD) reported_events;
  POST_COMPLETION_FOR_REQ(handle->loop, req);

  return 0;
}


static void uv__slow_poll_submit_poll_req(uv_loop_t* loop, uv_poll_t* handle) {
  uv_req_t* req;

  /* Find a yet unsubmitted req to submit. */
  if (handle->submitted_events_1 == 0) {
    req = &handle->poll_req_1;
    handle->submitted_events_1 = handle->events;
    handle->mask_events_1 = 0;
    handle->mask_events_2 = handle->events;
  } else if (handle->submitted_events_2 == 0) {
    req = &handle->poll_req_2;
    handle->submitted_events_2 = handle->events;
    handle->mask_events_1 = handle->events;
    handle->mask_events_2 = 0;
  } else {
    assert(0);
    return;
  }

  if (!QueueUserWorkItem(uv__slow_poll_thread_proc,
                         (void*) req,
                         WT_EXECUTELONGFUNCTION)) {
    /* Make this req pending, reporting an error. */
    SET_REQ_ERROR(req, GetLastError());
    uv__insert_pending_req(loop, req);
  }
}



static void uv__slow_poll_process_poll_req(uv_loop_t* loop, uv_poll_t* handle,
    uv_req_t* req) {
  unsigned char mask_events;
  int err;

  if (req == &handle->poll_req_1) {
    handle->submitted_events_1 = 0;
    mask_events = handle->mask_events_1;
  } else if (req == &handle->poll_req_2) {
    handle->submitted_events_2 = 0;
    mask_events = handle->mask_events_2;
  } else {
    assert(0);
    return;
  }

  if (!REQ_SUCCESS(req)) {
    /* Error. */
    if (handle->events != 0) {
      err = GET_REQ_ERROR(req);
      handle->events = 0; /* Stop the watcher */
      handle->poll_cb(handle, uv_translate_sys_error(err), 0);
    }
  } else {
    /* Got some events. */
    int events = req->u.io.overlapped.InternalHigh & handle->events & ~mask_events;
    if (events != 0) {
      handle->poll_cb(handle, 0, events);
    }
  }

  if ((handle->events & ~(handle->submitted_events_1 |
      handle->submitted_events_2)) != 0) {
    uv__slow_poll_submit_poll_req(loop, handle);
  } else if ((handle->flags & UV_HANDLE_CLOSING) &&
             handle->submitted_events_1 == 0 &&
             handle->submitted_events_2 == 0) {
    uv__want_endgame(loop, (uv_handle_t*) handle);
  }
}


int uv_poll_init(uv_loop_t* loop, uv_poll_t* handle, int fd) {
  return uv_poll_init_socket(loop, handle, (SOCKET) uv__get_osfhandle(fd));
}


int uv_poll_init_socket(uv_loop_t* loop, uv_poll_t* handle,
    uv_os_sock_t socket) {
  WSAPROTOCOL_INFOW protocol_info;
  int len;
  SOCKET peer_socket, base_socket;
  DWORD bytes;
  DWORD yes = 1;

  /* Set the socket to nonblocking mode */
  if (ioctlsocket(socket, FIONBIO, &yes) == SOCKET_ERROR)
    return uv_translate_sys_error(WSAGetLastError());

/* Try to obtain a base handle for the socket. This increases this chances that
 * we find an AFD handle and are able to use the fast poll mechanism. This will
 * always fail on windows XP/2k3, since they don't support the. SIO_BASE_HANDLE
 * ioctl. */
#ifndef NDEBUG
  base_socket = INVALID_SOCKET;
#endif

  if (WSAIoctl(socket,
               SIO_BASE_HANDLE,
               NULL,
               0,
               &base_socket,
               sizeof base_socket,
               &bytes,
               NULL,
               NULL) == 0) {
    assert(base_socket != 0 && base_socket != INVALID_SOCKET);
    socket = base_socket;
  }

  uv__handle_init(loop, (uv_handle_t*) handle, UV_POLL);
  handle->socket = socket;
  handle->events = 0;

  /* Obtain protocol information about the socket. */
  len = sizeof protocol_info;
  if (getsockopt(socket,
                 SOL_SOCKET,
                 SO_PROTOCOL_INFOW,
                 (char*) &protocol_info,
                 &len) != 0) {
    return uv_translate_sys_error(WSAGetLastError());
  }

  /* Get the peer socket that is needed to enable fast poll. If the returned
   * value is NULL, the protocol is not implemented by MSAFD and we'll have to
   * use slow mode. */
  peer_socket = uv__fast_poll_get_peer_socket(loop, &protocol_info);

  if (peer_socket != INVALID_SOCKET) {
    /* Initialize fast poll specific fields. */
    handle->peer_socket = peer_socket;
  } else {
    /* Initialize slow poll specific fields. */
    handle->flags |= UV_HANDLE_POLL_SLOW;
  }

  /* Initialize 2 poll reqs. */
  handle->submitted_events_1 = 0;
  UV_REQ_INIT(&handle->poll_req_1, UV_POLL_REQ);
  handle->poll_req_1.data = handle;

  handle->submitted_events_2 = 0;
  UV_REQ_INIT(&handle->poll_req_2, UV_POLL_REQ);
  handle->poll_req_2.data = handle;

  return 0;
}


static int uv__poll_set(uv_poll_t* handle, int events, uv_poll_cb cb) {
  int submitted_events;

  assert(handle->type == UV_POLL);
  assert(!(handle->flags & UV_HANDLE_CLOSING));
  assert((events & ~(UV_READABLE | UV_WRITABLE | UV_DISCONNECT |
                     UV_PRIORITIZED)) == 0);

  handle->events = events;
  handle->poll_cb = cb;

  if (handle->events == 0) {
    uv__handle_stop(handle);
    return 0;
  }

  uv__handle_start(handle);
  submitted_events = handle->submitted_events_1 | handle->submitted_events_2;

  if (handle->events & ~submitted_events) {
    if (handle->flags & UV_HANDLE_POLL_SLOW) {
      uv__slow_poll_submit_poll_req(handle->loop, handle);
    } else {
      uv__fast_poll_submit_poll_req(handle->loop, handle);
    }
  }

  return 0;
}


int uv_poll_start(uv_poll_t* handle, int events, uv_poll_cb cb) {
  return uv__poll_set(handle, events, cb);
}


int uv_poll_stop(uv_poll_t* handle) {
  return uv__poll_set(handle, 0, handle->poll_cb);
}


void uv__process_poll_req(uv_loop_t* loop, uv_poll_t* handle, uv_req_t* req) {
  if (!(handle->flags & UV_HANDLE_POLL_SLOW)) {
    uv__fast_poll_process_poll_req(loop, handle, req);
  } else {
    uv__slow_poll_process_poll_req(loop, handle, req);
  }
}


int uv__poll_close(uv_loop_t* loop, uv_poll_t* handle) {
  AFD_POLL_INFO afd_poll_info;
  DWORD error;
  int result;

  handle->events = 0;
  uv__handle_closing(handle);

  if (handle->submitted_events_1 == 0 &&
      handle->submitted_events_2 == 0) {
    uv__want_endgame(loop, (uv_handle_t*) handle);
    return 0;
  }

  if (handle->flags & UV_HANDLE_POLL_SLOW)
    return 0;

  /* Cancel outstanding poll requests by executing another, unique poll
   * request that forces the outstanding ones to return. */
  afd_poll_info.Exclusive = TRUE;
  afd_poll_info.NumberOfHandles = 1;
  afd_poll_info.Timeout.QuadPart = INT64_MAX;
  afd_poll_info.Handles[0].Handle = (HANDLE) handle->socket;
  afd_poll_info.Handles[0].Status = 0;
  afd_poll_info.Handles[0].Events = AFD_POLL_ALL;

  result = uv__msafd_poll(handle->socket,
                          &afd_poll_info,
                          uv__get_afd_poll_info_dummy(),
                          uv__get_overlapped_dummy());

  if (result == SOCKET_ERROR) {
    error = WSAGetLastError();
    if (error != WSA_IO_PENDING)
      return uv_translate_sys_error(error);
  }

  return 0;
}


void uv__poll_endgame(uv_loop_t* loop, uv_poll_t* handle) {
  assert(handle->flags & UV_HANDLE_CLOSING);
  assert(!(handle->flags & UV_HANDLE_CLOSED));

  assert(handle->submitted_events_1 == 0);
  assert(handle->submitted_events_2 == 0);

  uv__handle_close(handle);
}
