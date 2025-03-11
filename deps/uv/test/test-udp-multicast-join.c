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
  ASSERT_NE((uv_udp_t*)(handle) == &server || (uv_udp_t*)(handle) == &client, 0)

#define MULTICAST_ADDR "239.255.0.1"

static uv_udp_t server;
static uv_udp_t client;
static uv_udp_send_t req;
static uv_udp_send_t req_ss;

static int darwin_ebusy_errors;
static int cl_recv_cb_called;
static int sv_send_cb_called;
static int close_cb_called;

static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  static char slab[65536];
  CHECK_HANDLE(handle);
  ASSERT_LE(suggested_size, sizeof(slab));
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void close_cb(uv_handle_t* handle) {
  CHECK_HANDLE(handle);
  close_cb_called++;
}


static void sv_send_cb(uv_udp_send_t* req, int status) {
  ASSERT_NOT_NULL(req);
  ASSERT_OK(status);
  CHECK_HANDLE(req->handle);

  sv_send_cb_called++;

  if (sv_send_cb_called == 2)
    uv_close((uv_handle_t*) req->handle, close_cb);
}


static int do_send(uv_udp_send_t* send_req) {
  uv_buf_t buf;
  struct sockaddr_in addr;
  
  buf = uv_buf_init("PING", 4);

  ASSERT_OK(uv_ip4_addr(MULTICAST_ADDR, TEST_PORT, &addr));

  /* client sends "PING" */
  return uv_udp_send(send_req,
                     &client,
                     &buf,
                     1,
                     (const struct sockaddr*) &addr,
                     sv_send_cb);
}


static void cl_recv_cb(uv_udp_t* handle,
                       ssize_t nread,
                       const uv_buf_t* buf,
                       const struct sockaddr* addr,
                       unsigned flags) {
  CHECK_HANDLE(handle);
  ASSERT_OK(flags);

  if (nread < 0) {
    ASSERT(0 && "unexpected error");
  }

  if (nread == 0) {
    /* Returning unused buffer. Don't count towards cl_recv_cb_called */
    ASSERT_NULL(addr);
    return;
  }

  ASSERT_NOT_NULL(addr);
  ASSERT_EQ(4, nread);
  ASSERT(!memcmp("PING", buf->base, nread));

  cl_recv_cb_called++;

  if (cl_recv_cb_called == 2) {
    /* we are done with the server handle, we can close it */
    uv_close((uv_handle_t*) &server, close_cb);
  } else {
    int r;
    char source_addr[64];

    r = uv_ip4_name((const struct sockaddr_in*)addr, source_addr, sizeof(source_addr));
    ASSERT_OK(r);

    r = uv_udp_set_membership(&server, MULTICAST_ADDR, NULL, UV_LEAVE_GROUP);
    ASSERT_OK(r);

#if !defined(__NetBSD__)
    r = uv_udp_set_source_membership(&server, MULTICAST_ADDR, NULL, source_addr, UV_JOIN_GROUP);
#if defined(__APPLE__)
    if (r == UV_EBUSY) {
      uv_close((uv_handle_t*) &server, close_cb);
      darwin_ebusy_errors++;
      return;
    }
#endif
    ASSERT_OK(r);
#endif

    r = do_send(&req_ss);
    ASSERT_OK(r);
  }
}


TEST_IMPL(udp_multicast_join) {
#if defined(__OpenBSD__)
  RETURN_SKIP("Test does not currently work in OpenBSD");
#endif
  int r;
  struct sockaddr_in addr;

  ASSERT_OK(uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));

  r = uv_udp_init(uv_default_loop(), &server);
  ASSERT_OK(r);

  r = uv_udp_init(uv_default_loop(), &client);
  ASSERT_OK(r);

  /* bind to the desired port */
  r = uv_udp_bind(&server, (const struct sockaddr*) &addr, 0);
  ASSERT_OK(r);

  /* join the multicast channel */
  r = uv_udp_set_membership(&server, MULTICAST_ADDR, NULL, UV_JOIN_GROUP);
  if (r == UV_ENODEV)
    RETURN_SKIP("No multicast support.");
  if (r == UV_ENOEXEC)
    RETURN_SKIP("No multicast support (likely a firewall issue).");
  ASSERT_OK(r);
#if defined(__ANDROID__)
  /* It returns an ENOSYS error */
  RETURN_SKIP("Test does not currently work in ANDROID");
#endif

  r = uv_udp_recv_start(&server, alloc_cb, cl_recv_cb);
  ASSERT_OK(r);

  r = do_send(&req);
  ASSERT_OK(r);

  ASSERT_OK(close_cb_called);
  ASSERT_OK(cl_recv_cb_called);
  ASSERT_OK(sv_send_cb_called);

  /* run the loop till all events are processed */
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  if (darwin_ebusy_errors > 0)
    RETURN_SKIP("Unexplained macOS IP_ADD_SOURCE_MEMBERSHIP EBUSY bug");

  ASSERT_EQ(2, cl_recv_cb_called);
  ASSERT_EQ(2, sv_send_cb_called);
  ASSERT_EQ(2, close_cb_called);

  MAKE_VALGRIND_HAPPY(uv_default_loop());
  return 0;
}
