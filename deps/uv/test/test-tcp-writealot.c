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


#define WRITES            3
#define CHUNKS_PER_WRITE  4096
#define CHUNK_SIZE        10024 /* 10 kb */

#define TOTAL_BYTES       (WRITES * CHUNKS_PER_WRITE * CHUNK_SIZE)

static char* send_buffer;

static int shutdown_cb_called = 0;
static int connect_cb_called = 0;
static int write_cb_called = 0;
static int close_cb_called = 0;
static size_t bytes_sent = 0;
static size_t bytes_sent_done = 0;
static size_t bytes_received_done = 0;

static uv_connect_t connect_req;
static uv_shutdown_t shutdown_req;
static uv_write_t write_reqs[WRITES];


static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
  buf->base = malloc(size);
  buf->len = size;
}


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  close_cb_called++;
}


static void shutdown_cb(uv_shutdown_t* req, int status) {
  uv_tcp_t* tcp;

  ASSERT(req == &shutdown_req);
  ASSERT(status == 0);

  tcp = (uv_tcp_t*)(req->handle);

  /* The write buffer should be empty by now. */
  ASSERT(tcp->write_queue_size == 0);

  /* Now we wait for the EOF */
  shutdown_cb_called++;

  /* We should have had all the writes called already. */
  ASSERT(write_cb_called == WRITES);
}


static void read_cb(uv_stream_t* tcp, ssize_t nread, const uv_buf_t* buf) {
  ASSERT(tcp != NULL);

  if (nread >= 0) {
    bytes_received_done += nread;
  }
  else {
    ASSERT(nread == UV_EOF);
    printf("GOT EOF\n");
    uv_close((uv_handle_t*)tcp, close_cb);
  }

  free(buf->base);
}


static void write_cb(uv_write_t* req, int status) {
  ASSERT(req != NULL);

  if (status) {
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(status));
    ASSERT(0);
  }

  bytes_sent_done += CHUNKS_PER_WRITE * CHUNK_SIZE;
  write_cb_called++;
}


static void connect_cb(uv_connect_t* req, int status) {
  uv_buf_t send_bufs[CHUNKS_PER_WRITE];
  uv_stream_t* stream;
  int i, j, r;

  ASSERT(req == &connect_req);
  ASSERT(status == 0);

  stream = req->handle;
  connect_cb_called++;

  /* Write a lot of data */
  for (i = 0; i < WRITES; i++) {
    uv_write_t* write_req = write_reqs + i;

    for (j = 0; j < CHUNKS_PER_WRITE; j++) {
      send_bufs[j] = uv_buf_init(send_buffer + bytes_sent, CHUNK_SIZE);
      bytes_sent += CHUNK_SIZE;
    }

    r = uv_write(write_req, stream, send_bufs, CHUNKS_PER_WRITE, write_cb);
    ASSERT(r == 0);
  }

  /* Shutdown on drain. */
  r = uv_shutdown(&shutdown_req, stream, shutdown_cb);
  ASSERT(r == 0);

  /* Start reading */
  r = uv_read_start(stream, alloc_cb, read_cb);
  ASSERT(r == 0);
}


TEST_IMPL(tcp_writealot) {
  struct sockaddr_in addr;
  uv_tcp_t client;
  int r;

  ASSERT(0 == uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

  send_buffer = calloc(1, TOTAL_BYTES);
  ASSERT(send_buffer != NULL);

  r = uv_tcp_init(uv_default_loop(), &client);
  ASSERT(r == 0);

  r = uv_tcp_connect(&connect_req,
                     &client,
                     (const struct sockaddr*) &addr,
                     connect_cb);
  ASSERT(r == 0);

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(shutdown_cb_called == 1);
  ASSERT(connect_cb_called == 1);
  ASSERT(write_cb_called == WRITES);
  ASSERT(close_cb_called == 1);
  ASSERT(bytes_sent == TOTAL_BYTES);
  ASSERT(bytes_sent_done == TOTAL_BYTES);
  ASSERT(bytes_received_done == TOTAL_BYTES);

  free(send_buffer);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
