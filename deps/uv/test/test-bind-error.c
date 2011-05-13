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
#include <stdio.h>
#include <stdlib.h>


static int close_cb_called = 0;


static void close_cb(uv_handle_t* handle, int status) {
  ASSERT(handle != NULL);
  ASSERT(status == 0);

  close_cb_called++;
}


static uv_buf alloc_cb(uv_handle_t* handle, size_t size) {
  uv_buf buf = {0, 0};
  FATAL("alloc should not be called");
  return buf;
}


TEST_IMPL(bind_error_addrinuse) {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", TEST_PORT);
  uv_handle_t server1, server2;
  int r;

  uv_init(alloc_cb);

  r = uv_tcp_init(&server1, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_bind(&server1, (struct sockaddr*) &addr);
  ASSERT(r == 0);

  r = uv_tcp_init(&server2, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_bind(&server2, (struct sockaddr*) &addr);
  ASSERT(r == 0);

  r = uv_listen(&server1, 128, NULL);
  ASSERT(r == 0);
  r = uv_listen(&server2, 128, NULL);
  ASSERT(r == -1);

  ASSERT(uv_last_error().code == UV_EADDRINUSE);

  uv_close(&server1);
  uv_close(&server2);

  uv_run();

  ASSERT(close_cb_called == 2);

  return 0;
}


TEST_IMPL(bind_error_addrnotavail_1) {
  struct sockaddr_in addr = uv_ip4_addr("127.255.255.255", TEST_PORT);
  uv_handle_t server;
  int r;

  uv_init(alloc_cb);

  r = uv_tcp_init(&server, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_bind(&server, (struct sockaddr*) &addr);

  /* It seems that Linux is broken here - bind succeeds. */
  if (r == -1) {
    ASSERT(uv_last_error().code == UV_EADDRNOTAVAIL);
  }

  uv_close(&server);

  uv_run();

  ASSERT(close_cb_called == 1);

  return 0;
}


TEST_IMPL(bind_error_addrnotavail_2) {
  struct sockaddr_in addr = uv_ip4_addr("4.4.4.4", TEST_PORT);
  uv_handle_t server;
  int r;

  uv_init(alloc_cb);

  r = uv_tcp_init(&server, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_bind(&server, (struct sockaddr*) &addr);
  ASSERT(r == -1);
  ASSERT(uv_last_error().code == UV_EADDRNOTAVAIL);

  uv_close(&server);

  uv_run();

  ASSERT(close_cb_called == 1);

  return 0;
}


TEST_IMPL(bind_error_fault) {
  char garbage[] = "blah blah blah blah blah blah blah blah blah blah blah blah";
  uv_handle_t server;
  int r;

  uv_init(alloc_cb);

  r = uv_tcp_init(&server, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_bind(&server, (struct sockaddr*) &garbage);
  ASSERT(r == -1);

  ASSERT(uv_last_error().code == UV_EFAULT);

  uv_close(&server);

  uv_run();

  ASSERT(close_cb_called == 1);

  return 0;
}

/* Notes: On Linux uv_bind(server, NULL) will segfault the program.  */

TEST_IMPL(bind_error_inval) {
  struct sockaddr_in addr1 = uv_ip4_addr("0.0.0.0", TEST_PORT);
  struct sockaddr_in addr2 = uv_ip4_addr("0.0.0.0", TEST_PORT_2);
  uv_handle_t server;
  int r;

  uv_init(alloc_cb);

  r = uv_tcp_init(&server, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_bind(&server, (struct sockaddr*) &addr1);
  ASSERT(r == 0);
  r = uv_bind(&server, (struct sockaddr*) &addr2);
  ASSERT(r == -1);

  ASSERT(uv_last_error().code == UV_EINVAL);

  uv_close(&server);

  uv_run();

  ASSERT(close_cb_called == 1);

  return 0;
}
