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

#include "../uv.h"
#include "task.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strlen */

static int completed_pingers = 0;

#define NUM_PINGS 1000

/* 64 bytes is enough for a pinger */
#define BUFSIZE 10240

static char PING[] = "PING\n";


typedef struct {
  int pongs;
  int state;
  uv_tcp_t tcp;
  uv_req_t connect_req;
  uv_req_t read_req;
  char read_buffer[BUFSIZE];
} pinger_t;

void pinger_try_read(pinger_t* pinger);


static uv_buf_t alloc_cb(uv_tcp_t* tcp, size_t size) {
  uv_buf_t buf;
  buf.base = (char*)malloc(size);
  buf.len = size;
  return buf;
}


static void pinger_on_close(uv_handle_t* handle, int status) {
  pinger_t* pinger = (pinger_t*)handle->data;

  ASSERT(status == 0);
  ASSERT(NUM_PINGS == pinger->pongs);

  free(pinger);

  completed_pingers++;
}


static void pinger_after_write(uv_req_t *req, int status) {
  ASSERT(status == 0);

  free(req);
}


static void pinger_write_ping(pinger_t* pinger) {
  uv_req_t *req;
  uv_buf_t buf;

  buf.base = (char*)&PING;
  buf.len = strlen(PING);

  req = (uv_req_t*)malloc(sizeof(*req));
  uv_req_init(req, (uv_handle_t*)(&pinger->tcp), pinger_after_write);

  if (uv_write(req, &buf, 1)) {
    FATAL("uv_write failed");
  }

  puts("PING");
}


static void pinger_read_cb(uv_tcp_t* tcp, int nread, uv_buf_t buf) {
  unsigned int i;
  pinger_t* pinger;

  pinger = (pinger_t*)tcp->data;

  if (nread < 0) {
    ASSERT(uv_last_error().code == UV_EOF);

    puts("got EOF");

    if (buf.base) {
      free(buf.base);
    }

    uv_close((uv_handle_t*)(&pinger->tcp));

    return;
  }

  /* Now we count the pings */
  for (i = 0; i < nread; i++) {
    ASSERT(buf.base[i] == PING[pinger->state]);
    pinger->state = (pinger->state + 1) % (sizeof(PING) - 1);
    if (pinger->state == 0) {
      printf("PONG %d\n", pinger->pongs);
      pinger->pongs++;
      if (pinger->pongs < NUM_PINGS) {
        pinger_write_ping(pinger);
      } else {
        uv_close((uv_handle_t*)(&pinger->tcp));
        return;
      }
    }
  }
}


static void pinger_on_connect(uv_req_t *req, int status) {
  pinger_t *pinger = (pinger_t*)req->handle->data;

  ASSERT(status == 0);

  pinger_write_ping(pinger);

  uv_read_start((uv_tcp_t*)(req->handle), alloc_cb, pinger_read_cb);
}


static void pinger_new() {
  int r;
  struct sockaddr_in server_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  pinger_t *pinger;

  pinger = (pinger_t*)malloc(sizeof(*pinger));
  pinger->state = 0;
  pinger->pongs = 0;

  /* Try to connec to the server and do NUM_PINGS ping-pongs. */
  r = uv_tcp_init(&pinger->tcp, pinger_on_close, (void*)pinger);
  ASSERT(!r);

  /* We are never doing multiple reads/connects at a time anyway. */
  /* so these handles can be pre-initialized. */
  uv_req_init(&pinger->connect_req, (uv_handle_t*)(&pinger->tcp),
      pinger_on_connect);

  r = uv_connect(&pinger->connect_req, server_addr);
  ASSERT(!r);
}


TEST_IMPL(ping_pong) {
  uv_init();

  pinger_new();
  uv_run();

  ASSERT(completed_pingers == 1);

  return 0;
}
