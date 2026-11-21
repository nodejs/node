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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_HANDLE(handle) \
  ASSERT_NE((uv_udp_t*)(handle) == &recver || (uv_udp_t*)(handle) == &sender, 0)

#define BUFFER_MULTIPLIER 20
#define MAX_DGRAM_SIZE (64 * 1024)
#define NUM_SENDS 40

static uv_udp_t recver;
static uv_udp_t sender;
static uv_timer_t timeout;
static int recv_cb_called;
static int received_datagrams;
static int read_bytes;
static int close_cb_called;
static int alloc_cb_called;
static int terminal_data_cb_called;
static int terminal_free_cb_called;
static int terminal_drain_cb_called;
static int terminal_timeout_cb_called;
static int duplicate_terminal_cb;
#ifndef _WIN32
static int zero_namelen_chunk_cb_called;
static int zero_namelen_free_cb_called;
#endif


static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  size_t buffer_size;
  CHECK_HANDLE(handle);

  /* Only allocate enough room for multiple dgrams if we can actually recv them */
  buffer_size = MAX_DGRAM_SIZE;
  if (uv_udp_using_recvmmsg((uv_udp_t*)handle))
    buffer_size *= BUFFER_MULTIPLIER;

  /* Actually malloc to exercise free'ing the buffer later */
  buf->base = malloc(buffer_size);
  ASSERT_NOT_NULL(buf->base);
  buf->len = buffer_size;
  alloc_cb_called++;
}


static void close_cb(uv_handle_t* handle) {
  CHECK_HANDLE(handle);
  ASSERT(uv_is_closing(handle));
  close_cb_called++;
}


static void recv_cb(uv_udp_t* handle,
                    ssize_t nread,
                    const uv_buf_t* rcvbuf,
                    const struct sockaddr* addr,
                    unsigned flags) {
  ASSERT_GE(nread, 0);
  read_bytes += nread;

  /* free and return if this is a mmsg free-only callback invocation */
  if (flags & UV_UDP_MMSG_FREE) {
    ASSERT_OK(nread);
    ASSERT_NULL(addr);
    free(rcvbuf->base);
    return;
  }

  if (nread == 0) {
    /* There can be no more available data for the time being. */
    ASSERT_NULL(addr);
  } else {
    ASSERT_EQ(4, nread);
    ASSERT_NOT_NULL(addr);
    ASSERT_MEM_EQ("PING", rcvbuf->base, nread);
    received_datagrams++;
  }

  recv_cb_called++;
  if (received_datagrams == NUM_SENDS) {
    uv_close((uv_handle_t*) handle, close_cb);
    uv_close((uv_handle_t*) &sender, close_cb);
  }

  /* Don't free if the buffer could be reused via mmsg */
  if (rcvbuf && !(flags & UV_UDP_MMSG_CHUNK))
    free(rcvbuf->base);
}


static void small_alloc_cb(uv_handle_t* handle,
                           size_t suggested_size,
                           uv_buf_t* buf) {
  CHECK_HANDLE(handle);
  buf->len = 64;
  buf->base = malloc(buf->len);
  ASSERT_NOT_NULL(buf->base);
  alloc_cb_called++;
}


static void small_recv_cb(uv_udp_t* handle,
                          ssize_t nread,
                          const uv_buf_t* rcvbuf,
                          const struct sockaddr* addr,
                          unsigned flags) {
  CHECK_HANDLE(handle);
  ASSERT_EQ(UV_EINVAL, nread);
  uv_close((uv_handle_t*) &recver, close_cb);
  uv_close((uv_handle_t*) &sender, close_cb);
  free(rcvbuf->base);
  recv_cb_called++;
}


static void terminal_alloc_cb(uv_handle_t* handle,
                              size_t suggested_size,
                              uv_buf_t* buf) {
  CHECK_HANDLE(handle);
  buf->base = malloc(suggested_size);
  ASSERT_NOT_NULL(buf->base);
  buf->len = suggested_size;
}


static void timeout_cb(uv_timer_t* handle) {
  terminal_timeout_cb_called++;

  if (!uv_is_closing((uv_handle_t*) &recver))
    uv_close((uv_handle_t*) &recver, close_cb);
  if (!uv_is_closing((uv_handle_t*) &sender))
    uv_close((uv_handle_t*) &sender, close_cb);
  uv_close((uv_handle_t*) handle, NULL);
}


static void terminal_recv_cb(uv_udp_t* handle,
                             ssize_t nread,
                             const uv_buf_t* rcvbuf,
                             const struct sockaddr* addr,
                             unsigned flags) {
  CHECK_HANDLE(handle);

  if (flags & UV_UDP_MMSG_FREE) {
    ASSERT_OK(nread);
    ASSERT_NULL(addr);
    terminal_free_cb_called++;
    free(rcvbuf->base);
    return;
  }

  if (nread > 0) {
    ASSERT_EQ(4, nread);
    ASSERT_NOT_NULL(addr);
    ASSERT_MEM_EQ("PING", rcvbuf->base, nread);
    terminal_data_cb_called++;
    return;
  }

  ASSERT_NULL(addr);
  terminal_drain_cb_called++;

  if (terminal_drain_cb_called == 1) {
    free(rcvbuf->base);

    if (!uv_is_closing((uv_handle_t*) &recver))
      uv_close((uv_handle_t*) &recver, close_cb);
    if (!uv_is_closing((uv_handle_t*) &sender))
      uv_close((uv_handle_t*) &sender, close_cb);
    if (!uv_is_closing((uv_handle_t*) &timeout))
      uv_close((uv_handle_t*) &timeout, NULL);
    return;
  }

  duplicate_terminal_cb = 1;
}

#ifndef _WIN32
static void zero_namelen_recv_cb(uv_udp_t* handle,
                                 ssize_t nread,
                                 const uv_buf_t* rcvbuf,
                                 const struct sockaddr* addr,
                                 unsigned flags) {
  CHECK_HANDLE(handle);

  if (flags & UV_UDP_MMSG_FREE) {
    ASSERT_EQ(0, nread);
    ASSERT_NULL(addr);
    zero_namelen_free_cb_called++;
    free(rcvbuf->base);
    return;
  }

  ASSERT_EQ(0, nread);
  ASSERT_NULL(addr);
  zero_namelen_chunk_cb_called++;

  if (!uv_is_closing((uv_handle_t*) &recver))
    uv_close((uv_handle_t*) &recver, close_cb);

  /* Don't free if the buffer could be reused via mmsg */
  if (rcvbuf && !(flags & UV_UDP_MMSG_CHUNK))
    free(rcvbuf->base);
}


TEST_IMPL(udp_mmsg_namelen_zero) {
  uv_loop_t* loop;
  struct sockaddr_in recv_addr;
  struct sockaddr_in send_addr;
  uv_os_fd_t fd;

  loop = uv_default_loop();

  ASSERT_OK(uv_udp_init_ex(loop, &recver, AF_UNSPEC | UV_UDP_RECVMMSG));
  if (!uv_udp_using_recvmmsg(&recver)) {
    uv_close((uv_handle_t*) &recver, close_cb);
    ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));
    ASSERT_EQ(1, close_cb_called);
    MAKE_VALGRIND_HAPPY(loop);
    return 0;
  }

  ASSERT_OK(uv_ip4_addr("0.0.0.0", TEST_PORT, &recv_addr));
  ASSERT_OK(uv_udp_bind(&recver, (const struct sockaddr*) &recv_addr, 0));

  ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT_2, &send_addr));
  ASSERT_OK(uv_udp_connect(&recver, (const struct sockaddr*) &send_addr));

  ASSERT_OK(uv_fileno((const uv_handle_t*) &recver, &fd));
  ASSERT_OK(shutdown(fd, SHUT_RD));

  ASSERT_OK(uv_udp_recv_start(&recver, alloc_cb, zero_namelen_recv_cb));

  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));

  /* On some platforms as FreeBSD, multiple chunks will be received, one per
   * buffer */
  ASSERT_GE(zero_namelen_chunk_cb_called, 1);
  /* On others such as Linux, the free callback won't be called as recvmmsg is
   * returning EINVAL */
  ASSERT_LE(zero_namelen_free_cb_called, 1);
  ASSERT_EQ(1, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}
#endif

TEST_IMPL(udp_mmsg) {
  struct sockaddr_in addr;
  uv_buf_t buf;
  int i;

  ASSERT_OK(uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));

  ASSERT_OK(uv_udp_init_ex(uv_default_loop(), &recver,
                           AF_UNSPEC | UV_UDP_RECVMMSG));

  ASSERT_OK(uv_udp_bind(&recver, (const struct sockaddr*) &addr, 0));

  ASSERT_OK(uv_udp_recv_start(&recver, alloc_cb, recv_cb));

  ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

  ASSERT_OK(uv_udp_init(uv_default_loop(), &sender));

  buf = uv_buf_init("PING", 4);
  for (i = 0; i < NUM_SENDS; i++) {
    ASSERT_EQ(4, uv_udp_try_send(&sender, &buf, 1, (const struct sockaddr*) &addr));
  }

  ASSERT_OK(uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT_EQ(2, close_cb_called);
  ASSERT_EQ(received_datagrams, NUM_SENDS);

  ASSERT_OK(sender.send_queue_size);
  ASSERT_OK(recver.send_queue_size);

  printf("%d allocs for %d recvs\n", alloc_cb_called, recv_cb_called);

  /* On platforms that don't support mmsg, each recv gets its own alloc */
  if (uv_udp_using_recvmmsg(&recver))
    ASSERT_EQ(read_bytes, NUM_SENDS * 4); /* we're sending 4 bytes per datagram */
  else
    ASSERT_EQ(alloc_cb_called, recv_cb_called);

  MAKE_VALGRIND_HAPPY(uv_default_loop());
  return 0;
}


TEST_IMPL(udp_mmsg_small_buf) {
  struct sockaddr_in addr;
  uv_loop_t* loop;
  uv_buf_t buf;

  loop = uv_default_loop();
  ASSERT_OK(uv_udp_init_ex(loop, &recver, AF_UNSPEC | UV_UDP_RECVMMSG));
  if (uv_udp_using_recvmmsg(&recver)) {
    ASSERT_OK(uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));
    ASSERT_OK(uv_udp_bind(&recver, (const struct sockaddr*) &addr, 0));
    ASSERT_OK(uv_udp_recv_start(&recver, small_alloc_cb, small_recv_cb));
    ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
    ASSERT_OK(uv_udp_init(loop, &sender));
    buf = uv_buf_init("PING", 4);
    ASSERT_EQ(4, uv_udp_try_send(&sender, &buf, 1, (const struct sockaddr*) &addr));
    ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));
    ASSERT_EQ(1, recv_cb_called);
    ASSERT_EQ(2, close_cb_called);
  } else {
    uv_close((uv_handle_t*) &recver, close_cb);
    ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));
    ASSERT_EQ(1, close_cb_called);
  }
  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}


TEST_IMPL(udp_mmsg_single_drain_cb) {
  struct sockaddr_in addr;
  uv_loop_t* loop;
  uv_buf_t buf;

  close_cb_called = 0;
  terminal_data_cb_called = 0;
  terminal_free_cb_called = 0;
  terminal_drain_cb_called = 0;
  terminal_timeout_cb_called = 0;
  duplicate_terminal_cb = 0;

  loop = uv_default_loop();

  ASSERT_OK(uv_udp_init_ex(loop, &recver, AF_UNSPEC | UV_UDP_RECVMMSG));
  if (!uv_udp_using_recvmmsg(&recver)) {
    uv_close((uv_handle_t*) &recver, close_cb);
    ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));
    ASSERT_EQ(1, close_cb_called);
    MAKE_VALGRIND_HAPPY(loop);
    return 0;
  }

  ASSERT_OK(uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));
  ASSERT_OK(uv_udp_bind(&recver, (const struct sockaddr*) &addr, 0));
  ASSERT_OK(uv_udp_recv_start(&recver, terminal_alloc_cb, terminal_recv_cb));

  ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
  ASSERT_OK(uv_udp_init(loop, &sender));
  ASSERT_OK(uv_timer_init(loop, &timeout));
  ASSERT_OK(uv_timer_start(&timeout, timeout_cb, 5000, 0));

  buf = uv_buf_init("PING", 4);
  ASSERT_EQ(4, uv_udp_try_send(&sender, &buf, 1, (const struct sockaddr*) &addr));

  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));

  ASSERT_EQ(0, terminal_timeout_cb_called);
  ASSERT_EQ(0, duplicate_terminal_cb);
  ASSERT_EQ(1, terminal_data_cb_called);
  ASSERT_EQ(1, terminal_free_cb_called);
  ASSERT_EQ(1, terminal_drain_cb_called);
  ASSERT_EQ(2, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}
