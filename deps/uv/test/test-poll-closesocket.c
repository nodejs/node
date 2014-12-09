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

#ifdef _WIN32

#include <errno.h>

#include "uv.h"
#include "task.h"

uv_os_sock_t sock;
uv_poll_t handle;

static int close_cb_called = 0;


static void close_cb(uv_handle_t* h) {
  close_cb_called++;
}


static void poll_cb(uv_poll_t* h, int status, int events) {
  int r;

  ASSERT(status == 0);
  ASSERT(h == &handle);

  r = uv_poll_start(&handle, UV_READABLE, poll_cb);
  ASSERT(r == 0);

  closesocket(sock);
  uv_close((uv_handle_t*) &handle, close_cb);

}


TEST_IMPL(poll_closesocket) {
  struct WSAData wsa_data;
  int r;
  unsigned long on;
  struct sockaddr_in addr;

  r = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  ASSERT(r == 0);

  sock = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT(sock != INVALID_SOCKET);
  on = 1;
  r = ioctlsocket(sock, FIONBIO, &on);
  ASSERT(r == 0);

  addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  r = connect(sock, (const struct sockaddr*) &addr, sizeof addr);
  ASSERT(r != 0);
  ASSERT(WSAGetLastError() == WSAEWOULDBLOCK);

  r = uv_poll_init_socket(uv_default_loop(), &handle, sock);
  ASSERT(r == 0);
  r = uv_poll_start(&handle, UV_WRITABLE, poll_cb);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif
