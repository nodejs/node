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
#include <string.h> /* strlen */

/* Run the benchmark for this many ms */
#define TIME 5000


typedef struct {
  int pongs;
  int state;
  uv_tcp_t tcp;
  uv_connect_t connect_req;
  uv_shutdown_t shutdown_req;
} pinger_t;

typedef struct buf_s {
  uv_buf_t uv_buf_t;
  struct buf_s* next;
} buf_t;


static char PING[] = "PING\n";

static uv_loop_t* loop;

static buf_t* buf_freelist = NULL;
static int pinger_shutdown_cb_called;
static int completed_pingers = 0;
static int64_t start_time;


static uv_buf_t buf_alloc(uv_handle_t* tcp, size_t size) {
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


static void pinger_close_cb(uv_handle_t* handle) {
  pinger_t* pinger;

  pinger = (pinger_t*)handle->data;
  LOGF("ping_pongs: %d roundtrips/s\n", (1000 * pinger->pongs) / TIME);

  free(pinger);

  completed_pingers++;
}


static void pinger_write_cb(uv_write_t* req, int status) {
  ASSERT(status == 0);

  free(req);
}


static void pinger_write_ping(pinger_t* pinger) {
  uv_write_t* req;
  uv_buf_t buf;

  buf.base = (char*)&PING;
  buf.len = strlen(PING);

  req = malloc(sizeof *req);
  if (uv_write(req, (uv_stream_t*) &pinger->tcp, &buf, 1, pinger_write_cb)) {
    FATAL("uv_write failed");
  }
}


static void pinger_shutdown_cb(uv_shutdown_t* req, int status) {
  ASSERT(status == 0);
  pinger_shutdown_cb_called++;

  /*
   * The close callback has not been triggered yet. We must wait for EOF
   * until we close the connection.
   */
  ASSERT(completed_pingers == 0);
}


static void pinger_read_cb(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  ssize_t i;
  pinger_t* pinger;

  pinger = (pinger_t*)tcp->data;

  if (nread < 0) {
    ASSERT(uv_last_error(loop).code == UV_EOF);

    if (buf.base) {
      buf_free(buf);
    }

    ASSERT(pinger_shutdown_cb_called == 1);
    uv_close((uv_handle_t*)tcp, pinger_close_cb);

    return;
  }

  /* Now we count the pings */
  for (i = 0; i < nread; i++) {
    ASSERT(buf.base[i] == PING[pinger->state]);
    pinger->state = (pinger->state + 1) % (sizeof(PING) - 1);
    if (pinger->state == 0) {
      pinger->pongs++;
      if (uv_now(loop) - start_time > TIME) {
        uv_shutdown(&pinger->shutdown_req, (uv_stream_t*) tcp, pinger_shutdown_cb);
        break;
      } else {
        pinger_write_ping(pinger);
      }
    }
  }

  buf_free(buf);
}


static void pinger_connect_cb(uv_connect_t* req, int status) {
  pinger_t *pinger = (pinger_t*)req->handle->data;

  ASSERT(status == 0);

  pinger_write_ping(pinger);

  if (uv_read_start(req->handle, buf_alloc, pinger_read_cb)) {
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

  /* Try to connect to the server and do NUM_PINGS ping-pongs. */
  r = uv_tcp_init(loop, &pinger->tcp);
  ASSERT(!r);

  pinger->tcp.data = pinger;

  uv_tcp_bind(&pinger->tcp, client_addr);

  r = uv_tcp_connect(&pinger->connect_req, &pinger->tcp, server_addr, pinger_connect_cb);
  ASSERT(!r);
}


BENCHMARK_IMPL(ping_pongs) {
  loop = uv_default_loop();

  start_time = uv_now(loop);

  pinger_new();
  uv_run(loop);

  ASSERT(completed_pingers == 1);

  return 0;
}
