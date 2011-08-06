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

#include "task.h"
#include "uv.h"

/* Base class for tcp_conn_rec and pipe_conn_rec.
 * The ordering of fields matters!
 */
typedef struct {
  uv_connect_t conn_req;
  uv_write_t write_req;
  uv_stream_t stream;
} conn_rec;

typedef struct {
  uv_connect_t conn_req;
  uv_write_t write_req;
  uv_tcp_t stream;
} tcp_conn_rec;

typedef struct {
  uv_connect_t conn_req;
  uv_write_t write_req;
  uv_pipe_t stream;
} pipe_conn_rec;

static char buffer[] = "QS";

static int64_t start_time, end_time;
static int closed_streams, concurrency;

typedef void *(*setup_fn)(int num, void* arg);
typedef int (*connect_fn)(int num, void* handles, void* arg);

static uv_buf_t alloc_cb(uv_stream_t* stream, size_t suggested_size);
static void connect_cb(uv_connect_t* conn_req, int status);
static void read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
static void close_cb(uv_handle_t* handle);


static uv_buf_t alloc_cb(uv_stream_t* stream, size_t suggested_size) {
  static char slab[65536];
  uv_buf_t buf;
  buf.base = slab;
  buf.len = sizeof(slab);
  return buf;
}


static void connect_cb(uv_connect_t* req, int status) {
  conn_rec* conn;
  uv_buf_t buf;
  int r;

  ASSERT(req != NULL);
  ASSERT(status == 0);

  conn = req->data;
  ASSERT(conn != NULL);

  r = uv_read_start(&conn->stream, alloc_cb, read_cb);
  ASSERT(r == 0);

  buf.base = buffer;
  buf.len = sizeof(buffer) - 1;

  r = uv_write(&conn->write_req, &conn->stream, &buf, 1, NULL);
  ASSERT(r == 0);
}


static void read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  ASSERT(stream != NULL);
  ASSERT(nread == -1 && uv_last_error().code == UV_EOF);
  uv_close((uv_handle_t*)stream, close_cb);
}


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  closed_streams++;

  if (closed_streams == concurrency) {
    uv_update_time();
    end_time = uv_now();
  }
}


static void* tcp_do_setup(int num, void* arg) {
  tcp_conn_rec* conns;
  tcp_conn_rec* pe;
  tcp_conn_rec* p;
  int r;

  concurrency = num;
  closed_streams = 0;

  conns = calloc(num, sizeof(tcp_conn_rec));
  ASSERT(conns != NULL);

  for (p = conns, pe = p + num; p < pe; p++) {
    r = uv_tcp_init(&p->stream);
    ASSERT(r == 0);
    p->stream.data = p;
    p->conn_req.data = p;
    p->write_req.data = p;
    p->conn_req.handle = (uv_stream_t*)&p->stream;
    p->write_req.handle = (uv_stream_t*)&p->stream;
  }

  return conns;
}


static void* pipe_do_setup(int num, void* arg) {
  pipe_conn_rec* conns;
  pipe_conn_rec* pe;
  pipe_conn_rec* p;
  int r;

  concurrency = num;
  closed_streams = 0;

  conns = calloc(num, sizeof(pipe_conn_rec));
  ASSERT(conns != NULL);

  for (p = conns, pe = p + num; p < pe; p++) {
    r = uv_pipe_init(&p->stream);
    ASSERT(r == 0);
    p->stream.data = p;
    p->conn_req.data = p;
    p->write_req.data = p;
    p->conn_req.handle = (uv_stream_t*)&p->stream;
    p->write_req.handle = (uv_stream_t*)&p->stream;
  }

  return conns;
}


static int tcp_do_connect(int num, void* conns, void* arg) {
  struct sockaddr_in addr;
  tcp_conn_rec* pe;
  tcp_conn_rec* p;
  int r;

  addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  for (p = conns, pe = p + num; p < pe; p++) {
    r = uv_tcp_connect(&p->conn_req, &p->stream, addr, connect_cb);
    ASSERT(r == 0);
  }

  return 0;
}


static int pipe_do_connect(int num, void* conns, void* arg) {
  pipe_conn_rec* pe;
  pipe_conn_rec* p;
  int r;

  for (p = conns, pe = p + num; p < pe; p++) {
    r = uv_pipe_connect(&p->conn_req, &p->stream, TEST_PIPENAME, connect_cb);
    ASSERT(r == 0);
  }

  return 0;
}


static int pound_it(int concurrency,
                    const char* type,
                    setup_fn do_setup,
                    connect_fn do_connect,
                    void* arg) {
  void* state;
  int r;

  uv_init();

  state = do_setup(concurrency, arg);
  ASSERT(state != NULL);

  uv_update_time();
  start_time = uv_now();

  r = do_connect(concurrency, state, arg);
  ASSERT(!r);

  uv_run();

  LOGF("%s-conn-pound-%d: %.0f accepts/s\n",
       type, concurrency, 
       (double) concurrency / (double) (end_time - start_time) * 1000.0);

  return 0;
}


BENCHMARK_IMPL(tcp4_pound_100) {
  return pound_it(100, "tcp", tcp_do_setup, tcp_do_connect, NULL);
}


BENCHMARK_IMPL(tcp4_pound_1000) {
  return pound_it(1000, "tcp", tcp_do_setup, tcp_do_connect, NULL);
}


BENCHMARK_IMPL(pipe_pound_100) {
  return pound_it(100, "pipe", pipe_do_setup, pipe_do_connect, NULL);
}


BENCHMARK_IMPL(pipe_pound_1000) {
  return pound_it(1000, "pipe", pipe_do_setup, pipe_do_connect, NULL);
}