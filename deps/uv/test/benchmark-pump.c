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
#include "../uv.h"

#include <math.h>
#include <stdio.h>


#define TARGET_CONNECTIONS          100
#define WRITE_BUFFER_SIZE           8192
#define MAX_SIMULTANEOUS_CONNECTS   100

#define PRINT_STATS                 0
#define STATS_INTERVAL              1000 /* msec */
#define STATS_COUNT                 5


static void do_write(uv_handle_t* handle);
static void maybe_connect_some();

static uv_req_t* req_alloc();
static void req_free(uv_req_t* uv_req);

static uv_buf_t buf_alloc(uv_handle_t* handle, size_t size);
static void buf_free(uv_buf_t uv_buf_t);


static struct sockaddr_in listen_addr;
static struct sockaddr_in connect_addr;

static int64_t start_time;

static int max_connect_socket = 0;
static int read_sockets = 0;
static int write_sockets = 0;

static int64_t nrecv = 0;
static int64_t nrecv_total = 0;
static int64_t nsent = 0;
static int64_t nsent_total = 0;

static int stats_left = 0;

static char write_buffer[WRITE_BUFFER_SIZE];

static uv_handle_t read_handles[TARGET_CONNECTIONS];
static uv_handle_t write_handles[TARGET_CONNECTIONS];

static uv_handle_t timer_handle;


static double gbit(int64_t bytes, int64_t passed_ms) {
  double gbits = ((double)bytes / (1024 * 1024 * 1024)) * 8;
  return gbits / ((double)passed_ms / 1000);
}


static void show_stats(uv_handle_t *handle, int status) {
  int64_t diff;

#if PRINT_STATS
  LOGF("connections: %d, read: %.1f gbit/s, write: %.1f gbit/s\n",
       read_sockets,
       gbit(nrecv, STATS_INTERVAL),
       gbit(nsent, STATS_INTERVAL));
#endif

  /* Exit if the show is over */
  if (!--stats_left) {

    uv_update_time();
    diff = uv_now() - start_time;

    LOGF("pump_%d: %.1f gbit/s\n", read_sockets,
        gbit(nrecv_total, diff));

    exit(0);
  }

  /* Reset read and write counters */
  nrecv = 0;
  nsent = 0;
}


void close_cb(uv_handle_t* handle, int status) {
  ASSERT(status == 0);
}


static void start_stats_collection() {
  uv_req_t* req = req_alloc();
  int r;

  /* Show-stats timer */
  stats_left = STATS_COUNT;
  r = uv_timer_init(&timer_handle, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_timer_start(&timer_handle, show_stats, STATS_INTERVAL, STATS_INTERVAL);
  ASSERT(r == 0);
}


static void read_cb(uv_handle_t* handle, int bytes, uv_buf_t buf) {
  ASSERT(bytes >= 0);

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

  do_write(req->handle);
}


static void do_write(uv_handle_t* handle) {
  uv_req_t* req;
  uv_buf_t buf;
  int r;

  buf.base = (char*) &write_buffer;
  buf.len = sizeof write_buffer;

  while (handle->write_queue_size == 0) {
    req = req_alloc();
    uv_req_init(req, handle, write_cb);

    r = uv_write(req, &buf, 1);
    ASSERT(r == 0);
  }
}

static void maybe_start_writing() {
  int i;

  if (read_sockets == TARGET_CONNECTIONS &&
      write_sockets == TARGET_CONNECTIONS) {
    start_stats_collection();

    /* Yay! start writing */
    for (i = 0; i < write_sockets; i++) {
      do_write(&write_handles[i]);
    }
  }
}


static void connect_cb(uv_req_t* req, int status) {
  if (status) LOG(uv_strerror(uv_last_error()));
  ASSERT(status == 0);

  write_sockets++;
  req_free(req);

  maybe_connect_some();
  maybe_start_writing();
}


static void do_connect(uv_handle_t* handle, struct sockaddr* addr) {
  uv_req_t* req;
  int r;

  r = uv_tcp_init(handle, close_cb, NULL);
  ASSERT(r == 0);

  req = req_alloc();
  uv_req_init(req, handle, connect_cb);
  r = uv_connect(req, addr);
  ASSERT(r == 0);
}


static void maybe_connect_some() {
  while (max_connect_socket < TARGET_CONNECTIONS &&
         max_connect_socket < write_sockets + MAX_SIMULTANEOUS_CONNECTS) {
    do_connect(&write_handles[max_connect_socket++],
               (struct sockaddr*) &connect_addr);
  }
}


static void accept_cb(uv_handle_t* server) {
  uv_handle_t* handle;
  int r;

  ASSERT(read_sockets < TARGET_CONNECTIONS);
  handle = &read_handles[read_sockets];

  r = uv_accept(server, handle, close_cb, NULL);
  ASSERT(r == 0);

  r = uv_read_start(handle, read_cb);
  ASSERT(r == 0);

  read_sockets++;

  maybe_start_writing();
}


BENCHMARK_IMPL(pump) {
  uv_handle_t server;
  int r;

  uv_init(buf_alloc);

  listen_addr = uv_ip4_addr("0.0.0.0", TEST_PORT);
  connect_addr = uv_ip4_addr("127.0.0.1", TEST_PORT);

  /* Server */
  r = uv_tcp_init(&server, close_cb, NULL);
  ASSERT(r == 0);
  r = uv_bind(&server, (struct sockaddr*) &listen_addr);
  ASSERT(r == 0);
  r = uv_listen(&server, TARGET_CONNECTIONS, accept_cb);
  ASSERT(r == 0);

  uv_update_time();
  start_time = uv_now();

  /* Start making connections */
  maybe_connect_some();

  uv_run();

  return 0;
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


static uv_buf_t buf_alloc(uv_handle_t* handle, size_t size) {
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
