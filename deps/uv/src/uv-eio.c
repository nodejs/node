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


static uv_async_t uv_eio_want_poll_notifier;
static uv_async_t uv_eio_done_poll_notifier;
static uv_idle_t uv_eio_poller;
static int uv_eio_init_count;


static void uv_eio_do_poll(uv_idle_t* watcher, int status) {
  assert(watcher == &uv_eio_poller);

  /* printf("uv_eio_poller\n"); */

  if (eio_poll() != -1 && uv_is_active((uv_handle_t*) &uv_eio_poller)) {
    /* printf("uv_eio_poller stop\n"); */
    uv_idle_stop(&uv_eio_poller);
    uv_unref();
  }
}


/* Called from the main thread. */
static void uv_eio_want_poll_notifier_cb(uv_async_t* watcher, int status) {
  assert(watcher == &uv_eio_want_poll_notifier);

  /* printf("want poll notifier\n"); */

  if (eio_poll() == -1 && !uv_is_active((uv_handle_t*) &uv_eio_poller)) {
    /* printf("uv_eio_poller start\n"); */
    uv_idle_start(&uv_eio_poller, uv_eio_do_poll);
    uv_ref();
  }
}


static void uv_eio_done_poll_notifier_cb(uv_async_t* watcher, int revents) {
  assert(watcher == &uv_eio_done_poll_notifier);

  /* printf("done poll notifier\n"); */

  if (eio_poll() != -1 && uv_is_active((uv_handle_t*) &uv_eio_poller)) {
    /* printf("uv_eio_poller stop\n"); */
    uv_idle_stop(&uv_eio_poller);
    uv_unref();
  }
}


/*
 * uv_eio_want_poll() is called from the EIO thread pool each time an EIO
 * request (that is, one of the node.fs.* functions) has completed.
 */
static void uv_eio_want_poll(void) {
  /* Signal the main thread that eio_poll need to be processed. */
  uv_async_send(&uv_eio_want_poll_notifier);
}


static void uv_eio_done_poll(void) {
  /*
   * Signal the main thread that we should stop calling eio_poll().
   * from the idle watcher.
   */
  uv_async_send(&uv_eio_done_poll_notifier);
}


void uv_eio_init() {
  if (uv_eio_init_count == 0) {
    uv_eio_init_count++;

    uv_idle_init(&uv_eio_poller);
    uv_idle_start(&uv_eio_poller, uv_eio_do_poll);

    uv_async_init(&uv_eio_want_poll_notifier, uv_eio_want_poll_notifier_cb);
    uv_unref();

    uv_async_init(&uv_eio_done_poll_notifier, uv_eio_done_poll_notifier_cb);
    uv_unref();

    eio_init(uv_eio_want_poll, uv_eio_done_poll);
    /*
     * Don't handle more than 10 reqs on each eio_poll(). This is to avoid
     * race conditions. See Node's test/simple/test-eio-race.js
     */
    eio_set_max_poll_reqs(10);
  }
}
