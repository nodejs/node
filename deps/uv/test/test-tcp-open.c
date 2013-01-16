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
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
# include <unistd.h>
#endif

static int shutdown_cb_called = 0;
static int connect_cb_called = 0;
static int write_cb_called = 0;
static int close_cb_called = 0;

static uv_connect_t connect_req;
static uv_shutdown_t shutdown_req;
static uv_write_t write_req;


static void startup(void) {
#ifdef _WIN32
    struct WSAData wsa_data;
    int r = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    ASSERT(r == 0);
#endif
}


static uv_os_sock_t create_tcp_socket(void) {
  uv_os_sock_t sock;

  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
#ifdef _WIN32
  ASSERT(sock != INVALID_SOCKET);
#else
  ASSERT(sock >= 0);
#endif

#ifndef _WIN32
  {
    /* Allow reuse of the port. */
    int yes = 1;
    int r = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    ASSERT(r == 0);
  }
#endif

  return sock;
}


static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size) {
  static char slab[65536];
  ASSERT(suggested_size <= sizeof slab);
  return uv_buf_init(slab, sizeof slab);
}


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  close_cb_called++;
}


static void shutdown_cb(uv_shutdown_t* req, int status) {
  ASSERT(req == &shutdown_req);
  ASSERT(status == 0);

  /* Now we wait for the EOF */
  shutdown_cb_called++;
}


static void read_cb(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  ASSERT(tcp != NULL);

  if (nread >= 0) {
    ASSERT(nread == 4);
    ASSERT(memcmp("PING", buf.base, nread) == 0);
  }
  else {
    ASSERT(uv_last_error(uv_default_loop()).code == UV_EOF);
    printf("GOT EOF\n");
    uv_close((uv_handle_t*)tcp, close_cb);
  }
}


static void write_cb(uv_write_t* req, int status) {
  ASSERT(req != NULL);

  if (status) {
    uv_err_t err = uv_last_error(uv_default_loop());
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
    ASSERT(0);
  }

  write_cb_called++;
}


static void connect_cb(uv_connect_t* req, int status) {
  uv_buf_t buf = uv_buf_init("PING", 4);
  uv_stream_t* stream;
  int r;

  ASSERT(req == &connect_req);
  ASSERT(status == 0);

  stream = req->handle;
  connect_cb_called++;

  r = uv_write(&write_req, stream, &buf, 1, write_cb);
  ASSERT(r == 0);

  /* Shutdown on drain. */
  r = uv_shutdown(&shutdown_req, stream, shutdown_cb);
  ASSERT(r == 0);

  /* Start reading */
  r = uv_read_start(stream, alloc_cb, read_cb);
  ASSERT(r == 0);
}


TEST_IMPL(tcp_open) {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_tcp_t client;
  uv_os_sock_t sock;
  int r;

  startup();
  sock = create_tcp_socket();

  r = uv_tcp_init(uv_default_loop(), &client);
  ASSERT(r == 0);

  r = uv_tcp_open(&client, sock);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req, &client, addr, connect_cb);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(shutdown_cb_called == 1);
  ASSERT(connect_cb_called == 1);
  ASSERT(write_cb_called == 1);
  ASSERT(close_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
