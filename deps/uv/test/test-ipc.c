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

static uv_pipe_t channel;
static uv_tcp_t tcp_server;
static uv_tcp_t tcp_connection;

static int exit_cb_called;
static int read2_cb_called;
static int tcp_write_cb_called;
static int tcp_read_cb_called;
static int on_pipe_read_called;
static int local_conn_accepted;
static int remote_conn_accepted;
static int tcp_server_listening;
static uv_write_t write_req;
static uv_pipe_t channel;
static uv_tcp_t tcp_server;
static uv_write_t conn_notify_req;
static int close_cb_called;
static int connection_accepted;
static int tcp_conn_read_cb_called;
static int tcp_conn_write_cb_called;

typedef struct {
  uv_connect_t conn_req;
  uv_write_t tcp_write_req;
  uv_tcp_t conn;
} tcp_conn;

#define CONN_COUNT 100


static void close_server_conn_cb(uv_handle_t* handle) {
  free(handle);
}


static void on_connection(uv_stream_t* server, int status) {
  uv_tcp_t* conn;
  int r;

  if (!local_conn_accepted) {
    /* Accept the connection and close it.  Also and close the server. */
    ASSERT(status == 0);
    ASSERT((uv_stream_t*)&tcp_server == server);

    conn = malloc(sizeof(*conn));
    ASSERT(conn);
    r = uv_tcp_init(server->loop, conn);
    ASSERT(r == 0);

    r = uv_accept(server, (uv_stream_t*)conn);
    ASSERT(r == 0);

    uv_close((uv_handle_t*)conn, close_server_conn_cb);
    uv_close((uv_handle_t*)server, NULL);
    local_conn_accepted = 1;
  }
}


static void exit_cb(uv_process_t* process, int exit_status, int term_signal) {
  printf("exit_cb\n");
  exit_cb_called++;
  ASSERT(exit_status == 0);
  uv_close((uv_handle_t*)process, NULL);
}


static uv_buf_t on_alloc(uv_handle_t* handle, size_t suggested_size) {
  return uv_buf_init(malloc(suggested_size), suggested_size);
}


static void close_client_conn_cb(uv_handle_t* handle) {
  tcp_conn* p = (tcp_conn*)handle->data;
  free(p);
}


static void connect_cb(uv_connect_t* req, int status) {
  uv_close((uv_handle_t*)req->handle, close_client_conn_cb);
}


static void make_many_connections(void) {
  tcp_conn* conn;
  struct sockaddr_in addr;
  int r, i;

  for (i = 0; i < CONN_COUNT; i++) {
    conn = malloc(sizeof(*conn));
    ASSERT(conn);

    r = uv_tcp_init(uv_default_loop(), &conn->conn);
    ASSERT(r == 0);

    addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

    r = uv_tcp_connect(&conn->conn_req, (uv_tcp_t*)&conn->conn, addr, connect_cb);
    ASSERT(r == 0);

    conn->conn.data = conn;
  }
}


static void on_read(uv_pipe_t* pipe, ssize_t nread, uv_buf_t buf,
    uv_handle_type pending) {
  int r;
  uv_buf_t outbuf;
  uv_err_t err;

  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf.base);
    return;
  }

  if (nread < 0) {
    err = uv_last_error(pipe->loop);
    if (err.code == UV_EOF) {
      free(buf.base);
      return;
    }

    printf("error recving on channel: %s\n", uv_strerror(err));
    abort();
  }

  fprintf(stderr, "got %d bytes\n", (int)nread);

  if (!tcp_server_listening) {
    ASSERT(nread > 0 && buf.base && pending != UV_UNKNOWN_HANDLE);
    read2_cb_called++;

    /* Accept the pending TCP server, and start listening on it. */
    ASSERT(pending == UV_TCP);
    r = uv_tcp_init(uv_default_loop(), &tcp_server);
    ASSERT(r == 0);

    r = uv_accept((uv_stream_t*)pipe, (uv_stream_t*)&tcp_server);
    ASSERT(r == 0);

    r = uv_listen((uv_stream_t*)&tcp_server, 12, on_connection);
    ASSERT(r == 0);

    tcp_server_listening = 1;

    /* Make sure that the expected data is correctly multiplexed. */
    ASSERT(memcmp("hello\n", buf.base, nread) == 0);

    outbuf = uv_buf_init("world\n", 6);
    r = uv_write(&write_req, (uv_stream_t*)pipe, &outbuf, 1, NULL);
    ASSERT(r == 0);

    /* Create a bunch of connections to get both servers to accept. */
    make_many_connections();
  } else if (memcmp("accepted_connection\n", buf.base, nread) == 0) {
    /* Remote server has accepted a connection.  Close the channel. */
    ASSERT(pending == UV_UNKNOWN_HANDLE);
    remote_conn_accepted = 1;
    uv_close((uv_handle_t*)&channel, NULL);
  }

  free(buf.base);
}


void spawn_helper(uv_pipe_t* channel,
                  uv_process_t* process,
                  const char* helper) {
  uv_process_options_t options;
  size_t exepath_size;
  char exepath[1024];
  char* args[3];
  int r;
  uv_stdio_container_t stdio[1];

  r = uv_pipe_init(uv_default_loop(), channel, 1);
  ASSERT(r == 0);
  ASSERT(channel->ipc);

  exepath_size = sizeof(exepath);
  r = uv_exepath(exepath, &exepath_size);
  ASSERT(r == 0);

  exepath[exepath_size] = '\0';
  args[0] = exepath;
  args[1] = (char*)helper;
  args[2] = NULL;

  memset(&options, 0, sizeof(options));
  options.file = exepath;
  options.args = args;
  options.exit_cb = exit_cb;

  options.stdio = stdio;
  options.stdio[0].flags = UV_CREATE_PIPE |
    UV_READABLE_PIPE | UV_WRITABLE_PIPE;
  options.stdio[0].data.stream = (uv_stream_t*)channel;
  options.stdio_count = 1;

  r = uv_spawn(uv_default_loop(), process, options);
  ASSERT(r == 0);
}


static void on_tcp_write(uv_write_t* req, int status) {
  ASSERT(status == 0);
  ASSERT(req->handle == (uv_stream_t*)&tcp_connection);
  tcp_write_cb_called++;
}


static uv_buf_t on_read_alloc(uv_handle_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = (char*)malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}


static void on_tcp_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  ASSERT(nread > 0);
  ASSERT(memcmp("hello again\n", buf.base, nread) == 0);
  ASSERT(tcp == (uv_stream_t*)&tcp_connection);
  free(buf.base);

  tcp_read_cb_called++;

  uv_close((uv_handle_t*)tcp, NULL);
  uv_close((uv_handle_t*)&channel, NULL);
}


static void on_read_connection(uv_pipe_t* pipe, ssize_t nread, uv_buf_t buf,
    uv_handle_type pending) {
  int r;
  uv_buf_t outbuf;
  uv_err_t err;

  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf.base);
    return;
  }

  if (nread < 0) {
    err = uv_last_error(pipe->loop);
    if (err.code == UV_EOF) {
      free(buf.base);
      return;
    }

    printf("error recving on channel: %s\n", uv_strerror(err));
    abort();
  }

  fprintf(stderr, "got %d bytes\n", (int)nread);

  ASSERT(nread > 0 && buf.base && pending != UV_UNKNOWN_HANDLE);
  read2_cb_called++;

  /* Accept the pending TCP connection */
  ASSERT(pending == UV_TCP);
  r = uv_tcp_init(uv_default_loop(), &tcp_connection);
  ASSERT(r == 0);

  r = uv_accept((uv_stream_t*)pipe, (uv_stream_t*)&tcp_connection);
  ASSERT(r == 0);

  /* Make sure that the expected data is correctly multiplexed. */
  ASSERT(memcmp("hello\n", buf.base, nread) == 0);

  /* Write/read to/from the connection */
  outbuf = uv_buf_init("world\n", 6);
  r = uv_write(&write_req, (uv_stream_t*)&tcp_connection, &outbuf, 1,
    on_tcp_write);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)&tcp_connection, on_read_alloc, on_tcp_read);
  ASSERT(r == 0);

  free(buf.base);
}


static int run_ipc_test(const char* helper, uv_read2_cb read_cb) {
  uv_process_t process;
  int r;

  spawn_helper(&channel, &process, helper);
  uv_read2_start((uv_stream_t*)&channel, on_alloc, read_cb);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(ipc_listen_before_write) {
  int r = run_ipc_test("ipc_helper_listen_before_write", on_read);
  ASSERT(local_conn_accepted == 1);
  ASSERT(remote_conn_accepted == 1);
  ASSERT(read2_cb_called == 1);
  ASSERT(exit_cb_called == 1);
  return r;
}


TEST_IMPL(ipc_listen_after_write) {
  int r = run_ipc_test("ipc_helper_listen_after_write", on_read);
  ASSERT(local_conn_accepted == 1);
  ASSERT(remote_conn_accepted == 1);
  ASSERT(read2_cb_called == 1);
  ASSERT(exit_cb_called == 1);
  return r;
}


TEST_IMPL(ipc_tcp_connection) {
  int r = run_ipc_test("ipc_helper_tcp_connection", on_read_connection);
  ASSERT(read2_cb_called == 1);
  ASSERT(tcp_write_cb_called == 1);
  ASSERT(tcp_read_cb_called == 1);
  ASSERT(exit_cb_called == 1);
  return r;
}


#ifdef _WIN32
TEST_IMPL(listen_with_simultaneous_accepts) {
  uv_tcp_t server;
  int r;
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", TEST_PORT);

  r = uv_tcp_init(uv_default_loop(), &server);
  ASSERT(r == 0);

  r = uv_tcp_bind(&server, addr);
  ASSERT(r == 0);

  r = uv_tcp_simultaneous_accepts(&server, 1);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&server, SOMAXCONN, NULL);
  ASSERT(r == 0);
  ASSERT(server.reqs_pending == 32);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(listen_no_simultaneous_accepts) {
  uv_tcp_t server;
  int r;
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", TEST_PORT);

  r = uv_tcp_init(uv_default_loop(), &server);
  ASSERT(r == 0);

  r = uv_tcp_bind(&server, addr);
  ASSERT(r == 0);

  r = uv_tcp_simultaneous_accepts(&server, 0);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&server, SOMAXCONN, NULL);
  ASSERT(r == 0);
  ASSERT(server.reqs_pending == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif


/* Everything here runs in a child process. */

static tcp_conn conn;


static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}


static void conn_notify_write_cb(uv_write_t* req, int status) {
  uv_close((uv_handle_t*)&tcp_server, close_cb);
  uv_close((uv_handle_t*)&channel, close_cb);
}


static void tcp_connection_write_cb(uv_write_t* req, int status) {
  ASSERT((uv_handle_t*)&conn.conn == (uv_handle_t*)req->handle);
  uv_close((uv_handle_t*)req->handle, close_cb);
  uv_close((uv_handle_t*)&channel, close_cb);
  uv_close((uv_handle_t*)&tcp_server, close_cb);
  tcp_conn_write_cb_called++;
}


static void on_tcp_child_process_read(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  uv_buf_t outbuf;
  int r;

  if (nread < 0) {
    if (uv_last_error(tcp->loop).code == UV_EOF) {
      free(buf.base);
      return;
    }

    printf("error recving on tcp connection: %s\n",
      uv_strerror(uv_last_error(tcp->loop)));
    abort();
  }

  ASSERT(nread > 0);
  ASSERT(memcmp("world\n", buf.base, nread) == 0);
  on_pipe_read_called++;
  free(buf.base);

  /* Write to the socket */
  outbuf = uv_buf_init("hello again\n", 12);
  r = uv_write(&conn.tcp_write_req, tcp, &outbuf, 1, tcp_connection_write_cb);
  ASSERT(r == 0);

  tcp_conn_read_cb_called++;
}


static void connect_child_process_cb(uv_connect_t* req, int status) {
  int r;

  ASSERT(status == 0);
  r = uv_read_start(req->handle, on_read_alloc, on_tcp_child_process_read);
  ASSERT(r == 0);
}


static void ipc_on_connection(uv_stream_t* server, int status) {
  int r;
  uv_buf_t buf;

  if (!connection_accepted) {
    /*
     * Accept the connection and close it.  Also let the other
     * side know.
     */
    ASSERT(status == 0);
    ASSERT((uv_stream_t*)&tcp_server == server);

    r = uv_tcp_init(server->loop, &conn.conn);
    ASSERT(r == 0);

    r = uv_accept(server, (uv_stream_t*)&conn.conn);
    ASSERT(r == 0);

    uv_close((uv_handle_t*)&conn.conn, close_cb);

    buf = uv_buf_init("accepted_connection\n", 20);
    r = uv_write2(&conn_notify_req, (uv_stream_t*)&channel, &buf, 1,
      NULL, conn_notify_write_cb);
    ASSERT(r == 0);

    connection_accepted = 1;
  }
}


static void ipc_on_connection_tcp_conn(uv_stream_t* server, int status) {
  int r;
  uv_buf_t buf;
  uv_tcp_t* conn;

  ASSERT(status == 0);
  ASSERT((uv_stream_t*)&tcp_server == server);

  conn = malloc(sizeof(*conn));
  ASSERT(conn);

  r = uv_tcp_init(server->loop, conn);
  ASSERT(r == 0);

  r = uv_accept(server, (uv_stream_t*)conn);
  ASSERT(r == 0);

  /* Send the accepted connection to the other process */
  buf = uv_buf_init("hello\n", 6);
  r = uv_write2(&conn_notify_req, (uv_stream_t*)&channel, &buf, 1,
    (uv_stream_t*)conn, NULL);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)conn, on_read_alloc, on_tcp_child_process_read);
  ASSERT(r == 0);

  uv_close((uv_handle_t*)conn, close_cb);
}


int ipc_helper(int listen_after_write) {
  /*
   * This is launched from test-ipc.c. stdin is a duplex channel that we
   * over which a handle will be transmitted.
   */

  uv_write_t write_req;
  int r;
  uv_buf_t buf;

  r = uv_pipe_init(uv_default_loop(), &channel, 1);
  ASSERT(r == 0);

  uv_pipe_open(&channel, 0);

  ASSERT(uv_is_readable((uv_stream_t*) &channel));
  ASSERT(uv_is_writable((uv_stream_t*) &channel));
  ASSERT(!uv_is_closing((uv_handle_t*) &channel));

  r = uv_tcp_init(uv_default_loop(), &tcp_server);
  ASSERT(r == 0);

  r = uv_tcp_bind(&tcp_server, uv_ip4_addr("0.0.0.0", TEST_PORT));
  ASSERT(r == 0);

  if (!listen_after_write) {
    r = uv_listen((uv_stream_t*)&tcp_server, 12, ipc_on_connection);
    ASSERT(r == 0);
  }

  buf = uv_buf_init("hello\n", 6);
  r = uv_write2(&write_req, (uv_stream_t*)&channel, &buf, 1,
      (uv_stream_t*)&tcp_server, NULL);
  ASSERT(r == 0);

  if (listen_after_write) {
    r = uv_listen((uv_stream_t*)&tcp_server, 12, ipc_on_connection);
    ASSERT(r == 0);
  }

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(connection_accepted == 1);
  ASSERT(close_cb_called == 3);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


int ipc_helper_tcp_connection(void) {
  /*
   * This is launched from test-ipc.c. stdin is a duplex channel that we
   * over which a handle will be transmitted.
   */

  int r;
  struct sockaddr_in addr;

  r = uv_pipe_init(uv_default_loop(), &channel, 1);
  ASSERT(r == 0);

  uv_pipe_open(&channel, 0);

  ASSERT(uv_is_readable((uv_stream_t*)&channel));
  ASSERT(uv_is_writable((uv_stream_t*)&channel));
  ASSERT(!uv_is_closing((uv_handle_t*)&channel));

  r = uv_tcp_init(uv_default_loop(), &tcp_server);
  ASSERT(r == 0);

  r = uv_tcp_bind(&tcp_server, uv_ip4_addr("0.0.0.0", TEST_PORT));
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&tcp_server, 12, ipc_on_connection_tcp_conn);
  ASSERT(r == 0);

  /* Make a connection to the server */
  r = uv_tcp_init(uv_default_loop(), &conn.conn);
  ASSERT(r == 0);

  addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  r = uv_tcp_connect(&conn.conn_req, (uv_tcp_t*)&conn.conn, addr, connect_child_process_cb);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);

  ASSERT(tcp_conn_read_cb_called == 1);
  ASSERT(tcp_conn_write_cb_called == 1);
  ASSERT(close_cb_called == 4);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
