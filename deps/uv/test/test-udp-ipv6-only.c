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
  ASSERT((uv_udp_t*)(handle) == &server1 \
      || (uv_udp_t*)(handle) == &server2 \
      || (uv_udp_t*)(handle) == &client)

#define CHECK_REQ(req) \
  ASSERT((req) == &req_);

static uv_udp_t client;
static uv_udp_t server1;
static uv_udp_t server2;
static uv_udp_send_t req_;

static int send_cb_called;
static int recv_cb_called;
static int close_cb_called;


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  return uv_buf_init(slab, sizeof slab);
}


static void close_cb(uv_handle_t* handle) {
  CHECK_HANDLE(handle);
  close_cb_called++;
}


static void send_cb(uv_udp_send_t* req, int status) {
  CHECK_REQ(req);
  CHECK_HANDLE(req->handle);
  ASSERT(status == 0);
  send_cb_called++;
}

static void ipv4_recv_cb(uv_udp_t* handle,
                         ssize_t nread,
                         uv_buf_t buf,
                         struct sockaddr* addr,
                         unsigned flags) {
  CHECK_HANDLE(handle);
  ASSERT(nread >= 0);

  uv_close((uv_handle_t*)&server1, close_cb);
  uv_close((uv_handle_t*)&server2, close_cb);
  uv_close((uv_handle_t*)&client, close_cb);

  recv_cb_called++;
}


static void ipv6_recv_cb(uv_udp_t* handle,
                         ssize_t nread,
                         uv_buf_t buf,
                         struct sockaddr* addr,
                         unsigned flags) {
  ASSERT(0 && "this function should not have been called");
}


TEST_IMPL(udp_ipv6_only) {
  struct sockaddr_in6 addr6;
  struct sockaddr_in addr;
  uv_buf_t buf;
  int r;

  uv_init();

  addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  addr6 = uv_ip6_addr(":::0", TEST_PORT);

  r = uv_udp_init(&server1);
  ASSERT(r == 0);

  r = uv_udp_bind(&server1, addr, 0);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&server1, alloc_cb, ipv4_recv_cb);
  ASSERT(r == 0);

  r = uv_udp_init(&server2);
  ASSERT(r == 0);

  r = uv_udp_bind6(&server2, addr6, UV_UDP_IPV6ONLY);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&server2, alloc_cb, ipv6_recv_cb);
  ASSERT(r == 0);

  r = uv_udp_init(&client);
  ASSERT(r == 0);

  buf = uv_buf_init("PING", 4);

  /* This should fail but still call send_cb(). */
  r = uv_udp_send(&req_, &client, &buf, 1, addr, send_cb);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);
  ASSERT(send_cb_called == 0);
  ASSERT(recv_cb_called == 0);

  uv_run();

  ASSERT(recv_cb_called == 1);
  ASSERT(send_cb_called == 1);
  ASSERT(close_cb_called == 3);

  return 0;
}
