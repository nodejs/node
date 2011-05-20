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

#include "../uv.h"
#include "task.h"

#include <stdlib.h>
#include <stdio.h>


static uv_handle_t handle;
static uv_req_t req;
static int connect_cb_calls;
static int close_cb_calls;


static void on_close(uv_handle_t* handle, int status) {
  ASSERT(status == 0);
  close_cb_calls++;
}


static void on_connect(uv_req_t *req, int status) {
  ASSERT(status == -1);
  ASSERT(uv_last_error().code == UV_ECONNREFUSED);
  connect_cb_calls++;
  uv_close(req->handle);
}


static uv_buf alloc_cb(uv_handle_t* handle, size_t size) {
  uv_buf buf = {0, 0};
  FATAL("alloc should not be called");
  return buf;
}


TEST_IMPL(connection_fail) {
  struct sockaddr_in client_addr, server_addr;
  int r;

  uv_init(alloc_cb);

  client_addr = uv_ip4_addr("0.0.0.0", 0);

  /* There should be no servers listening on this port. */
  server_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  /* Try to connec to the server and do NUM_PINGS ping-pongs. */
  r = uv_tcp_init(&handle, on_close, NULL);
  ASSERT(!r);

  /* We are never doing multiple reads/connects at a time anyway. */
  /* so these handles can be pre-initialized. */
  uv_req_init(&req, &handle, on_connect);

  uv_bind(&handle, (struct sockaddr*)&client_addr);
  r = uv_connect(&req, (struct sockaddr*)&server_addr);
  ASSERT(!r);

  uv_run();

  ASSERT(connect_cb_calls == 1);
  ASSERT(close_cb_calls == 1);

  return 0;
}
