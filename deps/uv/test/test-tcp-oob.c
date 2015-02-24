/* Copyright Fedor Indutny. All rights reserved.
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

#if !defined(_WIN32)

#include "uv.h"
#include "task.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

static uv_tcp_t server_handle;
static uv_tcp_t client_handle;
static uv_tcp_t peer_handle;
static uv_idle_t idle;
static uv_connect_t connect_req;
static int ticks;
static const int kMaxTicks = 10;


static void set_nonblocking(int fd, int set) {
  int r;
  int flags = fcntl(fd, F_GETFL, 0);
  ASSERT(flags >= 0);
  if (set)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  r = fcntl(fd, F_SETFL, flags);
  ASSERT(r >= 0);
}


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t size) {
  static char storage[1024];
  return uv_buf_init(storage, sizeof(storage));
}


static void idle_cb(uv_idle_t* idle, int status) {
  ASSERT(status == 0);
  if (++ticks < kMaxTicks)
    return;

  uv_close((uv_handle_t*) &server_handle, NULL);
  uv_close((uv_handle_t*) &client_handle, NULL);
  uv_close((uv_handle_t*) &peer_handle, NULL);
  uv_close((uv_handle_t*) idle, NULL);
}


static void read_cb(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  ASSERT(nread > 0);
  ASSERT(0 == uv_idle_start(&idle, idle_cb));
}


static void connect_cb(uv_connect_t* req, int status) {
  ASSERT(req->handle == (uv_stream_t*) &client_handle);
  ASSERT(0 == status);
}


static void connection_cb(uv_stream_t* handle, int status) {
  int r;
  int fd;

  ASSERT(0 == status);
  ASSERT(0 == uv_accept(handle, (uv_stream_t*) &peer_handle));
  ASSERT(0 == uv_read_start((uv_stream_t*) &peer_handle, alloc_cb, read_cb));

  /* Send some OOB data */
  fd = client_handle.io_watcher.fd;
  set_nonblocking(fd, 0);

  /* The problem triggers only on a second message, it seem that xnu is not
   * triggering `kevent()` for the first one
   */
  do {
    r = send(fd, "hello", 5, MSG_OOB);
  } while (r < 0 && errno == EINTR);
  ASSERT(5 == r);

  do {
    r = send(fd, "hello", 5, MSG_OOB);
  } while (r < 0 && errno == EINTR);
  ASSERT(5 == r);

  set_nonblocking(fd, 1);
}


TEST_IMPL(tcp_oob) {
  uv_loop_t* loop;
  loop = uv_default_loop();

  ASSERT(0 == uv_tcp_init(loop, &server_handle));
  ASSERT(0 == uv_tcp_init(loop, &client_handle));
  ASSERT(0 == uv_tcp_init(loop, &peer_handle));
  ASSERT(0 == uv_idle_init(loop, &idle));
  ASSERT(0 == uv_tcp_bind(&server_handle, uv_ip4_addr("127.0.0.1", TEST_PORT)));
  ASSERT(0 == uv_listen((uv_stream_t*) &server_handle, 1, connection_cb));

  /* Ensure two separate packets */
  ASSERT(0 == uv_tcp_nodelay(&client_handle, 1));

  ASSERT(0 == uv_tcp_connect(&connect_req,
                             &client_handle,
                             uv_ip4_addr("127.0.0.1", TEST_PORT),
                             connect_cb));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  ASSERT(ticks == kMaxTicks);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif
