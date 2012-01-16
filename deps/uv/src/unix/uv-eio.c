/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

/* This file integrates the libuv event loop with the libeio thread pool */

#include "uv.h"
#include "eio.h"

#include <assert.h>
#include <stdio.h>


static void uv_eio_do_poll(uv_idle_t* watcher, int status) {
  assert(watcher == &(watcher->loop->uv_eio_poller));

  /* printf("uv_eio_poller\n"); */

  if (eio_poll(&watcher->loop->uv_eio_channel) != -1 && uv_is_active((uv_handle_t*) watcher)) {
    /* printf("uv_eio_poller stop\n"); */
    uv_idle_stop(watcher);
    uv_unref(watcher->loop);
  }
}


/* Called from the main thread. */
static void uv_eio_want_poll_notifier_cb(uv_async_t* watcher, int status) {
  uv_loop_t* loop = watcher->loop;

  assert(watcher == &loop->uv_eio_want_poll_notifier);

  /* printf("want poll notifier\n"); */

  if (eio_poll(&watcher->loop->uv_eio_channel) == -1 && !uv_is_active((uv_handle_t*) &loop->uv_eio_poller)) {
    /* printf("uv_eio_poller start\n"); */
    uv_idle_start(&loop->uv_eio_poller, uv_eio_do_poll);
    uv_ref(loop);
  }
}


static void uv_eio_done_poll_notifier_cb(uv_async_t* watcher, int revents) {
  uv_loop_t* loop = watcher->loop;

  assert(watcher == &loop->uv_eio_done_poll_notifier);

  /* printf("done poll notifier\n"); */

  if (eio_poll(&watcher->loop->uv_eio_channel) != -1 && uv_is_active((uv_handle_t*) &loop->uv_eio_poller)) {
    /* printf("uv_eio_poller stop\n"); */
    uv_idle_stop(&loop->uv_eio_poller);
    uv_unref(loop);
  }
}


/*
 * uv_eio_want_poll() is called from the EIO thread pool each time an EIO
 * request (that is, one of the node.fs.* functions) has completed.
 */
static void uv_eio_want_poll(eio_channel *channel) {
  /* Signal the main thread that eio_poll need to be processed. */

  /*
   * TODO need to select the correct uv_loop_t and async_send to
   * uv_eio_want_poll_notifier.
   */

  uv_async_send(&((uv_loop_t *)channel->data)->uv_eio_want_poll_notifier);
}


static void uv_eio_done_poll(eio_channel *channel) {
  /*
   * Signal the main thread that we should stop calling eio_poll().
   * from the idle watcher.
   */
  uv_async_send(&((uv_loop_t *)channel->data)->uv_eio_done_poll_notifier);
}


static void uv__eio_init(void) {
  eio_init(uv_eio_want_poll, uv_eio_done_poll);
}

static uv_once_t uv__eio_init_once_guard = UV_ONCE_INIT;


void uv_eio_init(uv_loop_t* loop) {
  if (loop->counters.eio_init == 0) {
    loop->counters.eio_init++;

    uv_idle_init(loop, &loop->uv_eio_poller);
    uv_idle_start(&loop->uv_eio_poller, uv_eio_do_poll);

    loop->uv_eio_want_poll_notifier.data = loop;
    uv_async_init(loop, &loop->uv_eio_want_poll_notifier,
        uv_eio_want_poll_notifier_cb);
    uv_unref(loop);

    uv_async_init(loop, &loop->uv_eio_done_poll_notifier,
        uv_eio_done_poll_notifier_cb);
    uv_unref(loop);

    uv_once(&uv__eio_init_once_guard, uv__eio_init);
  }
}
