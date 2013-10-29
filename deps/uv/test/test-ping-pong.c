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

static int completed_pingers = 0;

#define NUM_PINGS 1000

/* 64 bytes is enough for a pinger */
#define BUFSIZE 10240

static char PING[] = "PING\n";
static int pinger_on_connect_count;


typedef struct {
  int pongs;
  int state;
  union {
    uv_tcp_t tcp;
    uv_pipe_t pipe;
  } stream;
  uv_connect_t connect_req;
  char read_buffer[BUFSIZE];
} pinger_t;


static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
  buf->base = malloc(size);
  buf->len = size;
}


static void pinger_on_close(uv_handle_t* handle) {
  pinger_t* pinger = (pinger_t*)handle->data;

  ASSERT(NUM_PINGS == pinger->pongs);

  free(pinger);

  completed_pingers++;
}


static void pinger_after_write(uv_write_t *req, int status) {
  ASSERT(status == 0);
  free(req);
}


static void pinger_write_ping(pinger_t* pinger) {
  uv_write_t *req;
  uv_buf_t buf;

  buf = uv_buf_init(PING, sizeof(PING) - 1);

  req = malloc(sizeof(*req));
  if (uv_write(req,
               (uv_stream_t*) &pinger->stream.tcp,
               &buf,
               1,
               pinger_after_write)) {
    FATAL("uv_write failed");
  }

  puts("PING");
}


static void pinger_read_cb(uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf) {
  ssize_t i;
  pinger_t* pinger;

  pinger = (pinger_t*)stream->data;

  if (nread < 0) {
    ASSERT(nread == UV_EOF);

    puts("got EOF");
    free(buf->base);

    uv_close((uv_handle_t*)(&pinger->stream.tcp), pinger_on_close);

    return;
  }

  /* Now we count the pings */
  for (i = 0; i < nread; i++) {
    ASSERT(buf->base[i] == PING[pinger->state]);
    pinger->state = (pinger->state + 1) % (sizeof(PING) - 1);

    if (pinger->state != 0)
      continue;

    printf("PONG %d\n", pinger->pongs);
    pinger->pongs++;

    if (pinger->pongs < NUM_PINGS) {
      pinger_write_ping(pinger);
    } else {
      uv_close((uv_handle_t*)(&pinger->stream.tcp), pinger_on_close);
      break;
    }
  }

  free(buf->base);
}


static void pinger_on_connect(uv_connect_t *req, int status) {
  pinger_t *pinger = (pinger_t*)req->handle->data;

  pinger_on_connect_count++;

  ASSERT(status == 0);

  ASSERT(1 == uv_is_readable(req->handle));
  ASSERT(1 == uv_is_writable(req->handle));
  ASSERT(0 == uv_is_closing((uv_handle_t *) req->handle));

  pinger_write_ping(pinger);

  uv_read_start((uv_stream_t*)(req->handle), alloc_cb, pinger_read_cb);
}


/* same ping-pong test, but using IPv6 connection */
static void tcp_pinger_v6_new(void) {
  int r;
  struct sockaddr_in6 server_addr;
  pinger_t *pinger;

  ASSERT(0 ==uv_ip6_addr("::1", TEST_PORT, &server_addr));
  pinger = malloc(sizeof(*pinger));
  pinger->state = 0;
  pinger->pongs = 0;

  /* Try to connect to the server and do NUM_PINGS ping-pongs. */
  r = uv_tcp_init(uv_default_loop(), &pinger->stream.tcp);
  pinger->stream.tcp.data = pinger;
  ASSERT(!r);

  /* We are never doing multiple reads/connects at a time anyway. */
  /* so these handles can be pre-initialized. */
  r = uv_tcp_connect(&pinger->connect_req,
                     &pinger->stream.tcp,
                     (const struct sockaddr*) &server_addr,
                     pinger_on_connect);
  ASSERT(!r);

  /* Synchronous connect callbacks are not allowed. */
  ASSERT(pinger_on_connect_count == 0);
}


static void tcp_pinger_new(void) {
  int r;
  struct sockaddr_in server_addr;
  pinger_t *pinger;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &server_addr));
  pinger = malloc(sizeof(*pinger));
  pinger->state = 0;
  pinger->pongs = 0;

  /* Try to connect to the server and do NUM_PINGS ping-pongs. */
  r = uv_tcp_init(uv_default_loop(), &pinger->stream.tcp);
  pinger->stream.tcp.data = pinger;
  ASSERT(!r);

  /* We are never doing multiple reads/connects at a time anyway. */
  /* so these handles can be pre-initialized. */
  r = uv_tcp_connect(&pinger->connect_req,
                     &pinger->stream.tcp,
                     (const struct sockaddr*) &server_addr,
                     pinger_on_connect);
  ASSERT(!r);

  /* Synchronous connect callbacks are not allowed. */
  ASSERT(pinger_on_connect_count == 0);
}


static void pipe_pinger_new(void) {
  int r;
  pinger_t *pinger;

  pinger = (pinger_t*)malloc(sizeof(*pinger));
  pinger->state = 0;
  pinger->pongs = 0;

  /* Try to connect to the server and do NUM_PINGS ping-pongs. */
  r = uv_pipe_init(uv_default_loop(), &pinger->stream.pipe, 0);
  pinger->stream.pipe.data = pinger;
  ASSERT(!r);

  /* We are never doing multiple reads/connects at a time anyway. */
  /* so these handles can be pre-initialized. */

  uv_pipe_connect(&pinger->connect_req, &pinger->stream.pipe, TEST_PIPENAME,
      pinger_on_connect);

  /* Synchronous connect callbacks are not allowed. */
  ASSERT(pinger_on_connect_count == 0);
}


TEST_IMPL(tcp_ping_pong) {
  tcp_pinger_new();
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(completed_pingers == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(tcp_ping_pong_v6) {
  tcp_pinger_v6_new();
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(completed_pingers == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(pipe_ping_pong) {
  pipe_pinger_new();
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(completed_pingers == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
