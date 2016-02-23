/* Copyright (c) 2015, Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "uv.h"
#include "task.h"

#ifdef _WIN32

TEST_IMPL(pipe_set_non_blocking) {
  RETURN_SKIP("Test not implemented on Windows.");
}

#else  /* !_WIN32 */

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

struct thread_ctx {
  uv_barrier_t barrier;
  int fd;
};

static void thread_main(void* arg) {
  struct thread_ctx* ctx;
  char buf[4096];
  ssize_t n;

  ctx = arg;
  uv_barrier_wait(&ctx->barrier);

  do
    n = read(ctx->fd, buf, sizeof(buf));
  while (n > 0 || (n == -1 && errno == EINTR));

  ASSERT(n == 0);
}

TEST_IMPL(pipe_set_non_blocking) {
  struct thread_ctx ctx;
  uv_pipe_t pipe_handle;
  uv_thread_t thread;
  size_t nwritten;
  char data[4096];
  uv_buf_t buf;
  int fd[2];
  int n;

  ASSERT(0 == uv_pipe_init(uv_default_loop(), &pipe_handle, 0));
  ASSERT(0 == socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
  ASSERT(0 == uv_pipe_open(&pipe_handle, fd[0]));
  ASSERT(0 == uv_stream_set_blocking((uv_stream_t*) &pipe_handle, 1));

  ctx.fd = fd[1];
  ASSERT(0 == uv_barrier_init(&ctx.barrier, 2));
  ASSERT(0 == uv_thread_create(&thread, thread_main, &ctx));
  uv_barrier_wait(&ctx.barrier);

  buf.len = sizeof(data);
  buf.base = data;
  memset(data, '.', sizeof(data));

  nwritten = 0;
  while (nwritten < 10 << 20) {
    /* The stream is in blocking mode so uv_try_write() should always succeed
     * with the exact number of bytes that we wanted written.
     */
    n = uv_try_write((uv_stream_t*) &pipe_handle, &buf, 1);
    ASSERT(n == sizeof(data));
    nwritten += n;
  }

  uv_close((uv_handle_t*) &pipe_handle, NULL);
  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT(0 == uv_thread_join(&thread));
  ASSERT(0 == close(fd[1]));  /* fd[0] is closed by uv_close(). */
  uv_barrier_destroy(&ctx.barrier);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#endif  /* !_WIN32 */
