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
#include <stdio.h>
#include <stdlib.h>


#define WRITES            3
#define CHUNKS_PER_WRITE  3
#define CHUNK_SIZE        10485760 /* 10 MB */

#define TOTAL_BYTES       (WRITES * CHUNKS_PER_WRITE * CHUNK_SIZE)


static char* send_buffer;


static int shutdown_cb_called = 0;
static int connect_cb_called = 0;
static int write_cb_called = 0;
static int close_cb_called = 0;
static int bytes_sent = 0;
static int bytes_sent_done = 0;
static int bytes_received = 0;
static int bytes_received_done = 0;


static void close_cb(uv_handle_t* handle, int status) {
  ASSERT(handle != NULL);
  ASSERT(status == 0);

  free(handle);

  close_cb_called++;
}


static void shutdown_cb(uv_req_t* req, int status) {
  ASSERT(req);
  ASSERT(status == 0);

  /* The write buffer should be empty by now. */
  ASSERT(req->handle->write_queue_size == 0);

  /* Now we wait for the EOF */
  shutdown_cb_called++;

  /* We should have had all the writes called already. */
  ASSERT(write_cb_called == WRITES);

  free(req);
}


static void read_cb(uv_handle_t* handle, int nread, uv_buf buf) {
  ASSERT(handle != NULL);

  if (nread < 0) {
    ASSERT(uv_last_error().code == UV_EOF);
    printf("GOT EOF\n");

    if (buf.base) {
      free(buf.base);
    }

    uv_close(handle);
    return;
  }

  bytes_received_done += nread;

  free(buf.base);
}


static void write_cb(uv_req_t* req, int status) {
  ASSERT(req != NULL);

  if (status) {
    uv_err_t err = uv_last_error();
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
    ASSERT(0);
  }

  bytes_sent_done += CHUNKS_PER_WRITE * CHUNK_SIZE;
  write_cb_called++;

  free(req);
}


static void connect_cb(uv_req_t* req, int status) {
  uv_buf send_bufs[CHUNKS_PER_WRITE];
  uv_handle_t* handle;
  int i, j, r;

  ASSERT(req != NULL);
  ASSERT(status == 0);

  handle = req->handle;

  connect_cb_called++;
  free(req);

  /* Write a lot of data */
  for (i = 0; i < WRITES; i++) {
    for (j = 0; j < CHUNKS_PER_WRITE; j++) {
      send_bufs[j].len = CHUNK_SIZE;
      send_bufs[j].base = send_buffer + bytes_sent;
      bytes_sent += CHUNK_SIZE;
    }

    req = (uv_req_t*)malloc(sizeof *req);
    ASSERT(req != NULL);

    uv_req_init(req, handle, write_cb);
    r = uv_write(req, (uv_buf*)&send_bufs, CHUNKS_PER_WRITE);
    ASSERT(r == 0);
  }

  /* Shutdown on drain. FIXME: dealloc req? */
  req = (uv_req_t*) malloc(sizeof(uv_req_t));
  ASSERT(req != NULL);
  uv_req_init(req, handle, shutdown_cb);
  r = uv_shutdown(req);
  ASSERT(r == 0);

  /* Start reading */
  req = (uv_req_t*)malloc(sizeof *req);
  ASSERT(req != NULL);

  uv_req_init(req, handle, read_cb);
  r = uv_read_start(handle, read_cb);
  ASSERT(r == 0);
}


static uv_buf alloc_cb(uv_handle_t* handle, size_t size) {
  uv_buf buf;
  buf.base = (char*)malloc(size);
  buf.len = size;
  return buf;
}


TEST_IMPL(tcp_writealot) {
  struct sockaddr_in addr = uv_ip4_addr("127.0.0.1", TEST_PORT);
  uv_handle_t* client = (uv_handle_t*)malloc(sizeof *client);
  uv_req_t* connect_req = (uv_req_t*)malloc(sizeof *connect_req);
  int r;

  ASSERT(client != NULL);
  ASSERT(connect_req != NULL);

  send_buffer = (char*)malloc(TOTAL_BYTES + 1);

  ASSERT(send_buffer != NULL);

  uv_init(alloc_cb);

  r = uv_tcp_init(client, close_cb, NULL);
  ASSERT(r == 0);

  uv_req_init(connect_req, client, connect_cb);
  r = uv_connect(connect_req, (struct sockaddr*)&addr);
  ASSERT(r == 0);

  uv_run();

  ASSERT(shutdown_cb_called == 1);
  ASSERT(connect_cb_called == 1);
  ASSERT(write_cb_called == WRITES);
  ASSERT(close_cb_called == 1);
  ASSERT(bytes_sent == TOTAL_BYTES);
  ASSERT(bytes_sent_done == TOTAL_BYTES);
  ASSERT(bytes_received_done == TOTAL_BYTES);

  return 0;
}
