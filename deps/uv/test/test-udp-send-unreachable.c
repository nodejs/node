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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_HANDLE(handle) \
  ASSERT((uv_udp_t*)(handle) == &client)

static uv_udp_t client;
static uv_timer_t timer;

static int send_cb_called;
static int recv_cb_called;
static int close_cb_called;
static int alloc_cb_called;
static int timer_cb_called;


static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  ASSERT(suggested_size <= sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
  alloc_cb_called++;
}


static void close_cb(uv_handle_t* handle) {
  ASSERT(1 == uv_is_closing(handle));
  close_cb_called++;
}


static void send_cb(uv_udp_send_t* req, int status) {
  ASSERT(req != NULL);
  ASSERT(status == 0);
  CHECK_HANDLE(req->handle);
  send_cb_called++;
}


static void recv_cb(uv_udp_t* handle,
                       ssize_t nread,
                       const uv_buf_t* rcvbuf,
                       const struct sockaddr* addr,
                       unsigned flags) {
  CHECK_HANDLE(handle);
  recv_cb_called++;

  if (nread < 0) {
    ASSERT(0 && "unexpected error");
  } else if (nread == 0) {
    /* Returning unused buffer */
    ASSERT(addr == NULL);
  } else {
    ASSERT(addr != NULL);
  }
}


static void timer_cb(uv_timer_t* h) {
  ASSERT(h == &timer);
  timer_cb_called++;
  uv_close((uv_handle_t*) &client, close_cb);
  uv_close((uv_handle_t*) h, close_cb);
}


TEST_IMPL(udp_send_unreachable) {
  struct sockaddr_in addr;
  struct sockaddr_in addr2;
  uv_udp_send_t req1, req2;
  uv_buf_t buf;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT_2, &addr2));

  r = uv_timer_init( uv_default_loop(), &timer );
  ASSERT(r == 0);

  r = uv_timer_start( &timer, timer_cb, 1000, 0 );
  ASSERT(r == 0);

  r = uv_udp_init(uv_default_loop(), &client);
  ASSERT(r == 0);

  r = uv_udp_bind(&client, (const struct sockaddr*) &addr2, 0);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&client, alloc_cb, recv_cb);
  ASSERT(r == 0);

  /* client sends "PING", then "PANG" */
  buf = uv_buf_init("PING", 4);

  r = uv_udp_send(&req1,
                  &client,
                  &buf,
                  1,
                  (const struct sockaddr*) &addr,
                  send_cb);
  ASSERT(r == 0);

  buf = uv_buf_init("PANG", 4);

  r = uv_udp_send(&req2,
                  &client,
                  &buf,
                  1,
                  (const struct sockaddr*) &addr,
                  send_cb);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(send_cb_called == 2);
  ASSERT(recv_cb_called == alloc_cb_called);
  ASSERT(timer_cb_called == 1);
  ASSERT(close_cb_called == 2);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
