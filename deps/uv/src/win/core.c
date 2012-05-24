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
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "uv.h"
#include "internal.h"


/* The only event loop we support right now */
static uv_loop_t uv_default_loop_;

/* uv_once intialization guards */
static uv_once_t uv_init_guard_ = UV_ONCE_INIT;
static uv_once_t uv_default_loop_init_guard_ = UV_ONCE_INIT;


static void uv_init(void) {
  /* Tell Windows that we will handle critical errors. */
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
    SEM_NOOPENFILEERRORBOX);

  /* Initialize winsock */
  uv_winsock_init();

  /* Fetch winapi function pointers */
  uv_winapi_init();

  /* Initialize FS */
  uv_fs_init();

  /* Initialize console */
  uv_console_init();
}


static void uv_loop_init(uv_loop_t* loop) {
  /* Create an I/O completion port */
  loop->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
  if (loop->iocp == NULL) {
    uv_fatal_error(GetLastError(), "CreateIoCompletionPort");
  }

  uv_update_time(loop);

#ifndef UV_LEAN_AND_MEAN
  ngx_queue_init(&loop->active_handles);
  ngx_queue_init(&loop->active_reqs);
#else
  loop->active_handles = 0;
  loop->active_reqs = 0;
#endif

  loop->pending_reqs_tail = NULL;

  loop->endgame_handles = NULL;

  RB_INIT(&loop->timers);

  loop->check_handles = NULL;
  loop->prepare_handles = NULL;
  loop->idle_handles = NULL;

  loop->next_prepare_handle = NULL;
  loop->next_check_handle = NULL;
  loop->next_idle_handle = NULL;

  memset(&loop->poll_peer_sockets, 0, sizeof loop->poll_peer_sockets);

  loop->channel = NULL;
  RB_INIT(&loop->ares_handles);

  loop->active_tcp_streams = 0;
  loop->active_udp_streams = 0;

  loop->last_err = uv_ok_;

  memset(&loop->counters, 0, sizeof loop->counters);
}


static void uv_default_loop_init(void) {
  /* Initialize libuv itself first */
  uv_once(&uv_init_guard_, uv_init);

  /* Initialize the main loop */
  uv_loop_init(&uv_default_loop_);
}


uv_loop_t* uv_default_loop(void) {
  uv_once(&uv_default_loop_init_guard_, uv_default_loop_init);
  return &uv_default_loop_;
}


uv_loop_t* uv_loop_new(void) {
  uv_loop_t* loop;

  /* Initialize libuv itself first */
  uv_once(&uv_init_guard_, uv_init);

  loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));

  if (!loop) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  uv_loop_init(loop);
  return loop;
}


void uv_loop_delete(uv_loop_t* loop) {
  if (loop != &uv_default_loop_) {
    int i;
    for (i = 0; i < ARRAY_SIZE(loop->poll_peer_sockets); i++) {
      SOCKET sock = loop->poll_peer_sockets[i];
      if (sock != 0 && sock != INVALID_SOCKET) {
        closesocket(sock);
      }
    }

    free(loop);
  }
}


static void uv_poll(uv_loop_t* loop, int block) {
  BOOL success;
  DWORD bytes, timeout;
  ULONG_PTR key;
  OVERLAPPED* overlapped;
  uv_req_t* req;

  if (block) {
    timeout = uv_get_poll_timeout(loop);
  } else {
    timeout = 0;
  }

  success = GetQueuedCompletionStatus(loop->iocp,
                                      &bytes,
                                      &key,
                                      &overlapped,
                                      timeout);

  if (overlapped) {
    /* Package was dequeued */
    req = uv_overlapped_to_req(overlapped);

    uv_insert_pending_req(loop, req);

  } else if (GetLastError() != WAIT_TIMEOUT) {
    /* Serious error */
    uv_fatal_error(GetLastError(), "GetQueuedCompletionStatus");
  }
}


static void uv_poll_ex(uv_loop_t* loop, int block) {
  BOOL success;
  DWORD timeout;
  uv_req_t* req;
  OVERLAPPED_ENTRY overlappeds[128];
  ULONG count;
  ULONG i;

  if (block) {
    timeout = uv_get_poll_timeout(loop);
  } else {
    timeout = 0;
  }

  assert(pGetQueuedCompletionStatusEx);

  success = pGetQueuedCompletionStatusEx(loop->iocp,
                                         overlappeds,
                                         ARRAY_SIZE(overlappeds),
                                         &count,
                                         timeout,
                                         FALSE);
  if (success) {
    for (i = 0; i < count; i++) {
      /* Package was dequeued */
      req = uv_overlapped_to_req(overlappeds[i].lpOverlapped);
      uv_insert_pending_req(loop, req);
    }
  } else if (GetLastError() != WAIT_TIMEOUT) {
    /* Serious error */
    uv_fatal_error(GetLastError(), "GetQueuedCompletionStatusEx");
  }
}

#ifndef UV_LEAN_AND_MEAN
# define UV_LOOP_ALIVE(loop)                                                  \
      (!ngx_queue_empty(&(loop)->active_handles) ||                           \
       !ngx_queue_empty(&(loop)->active_reqs) ||                              \
       (loop)->endgame_handles != NULL)
#else
# define UV_LOOP_ALIVE(loop)                                                  \
      ((loop)->active_handles > 0 &&                                          \
       (loop)->active_reqs > 0 &&                                             \
       (loop)->endgame_handles != NULL)
#endif


#define UV_LOOP_ONCE(loop, poll)                                              \
  do {                                                                        \
    uv_update_time((loop));                                                   \
    uv_process_timers((loop));                                                \
                                                                              \
    /* Call idle callbacks if nothing to do. */                               \
    if ((loop)->pending_reqs_tail == NULL &&                                  \
        (loop)->endgame_handles == NULL) {                                    \
      uv_idle_invoke((loop));                                                 \
    }                                                                         \
                                                                              \
    uv_process_reqs((loop));                                                  \
    uv_process_endgames((loop));                                              \
                                                                              \
    if (!UV_LOOP_ALIVE((loop))) {                                             \
      break;                                                                  \
    }                                                                         \
                                                                              \
    uv_prepare_invoke((loop));                                                \
                                                                              \
    poll((loop), (loop)->idle_handles == NULL &&                              \
                 (loop)->pending_reqs_tail == NULL &&                         \
                 (loop)->endgame_handles == NULL &&                           \
                 UV_LOOP_ALIVE((loop)));                                      \
                                                                              \
    uv_check_invoke((loop));                                                  \
  } while (0);

#define UV_LOOP(loop, poll)                                                   \
  while (UV_LOOP_ALIVE((loop))) {                                             \
    UV_LOOP_ONCE(loop, poll)                                                  \
  }


int uv_run_once(uv_loop_t* loop) {
  if (pGetQueuedCompletionStatusEx) {
    UV_LOOP_ONCE(loop, uv_poll_ex);
  } else {
    UV_LOOP_ONCE(loop, uv_poll);
  }
  return UV_LOOP_ALIVE(loop);
}


int uv_run(uv_loop_t* loop) {
  if (pGetQueuedCompletionStatusEx) {
    UV_LOOP(loop, uv_poll_ex);
  } else {
    UV_LOOP(loop, uv_poll);
  }

  assert(!UV_LOOP_ALIVE((loop)));
  return 0;
}
