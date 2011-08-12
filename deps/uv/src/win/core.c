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
#include "../uv-common.h"
#include "internal.h"


/* The only event loop we support right now */
uv_loop_t uv_main_loop_;


static void uv_loop_init() {
  /* Create an I/O completion port */
  LOOP->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
  if (LOOP->iocp == NULL) {
    uv_fatal_error(GetLastError(), "CreateIoCompletionPort");
  }

  LOOP->refs = 0;

  uv_update_time();

  LOOP->pending_reqs_tail = NULL;

  LOOP->endgame_handles = NULL;

  RB_INIT(&LOOP->timers);

  LOOP->check_handles = NULL;
  LOOP->prepare_handles = NULL;
  LOOP->idle_handles = NULL;

  LOOP->next_prepare_handle = NULL;
  LOOP->next_check_handle = NULL;
  LOOP->next_idle_handle = NULL;

  LOOP->last_error = uv_ok_;

  LOOP->err_str = NULL;
}


void uv_init() {
  /* Initialize winsock */
  uv_winsock_startup();

  /* Fetch winapi function pointers */
  uv_winapi_init();

  /* Intialize event loop */
  uv_loop_init();
}


void uv_ref() {
  LOOP->refs++;
}


void uv_unref() {
  LOOP->refs--;
}


static void uv_poll(int block) {
  BOOL success;
  DWORD bytes, timeout;
  ULONG_PTR key;
  OVERLAPPED* overlapped;
  uv_req_t* req;

  if (block) {
    timeout = uv_get_poll_timeout();
  } else {
    timeout = 0;
  }

  success = GetQueuedCompletionStatus(LOOP->iocp,
                                      &bytes,
                                      &key,
                                      &overlapped,
                                      timeout);

  if (overlapped) {
    /* Package was dequeued */
    req = uv_overlapped_to_req(overlapped);

    if (!success) {
      req->error = uv_new_sys_error(GetLastError());
    }

    uv_insert_pending_req(req);

  } else if (GetLastError() != WAIT_TIMEOUT) {
    /* Serious error */
    uv_fatal_error(GetLastError(), "GetQueuedCompletionStatus");
  }
}


int uv_run() {
  while (LOOP->refs > 0) {
    uv_update_time();
    uv_process_timers();

    /* Call idle callbacks if nothing to do. */
    if (LOOP->pending_reqs_tail == NULL && LOOP->endgame_handles == NULL) {
      uv_idle_invoke();
    }

    /* Completely flush all pending reqs and endgames. */
    /* We do even when we just called the idle callbacks because those may */
    /* have closed handles or started requests that short-circuited. */
    while (LOOP->pending_reqs_tail || LOOP->endgame_handles) {
      uv_process_endgames();
      uv_process_reqs();
    }

    if (LOOP->refs <= 0) {
      break;
    }

    uv_prepare_invoke();

    uv_poll(LOOP->idle_handles == NULL && LOOP->refs > 0);

    uv_check_invoke();
  }

  assert(LOOP->refs == 0);
  return 0;
}
