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
#include "uv-common.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


struct uv_ares_task_s {
  UV_HANDLE_FIELDS
  ares_socket_t sock;
  uv_poll_t poll_watcher;
  RB_ENTRY(uv_ares_task_s) node;
};


static int cmp_ares_tasks(const uv_ares_task_t* a, const uv_ares_task_t* b) {
  if (a->sock < b->sock) return -1;
  if (a->sock > b->sock) return 1;
  return 0;
}


RB_GENERATE_STATIC(uv__ares_tasks, uv_ares_task_s, node, cmp_ares_tasks)


/* Add ares handle to list. */
static void uv_add_ares_handle(uv_loop_t* loop, uv_ares_task_t* handle) {
  assert(loop == handle->loop);
  RB_INSERT(uv__ares_tasks, &loop->ares_handles, handle);
}


/* Find matching ares handle in list. */
static uv_ares_task_t* uv_find_ares_handle(uv_loop_t* loop, ares_socket_t sock) {
  uv_ares_task_t handle;
  handle.sock = sock;
  return RB_FIND(uv__ares_tasks, &loop->ares_handles, &handle);
}


/* Remove ares handle from list. */
static void uv_remove_ares_handle(uv_ares_task_t* handle) {
  RB_REMOVE(uv__ares_tasks, &handle->loop->ares_handles, handle);
}


/* Returns 1 if the ares_handles list is empty, 0 otherwise. */
static int uv_ares_handles_empty(uv_loop_t* loop) {
  return RB_EMPTY(&loop->ares_handles);
}


/* This is called once per second by loop->timer. It is used to constantly */
/* call back into c-ares for possibly processing timeouts. */
static void uv__ares_timeout(uv_timer_t* handle, int status) {
  assert(!uv_ares_handles_empty(handle->loop));
  ares_process_fd(handle->loop->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}


static void uv__ares_poll_cb(uv_poll_t* watcher, int status, int events) {
  uv_loop_t* loop = watcher->loop;
  uv_ares_task_t* task = container_of(watcher, uv_ares_task_t, poll_watcher);

  /* Reset the idle timer */
  uv_timer_again(&loop->ares_timer);

  if (status < 0) {
    /* An error happened. Just pretend that the socket is both readable and */
    /* writable. */
    ares_process_fd(loop->channel, task->sock, task->sock);
    return;
  }

  /* Process DNS responses */
  ares_process_fd(loop->channel,
                  events & UV_READABLE ? task->sock : ARES_SOCKET_BAD,
                  events & UV_WRITABLE ? task->sock : ARES_SOCKET_BAD);
}


static void uv__ares_poll_close_cb(uv_handle_t* watcher) {
  uv_ares_task_t* task = container_of(watcher, uv_ares_task_t, poll_watcher);
  free(task);
}


/* Allocates and returns a new uv_ares_task_t */
static uv_ares_task_t* uv__ares_task_create(uv_loop_t* loop, ares_socket_t sock) {
  uv_ares_task_t* task = (uv_ares_task_t*) malloc(sizeof *task);

  if (task == NULL) {
    /* Out of memory. */
    return NULL;
  }

  task->loop = loop;
  task->sock = sock;

  if (uv_poll_init_socket(loop, &task->poll_watcher, sock) < 0) {
    /* This should never happen. */
    free(task);
    return NULL;
  }

  return task;
}


/* Callback from ares when socket operation is started */
static void uv__ares_sockstate_cb(void* data, ares_socket_t sock,
    int read, int write) {
  uv_loop_t* loop = (uv_loop_t*) data;
  uv_ares_task_t* task;

  task = uv_find_ares_handle(loop, sock);

  if (read || write) {
    if (!task) {
      /* New socket */

      /* If this is the first socket then start the timer. */
      if (!uv_is_active((uv_handle_t*) &loop->ares_timer)) {
        assert(uv_ares_handles_empty(loop));
        uv_timer_start(&loop->ares_timer, uv__ares_timeout, 1000, 1000);
      }

      task = uv__ares_task_create(loop, sock);
      if (task == NULL) {
        /* This should never happen unless we're out of memory or something */
        /* is seriously wrong. The socket won't be polled, but the the query */
        /* will eventually time out. */
        return;
      }

      uv_add_ares_handle(loop, task);
    }

    /* This should never fail. If it fails anyway, the query will eventually */
    /* time out. */
    uv_poll_start(&task->poll_watcher,
                  (read ? UV_READABLE : 0) | (write ? UV_WRITABLE : 0),
                  uv__ares_poll_cb);

  } else {
    /* read == 0 and write == 0 this is c-ares's way of notifying us that */
    /* the socket is now closed. We must free the data associated with */
    /* socket. */
    assert(task &&
           "When an ares socket is closed we should have a handle for it");

    uv_remove_ares_handle(task);
    uv_close((uv_handle_t*) &task->poll_watcher, uv__ares_poll_close_cb);

    if (uv_ares_handles_empty(loop)) {
      uv_timer_stop(&loop->ares_timer);
    }
  }
}


/* C-ares integration initialize and terminate */
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

  /* Initialize the timeout timer. The timer won't be started until the */
  /* first socket is opened. */
  uv_timer_init(loop, &loop->ares_timer);

  return rc;
}


void uv_ares_destroy(uv_loop_t* loop, ares_channel channel) {
  /* Only allow destroy if did init. */
  if (loop->channel) {
    uv_timer_stop(&loop->ares_timer);
    ares_destroy(channel);
    loop->channel = NULL;
  }
}
