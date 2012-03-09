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


uv_handle_type uv_guess_handle(uv_file file) {
  HANDLE handle;
  DWORD mode;

  if (file < 0) {
    return UV_UNKNOWN_HANDLE;
  }

  handle = (HANDLE) _get_osfhandle(file);

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


int uv_is_active(uv_handle_t* handle) {
  switch (handle->type) {
    case UV_TIMER:
    case UV_IDLE:
    case UV_PREPARE:
    case UV_CHECK:
      return (handle->flags & UV_HANDLE_ACTIVE) ? 1 : 0;

    default:
      return 1;
  }
}


void uv_close(uv_handle_t* handle, uv_close_cb cb) {
  uv_tcp_t* tcp;
  uv_pipe_t* pipe;
  uv_udp_t* udp;
  uv_process_t* process;

  uv_loop_t* loop = handle->loop;

  if (handle->flags & UV_HANDLE_CLOSING) {
    return;
  }

  handle->flags |= UV_HANDLE_CLOSING;
  handle->close_cb = cb;

  /* Handle-specific close actions */
  switch (handle->type) {
    case UV_TCP:
      tcp = (uv_tcp_t*)handle;
      /* If we don't shutdown before calling closesocket, windows will */
      /* silently discard the kernel send buffer and reset the connection. */
      if ((tcp->flags & UV_HANDLE_CONNECTION) &&
          !(tcp->flags & UV_HANDLE_SHUT)) {
        shutdown(tcp->socket, SD_SEND);
        tcp->flags |= UV_HANDLE_SHUTTING | UV_HANDLE_SHUT;
      }
      tcp->flags &= ~(UV_HANDLE_READING | UV_HANDLE_LISTENING);
      closesocket(tcp->socket);
      if (tcp->reqs_pending == 0) {
        uv_want_endgame(loop, handle);
      }
      return;

    case UV_NAMED_PIPE:
      pipe = (uv_pipe_t*)handle;
      pipe->flags &= ~(UV_HANDLE_READING | UV_HANDLE_LISTENING);
      close_pipe(pipe, NULL, NULL);
      if (pipe->reqs_pending == 0) {
        uv_want_endgame(loop, handle);
      }
      return;

    case UV_TTY:
      uv_tty_close((uv_tty_t*) handle);
      return;

    case UV_UDP:
      udp = (uv_udp_t*) handle;
      uv_udp_recv_stop(udp);
      closesocket(udp->socket);
      if (udp->reqs_pending == 0) {
        uv_want_endgame(loop, handle);
      }
      return;

    case UV_TIMER:
      uv_timer_stop((uv_timer_t*)handle);
      uv_want_endgame(loop, handle);
      return;

    case UV_PREPARE:
      uv_prepare_stop((uv_prepare_t*)handle);
      uv_want_endgame(loop, handle);
      return;

    case UV_CHECK:
      uv_check_stop((uv_check_t*)handle);
      uv_want_endgame(loop, handle);
      return;

    case UV_IDLE:
      uv_idle_stop((uv_idle_t*)handle);
      uv_want_endgame(loop, handle);
      return;

    case UV_ASYNC:
      if (!((uv_async_t*)handle)->async_sent) {
        uv_want_endgame(loop, handle);
      }
      return;

    case UV_PROCESS:
      process = (uv_process_t*)handle;
      uv_process_close(loop, process);
      return;

    case UV_FS_EVENT:
      uv_fs_event_close(loop, (uv_fs_event_t*)handle);
      return;

    default:
      /* Not supported */
      abort();
  }
}


void uv_want_endgame(uv_loop_t* loop, uv_handle_t* handle) {
  if (!(handle->flags & UV_HANDLE_ENDGAME_QUEUED)) {
    handle->flags |= UV_HANDLE_ENDGAME_QUEUED;

    handle->endgame_next = loop->endgame_handles;
    loop->endgame_handles = handle;
  }
}


void uv_process_endgames(uv_loop_t* loop) {
  uv_handle_t* handle;

  while (loop->endgame_handles && loop->refs > 0) {
    handle = loop->endgame_handles;
    loop->endgame_handles = handle->endgame_next;

    handle->flags &= ~UV_HANDLE_ENDGAME_QUEUED;

    switch (handle->type) {
      case UV_TCP:
        uv_tcp_endgame(loop, (uv_tcp_t*) handle);
        break;

      case UV_NAMED_PIPE:
        uv_pipe_endgame(loop, (uv_pipe_t*) handle);
        break;

      case UV_TTY:
        uv_tty_endgame(loop, (uv_tty_t*) handle);
        break;

      case UV_UDP:
        uv_udp_endgame(loop, (uv_udp_t*) handle);
        break;

      case UV_TIMER:
        uv_timer_endgame(loop, (uv_timer_t*) handle);
        break;

      case UV_PREPARE:
      case UV_CHECK:
      case UV_IDLE:
        uv_loop_watcher_endgame(loop, handle);
        break;

      case UV_ASYNC:
        uv_async_endgame(loop, (uv_async_t*) handle);
        break;

      case UV_PROCESS:
        uv_process_endgame(loop, (uv_process_t*) handle);
        break;

      case UV_FS_EVENT:
        uv_fs_event_endgame(loop, (uv_fs_event_t*) handle);
        break;

      default:
        assert(0);
        break;
    }
  }
}
