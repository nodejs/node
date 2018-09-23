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

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

#define CHECK_HANDLE(handle)                \
  ASSERT((uv_udp_t*)(handle) == &server     \
      || (uv_udp_t*)(handle) == &client     \
      || (uv_timer_t*)(handle) == &timeout)

#define CHECK_REQ(req) \
  ASSERT((req) == &req_);

static uv_udp_t client;
static uv_udp_t server;
static uv_udp_send_t req_;
static uv_timer_t timeout;

static int send_cb_called;
static int recv_cb_called;
static int close_cb_called;

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
static int can_ipv6_ipv4_dual(void) {
  int v6only;
  size_t size = sizeof(int);

  if (sysctlbyname("net.inet6.ip6.v6only", &v6only, &size, NULL, 0))
    return 0;

  return v6only != 1;
}
#endif


static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  buf->base = slab;
  buf->len = sizeof(slab);
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


static void ipv6_recv_fail(uv_udp_t* handle,
                           ssize_t nread,
                           const uv_buf_t* buf,
                           const struct sockaddr* addr,
                           unsigned flags) {
  ASSERT(0 && "this function should not have been called");
}


static void ipv6_recv_ok(uv_udp_t* handle,
                         ssize_t nread,
                         const uv_buf_t* buf,
                         const struct sockaddr* addr,
                         unsigned flags) {
  CHECK_HANDLE(handle);
  ASSERT(nread >= 0);

  if (nread)
    recv_cb_called++;
}


static void timeout_cb(uv_timer_t* timer) {
  uv_close((uv_handle_t*)&server, close_cb);
  uv_close((uv_handle_t*)&client, close_cb);
  uv_close((uv_handle_t*)&timeout, close_cb);
}


static void do_test(uv_udp_recv_cb recv_cb, int bind_flags) {
  struct sockaddr_in6 addr6;
  struct sockaddr_in addr;
  uv_buf_t buf;
  int r;

  ASSERT(0 == uv_ip6_addr("::0", TEST_PORT, &addr6));

  r = uv_udp_init(uv_default_loop(), &server);
  ASSERT(r == 0);

  r = uv_udp_bind(&server, (const struct sockaddr*) &addr6, bind_flags);
  ASSERT(r == 0);

  r = uv_udp_recv_start(&server, alloc_cb, recv_cb);
  ASSERT(r == 0);

  r = uv_udp_init(uv_default_loop(), &client);
  ASSERT(r == 0);

  buf = uv_buf_init("PING", 4);
  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

  r = uv_udp_send(&req_,
                  &client,
                  &buf,
                  1,
                  (const struct sockaddr*) &addr,
                  send_cb);
  ASSERT(r == 0);

  r = uv_timer_init(uv_default_loop(), &timeout);
  ASSERT(r == 0);

  r = uv_timer_start(&timeout, timeout_cb, 500, 0);
  ASSERT(r == 0);

  ASSERT(close_cb_called == 0);
  ASSERT(send_cb_called == 0);
  ASSERT(recv_cb_called == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 3);

  MAKE_VALGRIND_HAPPY();
}


TEST_IMPL(udp_dual_stack) {
#if defined(__CYGWIN__) || defined(__MSYS__)
  /* FIXME: Does Cygwin support this?  */
  RETURN_SKIP("FIXME: This test needs more investigation on Cygwin");
#endif

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
  if (!can_ipv6_ipv4_dual())
    RETURN_SKIP("IPv6-IPv4 dual stack not supported");
#elif defined(__OpenBSD__)
  RETURN_SKIP("IPv6-IPv4 dual stack not supported");
#endif

  do_test(ipv6_recv_ok, 0);

  ASSERT(recv_cb_called == 1);
  ASSERT(send_cb_called == 1);

  return 0;
}


TEST_IMPL(udp_ipv6_only) {
  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  do_test(ipv6_recv_fail, UV_UDP_IPV6ONLY);

  ASSERT(recv_cb_called == 0);
  ASSERT(send_cb_called == 1);

  return 0;
}
