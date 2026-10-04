/* Copyright libuv project contributors. All rights reserved.
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

/* Regression test: a write that completes synchronously on a stream that is
 * also being read must not leave a no-op io watcher update queued, because
 * embedders that drive the loop with uv_backend_fd()/uv_backend_timeout()
 * see that as "poll again immediately" and spin an extra iteration.
 */

#include "uv.h"
#include "task.h"

static uv_tcp_t server;
static uv_tcp_t client;
static uv_tcp_t incoming;
static uv_connect_t connect_req;
static uv_write_t ping_req;
static uv_write_t pong_req;
static char slab[64];
static int pong_written;
static int pong_write_cb_called;
static int close_cb_called;

static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}

static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
  *buf = uv_buf_init(slab, sizeof(slab));
}

static void pong_write_cb(uv_write_t* req, int status) {
  ASSERT_OK(status);
  pong_write_cb_called++;
}

static void incoming_read_cb(uv_stream_t* stream,
                             ssize_t nread,
                             const uv_buf_t* buf) {
  uv_buf_t pong;

  if (nread == 0)
    return;
  ASSERT_EQ(4, nread);
  ASSERT_OK(pong_written);
  /* We are inside uv__io_poll(). A small write on an idle loopback socket
   * completes synchronously; its callback is deferred via the pending queue
   * and runs before this loop iteration ends. */
  pong = uv_buf_init("pong", 4);
  ASSERT_OK(uv_write(&pong_req, stream, &pong, 1, pong_write_cb));
  ASSERT_UINT64_EQ(0, stream->write_queue_size);
  pong_written++;
}

static void client_read_cb(uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf) {
  ASSERT_GE(nread, 0);
}

static void ping_write_cb(uv_write_t* req, int status) {
  ASSERT_OK(status);
}

static void connect_cb(uv_connect_t* req, int status) {
  uv_buf_t ping;
  ASSERT_OK(status);
  ASSERT_OK(uv_read_start((uv_stream_t*) &client, alloc_cb, client_read_cb));
  ping = uv_buf_init("ping", 4);
  ASSERT_OK(uv_write(&ping_req, (uv_stream_t*) &client, &ping, 1,
                     ping_write_cb));
}

static void connection_cb(uv_stream_t* s, int status) {
  ASSERT_OK(status);
  ASSERT_OK(uv_tcp_init(s->loop, &incoming));
  ASSERT_OK(uv_accept(s, (uv_stream_t*) &incoming));
  ASSERT_OK(uv_read_start((uv_stream_t*) &incoming,
                          alloc_cb,
                          incoming_read_cb));
}

TEST_IMPL(tcp_write_in_read_cb_backend_timeout) {
#if defined(_WIN32)
  RETURN_SKIP("Stream writes do not complete synchronously on Windows.");
#elif defined(__sun)
  RETURN_SKIP("Event ports re-arm file descriptors on every iteration.");
#else
  struct sockaddr_in addr;
  uv_loop_t* loop;

  loop = uv_default_loop();
  ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
  ASSERT_OK(uv_tcp_init(loop, &server));
  ASSERT_OK(uv_tcp_bind(&server, (struct sockaddr*) &addr, 0));
  ASSERT_OK(uv_listen((uv_stream_t*) &server, 1, connection_cb));
  ASSERT_OK(uv_tcp_init(loop, &client));
  ASSERT_OK(uv_tcp_connect(&connect_req,
                           &client,
                           (struct sockaddr*) &addr,
                           connect_cb));

  while (pong_write_cb_called == 0)
    ASSERT_EQ(1, uv_run(loop, UV_RUN_ONCE));

  ASSERT_EQ(1, pong_written);
  /* Three reading TCP handles, no timers, nothing pending: an embedder should
   * now be told to block indefinitely, not to spin. */
  ASSERT_EQ(-1, uv_backend_timeout(loop));

  uv_close((uv_handle_t*) &incoming, close_cb);
  uv_close((uv_handle_t*) &client, close_cb);
  uv_close((uv_handle_t*) &server, close_cb);
  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));
  ASSERT_EQ(3, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
#endif
}
