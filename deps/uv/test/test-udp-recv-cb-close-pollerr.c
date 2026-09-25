/* Copyright libuv contributors. All rights reserved.
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

static uv_udp_send_t send_req;
static uv_udp_t client;
static int alloc_cb_called;
static int recv_cb_called;
static int send_cb_called;

static void alloc_cb(uv_handle_t* handle, size_t sz, uv_buf_t* buf) {
  alloc_cb_called++;
  buf->base = "uv";
  buf->len = 2;
}

static void recv_cb(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
                    const struct sockaddr* addr, unsigned flags) {
  recv_cb_called++;
  /* Stop receiving and unset recv_cb and alloc_cb */
  uv_close((uv_handle_t*) handle, NULL);
}

static void send_cb(uv_udp_send_t* req, int status) {
  send_cb_called++;
  /* The send completed; now just close the handle and let the loop drain.
   * If a POLLERR arrives before the close completes, uv__udp_io must not
   * crash when no recv callback is installed. */
  uv_close((uv_handle_t*) req->handle, NULL);
}

/* Refs: https://github.com/libuv/libuv/issues/5030 */
TEST_IMPL(udp_recv_cb_close_pollerr) {
#ifndef __linux__
  RETURN_SKIP("ICMP error handling is Linux-specific");
#endif
  struct sockaddr_in any_addr;
  struct sockaddr_in addr;
  uv_buf_t buf;

  ASSERT_OK(uv_udp_init(uv_default_loop(), &client));

  ASSERT_OK(uv_ip4_addr("0.0.0.0", 0, &any_addr));
  ASSERT_OK(uv_udp_bind(&client, (const struct sockaddr*) &any_addr,
                        UV_UDP_LINUX_RECVERR));

  ASSERT_OK(uv_ip4_addr("127.0.0.1", 9999, &addr));
  ASSERT_OK(uv_udp_connect(&client, (const struct sockaddr*) &addr));

  ASSERT_OK(uv_udp_recv_start(&client, alloc_cb, recv_cb));

  buf = uv_buf_init("PING", 4);
  ASSERT_GT(uv_udp_try_send(&client, &buf, 1, NULL), 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT_EQ(1, alloc_cb_called);
  ASSERT_EQ(1, recv_cb_called);

  MAKE_VALGRIND_HAPPY(uv_default_loop());
  return 0;
}

/* Same as above but WITHOUT uv_udp_recv_start.
 * The ICMP POLLERR still fires on the fd; uv__udp_io must not crash when
 * no recv/alloc callback is installed. */
TEST_IMPL(udp_send_pollerr_no_recv) {
#ifndef __linux__
  RETURN_SKIP("ICMP error handling is Linux-specific");
#endif
  struct sockaddr_in any_addr;
  struct sockaddr_in addr;
  uv_buf_t buf;

  ASSERT_OK(uv_udp_init(uv_default_loop(), &client));

  ASSERT_OK(uv_ip4_addr("0.0.0.0", 0, &any_addr));
  ASSERT_OK(uv_udp_bind(&client, (const struct sockaddr*) &any_addr,
                        UV_UDP_LINUX_RECVERR));

  ASSERT_OK(uv_ip4_addr("127.0.0.1", 9999, &addr));
  ASSERT_OK(uv_udp_connect(&client, (const struct sockaddr*) &addr));

  buf = uv_buf_init("PING", 4);
  ASSERT_OK(uv_udp_send(&send_req, &client, &buf, 1, NULL, send_cb));

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT_EQ(1, send_cb_called);

  MAKE_VALGRIND_HAPPY(uv_default_loop());
  return 0;
}