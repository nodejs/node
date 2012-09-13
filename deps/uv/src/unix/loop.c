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

#include "uv.h"
#include "tree.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int uv__loop_init(uv_loop_t* loop, int default_loop) {
  unsigned int i;
  int flags;

#if HAVE_KQUEUE
  flags = EVBACKEND_KQUEUE;
#else
  flags = EVFLAG_AUTO;
#endif

  memset(loop, 0, sizeof(*loop));

  RB_INIT(&loop->timer_handles);
  ngx_queue_init(&loop->active_reqs);
  ngx_queue_init(&loop->idle_handles);
  ngx_queue_init(&loop->async_handles);
  ngx_queue_init(&loop->check_handles);
  ngx_queue_init(&loop->prepare_handles);
  ngx_queue_init(&loop->handle_queue);
  loop->closing_handles = NULL;
  loop->signal_ctx = NULL;
  loop->time = uv_hrtime() / 1000000;
  loop->async_pipefd[0] = -1;
  loop->async_pipefd[1] = -1;
  loop->emfile_fd = -1;
  loop->ev = (default_loop ? ev_default_loop : ev_loop_new)(flags);
  ev_set_userdata(loop->ev, loop);
  eio_channel_init(&loop->uv_eio_channel, loop);

  uv_signal_init(loop, &loop->child_watcher);
  uv__handle_unref(&loop->child_watcher);
  loop->child_watcher.flags |= UV__HANDLE_INTERNAL;

  for (i = 0; i < ARRAY_SIZE(loop->process_handles); i++)
    ngx_queue_init(loop->process_handles + i);

  if (uv__platform_loop_init(loop, default_loop))
    return -1;

  return 0;
}


void uv__loop_delete(uv_loop_t* loop) {
  uv__platform_loop_delete(loop);
  uv__signal_unregister(loop);
  ev_loop_destroy(loop->ev);

  if (loop->emfile_fd != -1) {
    close(loop->emfile_fd);
    loop->emfile_fd = -1;
  }
}
