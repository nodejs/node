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

#include "uv.h"
#include "internal.h"
#include "atomicops-inl.h"
#include "handle-inl.h"
#include "req-inl.h"


void uv_async_endgame(uv_loop_t* loop, uv_async_t* handle) {
  if (handle->flags & UV__HANDLE_CLOSING &&
      !handle->async_sent) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    uv__handle_close(handle);
  }
}


int uv_async_init(uv_loop_t* loop, uv_async_t* handle, uv_async_cb async_cb) {
  uv_req_t* req;

  uv__handle_init(loop, (uv_handle_t*) handle, UV_ASYNC);
  handle->async_sent = 0;
  handle->async_cb = async_cb;

  req = &handle->async_req;
  uv_req_init(loop, req);
  req->type = UV_WAKEUP;
  req->data = handle;

  uv__handle_start(handle);

  return 0;
}


void uv_async_close(uv_loop_t* loop, uv_async_t* handle) {
  if (!((uv_async_t*)handle)->async_sent) {
    uv_want_endgame(loop, (uv_handle_t*) handle);
  }

  uv__handle_closing(handle);
}


int uv_async_send(uv_async_t* handle) {
  uv_loop_t* loop = handle->loop;

  if (handle->type != UV_ASYNC) {
    /* Can't set errno because that's not thread-safe. */
    return -1;
  }

  /* The user should make sure never to call uv_async_send to a closing */
  /* or closed handle. */
  assert(!(handle->flags & UV__HANDLE_CLOSING));

  if (!uv__atomic_exchange_set(&handle->async_sent)) {
    POST_COMPLETION_FOR_REQ(loop, &handle->async_req);
  }

  return 0;
}


void uv_process_async_wakeup_req(uv_loop_t* loop, uv_async_t* handle,
    uv_req_t* req) {
  assert(handle->type == UV_ASYNC);
  assert(req->type == UV_WAKEUP);

  handle->async_sent = 0;

  if (!(handle->flags & UV__HANDLE_CLOSING)) {
    handle->async_cb((uv_async_t*) handle, 0);
  } else {
    uv_want_endgame(loop, (uv_handle_t*)handle);
  }
}
