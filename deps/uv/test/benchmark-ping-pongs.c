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

/* Run the benchmark for this many ms */
#define TIME 5000


typedef struct {
  int pongs;
  int state;
  uv_tcp_t tcp;
  uv_req_t connect_req;
  uv_req_t shutdown_req;
} pinger_t;

typedef struct buf_s {
  uv_buf_t uv_buf_t;
  struct buf_s* next;
} buf_t;


static char PING[] = "PING\n";

static buf_t* buf_freelist = NULL;
static int pinger_shutdown_cb_called;
static int completed_pingers = 0;
static int64_t start_time;


static uv_buf_t buf_alloc(uv_tcp_t* tcp, size_t size) {
  buf_t* ab;

  ab = buf_freelist;

  if (ab != NULL) {
    buf_freelist = ab->next;
    return ab->uv_buf_t;
  }

  ab = (buf_t*) malloc(size + sizeof *ab);
  ab->uv_buf_t.len = size;
  ab->uv_buf_t.base = ((char*) ab) + sizeof *ab;

  return ab->uv_buf_t;
}


static void buf_free(uv_buf_t uv_buf_t) {
  buf_t* ab = (buf_t*) (uv_buf_t.base - sizeof *ab);

  ab->next = buf_freelist;
  buf_freelist = ab;
}


static void pinger_close_cb(uv_handle_t* handle, int status) {
  pinger_t* pinger;

  ASSERT(status == 0);

  pinger = (pinger_t*)handle->data;
  LOGF("ping_pongs: %d roundtrips/s\n", (1000 * pinger->pongs) / TIME);

  free(pinger);

  completed_pingers++;
}


static void pinger_write_cb(uv_req_t *req, int status) {
  ASSERT(status == 0);

  free(req);
}


static void pinger_write_ping(pinger_t* pinger) {
  uv_req_t *req;
  uv_buf_t buf;

  buf.base = (char*)&PING;
  buf.len = strlen(PING);

  req = (uv_req_t*)malloc(sizeof(*req));
  uv_req_init(req, (uv_handle_t*)(&pinger->tcp), pinger_write_cb);

  if (uv_write(req, &buf, 1)) {
    FATAL("uv_write failed");
  }
}


static void pinger_shutdown_cb(uv_handle_t* handle, int status) {
  ASSERT(status == 0);
  pinger_shutdown_cb_called++;

  /*
   * The close callback has not been triggered yet. We must wait for EOF
   * until we close the connection.
   */
  ASSERT(completed_pingers == 0);
}


static void pinger_read_cb(uv_tcp_t* tcp, int nread, uv_buf_t buf) {
  unsigned int i;
  pinger_t* pinger;

  pinger = (pinger_t*)tcp->data;

  if (nread < 0) {
    ASSERT(uv_last_error().code == UV_EOF);

    if (buf.base) {
      buf_free(buf);
    }

    ASSERT(pinger_shutdown_cb_called == 1);
    uv_close((uv_handle_t*)tcp);

    return;
  }

  /* Now we count the pings */
  for (i = 0; i < nread; i++) {
    ASSERT(buf.base[i] == PING[pinger->state]);
    pinger->state = (pinger->state + 1) % (sizeof(PING) - 1);
    if (pinger->state == 0) {
      pinger->pongs++;
      if (uv_now() - start_time > TIME) {
        uv_req_init(&pinger->shutdown_req, (uv_handle_t*)tcp, pinger_shutdown_cb);
        uv_shutdown(&pinger->shutdown_req);
        break;
      } else {
        pinger_write_ping(pinger);
      }
    }
  }

  buf_free(buf);
}


static void pinger_connect_cb(uv_req_t *req, int status) {
  pinger_t *pinger = (pinger_t*)req->handle->data;

  ASSERT(status == 0);

  pinger_write_ping(pinger);

  if (uv_read_start((uv_tcp_t*)(req->handle), buf_alloc, pinger_read_cb)) {
    FATAL("uv_read_start failed");
  }
}


static void pinger_new() {
  int r;
  struct sockaddr_in client_addr = uv_ip4_addr("0.0.0.0", 0);
  struct sockaddr_in server_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  pinger_t *pinger;

  pinger = (pinger_t*)malloc(sizeof(*pinger));
  pinger->state = 0;
  pinger->pongs = 0;

  /* Try to connec to the server and do NUM_PINGS ping-pongs. */
  r = uv_tcp_init(&pinger->tcp, pinger_close_cb, (void*)pinger);
  ASSERT(!r);

  /* We are never doing multiple reads/connects at a time anyway. */
  /* so these handles can be pre-initialized. */
  uv_req_init(&pinger->connect_req, (uv_handle_t*)&pinger->tcp,
      pinger_connect_cb);

  uv_bind(&pinger->tcp, client_addr);
  r = uv_connect(&pinger->connect_req, server_addr);
  ASSERT(!r);
}


BENCHMARK_IMPL(ping_pongs) {
  uv_init();
  start_time = uv_now();

  pinger_new();
  uv_run();

  ASSERT(completed_pingers == 1);

  return 0;
}
