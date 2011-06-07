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

/*
 * TODO: Add explanation of why we want on_close to be called from fresh
 * stack.
 */

#include "../uv.h"
#include "task.h"


static const char MESSAGE[] = "Failure is for the weak. Everyone dies alone.";

static uv_tcp_t client;
static uv_timer_t timer;
static uv_req_t connect_req, write_req, shutdown_req;

static int nested = 0;
static int close_cb_called = 0;
static int connect_cb_called = 0;
static int write_cb_called = 0;
static int timer_cb_called = 0;
static int bytes_received = 0;
static int shutdown_cb_called = 0;


static uv_buf_t alloc_cb(uv_tcp_t* tcp, size_t size) {
  uv_buf_t buf;
  buf.len = size;
  buf.base = (char*) malloc(size);
  ASSERT(buf.base);
  return buf;
}


static void close_cb(uv_handle_t* handle, int status) {
  ASSERT(status == 0);
  ASSERT(nested == 0 && "close_cb must be called from a fresh stack");

  close_cb_called++;
}


static void shutdown_cb(uv_req_t* req, int status) {
  ASSERT(status == 0);
  ASSERT(nested == 0 && "shutdown_cb must be called from a fresh stack");

  shutdown_cb_called++;
}


static void read_cb(uv_tcp_t* tcp, int nread, uv_buf_t buf) {
  ASSERT(nested == 0 && "read_cb must be called from a fresh stack");

  printf("Read. nread == %d\n", nread);
  free(buf.base);

  if (nread == 0) {
    ASSERT(uv_last_error().code == UV_EAGAIN);
    return;

  } else if (nread == -1) {
    ASSERT(uv_last_error().code == UV_EOF);

    nested++;
    if (uv_close((uv_handle_t*)tcp)) {
      FATAL("uv_close failed");
    }
    nested--;

    return;
  }

  bytes_received += nread;

  /* We call shutdown here because when bytes_received == sizeof MESSAGE */
  /* there will be no more data sent nor received, so here it would be */
  /* possible for a backend to to call shutdown_cb immediately and *not* */
  /* from a fresh stack. */
  if (bytes_received == sizeof MESSAGE) {
    nested++;
    uv_req_init(&shutdown_req, (uv_handle_t*)tcp, shutdown_cb);

    puts("Shutdown");

    if (uv_shutdown(&shutdown_req)) {
      FATAL("uv_shutdown failed");
    }
    nested--;
  }
}


static void timer_cb(uv_handle_t* handle, int status) {
  int r;

  ASSERT(handle == (uv_handle_t*)&timer);
  ASSERT(status == 0);
  ASSERT(nested == 0 && "timer_cb must be called from a fresh stack");

  puts("Timeout complete. Now read data...");

  nested++;
  if (uv_read_start(&client, alloc_cb, read_cb)) {
    FATAL("uv_read_start failed");
  }
  nested--;

  timer_cb_called++;

  r = uv_close(handle);
  ASSERT(r == 0);
}


static void write_cb(uv_req_t* req, int status) {
  int r;

  ASSERT(status == 0);
  ASSERT(nested == 0 && "write_cb must be called from a fresh stack");

  puts("Data written. 500ms timeout...");

  /* After the data has been sent, we're going to wait for a while, then */
  /* start reading. This makes us certain that the message has been echoed */
  /* back to our receive buffer when we start reading. This maximizes the */
  /* tempation for the backend to use dirty stack for calling read_cb. */
  nested++;
  r = uv_timer_init(&timer, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_timer_start(&timer, timer_cb, 500, 0);
  ASSERT(r == 0);
  nested--;

  write_cb_called++;
}


static void connect_cb(uv_req_t* req, int status) {
  uv_buf_t buf;

  puts("Connected. Write some data to echo server...");

  ASSERT(status == 0);
  ASSERT(nested == 0 && "connect_cb must be called from a fresh stack");

  nested++;

  buf.base = (char*) &MESSAGE;
  buf.len = sizeof MESSAGE;

  uv_req_init(&write_req, req->handle, write_cb);

  if (uv_write(&write_req, &buf, 1)) {
    FATAL("uv_write failed");
  }

  nested--;

  connect_cb_called++;
}


TEST_IMPL(callback_stack) {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  uv_init();

  if (uv_tcp_init(&client, &close_cb, NULL)) {
    FATAL("uv_tcp_init failed");
  }

  puts("Connecting...");

  nested++;
  uv_req_init(&connect_req, (uv_handle_t*)&client, connect_cb);
  if (uv_connect(&connect_req, addr)) {
    FATAL("uv_connect failed");
  }
  nested--;

  uv_run();

  ASSERT(nested == 0);
  ASSERT(connect_cb_called == 1 && "connect_cb must be called exactly once");
  ASSERT(write_cb_called == 1 && "write_cb must be called exactly once");
  ASSERT(timer_cb_called == 1 && "timer_cb must be called exactly once");
  ASSERT(bytes_received == sizeof MESSAGE);
  ASSERT(shutdown_cb_called == 1 && "shutdown_cb must be called exactly once");
  ASSERT(close_cb_called == 2 && "close_cb must be called exactly twice");

  return 0;
}
