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

#include <stdlib.h>
#include <stdio.h>

static int getsocknamecount = 0;


static uv_tcp_t tcp;
static uv_req_t connect_req;
static uv_tcp_t tcpServer;


static uv_buf_t alloc(uv_stream_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = (char*) malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}


static void on_close(uv_handle_t* peer) {
  free(peer);
  uv_close((uv_handle_t*)&tcpServer, NULL);
}


static void after_shutdown(uv_req_t* req, int status) {
  uv_close(req->handle, on_close);
  free(req);
}


static void after_read(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  uv_req_t* req;

  if (buf.base) {
    free(buf.base);
  }

  req = (uv_req_t*) malloc(sizeof *req);
  uv_req_init(req, (uv_handle_t*)handle, (void *(*)(void *))after_shutdown);
  uv_shutdown(req);
}


static void on_connection(uv_handle_t* server, int status) {
  struct sockaddr sockname;
  int namelen = sizeof(sockname);
  uv_handle_t* handle;
  int r;

  if (status != 0) {
    fprintf(stderr, "Connect error %d\n", uv_last_error().code);
  }
  ASSERT(status == 0);

  handle = (uv_handle_t*) malloc(sizeof(uv_tcp_t));
  ASSERT(handle != NULL);

  uv_tcp_init((uv_tcp_t*)handle);

  /* associate server with stream */
  handle->data = server;

  r = uv_accept(server, (uv_stream_t*)handle);
  ASSERT(r == 0);

  status = uv_getsockname((uv_tcp_t*)handle, &sockname, &namelen);
  if (status != 0) {
    fprintf(stderr, "uv_getsockname error (accepted) %d\n", uv_last_error().code);
  }
  ASSERT(status == 0);

  getsocknamecount++;

  r = uv_read_start((uv_stream_t*)handle, alloc, after_read);
  ASSERT(r == 0);

}


static void on_connect(void* req) {
  struct sockaddr sockname;
  int namelen = sizeof(sockname);
  int status;

  status = uv_getsockname(&tcp, &sockname, &namelen);
  if (status != 0) {
    fprintf(stderr, "uv_getsockname error (connector) %d\n", uv_last_error().code);
  }
  ASSERT(status == 0);

  getsocknamecount++;

  uv_close((uv_handle_t*)&tcp, NULL);
}


static int tcp_listener(int port) {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", port);
  struct sockaddr sockname;
  int namelen = sizeof(sockname);
  int r;

  r = uv_tcp_init(&tcpServer);
  if (r) {
    fprintf(stderr, "Socket creation error\n");
    return 1;
  }

  r = uv_tcp_bind(&tcpServer, addr);
  if (r) {
    fprintf(stderr, "Bind error\n");
    return 1;
  }

  r = uv_tcp_listen(&tcpServer, 128, on_connection);
  if (r) {
    fprintf(stderr, "Listen error\n");
    return 1;
  }

  r = uv_getsockname(&tcpServer, &sockname, &namelen);
  if (r != 0) {
    fprintf(stderr, "uv_getsockname error (listening) %d\n", uv_last_error().code);
  }
  ASSERT(r == 0);
  getsocknamecount++;

  return 0;
}


static void tcp_connector() {
  int r;
  struct sockaddr_in server_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  r = uv_tcp_init(&tcp);
  tcp.data = &connect_req;
  ASSERT(!r);

  uv_req_init(&connect_req, (uv_handle_t*)(&tcp), (void *(*)(void *))on_connect);

  r = uv_tcp_connect(&connect_req, server_addr);
  ASSERT(!r);
}


TEST_IMPL(getsockname) {
  uv_init();

  if (tcp_listener(TEST_PORT))
    return 1;

  tcp_connector();

  uv_run();

  ASSERT(getsocknamecount == 3);

  return 0;
}

