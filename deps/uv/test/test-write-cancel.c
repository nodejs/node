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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uv.h"
#include "task.h"

#define REQ_COUNT 100

static int close_cb_called;
static int write_cb_called;
static int cancelled_count;
static int connected;
static int closing;

static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}


/*
 * tcp_write_cancel
 */
static uv_tcp_t server;
static uv_tcp_t client;
static uv_tcp_t incoming;
static uv_write_t write_reqs[REQ_COUNT];
static char buf_data[16 * 1024];

static void connection_cb(uv_stream_t* tcp, int status) {
  ASSERT_OK(status);
  ASSERT_OK(uv_tcp_init(tcp->loop, &incoming));
  ASSERT_OK(uv_accept(tcp, (uv_stream_t*) &incoming));
  connected++;
  ASSERT_EQ(1, connected);
}

static void write_cb(uv_write_t* req, int status) {
  write_cb_called++;
  if (status == UV_ECANCELED && closing == 0)
    cancelled_count++;

  if (cancelled_count >= 5 && closing == 0) {
    closing++;
    ASSERT_EQ(1, closing);
    uv_close((uv_handle_t*) &client, close_cb);
    uv_close((uv_handle_t*) &server, close_cb);
    if (connected)
      uv_close((uv_handle_t*) &incoming, close_cb);
  }
}

static void connect_cb(uv_connect_t* req, int status) {
  uv_buf_t buf;
  int r;
  int i;
  int cancel_count;

  ASSERT_OK(status);

  buf = uv_buf_init(buf_data, sizeof(buf_data));

  /* Queue many writes to fill the socket buffer */
  for (i = 0; i < REQ_COUNT; i++) {
    r = uv_write(&write_reqs[i],
                 req->handle,
                 &buf,
                 1,
                 write_cb);
    ASSERT_OK(r);
  }

  /* Cancel the trailing writes which should be queued */
  cancel_count = 0;
  for (i = REQ_COUNT - 5; i < REQ_COUNT; i++) {
    r = uv_cancel((uv_req_t*) &write_reqs[i]);
    ASSERT_OK(r);
    cancel_count++;
  }

  ASSERT_EQ(5, cancel_count);
}

TEST_IMPL(tcp_write_cancel) {
  uv_connect_t connect_req;
  struct sockaddr_in addr;
  uv_loop_t* loop;
  int buffer_size;

  loop = uv_default_loop();

  close_cb_called = 0;
  write_cb_called = 0;
  cancelled_count = 0;
  connected = 0;
  closing = 0;
  buffer_size = sizeof(buf_data);
  memset(buf_data, 'A', sizeof(buf_data));

  ASSERT_OK(uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));
  ASSERT_OK(uv_tcp_init(loop, &server));
  ASSERT_OK(uv_tcp_bind(&server, (struct sockaddr*) &addr, 0));
  ASSERT_OK(uv_listen((uv_stream_t*) &server, 128, connection_cb));

  ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
  ASSERT_OK(uv_tcp_init(loop, &client));
  ASSERT_OK(uv_tcp_connect(&connect_req,
                           &client,
                           (struct sockaddr*) &addr,
                           connect_cb));
  /* Set the send buffer size small to ensure that writes get queued in
   * userspace rather than fitting entirely in the kernel's send buffer.
   * Also set the receive buffer small so the receiver doesn't drain data
   * and relieve backpressure. Note: Linux sets the actual buffer to twice
   * the requested size. */
  ASSERT_OK(uv_send_buffer_size((uv_handle_t*) &client, &buffer_size));
  ASSERT_OK(uv_recv_buffer_size((uv_handle_t*) &client, &buffer_size));

  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));

  /* The writes we cancelled should have gotten UV_ECANCELED callbacks */
  ASSERT_EQ(5, cancelled_count);
  ASSERT_EQ(2 + connected, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}


/*
 * pipe_write_cancel / pipe_write_cancel_all
 *
 * uv_pipe() with no flags creates non-overlapped handles, so on Windows
 * these exercise the synchronous (thread pool) write cancellation path.
 * The _overlapped variants pass UV_NONBLOCK_PIPE so that on Windows the
 * handles are overlapped and cancellation goes through CancelIoEx. Half
 * the writes use multiple buffers to exercise the coalesced write path.
 * The _ipc variant makes the writer an ipc pipe, whose framing writes are
 * always coalesced.
 */
static uv_pipe_t pipe_cancel_writer;
static uv_pipe_t pipe_cancel_reader;
static uv_pipe_t pipe_cancel_listener;
static uv_connect_t pipe_cancel_connect_req;
static int pipe_cancel_connected;
static int pipe_cancel_accepted;
static uv_write_t pipe_cancel_write_reqs[REQ_COUNT];
static char pipe_cancel_buf_data[64 * 1024];
static char pipe_cancel_after_close_buf_data[1024 * 1024];
static int cancel_target;
static uv_write_cb pipe_cancel_cb;

static void pipe_cancel_connect_cb(uv_connect_t* req, int status) {
  ASSERT_OK(status);
  pipe_cancel_connected = 1;
}

static void pipe_cancel_connection_cb(uv_stream_t* listener, int status) {
  ASSERT_OK(status);
  ASSERT_OK(uv_pipe_init(listener->loop, &pipe_cancel_reader, 0));
  ASSERT_OK(uv_accept(listener, (uv_stream_t*) &pipe_cancel_reader));
  pipe_cancel_accepted = 1;
}

static void pipe_cancel_setup(uv_loop_t* loop, int pipe_flags, int ipc) {
  uv_buf_t bufs[4];
  uv_file fds[2];
  int r;
  int i;

  close_cb_called = 0;
  write_cb_called = 0;
  cancelled_count = 0;
  closing = 0;
  memset(pipe_cancel_buf_data, 'D', sizeof(pipe_cancel_buf_data));

  if (ipc) {
    /* uv_pipe() endpoints are unidirectional, and an ipc pipe must be
     * duplex (uv_pipe_open() refuses anything else on Windows). Establish
     * a loopback named-pipe connection instead. */
    pipe_cancel_connected = 0;
    pipe_cancel_accepted = 0;
    ASSERT_OK(uv_pipe_init(loop, &pipe_cancel_listener, 0));
    ASSERT_OK(uv_pipe_bind(&pipe_cancel_listener, TEST_PIPENAME));
    ASSERT_OK(uv_listen((uv_stream_t*) &pipe_cancel_listener,
                        1,
                        pipe_cancel_connection_cb));
    ASSERT_OK(uv_pipe_init(loop, &pipe_cancel_writer, 1));
    uv_pipe_connect(&pipe_cancel_connect_req,
                    &pipe_cancel_writer,
                    TEST_PIPENAME,
                    pipe_cancel_connect_cb);
    while (!pipe_cancel_connected || !pipe_cancel_accepted)
      uv_run(loop, UV_RUN_ONCE);
    uv_close((uv_handle_t*) &pipe_cancel_listener, NULL);
  } else {
    ASSERT_OK(uv_pipe(fds, pipe_flags, pipe_flags));

    ASSERT_OK(uv_pipe_init(loop, &pipe_cancel_writer, 0));
    ASSERT_OK(uv_pipe_init(loop, &pipe_cancel_reader, 0));

    ASSERT_OK(uv_pipe_open(&pipe_cancel_writer, fds[1]));
    ASSERT_OK(uv_pipe_open(&pipe_cancel_reader, fds[0]));
  }

  bufs[0] = uv_buf_init(pipe_cancel_buf_data,
                         sizeof(pipe_cancel_buf_data) / 4);
  bufs[1] = uv_buf_init(pipe_cancel_buf_data +
                         sizeof(pipe_cancel_buf_data) / 4,
                         sizeof(pipe_cancel_buf_data) / 4);
  bufs[2] = uv_buf_init(pipe_cancel_buf_data +
                         sizeof(pipe_cancel_buf_data) / 2,
                         sizeof(pipe_cancel_buf_data) / 4);
  bufs[3] = uv_buf_init(pipe_cancel_buf_data +
                         3 * sizeof(pipe_cancel_buf_data) / 4,
                         sizeof(pipe_cancel_buf_data) / 4);

  /* Queue many writes to fill the pipe buffer. Alternate between single-buffer
   * and multi-buffer writes so the latter exercise the coalesced write path
   * on Windows. */
  for (i = 0; i < REQ_COUNT; i++) {
    if (i % 2 == 0) {
      r = uv_write(&pipe_cancel_write_reqs[i],
                   (uv_stream_t*) &pipe_cancel_writer,
                   &bufs[0],
                   1,
                   pipe_cancel_cb);
    } else {
      r = uv_write(&pipe_cancel_write_reqs[i],
                   (uv_stream_t*) &pipe_cancel_writer,
                   bufs,
                   4,
                   pipe_cancel_cb);
    }
    ASSERT_OK(r);
  }
}

static void pipe_cancel_close(void) {
  if (closing == 0) {
    closing++;
    ASSERT_EQ(1, closing);
    uv_close((uv_handle_t*) &pipe_cancel_writer, close_cb);
    uv_close((uv_handle_t*) &pipe_cancel_reader, close_cb);
  }
}

static void pipe_cancel_write_cb_tail(uv_write_t* req, int status) {
  write_cb_called++;
  if (status == UV_ECANCELED && closing == 0)
    cancelled_count++;

  if (cancelled_count >= cancel_target && closing == 0)
    pipe_cancel_close();
}

static int pipe_write_cancel_run(int pipe_flags, int ipc) {
  uv_loop_t* loop;
  int i;

  loop = uv_default_loop();
  cancel_target = 5;
  pipe_cancel_cb = pipe_cancel_write_cb_tail;
  pipe_cancel_setup(loop, pipe_flags, ipc);

  /* Cancel the trailing writes which should be queued. */
  for (i = REQ_COUNT - 5; i < REQ_COUNT; i++)
    ASSERT_OK(uv_cancel((uv_req_t*) &pipe_cancel_write_reqs[i]));

  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));

  ASSERT_EQ(5, cancelled_count);
  ASSERT_EQ(2, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}

TEST_IMPL(pipe_write_cancel) {
  return pipe_write_cancel_run(0, 0);
}

TEST_IMPL(pipe_write_cancel_overlapped) {
  return pipe_write_cancel_run(UV_NONBLOCK_PIPE, 0);
}

TEST_IMPL(pipe_write_cancel_ipc) {
  return pipe_write_cancel_run(UV_NONBLOCK_PIPE, 1);
}

static void pipe_cancel_write_cb_all(uv_write_t* req, int status) {
  write_cb_called++;
  if (status == UV_ECANCELED)
    cancelled_count++;
}

static int pipe_write_cancel_all_run(int pipe_flags, int ipc) {
  uv_loop_t* loop;
  int i;

  loop = uv_default_loop();
  pipe_cancel_cb = pipe_cancel_write_cb_all;
  pipe_cancel_setup(loop, pipe_flags, ipc);

  /* Cancel all writes. The first few may have already completed; one may be
   * currently blocking in the thread pool (on Windows). Both cases are
   * exercised by cancelling every request in order. */
  for (i = 0; i < REQ_COUNT; i++)
    uv_cancel((uv_req_t*) &pipe_cancel_write_reqs[i]);

  /* Close the handles so the event loop can drain. */
  uv_close((uv_handle_t*) &pipe_cancel_writer, close_cb);
  uv_close((uv_handle_t*) &pipe_cancel_reader, close_cb);

  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));

  /* Every write must have gotten a callback. */
  ASSERT_EQ(REQ_COUNT, write_cb_called);
  /* We don't know exactly how many were cancelled vs already completed,
   * but at least the tail end should have been cancelled. */
  ASSERT_GT(cancelled_count, 0);
  ASSERT_EQ(2, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}

TEST_IMPL(pipe_write_cancel_all) {
  return pipe_write_cancel_all_run(0, 0);
}

TEST_IMPL(pipe_write_cancel_all_overlapped) {
  return pipe_write_cancel_all_run(UV_NONBLOCK_PIPE, 0);
}

TEST_IMPL(pipe_write_cancel_after_close) {
  uv_buf_t buf;
  uv_file fds[2];
  uv_loop_t* loop;
  uv_pipe_t reader;
  uv_pipe_t writer;
  uv_write_t req;

  loop = uv_default_loop();
  close_cb_called = 0;
  write_cb_called = 0;
  cancelled_count = 0;
  memset(pipe_cancel_after_close_buf_data,
         'D',
         sizeof(pipe_cancel_after_close_buf_data));

  ASSERT_OK(uv_pipe(fds, 0, 0));
  ASSERT_OK(uv_pipe_init(loop, &writer, 0));
  ASSERT_OK(uv_pipe_init(loop, &reader, 0));
  ASSERT_OK(uv_pipe_open(&writer, fds[1]));
  ASSERT_OK(uv_pipe_open(&reader, fds[0]));

  buf = uv_buf_init(pipe_cancel_after_close_buf_data,
                    sizeof(pipe_cancel_after_close_buf_data));
  ASSERT_OK(uv_write(&req,
                     (uv_stream_t*) &writer,
                     &buf,
                     1,
                     pipe_cancel_write_cb_all));

  uv_close((uv_handle_t*) &writer, close_cb);
  ASSERT_EQ(UV_EBUSY, uv_cancel((uv_req_t*) &req));
  uv_close((uv_handle_t*) &reader, close_cb);

  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));

  ASSERT_EQ(1, write_cb_called);
  ASSERT_EQ(1, cancelled_count);
  ASSERT_EQ(2, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}


/*
 * tcp_write_nwritten
 */
static uv_tcp_t nwritten_server;
static uv_tcp_t nwritten_client;
static uv_tcp_t nwritten_incoming;
static uv_write_t nwritten_req;
static int nwritten_cb_called;
static int nwritten_accepted;
static size_t nwritten_value;
static char nwritten_buf_data[1024];

/* The client's write can complete before the server's connection callback
 * has run (and thus before nwritten_incoming is initialized), and vice
 * versa; tear down only once both have happened. */
static void nwritten_maybe_close(void) {
  if (nwritten_cb_called == 0 || !nwritten_accepted)
    return;
  uv_close((uv_handle_t*) &nwritten_client, close_cb);
  uv_close((uv_handle_t*) &nwritten_server, close_cb);
  uv_close((uv_handle_t*) &nwritten_incoming, close_cb);
}

static void nwritten_write_cb(uv_write_t* req, int status) {
  ASSERT_OK(status);
  nwritten_cb_called++;
  nwritten_value = uv_write_nwritten(req);
  nwritten_maybe_close();
}

static void nwritten_connection_cb(uv_stream_t* tcp, int status) {
  ASSERT_OK(status);
  ASSERT_OK(uv_tcp_init(tcp->loop, &nwritten_incoming));
  ASSERT_OK(uv_accept(tcp, (uv_stream_t*) &nwritten_incoming));
  nwritten_accepted = 1;
  nwritten_maybe_close();
}

static void nwritten_connect_cb(uv_connect_t* req, int status) {
  uv_buf_t buf;

  ASSERT_OK(status);

  buf = uv_buf_init(nwritten_buf_data, sizeof(nwritten_buf_data));
  ASSERT_OK(uv_write(&nwritten_req, req->handle, &buf, 1, nwritten_write_cb));
}

TEST_IMPL(tcp_write_nwritten) {
  uv_connect_t connect_req;
  struct sockaddr_in addr;
  uv_loop_t* loop;

  loop = uv_default_loop();

  close_cb_called = 0;
  nwritten_cb_called = 0;
  nwritten_accepted = 0;
  nwritten_value = 0;
  memset(nwritten_buf_data, 'B', sizeof(nwritten_buf_data));

  ASSERT_OK(uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));
  ASSERT_OK(uv_tcp_init(loop, &nwritten_server));
  ASSERT_OK(uv_tcp_bind(&nwritten_server, (struct sockaddr*) &addr, 0));
  ASSERT_OK(uv_listen((uv_stream_t*) &nwritten_server, 128, nwritten_connection_cb));

  ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));
  ASSERT_OK(uv_tcp_init(loop, &nwritten_client));
  ASSERT_OK(uv_tcp_connect(&connect_req,
                           &nwritten_client,
                           (struct sockaddr*) &addr,
                           nwritten_connect_cb));

  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));

  ASSERT_EQ(1, nwritten_cb_called);
  ASSERT_EQ(sizeof(nwritten_buf_data), nwritten_value);
  ASSERT_EQ(3, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}


/*
 * pipe_write_nwritten
 */
static uv_pipe_t pipe_client;
static uv_pipe_t pipe_server;
static uv_write_t pipe_write_req;
static int pipe_cb_called;
static size_t pipe_nwritten_value;
static char pipe_buf_data[1024];

static void pipe_write_cb(uv_write_t* req, int status) {
  ASSERT_OK(status);
  pipe_cb_called++;
  pipe_nwritten_value = uv_write_nwritten(req);

  uv_close((uv_handle_t*) &pipe_client, close_cb);
  uv_close((uv_handle_t*) &pipe_server, close_cb);
}

TEST_IMPL(pipe_write_nwritten) {
  uv_loop_t* loop;
  uv_buf_t buf;
  uv_file fds[2];

  loop = uv_default_loop();

  close_cb_called = 0;
  pipe_cb_called = 0;
  pipe_nwritten_value = 0;
  memset(pipe_buf_data, 'C', sizeof(pipe_buf_data));

  ASSERT_OK(uv_pipe(fds, 0, 0));

  ASSERT_OK(uv_pipe_init(loop, &pipe_client, 0));
  ASSERT_OK(uv_pipe_init(loop, &pipe_server, 0));

  ASSERT_OK(uv_pipe_open(&pipe_client, fds[1]));
  ASSERT_OK(uv_pipe_open(&pipe_server, fds[0]));

  buf = uv_buf_init(pipe_buf_data, sizeof(pipe_buf_data));
  ASSERT_OK(uv_write(&pipe_write_req,
                     (uv_stream_t*) &pipe_client,
                     &buf,
                     1,
                     pipe_write_cb));

  ASSERT_OK(uv_run(loop, UV_RUN_DEFAULT));

  ASSERT_EQ(1, pipe_cb_called);
  ASSERT_EQ(sizeof(pipe_buf_data), pipe_nwritten_value);
  ASSERT_EQ(2, close_cb_called);

  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}
