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
#include "internal.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>


/*
 * This is called once per second by loop->timer. It is used to
 * constantly callback into c-ares for possibly processing timeouts.
 */
static void uv__ares_timeout(struct ev_loop* ev, struct ev_timer* watcher,
    int revents) {
  uv_loop_t* loop = ev_userdata(ev);

  assert(ev == loop->ev);
  assert((uv_loop_t*)watcher->data == loop);
  assert(watcher == &loop->timer);
  assert(revents == EV_TIMER);
  assert(!uv_ares_handles_empty(loop));

  ares_process_fd(loop->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}


static void uv__ares_io(struct ev_loop* ev, struct ev_io* watcher,
    int revents) {
  uv_loop_t* loop = ev_userdata(ev);

  assert(ev == loop->ev);

  /* Reset the idle timer */
  ev_timer_again(ev, &loop->timer);

  /* Process DNS responses */
  ares_process_fd(loop->channel,
      revents & EV_READ ? watcher->fd : ARES_SOCKET_BAD,
      revents & EV_WRITE ? watcher->fd : ARES_SOCKET_BAD);
}


/* Allocates and returns a new uv_ares_task_t */
static uv_ares_task_t* uv__ares_task_create(int fd) {
  uv_ares_task_t* h = malloc(sizeof(uv_ares_task_t));

  if (h == NULL) {
    uv_fatal_error(ENOMEM, "malloc");
    return NULL;
  }

  h->sock = fd;

  ev_io_init(&h->read_watcher, uv__ares_io, fd, EV_READ);
  ev_io_init(&h->write_watcher, uv__ares_io, fd, EV_WRITE);

  h->read_watcher.data = h;
  h->write_watcher.data = h;

  return h;
}


/* Callback from ares when socket operation is started */
static void uv__ares_sockstate_cb(void* data, ares_socket_t sock,
    int read, int write) {
  uv_loop_t* loop = data;
  uv_ares_task_t* h;

  assert((uv_loop_t*)loop->timer.data == loop);

  h = uv_find_ares_handle(loop, sock);

  if (read || write) {
    if (!h) {
      /* New socket */

      /* If this is the first socket then start the timer. */
      if (!ev_is_active(&loop->timer)) {
        assert(uv_ares_handles_empty(loop));
        ev_timer_again(loop->ev, &loop->timer);
      }

      h = uv__ares_task_create(sock);
      uv_add_ares_handle(loop, h);
    }

    if (read) {
      ev_io_start(loop->ev, &h->read_watcher);
    } else {
      ev_io_stop(loop->ev, &h->read_watcher);
    }

    if (write) {
      ev_io_start(loop->ev, &h->write_watcher);
    } else {
      ev_io_stop(loop->ev, &h->write_watcher);
    }

  } else {
    /*
     * read == 0 and write == 0 this is c-ares's way of notifying us that
     * the socket is now closed. We must free the data associated with
     * socket.
     */
    assert(h && "When an ares socket is closed we should have a handle for it");

    ev_io_stop(loop->ev, &h->read_watcher);
    ev_io_stop(loop->ev, &h->write_watcher);

    uv_remove_ares_handle(h);
    free(h);

    if (uv_ares_handles_empty(loop)) {
      ev_timer_stop(loop->ev, &loop->timer);
    }
  }
}


/* c-ares integration initialize and terminate */
/* TODO: share this with windows? */
int uv_ares_init_options(uv_loop_t* loop, ares_channel *channelptr,
    struct ares_options *options, int optmask) {
  int rc;

  /* only allow single init at a time */
  if (loop->channel != NULL) {
    uv__set_artificial_error(loop, UV_EALREADY);
    return -1;
  }

  /* set our callback as an option */
  options->sock_state_cb = uv__ares_sockstate_cb;
  options->sock_state_cb_data = loop;
  optmask |= ARES_OPT_SOCK_STATE_CB;

  /* We do the call to ares_init_option for caller. */
  rc = ares_init_options(channelptr, options, optmask);

  /* if success, save channel */
  if (rc == ARES_SUCCESS) {
    loop->channel = *channelptr;
  } 

  /*
   * Initialize the timeout timer. The timer won't be started until the
   * first socket is opened.
   */
  ev_timer_init(&loop->timer, uv__ares_timeout, 1., 1.);
  loop->timer.data = loop;

  return rc;
}


/* TODO share this with windows? */
void uv_ares_destroy(uv_loop_t* loop, ares_channel channel) {
  /* only allow destroy if did init */
  if (loop->channel) {
    ev_timer_stop(loop->ev, &loop->timer);
    ares_destroy(channel);
    loop->channel = NULL;
  }
}
