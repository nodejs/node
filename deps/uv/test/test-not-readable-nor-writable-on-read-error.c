/* Copyright the libuv project contributors. All rights reserved.
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
#include "task.h"

static uv_loop_t loop;
static uv_tcp_t tcp_client;
static uv_connect_t connect_req;
static uv_write_t write_req;
static char reset_me_cmd[] = {'Q', 'S', 'H'};

static int connect_cb_called;
static int read_cb_called;
static int write_cb_called;
static int close_cb_called;

static void write_cb(uv_write_t* req, int status) {
  write_cb_called++;
  ASSERT(status == 0);
}

static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[64];
  buf->base = slab;
  buf->len = sizeof(slab);
}

static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}

static void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
  read_cb_called++;

  ASSERT((nread < 0) && (nread != UV_EOF));
  ASSERT(0 == uv_is_writable(handle));
  ASSERT(0 == uv_is_readable(handle));

  uv_close((uv_handle_t*) handle, close_cb);
}

static void connect_cb(uv_connect_t* req, int status) {
  int r;
  uv_buf_t reset_me;

  connect_cb_called++;
  ASSERT(status == 0);

  r = uv_read_start((uv_stream_t*) &tcp_client, alloc_cb, read_cb);
  ASSERT(r == 0);

  reset_me = uv_buf_init(reset_me_cmd, sizeof(reset_me_cmd));

  r = uv_write(&write_req,
               (uv_stream_t*) &tcp_client,
               &reset_me,
               1,
               write_cb);

  ASSERT(r == 0);
}

TEST_IMPL(not_readable_nor_writable_on_read_error) {
  struct sockaddr_in sa;
  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &sa));
  ASSERT(0 == uv_loop_init(&loop));
  ASSERT(0 == uv_tcp_init(&loop, &tcp_client));

  ASSERT(0 == uv_tcp_connect(&connect_req,
                             &tcp_client,
                             (const struct sockaddr*) &sa,
                             connect_cb));

  ASSERT(0 == uv_run(&loop, UV_RUN_DEFAULT));

  ASSERT(connect_cb_called == 1);
  ASSERT(read_cb_called == 1);
  ASSERT(write_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
