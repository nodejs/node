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
#include "runner.h"
#include "task.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  uv_loop_t* loop;
  uv_thread_t thread;
  uv_async_t* recv_channel;
  uv_async_t* send_channel;
  uv_tcp_t server;
  uv_tcp_t conn;
  int connection_accepted;
  int close_cb_called;
} worker_t;

static uv_async_t send_channel;
static uv_async_t recv_channel;
static worker_t parent;
static worker_t child;

static volatile uv_stream_info_t dup_stream;

typedef struct {
  uv_connect_t conn_req;
  uv_tcp_t conn;
} tcp_conn;

#define CONN_COUNT 100

static void close_cb(uv_handle_t* handle) {
  worker_t* worker = (worker_t*)handle->data;
  ASSERT(worker);
  worker->close_cb_called++;
}


static void on_connection(uv_stream_t* server, int status) {
  int r;
  worker_t* worker = container_of(server, worker_t, server);

  if (!worker->connection_accepted) {
    /*
     * Accept the connection and close it.
     */
    ASSERT(status == 0);

    r = uv_tcp_init(server->loop, &worker->conn);
    ASSERT(r == 0);

    worker->conn.data = worker;

    r = uv_accept(server, (uv_stream_t*)&worker->conn);
    ASSERT(r == 0);

    worker->connection_accepted = 1;

    uv_close((uv_handle_t*)worker->recv_channel, close_cb);
    uv_close((uv_handle_t*)&worker->conn, close_cb);
    uv_close((uv_handle_t*)server, close_cb);
  }
}


static void close_client_conn_cb(uv_handle_t* handle) {
  tcp_conn* p = (tcp_conn*)handle->data;
  free(p);
}


static void connect_cb(uv_connect_t* req, int status) {
  uv_close((uv_handle_t*)req->handle, close_client_conn_cb);
}


static void make_many_connections() {
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


void on_parent_msg(uv_async_t* handle, int status) {
  int r;

  ASSERT(dup_stream.type == UV_TCP);

  /* Import the shared TCP server, and start listening on it. */
  r = uv_tcp_init(parent.loop, &parent.server);
  ASSERT(r == 0);

  parent.server.data = &parent;

  r = uv_import((uv_stream_t*)&parent.server,
    (uv_stream_info_t*)&dup_stream);
  ASSERT(r == 0);

  r = uv_listen((uv_stream_t*)&parent.server, 12, on_connection);
  ASSERT(r == 0);

  /* Create a bunch of connections to get both servers to accept. */
  make_many_connections();
}


void on_child_msg(uv_async_t* handle, int status) {
  ASSERT(!"no");
}


static void child_thread_entry(void* arg) {
  int r;
  int listen_after_write = *(int*) arg;

  r = uv_tcp_init(child.loop, &child.server);
  ASSERT(r == 0);

  child.server.data = &child;

  r = uv_tcp_bind(&child.server, uv_ip4_addr("0.0.0.0", TEST_PORT));
  ASSERT(r == 0);

  if (!listen_after_write) {
    r = uv_listen((uv_stream_t*)&child.server, 12, on_connection);
    ASSERT(r == 0);
  }

  r = uv_export((uv_stream_t*)&child.server,
    (uv_stream_info_t*)&dup_stream);
  ASSERT(r == 0);

  r = uv_async_send(child.send_channel);
  ASSERT(r == 0);

  if (listen_after_write) {
    r = uv_listen((uv_stream_t*)&child.server, 12, on_connection);
    ASSERT(r == 0);
  }

  r = uv_run(child.loop);
  ASSERT(r == 0);

  ASSERT(child.connection_accepted == 1);
  ASSERT(child.close_cb_called == 3);
}


static void run_ipc_threads_test(int listen_after_write) {
  int r;

  parent.send_channel = &send_channel;
  parent.recv_channel = &recv_channel;
  child.send_channel = &recv_channel;
  child.recv_channel = &send_channel;

  parent.loop = uv_default_loop();
  child.loop = uv_loop_new();
  ASSERT(child.loop);

  r = uv_async_init(parent.loop, parent.recv_channel, on_parent_msg);
  ASSERT(r == 0);
  parent.recv_channel->data = &parent;

  r = uv_async_init(child.loop, child.recv_channel, on_child_msg);
  ASSERT(r == 0);
  child.recv_channel->data = &child;

  r = uv_thread_create(&child.thread, child_thread_entry, &listen_after_write);
  ASSERT(r == 0);

  r = uv_run(parent.loop);
  ASSERT(r == 0);

  ASSERT(parent.connection_accepted == 1);
  ASSERT(parent.close_cb_called == 3);

  r = uv_thread_join(&child.thread);
  ASSERT(r == 0);

  /* Satisfy valgrind. Maybe we should delete the default loop from the
   * test runner.
   */
  uv_loop_delete(child.loop);
  uv_loop_delete(uv_default_loop());
}


TEST_IMPL(ipc_threads_listen_after_write) {
  run_ipc_threads_test(1);
  return 0;
}


TEST_IMPL(ipc_threads_listen_before_write) {
  run_ipc_threads_test(0);
  return 0;
}
