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

#include <math.h>
#include <stdio.h>


static int TARGET_CONNECTIONS;
#define WRITE_BUFFER_SIZE           8192
#define MAX_SIMULTANEOUS_CONNECTS   100

#define PRINT_STATS                 0
#define STATS_INTERVAL              1000 /* msec */
#define STATS_COUNT                 5


static void do_write(uv_stream_t*);
static void maybe_connect_some();

static uv_req_t* req_alloc();
static void req_free(uv_req_t* uv_req);

static uv_buf_t buf_alloc(uv_stream_t*, size_t size);
static void buf_free(uv_buf_t uv_buf_t);


static uv_tcp_t server;
static struct sockaddr_in listen_addr;
static struct sockaddr_in connect_addr;

static int64_t start_time;

static int max_connect_socket = 0;
static int max_read_sockets = 0;
static int read_sockets = 0;
static int write_sockets = 0;

static int64_t nrecv = 0;
static int64_t nrecv_total = 0;
static int64_t nsent = 0;
static int64_t nsent_total = 0;

static int stats_left = 0;

static char write_buffer[WRITE_BUFFER_SIZE];

/* Make this as large as you need. */
#define MAX_WRITE_HANDLES 1000

static uv_tcp_t write_handles[MAX_WRITE_HANDLES];

static uv_timer_t timer_handle;


static double gbit(int64_t bytes, int64_t passed_ms) {
  double gbits = ((double)bytes / (1024 * 1024 * 1024)) * 8;
  return gbits / ((double)passed_ms / 1000);
}


static void show_stats(uv_timer_t* handle, int status) {
  int64_t diff;

#if PRINT_STATS
  LOGF("connections: %d, write: %.1f gbit/s\n",
       write_sockets,
       gbit(nsent, STATS_INTERVAL));
#endif

  /* Exit if the show is over */
  if (!--stats_left) {

    uv_update_time();
    diff = uv_now() - start_time;

    LOGF("pump%d_client: %.1f gbit/s\n", write_sockets,
        gbit(nsent_total, diff));

    exit(0);
  }

  /* Reset read and write counters */
  nrecv = 0;
  nsent = 0;
}


static void read_show_stats() {
  int64_t diff;

  uv_update_time();
  diff = uv_now() - start_time;

  LOGF("pump%d_server: %.1f gbit/s\n", max_read_sockets,
      gbit(nrecv_total, diff));
}



void write_sockets_close_cb(uv_handle_t* handle) {
  /* If any client closes, the process is done. */
  exit(0);
}


void read_sockets_close_cb(uv_handle_t* handle) {
  free(handle);
  read_sockets--;

  /* If it's past the first second and everyone has closed their connection
   * Then print stats.
   */
  if (uv_now() - start_time > 1000 && read_sockets == 0) {
    read_show_stats();
    uv_close((uv_handle_t*)&server, NULL);
  }
}


static void start_stats_collection() {
  uv_req_t* req = req_alloc();
  int r;

  /* Show-stats timer */
  stats_left = STATS_COUNT;
  r = uv_timer_init(&timer_handle);
  ASSERT(r == 0);
  r = uv_timer_start(&timer_handle, show_stats, STATS_INTERVAL, STATS_INTERVAL);
  ASSERT(r == 0);

  uv_update_time();
  start_time = uv_now();
}


static void read_cb(uv_stream_t* tcp, ssize_t bytes, uv_buf_t buf) {
  if (nrecv_total == 0) {
    ASSERT(start_time == 0);
    uv_update_time();
    start_time = uv_now();
  }

  if (bytes < 0) {
    uv_close((uv_handle_t*)tcp, read_sockets_close_cb);
    return;
  }

  buf_free(buf);

  nrecv += bytes;
  nrecv_total += bytes;
}


static void write_cb(uv_req_t *req, int status) {
  uv_buf_t* buf = (uv_buf_t*) req->data;

  ASSERT(status == 0);

  req_free(req);

  nsent += sizeof write_buffer;
  nsent_total += sizeof write_buffer;

  do_write((uv_stream_t*)req->handle);
}


static void do_write(uv_stream_t* tcp) {
  uv_req_t* req;
  uv_buf_t buf;
  int r;

  buf.base = (char*) &write_buffer;
  buf.len = sizeof write_buffer;

  while (tcp->write_queue_size == 0) {
    req = req_alloc();
    uv_req_init(req, (uv_handle_t*)tcp, write_cb);

    r = uv_write(req, &buf, 1);
    ASSERT(r == 0);
  }
}


static void connect_cb(uv_req_t* req, int status) {
  int i;

  if (status) LOG(uv_strerror(uv_last_error()));
  ASSERT(status == 0);

  write_sockets++;
  req_free(req);

  maybe_connect_some();

  if (write_sockets == TARGET_CONNECTIONS) {
    start_stats_collection();

    /* Yay! start writing */
    for (i = 0; i < write_sockets; i++) {
      do_write((uv_stream_t*)&write_handles[i]);
    }
  }
}


static void maybe_connect_some() {
  uv_req_t* req;
  uv_tcp_t* tcp;
  int r;

  while (max_connect_socket < TARGET_CONNECTIONS &&
         max_connect_socket < write_sockets + MAX_SIMULTANEOUS_CONNECTS) {
    tcp = &write_handles[max_connect_socket++];

    r = uv_tcp_init(tcp);
    ASSERT(r == 0);

    req = req_alloc();
    uv_req_init(req, (uv_handle_t*)tcp, connect_cb);
    r = uv_tcp_connect(req, connect_addr);
    ASSERT(r == 0);
  }
}


static void connection_cb(uv_handle_t* s, int status) {
  uv_tcp_t* tcp;
  int r;

  ASSERT(&server == (uv_tcp_t*)s);
  ASSERT(status == 0);

  tcp = malloc(sizeof(uv_tcp_t));

  uv_tcp_init(tcp);

  r = uv_accept(s, (uv_stream_t*)tcp);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)tcp, buf_alloc, read_cb);
  ASSERT(r == 0);

  read_sockets++;
  max_read_sockets++;
}


/*
 * Request allocator
 */

typedef struct req_list_s {
  uv_req_t uv_req;
  struct req_list_s* next;
} req_list_t;


static req_list_t* req_freelist = NULL;


static uv_req_t* req_alloc() {
  req_list_t* req;

  req = req_freelist;
  if (req != NULL) {
    req_freelist = req->next;
    return (uv_req_t*) req;
  }

  req = (req_list_t*) malloc(sizeof *req);
  return (uv_req_t*) req;
}


static void req_free(uv_req_t* uv_req) {
  req_list_t* req = (req_list_t*) uv_req;

  req->next = req_freelist;
  req_freelist = req;
}


/*
 * Buffer allocator
 */

typedef struct buf_list_s {
  uv_buf_t uv_buf_t;
  struct buf_list_s* next;
} buf_list_t;


static buf_list_t* buf_freelist = NULL;


static uv_buf_t buf_alloc(uv_stream_t* tcp, size_t size) {
  buf_list_t* buf;

  buf = buf_freelist;
  if (buf != NULL) {
    buf_freelist = buf->next;
    return buf->uv_buf_t;
  }

  buf = (buf_list_t*) malloc(size + sizeof *buf);
  buf->uv_buf_t.len = (unsigned int)size;
  buf->uv_buf_t.base = ((char*) buf) + sizeof *buf;

  return buf->uv_buf_t;
}


static void buf_free(uv_buf_t uv_buf_t) {
  buf_list_t* buf = (buf_list_t*) (uv_buf_t.base - sizeof *buf);

  buf->next = buf_freelist;
  buf_freelist = buf;
}


HELPER_IMPL(pump_server) {
  int r;

  uv_init();
  listen_addr = uv_ip4_addr("0.0.0.0", TEST_PORT);

  /* Server */
  r = uv_tcp_init(&server);
  ASSERT(r == 0);
  r = uv_tcp_bind(&server, listen_addr);
  ASSERT(r == 0);
  r = uv_tcp_listen(&server, MAX_WRITE_HANDLES, connection_cb);
  ASSERT(r == 0);

  uv_run();

  return 0;
}


void pump(int n) {
  ASSERT(n <= MAX_WRITE_HANDLES);
  TARGET_CONNECTIONS = n;

  uv_init();

  connect_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  /* Start making connections */
  maybe_connect_some();

  uv_run();
}


BENCHMARK_IMPL(pump100_client) {
  pump(100);
  return 0;
}


BENCHMARK_IMPL(pump1_client) {
  pump(1);
  return 0;
}
