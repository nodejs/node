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
#include <string.h>

/* See test-ipc.ctx */
void spawn_helper(uv_pipe_t* channel,
                  uv_process_t* process,
                  const char* helper);

union handles {
  uv_handle_t handle;
  uv_stream_t stream;
  uv_pipe_t pipe;
  uv_tcp_t tcp;
  uv_tty_t tty;
};

struct echo_ctx {
  uv_pipe_t channel;
  uv_write_t write_req;
  uv_handle_type expected_type;
  union handles send;
  union handles recv;
};

static struct echo_ctx ctx;
static int num_recv_handles;


static void alloc_cb(uv_handle_t* handle,
                     size_t suggested_size,
                     uv_buf_t* buf) {
  /* we're not actually reading anything so a small buffer is okay */
  static char slab[8];
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void recv_cb(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf) {
  uv_handle_type pending;
  uv_pipe_t* pipe;
  int r;

  pipe = (uv_pipe_t*) handle;
  ASSERT(pipe == &ctx.channel);
  ASSERT(nread >= 0);
  ASSERT(1 == uv_pipe_pending_count(pipe));

  pending = uv_pipe_pending_type(pipe);
  ASSERT(pending == ctx.expected_type);

  if (pending == UV_NAMED_PIPE)
    r = uv_pipe_init(ctx.channel.loop, &ctx.recv.pipe, 0);
  else if (pending == UV_TCP)
    r = uv_tcp_init(ctx.channel.loop, &ctx.recv.tcp);
  else
    abort();
  ASSERT(r == 0);

  r = uv_accept(handle, &ctx.recv.stream);
  ASSERT(r == 0);

  uv_close((uv_handle_t*)&ctx.channel, NULL);
  uv_close(&ctx.send.handle, NULL);
  uv_close(&ctx.recv.handle, NULL);
  num_recv_handles++;
}


static int run_test(void) {
  uv_process_t process;
  uv_buf_t buf;
  int r;

  spawn_helper(&ctx.channel, &process, "ipc_send_recv_helper");

  buf = uv_buf_init(".", 1);
  r = uv_write2(&ctx.write_req,
                (uv_stream_t*)&ctx.channel,
                &buf, 1,
                &ctx.send.stream,
                NULL);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)&ctx.channel, alloc_cb, recv_cb);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(num_recv_handles == 1);

  return 0;
}


TEST_IMPL(ipc_send_recv_pipe) {
  int r;

  ctx.expected_type = UV_NAMED_PIPE;

  r = uv_pipe_init(uv_default_loop(), &ctx.send.pipe, 1);
  ASSERT(r == 0);

  r = uv_pipe_bind(&ctx.send.pipe, TEST_PIPENAME);
  ASSERT(r == 0);

  r = run_test();
  ASSERT(r == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(ipc_send_recv_tcp) {
  struct sockaddr_in addr;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

  ctx.expected_type = UV_TCP;

  r = uv_tcp_init(uv_default_loop(), &ctx.send.tcp);
  ASSERT(r == 0);

  r = uv_tcp_bind(&ctx.send.tcp, (const struct sockaddr*) &addr, 0);
  ASSERT(r == 0);

  r = run_test();
  ASSERT(r == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


/* Everything here runs in a child process. */

static void write2_cb(uv_write_t* req, int status) {
  ASSERT(status == 0);
  uv_close(&ctx.recv.handle, NULL);
  uv_close((uv_handle_t*)&ctx.channel, NULL);
}


static void read_cb(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* rdbuf) {
  uv_buf_t wrbuf;
  uv_pipe_t* pipe;
  uv_handle_type pending;
  int r;

  pipe = (uv_pipe_t*) handle;
  ASSERT(pipe == &ctx.channel);
  ASSERT(nread >= 0);
  ASSERT(1 == uv_pipe_pending_count(pipe));

  pending = uv_pipe_pending_type(pipe);
  ASSERT(pending == UV_NAMED_PIPE || pending == UV_TCP);

  wrbuf = uv_buf_init(".", 1);

  if (pending == UV_NAMED_PIPE)
    r = uv_pipe_init(ctx.channel.loop, &ctx.recv.pipe, 0);
  else if (pending == UV_TCP)
    r = uv_tcp_init(ctx.channel.loop, &ctx.recv.tcp);
  else
    abort();
  ASSERT(r == 0);

  r = uv_accept(handle, &ctx.recv.stream);
  ASSERT(r == 0);

  r = uv_write2(&ctx.write_req,
                (uv_stream_t*)&ctx.channel,
                &wrbuf,
                1,
                &ctx.recv.stream,
                write2_cb);
  ASSERT(r == 0);
}


/* stdin is a duplex channel over which a handle is sent.
 * We receive it and send it back where it came from.
 */
int ipc_send_recv_helper(void) {
  int r;

  memset(&ctx, 0, sizeof(ctx));

  r = uv_pipe_init(uv_default_loop(), &ctx.channel, 1);
  ASSERT(r == 0);

  uv_pipe_open(&ctx.channel, 0);
  ASSERT(1 == uv_is_readable((uv_stream_t*)&ctx.channel));
  ASSERT(1 == uv_is_writable((uv_stream_t*)&ctx.channel));
  ASSERT(0 == uv_is_closing((uv_handle_t*)&ctx.channel));

  r = uv_read_start((uv_stream_t*)&ctx.channel, alloc_cb, read_cb);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
