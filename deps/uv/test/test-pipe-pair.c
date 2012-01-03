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
#include "task.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define PING "PING"

static uv_pipe_t a;
static uv_pipe_t b;
static uv_write_t req;
static uv_buf_t buf;
static enum {
  STATE_MAIN_START,
  STATE_THREAD_START,
  STATE_MAIN_CLOSE,
  STATE_THREAD_CLOSE,
} state;


static void pinger_read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  ASSERT(state == STATE_MAIN_CLOSE);
  state = STATE_THREAD_CLOSE;

  ASSERT((uv_pipe_t*)stream == &b);

  ASSERT(nread < 0);
  ASSERT(uv_last_error(stream->loop).code == UV_EOF);

  free(buf.base);

  uv_close((uv_handle_t*)stream, NULL);
}


static void main_thread_read_cb(uv_stream_t* stream, ssize_t nread,
    uv_buf_t buf) {
  ASSERT(state == STATE_THREAD_START);
  state = STATE_MAIN_CLOSE;

  ASSERT((uv_pipe_t*)stream == &a);
  ASSERT(stream->loop == uv_default_loop());

  if (nread > 0) {
    ASSERT(strcmp(buf.base, PING) == 0);
    uv_close((uv_handle_t*)stream, NULL);
  }

  free(buf.base);
}


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t size) {
  uv_buf_t buf;
  buf.base = (char*)malloc(size);
  buf.len = size;
  return buf;
}


void start(void* data) {
  uv_loop_t* loop;
  int r;

  ASSERT(state == STATE_MAIN_START);
  state = STATE_THREAD_START;

  loop = data;

  buf = uv_buf_init(PING, strlen(PING));

  if (uv_write(&req, (uv_stream_t*)&b, &buf, 1, NULL)) {
    FATAL("uv_write failed");
  }

  uv_read_start((uv_stream_t*)&b, alloc_cb, pinger_read_cb);

  uv_run(loop);

  ASSERT(state == STATE_THREAD_CLOSE);
}


TEST_IMPL(pipe_pair) {
  int r;
  uv_err_t err;
  uv_thread_t tid;
  uv_loop_t* loop;

  state = STATE_MAIN_START;

  r = uv_pipe_init(uv_default_loop(), &a, 1);
  ASSERT(r == 0);

  loop = uv_loop_new();
  ASSERT(loop);

  r = uv_pipe_init(loop, &b, 1);
  ASSERT(r == 0);

  err = uv_pipe_pair(&a, &b);
  ASSERT(err.code == UV_OK);

  r = uv_thread_create(&tid, start, loop);
  ASSERT(r == 0);

  uv_read_start((uv_stream_t*)&a, alloc_cb, main_thread_read_cb);

  uv_run(uv_default_loop());
  uv_thread_join(&tid);

  ASSERT(state == STATE_THREAD_CLOSE);

  return 0;
}
