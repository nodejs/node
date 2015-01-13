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
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER) || defined(__MINGW64_VERSION_MAJOR)
#include <crtdbg.h>
#endif

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "req-inl.h"


static uv_loop_t default_loop_struct;
static uv_loop_t* default_loop_ptr;

/* uv_once initialization guards */
static uv_once_t uv_init_guard_ = UV_ONCE_INIT;


#if defined(_DEBUG) && (defined(_MSC_VER) || defined(__MINGW64_VERSION_MAJOR))
/* Our crt debug report handler allows us to temporarily disable asserts
 * just for the current thread.
 */

UV_THREAD_LOCAL int uv__crt_assert_enabled = TRUE;

static int uv__crt_dbg_report_handler(int report_type, char *message, int *ret_val) {
  if (uv__crt_assert_enabled || report_type != _CRT_ASSERT)
    return FALSE;

  if (ret_val) {
    /* Set ret_val to 0 to continue with normal execution.
     * Set ret_val to 1 to trigger a breakpoint.
    */

    if(IsDebuggerPresent())
      *ret_val = 1;
    else
      *ret_val = 0;
  }

  /* Don't call _CrtDbgReport. */
  return TRUE;
}
#else
UV_THREAD_LOCAL int uv__crt_assert_enabled = FALSE;
#endif


#if !defined(__MINGW32__) || __MSVCRT_VERSION__ >= 0x800
static void uv__crt_invalid_parameter_handler(const wchar_t* expression,
    const wchar_t* function, const wchar_t * file, unsigned int line,
    uintptr_t reserved) {
  /* No-op. */
}
#endif


static void uv_init(void) {
  /* Tell Windows that we will handle critical errors. */
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
               SEM_NOOPENFILEERRORBOX);

  /* Tell the CRT to not exit the application when an invalid parameter is
   * passed. The main issue is that invalid FDs will trigger this behavior.
   */
#if !defined(__MINGW32__) || __MSVCRT_VERSION__ >= 0x800
  _set_invalid_parameter_handler(uv__crt_invalid_parameter_handler);
#endif

  /* We also need to setup our debug report handler because some CRT
   * functions (eg _get_osfhandle) raise an assert when called with invalid
   * FDs even though they return the proper error code in the release build.
   */
#if defined(_DEBUG) && (defined(_MSC_VER) || defined(__MINGW64_VERSION_MAJOR))
  _CrtSetReportHook(uv__crt_dbg_report_handler);
#endif

  /* Fetch winapi function pointers. This must be done first because other
   * initialization code might need these function pointers to be loaded.
   */
  uv_winapi_init();

  /* Initialize winsock */
  uv_winsock_init();

  /* Initialize FS */
  uv_fs_init();

  /* Initialize signal stuff */
  uv_signals_init();

  /* Initialize console */
  uv_console_init();

  /* Initialize utilities */
  uv__util_init();
}


int uv_loop_init(uv_loop_t* loop) {
  /* Initialize libuv itself first */
  uv__once_init();

  /* Create an I/O completion port */
  loop->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
  if (loop->iocp == NULL)
    return uv_translate_sys_error(GetLastError());

  /* To prevent uninitialized memory access, loop->time must be initialized
   * to zero before calling uv_update_time for the first time.
   */
  loop->time = 0;
  uv_update_time(loop);

  QUEUE_INIT(&loop->wq);
  QUEUE_INIT(&loop->handle_queue);
  QUEUE_INIT(&loop->active_reqs);
  loop->active_handles = 0;

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

  loop->active_tcp_streams = 0;
  loop->active_udp_streams = 0;

  loop->timer_counter = 0;
  loop->stop_flag = 0;

  if (uv_mutex_init(&loop->wq_mutex))
    abort();

  if (uv_async_init(loop, &loop->wq_async, uv__work_done))
    abort();

  uv__handle_unref(&loop->wq_async);
  loop->wq_async.flags |= UV__HANDLE_INTERNAL;

  return 0;
}


void uv__once_init(void) {
  uv_once(&uv_init_guard_, uv_init);
}


uv_loop_t* uv_default_loop(void) {
  if (default_loop_ptr != NULL)
    return default_loop_ptr;

  if (uv_loop_init(&default_loop_struct))
    return NULL;

  default_loop_ptr = &default_loop_struct;
  return default_loop_ptr;
}


static void uv__loop_close(uv_loop_t* loop) {
  size_t i;

  /* close the async handle without needing an extra loop iteration */
  assert(!loop->wq_async.async_sent);
  loop->wq_async.close_cb = NULL;
  uv__handle_closing(&loop->wq_async);
  uv__handle_close(&loop->wq_async);

  for (i = 0; i < ARRAY_SIZE(loop->poll_peer_sockets); i++) {
    SOCKET sock = loop->poll_peer_sockets[i];
    if (sock != 0 && sock != INVALID_SOCKET)
      closesocket(sock);
  }

  uv_mutex_lock(&loop->wq_mutex);
  assert(QUEUE_EMPTY(&loop->wq) && "thread pool work queue not empty!");
  assert(!uv__has_active_reqs(loop));
  uv_mutex_unlock(&loop->wq_mutex);
  uv_mutex_destroy(&loop->wq_mutex);

  CloseHandle(loop->iocp);
}


int uv_loop_close(uv_loop_t* loop) {
  QUEUE* q;
  uv_handle_t* h;
  if (!QUEUE_EMPTY(&(loop)->active_reqs))
    return UV_EBUSY;
  QUEUE_FOREACH(q, &loop->handle_queue) {
    h = QUEUE_DATA(q, uv_handle_t, handle_queue);
    if (!(h->flags & UV__HANDLE_INTERNAL))
      return UV_EBUSY;
  }

  uv__loop_close(loop);

#ifndef NDEBUG
  memset(loop, -1, sizeof(*loop));
#endif
  if (loop == default_loop_ptr)
    default_loop_ptr = NULL;

  return 0;
}


uv_loop_t* uv_loop_new(void) {
  uv_loop_t* loop;

  loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));
  if (loop == NULL) {
    return NULL;
  }

  if (uv_loop_init(loop)) {
    free(loop);
    return NULL;
  }

  return loop;
}


void uv_loop_delete(uv_loop_t* loop) {
  uv_loop_t* default_loop;
  int err;
  default_loop = default_loop_ptr;
  err = uv_loop_close(loop);
  assert(err == 0);
  if (loop != default_loop)
    free(loop);
}


int uv__loop_configure(uv_loop_t* loop, uv_loop_option option, va_list ap) {
  return UV_ENOSYS;
}


int uv_backend_fd(const uv_loop_t* loop) {
  return -1;
}


int uv_backend_timeout(const uv_loop_t* loop) {
  if (loop->stop_flag != 0)
    return 0;

  if (!uv__has_active_handles(loop) && !uv__has_active_reqs(loop))
    return 0;

  if (loop->pending_reqs_tail)
    return 0;

  if (loop->endgame_handles)
    return 0;

  if (loop->idle_handles)
    return 0;

  return uv__next_timeout(loop);
}


static void uv_poll(uv_loop_t* loop, DWORD timeout) {
  DWORD bytes;
  ULONG_PTR key;
  OVERLAPPED* overlapped;
  uv_req_t* req;

  GetQueuedCompletionStatus(loop->iocp,
                            &bytes,
                            &key,
                            &overlapped,
                            timeout);

  if (overlapped) {
    /* Package was dequeued */
    req = uv_overlapped_to_req(overlapped);
    uv_insert_pending_req(loop, req);

    /* Some time might have passed waiting for I/O,
     * so update the loop time here.
     */
    uv_update_time(loop);
  } else if (GetLastError() != WAIT_TIMEOUT) {
    /* Serious error */
    uv_fatal_error(GetLastError(), "GetQueuedCompletionStatus");
  } else if (timeout > 0) {
    /* GetQueuedCompletionStatus can occasionally return a little early.
     * Make sure that the desired timeout is reflected in the loop time.
     */
    uv__time_forward(loop, timeout);
  }
}


static void uv_poll_ex(uv_loop_t* loop, DWORD timeout) {
  BOOL success;
  uv_req_t* req;
  OVERLAPPED_ENTRY overlappeds[128];
  ULONG count;
  ULONG i;

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

    /* Some time might have passed waiting for I/O,
     * so update the loop time here.
     */
    uv_update_time(loop);
  } else if (GetLastError() != WAIT_TIMEOUT) {
    /* Serious error */
    uv_fatal_error(GetLastError(), "GetQueuedCompletionStatusEx");
  } else if (timeout > 0) {
    /* GetQueuedCompletionStatus can occasionally return a little early.
     * Make sure that the desired timeout is reflected in the loop time.
     */
    uv__time_forward(loop, timeout);
  }
}


static int uv__loop_alive(const uv_loop_t* loop) {
  return loop->active_handles > 0 ||
         !QUEUE_EMPTY(&loop->active_reqs) ||
         loop->endgame_handles != NULL;
}


int uv_loop_alive(const uv_loop_t* loop) {
    return uv__loop_alive(loop);
}


int uv_run(uv_loop_t *loop, uv_run_mode mode) {
  DWORD timeout;
  int r;
  void (*poll)(uv_loop_t* loop, DWORD timeout);

  if (pGetQueuedCompletionStatusEx)
    poll = &uv_poll_ex;
  else
    poll = &uv_poll;

  r = uv__loop_alive(loop);
  if (!r)
    uv_update_time(loop);

  while (r != 0 && loop->stop_flag == 0) {
    uv_update_time(loop);
    uv_process_timers(loop);

    uv_process_reqs(loop);
    uv_idle_invoke(loop);
    uv_prepare_invoke(loop);

    timeout = 0;
    if ((mode & UV_RUN_NOWAIT) == 0)
      timeout = uv_backend_timeout(loop);

    (*poll)(loop, timeout);

    uv_check_invoke(loop);
    uv_process_endgames(loop);

    if (mode == UV_RUN_ONCE) {
      /* UV_RUN_ONCE implies forward progress: at least one callback must have
       * been invoked when it returns. uv__io_poll() can return without doing
       * I/O (meaning: no callbacks) when its timeout expires - which means we
       * have pending timers that satisfy the forward progress constraint.
       *
       * UV_RUN_NOWAIT makes no guarantees about progress so it's omitted from
       * the check.
       */
      uv_process_timers(loop);
    }

    r = uv__loop_alive(loop);
    if (mode & (UV_RUN_ONCE | UV_RUN_NOWAIT))
      break;
  }

  /* The if statement lets the compiler compile it to a conditional store.
   * Avoids dirtying a cache line.
   */
  if (loop->stop_flag != 0)
    loop->stop_flag = 0;

  return r;
}


int uv_fileno(const uv_handle_t* handle, uv_os_fd_t* fd) {
  uv_os_fd_t fd_out;

  switch (handle->type) {
  case UV_TCP:
    fd_out = (uv_os_fd_t)((uv_tcp_t*) handle)->socket;
    break;

  case UV_NAMED_PIPE:
    fd_out = ((uv_pipe_t*) handle)->handle;
    break;

  case UV_TTY:
    fd_out = ((uv_tty_t*) handle)->handle;
    break;

  case UV_UDP:
    fd_out = (uv_os_fd_t)((uv_udp_t*) handle)->socket;
    break;

  case UV_POLL:
    fd_out = (uv_os_fd_t)((uv_poll_t*) handle)->socket;
    break;

  default:
    return UV_EINVAL;
  }

  if (uv_is_closing(handle) || fd_out == INVALID_HANDLE_VALUE)
    return UV_EBADF;

  *fd = fd_out;
  return 0;
}


int uv__socket_sockopt(uv_handle_t* handle, int optname, int* value) {
  int r;
  int len;
  SOCKET socket;

  if (handle == NULL || value == NULL)
    return UV_EINVAL;

  if (handle->type == UV_TCP)
    socket = ((uv_tcp_t*) handle)->socket;
  else if (handle->type == UV_UDP)
    socket = ((uv_udp_t*) handle)->socket;
  else
    return UV_ENOTSUP;

  len = sizeof(*value);

  if (*value == 0)
    r = getsockopt(socket, SOL_SOCKET, optname, (char*) value, &len);
  else
    r = setsockopt(socket, SOL_SOCKET, optname, (const char*) value, len);

  if (r == SOCKET_ERROR)
    return uv_translate_sys_error(WSAGetLastError());

  return 0;
}
