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
#include <stdlib.h>

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"


uv_handle_type uv_guess_handle(uv_file file) {
  HANDLE handle;
  DWORD mode;

  if (file < 0) {
    return UV_UNKNOWN_HANDLE;
  }

  handle = uv__get_osfhandle(file);

  switch (GetFileType(handle)) {
    case FILE_TYPE_CHAR:
      if (GetConsoleMode(handle, &mode)) {
        return UV_TTY;
      } else {
        return UV_FILE;
      }

    case FILE_TYPE_PIPE:
      return UV_NAMED_PIPE;

    case FILE_TYPE_DISK:
      return UV_FILE;

    default:
      return UV_UNKNOWN_HANDLE;
  }
}


int uv_is_active(const uv_handle_t* handle) {
  return (handle->flags & UV_HANDLE_ACTIVE) &&
        !(handle->flags & UV_HANDLE_CLOSING);
}


void uv_close(uv_handle_t* handle, uv_close_cb cb) {
  uv_loop_t* loop = handle->loop;

  if (handle->flags & UV_HANDLE_CLOSING) {
    assert(0);
    return;
  }

  handle->close_cb = cb;

  /* Handle-specific close actions */
  switch (handle->type) {
    case UV_TCP:
      uv_tcp_close(loop, (uv_tcp_t*)handle);
      return;

    case UV_NAMED_PIPE:
      uv_pipe_close(loop, (uv_pipe_t*) handle);
      return;

    case UV_TTY:
      uv_tty_close((uv_tty_t*) handle);
      return;

    case UV_UDP:
      uv_udp_close(loop, (uv_udp_t*) handle);
      return;

    case UV_POLL:
      uv_poll_close(loop, (uv_poll_t*) handle);
      return;

    case UV_TIMER:
      uv_timer_stop((uv_timer_t*)handle);
      uv__handle_closing(handle);
      uv_want_endgame(loop, handle);
      return;

    case UV_PREPARE:
      uv_prepare_stop((uv_prepare_t*)handle);
      uv__handle_closing(handle);
      uv_want_endgame(loop, handle);
      return;

    case UV_CHECK:
      uv_check_stop((uv_check_t*)handle);
      uv__handle_closing(handle);
      uv_want_endgame(loop, handle);
      return;

    case UV_IDLE:
      uv_idle_stop((uv_idle_t*)handle);
      uv__handle_closing(handle);
      uv_want_endgame(loop, handle);
      return;

    case UV_ASYNC:
      uv_async_close(loop, (uv_async_t*) handle);
      return;

    case UV_SIGNAL:
      uv_signal_close(loop, (uv_signal_t*) handle);
      return;

    case UV_PROCESS:
      uv_process_close(loop, (uv_process_t*) handle);
      return;

    case UV_FS_EVENT:
      uv_fs_event_close(loop, (uv_fs_event_t*) handle);
      return;

    case UV_FS_POLL:
      uv__fs_poll_close((uv_fs_poll_t*) handle);
      uv__handle_closing(handle);
      uv_want_endgame(loop, handle);
      return;

    default:
      /* Not supported */
      abort();
  }
}


int uv_is_closing(const uv_handle_t* handle) {
  return !!(handle->flags & (UV_HANDLE_CLOSING | UV_HANDLE_CLOSED));
}


uv_os_fd_t uv_get_osfhandle(int fd) {
  return uv__get_osfhandle(fd);
}
