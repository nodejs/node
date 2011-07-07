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


typedef struct {
  uv_req_t req;
  uv_buf_t buf;
} write_req_t;


/* used to track multiple DNS requests received */
typedef struct {
  char* prevbuf_ptr;
  int prevbuf_pos;
  int prevbuf_rem;
} dnsstate;


/* modify handle to append dnsstate */
typedef struct {
  uv_tcp_t handle;
  dnsstate state;
} dnshandle;


static int server_closed;
static uv_tcp_t server;


static void after_write(uv_req_t* req, int status);
static void after_read(uv_stream_t*, ssize_t nread, uv_buf_t buf);
static void on_close(uv_handle_t* peer);
static void on_server_close(uv_handle_t* handle);
static void on_connection(uv_handle_t*, int status);

#define WRITE_BUF_LEN   (64*1024)
#define DNSREC_LEN      (4)

#define LEN_OFFSET 0
#define QUERYID_OFFSET 2
unsigned char DNSRsp[] = {0, 43, 0, 0, 0x81, 0x80, 0, 1, 0, 1, 0, 0, 0, 0 };
unsigned char qrecord[] = {5, 'e', 'c', 'h', 'o', 's', 3, 's', 'r', 'v', 0, 0, 1, 0, 1};
unsigned char arecord[] = {0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 5, 0xbd, 0, 4, 10, 0, 1, 1 };


static void after_write(uv_req_t* req, int status) {
  write_req_t* wr;

  if (status) {
    uv_err_t err = uv_last_error();
    fprintf(stderr, "uv_write error: %s\n", uv_strerror(err));
    ASSERT(0);
  }

  wr = (write_req_t*) req;

  /* Free the read/write buffer and the request */
  free(wr->buf.base);
  free(wr);
}


static void after_shutdown(uv_req_t* req, int status) {
  uv_close(req->handle, on_close);
  free(req);
}


static void addrsp(write_req_t* wr, char* hdr) {
  char * dnsrsp;
  short int rsplen;
  short int* reclen;

  rsplen = sizeof(DNSRsp) + sizeof(qrecord) + sizeof(arecord);

  ASSERT (rsplen + wr->buf.len < WRITE_BUF_LEN);

  dnsrsp = wr->buf.base + wr->buf.len;

  /* copy stock response */
  memcpy(dnsrsp, DNSRsp, sizeof(DNSRsp));
  memcpy(dnsrsp + sizeof(DNSRsp), qrecord, sizeof(qrecord));
  memcpy(dnsrsp + sizeof(DNSRsp) + sizeof(qrecord), arecord, sizeof(arecord));

  /* overwrite with network order length and id from request header */
  reclen = (short int*)dnsrsp;
  *reclen = htons(rsplen-2);
  dnsrsp[QUERYID_OFFSET] = hdr[QUERYID_OFFSET];
  dnsrsp[QUERYID_OFFSET+1] = hdr[QUERYID_OFFSET+1];

  wr->buf.len += rsplen;
}

static void process_req(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  write_req_t *wr;
  dnshandle* dns = (dnshandle*)handle;
  char hdrbuf[DNSREC_LEN];
  int hdrbuf_remaining = DNSREC_LEN;
  int rec_remaining = 0;
  int readbuf_remaining;
  char* dnsreq;
  char* hdrstart;
  int usingprev = 0;

  wr = (write_req_t*) malloc(sizeof *wr);
  uv_req_init(&wr->req, (uv_handle_t*)handle, after_write);
  wr->buf.base = (char*)malloc(WRITE_BUF_LEN);
  wr->buf.len = 0;

  if (dns->state.prevbuf_ptr != NULL) {
    dnsreq = dns->state.prevbuf_ptr + dns->state.prevbuf_pos;
    readbuf_remaining = dns->state.prevbuf_rem;
    usingprev = 1;
  } else {
    dnsreq = buf.base;
    readbuf_remaining = nread;
  }
  hdrstart = dnsreq;

  while (dnsreq != NULL) {
    /* something to process */
    while (readbuf_remaining > 0) {
      /* something to process in current buffer */
      if (hdrbuf_remaining > 0) {
        /* process len and id */
        if (readbuf_remaining < hdrbuf_remaining) {
          /* too little to get request header. save for next buffer */
          memcpy(&hdrbuf[DNSREC_LEN - hdrbuf_remaining], dnsreq, readbuf_remaining);
          hdrbuf_remaining = DNSREC_LEN - readbuf_remaining;
          break;
        } else {
          short int reclen_n;
          /* save header */
          memcpy(&hdrbuf[DNSREC_LEN - hdrbuf_remaining], dnsreq, hdrbuf_remaining);
          dnsreq += hdrbuf_remaining;
          readbuf_remaining -= hdrbuf_remaining;
          hdrbuf_remaining = 0;

          /* get record length */
          reclen_n = *((short int*)hdrbuf);
          rec_remaining = ntohs(reclen_n) - (DNSREC_LEN - 2);
        }
      }
          
      if (rec_remaining <= readbuf_remaining) {
        /* prepare reply */
        addrsp(wr, hdrbuf);

        /* move to next record */
        dnsreq += rec_remaining;
        hdrstart = dnsreq;
        readbuf_remaining -= rec_remaining;
        rec_remaining = 0;
        hdrbuf_remaining = DNSREC_LEN;
      } else {
        /* otherwise this buffer is done. */
        rec_remaining -= readbuf_remaining;
        break;
      }
    }

    /* if we had to use bytes from prev buffer, start processing the current one */
    if (usingprev == 1) {
      /* free previous buffer */
      free(dns->state.prevbuf_ptr);
      dnsreq = buf.base;
      readbuf_remaining = nread;
      usingprev = 0;
    } else {
      dnsreq = NULL;
    }
  }

  /* send write buffer */
  if (wr->buf.len > 0) {
    if (uv_write(&wr->req, &wr->buf, 1)) {
      FATAL("uv_write failed");
    }
  }

  if (readbuf_remaining > 0) {
    /* save start of record position, so we can continue on next read */
    dns->state.prevbuf_ptr = buf.base;
    dns->state.prevbuf_pos = hdrstart - buf.base;
    dns->state.prevbuf_rem = nread - dns->state.prevbuf_pos;
  } else {
    /* nothing left in this buffer */
    dns->state.prevbuf_ptr = NULL;
    dns->state.prevbuf_pos = 0;
    dns->state.prevbuf_rem = 0;
    free(buf.base);
  }
}

static void after_read(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  uv_req_t* req;

  if (nread < 0) {
    /* Error or EOF */
    ASSERT (uv_last_error().code == UV_EOF);

    if (buf.base) {
      free(buf.base);
    }

    req = (uv_req_t*) malloc(sizeof *req);
    uv_req_init(req, (uv_handle_t*)handle, after_shutdown);
    uv_shutdown(req);

    return;
  }

  if (nread == 0) {
    /* Everything OK, but nothing read. */
    free(buf.base);
    return;
  }
  /* process requests and send responses */
  process_req(handle, nread, buf);
}


static void on_close(uv_handle_t* peer) {
  free(peer);
}


static uv_buf_t buf_alloc(uv_stream_t* handle, size_t suggested_size) {
  uv_buf_t buf;
  buf.base = (char*) malloc(suggested_size);
  buf.len = suggested_size;
  return buf;
}


static void on_connection(uv_handle_t* server, int status) {
  dnshandle* handle;
  int r;

  ASSERT(status == 0);

  handle = (dnshandle*) malloc(sizeof *handle);
  ASSERT(handle != NULL);

  /* initialize read buffer state */
  handle->state.prevbuf_ptr = 0;
  handle->state.prevbuf_pos = 0;
  handle->state.prevbuf_rem = 0;

  uv_tcp_init((uv_tcp_t*)handle);

  r = uv_accept(server, (uv_stream_t*)handle);
  ASSERT(r == 0);

  r = uv_read_start((uv_stream_t*)handle, buf_alloc, after_read);
  ASSERT(r == 0);
}


static void on_server_close(uv_handle_t* handle) {
  ASSERT(handle == (uv_handle_t*)&server);
}


static int dns_start(int port) {
  struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", port);
  int r;

  r = uv_tcp_init(&server);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Socket creation error\n");
    return 1;
  }

  r = uv_tcp_bind(&server, addr);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Bind error\n");
    return 1;
  }

  r = uv_tcp_listen(&server, 128, on_connection);
  if (r) {
    /* TODO: Error codes */
    fprintf(stderr, "Listen error\n");
    return 1;
  }

  return 0;
}


HELPER_IMPL(dns_server) {
  uv_init();
  if (dns_start(TEST_PORT_2))
    return 1;

  uv_run();
  return 0;
}
