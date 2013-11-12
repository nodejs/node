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

#include <string.h>
#include <errno.h>

#if !defined(_WIN32)
#include <sys/time.h>
#include <sys/resource.h>  /* setrlimit() */
#endif

/* NOTE: Number should be big enough to trigger this problem */
static uv_udp_t sockets[2500];
static uv_udp_send_t reqs[ARRAY_SIZE(sockets)];
static char buf[1];
static unsigned int recv_cb_called;
static unsigned int send_cb_called;
static unsigned int close_cb_called;

static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size) {
  return uv_buf_init(buf, sizeof(buf));
}


static void recv_cb(uv_udp_t* handle,
                    ssize_t nread,
                    uv_buf_t buf,
                    struct sockaddr* addr,
                    unsigned flags) {
  recv_cb_called++;
}


static void send_cb(uv_udp_send_t* req, int status) {
  send_cb_called++;
}


static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}


TEST_IMPL(watcher_cross_stop) {
  uv_loop_t* loop = uv_default_loop();
  unsigned int i;
  struct sockaddr_in addr;
  uv_buf_t buf;
  char big_string[1024];

#if !defined(_WIN32)
  {
    struct rlimit lim;
    lim.rlim_cur = ARRAY_SIZE(sockets) + 32;
    lim.rlim_max = ARRAY_SIZE(sockets) + 32;
    if (setrlimit(RLIMIT_NOFILE, &lim))
      RETURN_SKIP("File descriptor limit too low.");
  }
#endif

  addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  memset(big_string, 'A', sizeof(big_string));
  buf = uv_buf_init(big_string, sizeof(big_string));

  for (i = 0; i < ARRAY_SIZE(sockets); i++) {
    ASSERT(0 == uv_udp_init(loop, &sockets[i]));
    ASSERT(0 == uv_udp_bind(&sockets[i], addr, 0));
    ASSERT(0 == uv_udp_recv_start(&sockets[i], alloc_cb, recv_cb));
    ASSERT(0 == uv_udp_send(&reqs[i], &sockets[i], &buf, 1, addr, send_cb));
  }

  while (recv_cb_called == 0)
    uv_run(loop, UV_RUN_ONCE);

  for (i = 0; i < ARRAY_SIZE(sockets); i++)
    uv_close((uv_handle_t*) &sockets[i], close_cb);

  ASSERT(0 < recv_cb_called && recv_cb_called <= ARRAY_SIZE(sockets));
  ASSERT(ARRAY_SIZE(sockets) == send_cb_called);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(ARRAY_SIZE(sockets) == close_cb_called);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
