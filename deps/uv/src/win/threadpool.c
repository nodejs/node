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
#include "req-inl.h"


static void uv_work_req_init(uv_loop_t* loop, uv_work_t* req,
    uv_work_cb work_cb, uv_after_work_cb after_work_cb) {
  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_WORK;
  req->loop = loop;
  req->work_cb = work_cb;
  req->after_work_cb = after_work_cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));
}


static DWORD WINAPI uv_work_thread_proc(void* parameter) {
  uv_work_t* req = (uv_work_t*)parameter;
  uv_loop_t* loop = req->loop;

  assert(req != NULL);
  assert(req->type == UV_WORK);
  assert(req->work_cb);

  req->work_cb(req);

  POST_COMPLETION_FOR_REQ(loop, req);

  return 0;
}


int uv_queue_work(uv_loop_t* loop, uv_work_t* req, uv_work_cb work_cb,
    uv_after_work_cb after_work_cb) {
  if (work_cb == NULL)
    return UV_EINVAL;

  uv_work_req_init(loop, req, work_cb, after_work_cb);

  if (!QueueUserWorkItem(&uv_work_thread_proc, req, WT_EXECUTELONGFUNCTION)) {
    return uv_translate_sys_error(GetLastError());
  }

  uv__req_register(loop, req);
  return 0;
}


int uv_cancel(uv_req_t* req) {
  return UV_ENOSYS;
}


void uv_process_work_req(uv_loop_t* loop, uv_work_t* req) {
  uv__req_unregister(loop, req);
  if(req->after_work_cb)
    req->after_work_cb(req, 0);
}
