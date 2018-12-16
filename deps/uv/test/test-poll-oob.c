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

#if !defined(_WIN32)

#include "uv.h"
#include "task.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

static uv_tcp_t server_handle;
static uv_tcp_t client_handle;
static uv_tcp_t peer_handle;
static uv_poll_t poll_req[2];
static uv_idle_t idle;
static uv_os_fd_t client_fd;
static uv_os_fd_t server_fd;
static int ticks;
static const int kMaxTicks = 10;
static int cli_pr_check = 0;
static int cli_rd_check = 0;
static int srv_rd_check = 0;

static int got_eagain(void) {
  return errno == EAGAIN
      || errno == EINPROGRESS
#ifdef EWOULDBLOCK
      || errno == EWOULDBLOCK
#endif
      ;
}

static void idle_cb(uv_idle_t* idle) {
  uv_sleep(100);
  if (++ticks < kMaxTicks)
    return;

  uv_poll_stop(&poll_req[0]);
  uv_poll_stop(&poll_req[1]);
  uv_close((uv_handle_t*) &server_handle, NULL);
  uv_close((uv_handle_t*) &client_handle, NULL);
  uv_close((uv_handle_t*) &peer_handle, NULL);
  uv_close((uv_handle_t*) idle, NULL);
}

static void poll_cb(uv_poll_t* handle, int status, int events) {
  char buffer[5];
  int n;
  int fd;

  ASSERT(0 == uv_fileno((uv_handle_t*)handle, &fd));
  memset(buffer, 0, 5);

  if (events & UV_PRIORITIZED) {
    do
      n = recv(client_fd, &buffer, 5, MSG_OOB);
    while (n == -1 && errno == EINTR);
    ASSERT(n >= 0 || errno != EINVAL);
    cli_pr_check = 1;
    ASSERT(0 == uv_poll_stop(&poll_req[0]));
    ASSERT(0 == uv_poll_start(&poll_req[0],
                              UV_READABLE | UV_WRITABLE,
                              poll_cb));
  }
  if (events & UV_READABLE) {
    if (fd == client_fd) {
      do
        n = recv(client_fd, &buffer, 5, 0);
      while (n == -1 && errno == EINTR);
      ASSERT(n >= 0 || errno != EINVAL);
      if (cli_rd_check == 1) {
        ASSERT(strncmp(buffer, "world", n) == 0);
        ASSERT(5 == n);
        cli_rd_check = 2;
      }
      if (cli_rd_check == 0) {
        ASSERT(n == 4);
        ASSERT(strncmp(buffer, "hello", n) == 0);
        cli_rd_check = 1;
        do {
          do
            n = recv(server_fd, &buffer, 5, 0);
          while (n == -1 && errno == EINTR);
          if (n > 0) {
            ASSERT(n == 5);
            ASSERT(strncmp(buffer, "world", n) == 0);
            cli_rd_check = 2;
          }
        } while (n > 0);

        ASSERT(got_eagain());
      }
    }
    if (fd == server_fd) {
      do
        n = recv(server_fd, &buffer, 3, 0);
      while (n == -1 && errno == EINTR);
      ASSERT(n >= 0 || errno != EINVAL);
      ASSERT(3 == n);
      ASSERT(strncmp(buffer, "foo", n) == 0);
      srv_rd_check = 1;
      uv_poll_stop(&poll_req[1]);
    }
  }
  if (events & UV_WRITABLE) {
    do {
      n = send(client_fd, "foo", 3, 0);
    } while (n < 0 && errno == EINTR);
    ASSERT(3 == n);
  }
}

static void connection_cb(uv_stream_t* handle, int status) {
  int r;

  ASSERT(0 == status);
  ASSERT(0 == uv_accept(handle, (uv_stream_t*) &peer_handle));
  ASSERT(0 == uv_fileno((uv_handle_t*) &peer_handle, &server_fd));
  ASSERT(0 == uv_poll_init_socket(uv_default_loop(), &poll_req[0], client_fd));
  ASSERT(0 == uv_poll_init_socket(uv_default_loop(), &poll_req[1], server_fd));
  ASSERT(0 == uv_poll_start(&poll_req[0],
                            UV_PRIORITIZED | UV_READABLE | UV_WRITABLE,
                            poll_cb));
  ASSERT(0 == uv_poll_start(&poll_req[1],
                            UV_READABLE,
                            poll_cb));
  do {
    r = send(server_fd, "hello", 5, MSG_OOB);
  } while (r < 0 && errno == EINTR);
  ASSERT(5 == r);

  do {
    r = send(server_fd, "world", 5, 0);
  } while (r < 0 && errno == EINTR);
  ASSERT(5 == r);

  ASSERT(0 == uv_idle_start(&idle, idle_cb));
}


TEST_IMPL(poll_oob) {
  struct sockaddr_in addr;
  int r = 0;
  uv_loop_t* loop;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
  loop = uv_default_loop();

  ASSERT(0 == uv_tcp_init(loop, &server_handle));
  ASSERT(0 == uv_tcp_init(loop, &client_handle));
  ASSERT(0 == uv_tcp_init(loop, &peer_handle));
  ASSERT(0 == uv_idle_init(loop, &idle));
  ASSERT(0 == uv_tcp_bind(&server_handle, (const struct sockaddr*) &addr, 0));
  ASSERT(0 == uv_listen((uv_stream_t*) &server_handle, 1, connection_cb));

  /* Ensure two separate packets */
  ASSERT(0 == uv_tcp_nodelay(&client_handle, 1));

  client_fd = socket(PF_INET, SOCK_STREAM, 0);
  ASSERT(client_fd >= 0);
  do {
    errno = 0;
    r = connect(client_fd, (const struct sockaddr*)&addr, sizeof(addr));
  } while (r == -1 && errno == EINTR);
  ASSERT(r == 0);

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  ASSERT(ticks == kMaxTicks);

  /* Did client receive the POLLPRI message */
  ASSERT(cli_pr_check == 1);
  /* Did client receive the POLLIN message */
  ASSERT(cli_rd_check == 2);
  /* Could we write with POLLOUT and did the server receive our POLLOUT message
   * through POLLIN.
   */
  ASSERT(srv_rd_check == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#else

typedef int file_has_no_tests; /* ISO C forbids an empty translation unit. */

#endif
