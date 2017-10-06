/* Copyright StrongLoop, Inc. All rights reserved.
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

#include "defs.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* A connection is modeled as an abstraction on top of two simple state
 * machines, one for reading and one for writing.  Either state machine
 * is, when active, in one of three states: busy, done or stop; the fourth
 * and final state, dead, is an end state and only relevant when shutting
 * down the connection.  A short overview:
 *
 *                          busy                  done           stop
 *  ----------|---------------------------|--------------------|------|
 *  readable  | waiting for incoming data | have incoming data | idle |
 *  writable  | busy writing out data     | completed write    | idle |
 *
 * We could remove the done state from the writable state machine. For our
 * purposes, it's functionally equivalent to the stop state.
 *
 * When the connection with upstream has been established, the client_ctx
 * moves into a state where incoming data from the client is sent upstream
 * and vice versa, incoming data from upstream is sent to the client.  In
 * other words, we're just piping data back and forth.  See conn_cycle()
 * for details.
 *
 * An interesting deviation from libuv's I/O model is that reads are discrete
 * rather than continuous events.  In layman's terms, when a read operation
 * completes, the connection stops reading until further notice.
 *
 * The rationale for this approach is that we have to wait until the data
 * has been sent out again before we can reuse the read buffer.
 *
 * It also pleasingly unifies with the request model that libuv uses for
 * writes and everything else; libuv may switch to a request model for
 * reads in the future.
 */
enum conn_state {
  c_busy,  /* Busy; waiting for incoming data or for a write to complete. */
  c_done,  /* Done; read incoming data or write finished. */
  c_stop,  /* Stopped. */
  c_dead
};

/* Session states. */
enum sess_state {
  s_handshake,        /* Wait for client handshake. */
  s_handshake_auth,   /* Wait for client authentication data. */
  s_req_start,        /* Start waiting for request data. */
  s_req_parse,        /* Wait for request data. */
  s_req_lookup,       /* Wait for upstream hostname DNS lookup to complete. */
  s_req_connect,      /* Wait for uv_tcp_connect() to complete. */
  s_proxy_start,      /* Connected. Start piping data. */
  s_proxy,            /* Connected. Pipe data back and forth. */
  s_kill,             /* Tear down session. */
  s_almost_dead_0,    /* Waiting for finalizers to complete. */
  s_almost_dead_1,    /* Waiting for finalizers to complete. */
  s_almost_dead_2,    /* Waiting for finalizers to complete. */
  s_almost_dead_3,    /* Waiting for finalizers to complete. */
  s_almost_dead_4,    /* Waiting for finalizers to complete. */
  s_dead              /* Dead. Safe to free now. */
};

static void do_next(client_ctx *cx);
static int do_handshake(client_ctx *cx);
static int do_handshake_auth(client_ctx *cx);
static int do_req_start(client_ctx *cx);
static int do_req_parse(client_ctx *cx);
static int do_req_lookup(client_ctx *cx);
static int do_req_connect_start(client_ctx *cx);
static int do_req_connect(client_ctx *cx);
static int do_proxy_start(client_ctx *cx);
static int do_proxy(client_ctx *cx);
static int do_kill(client_ctx *cx);
static int do_almost_dead(client_ctx *cx);
static int conn_cycle(const char *who, conn *a, conn *b);
static void conn_timer_reset(conn *c);
static void conn_timer_expire(uv_timer_t *handle);
static void conn_getaddrinfo(conn *c, const char *hostname);
static void conn_getaddrinfo_done(uv_getaddrinfo_t *req,
                                  int status,
                                  struct addrinfo *ai);
static int conn_connect(conn *c);
static void conn_connect_done(uv_connect_t *req, int status);
static void conn_read(conn *c);
static void conn_read_done(uv_stream_t *handle,
                           ssize_t nread,
                           const uv_buf_t *buf);
static void conn_alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf);
static void conn_write(conn *c, const void *data, unsigned int len);
static void conn_write_done(uv_write_t *req, int status);
static void conn_close(conn *c);
static void conn_close_done(uv_handle_t *handle);

/* |incoming| has been initialized by server.c when this is called. */
void client_finish_init(server_ctx *sx, client_ctx *cx) {
  conn *incoming;
  conn *outgoing;

  cx->sx = sx;
  cx->state = s_handshake;
  s5_init(&cx->parser);

  incoming = &cx->incoming;
  incoming->client = cx;
  incoming->result = 0;
  incoming->rdstate = c_stop;
  incoming->wrstate = c_stop;
  incoming->idle_timeout = sx->idle_timeout;
  CHECK(0 == uv_timer_init(sx->loop, &incoming->timer_handle));

  outgoing = &cx->outgoing;
  outgoing->client = cx;
  outgoing->result = 0;
  outgoing->rdstate = c_stop;
  outgoing->wrstate = c_stop;
  outgoing->idle_timeout = sx->idle_timeout;
  CHECK(0 == uv_tcp_init(cx->sx->loop, &outgoing->handle.tcp));
  CHECK(0 == uv_timer_init(cx->sx->loop, &outgoing->timer_handle));

  /* Wait for the initial packet. */
  conn_read(incoming);
}

/* This is the core state machine that drives the client <-> upstream proxy.
 * We move through the initial handshake and authentication steps first and
 * end up (if all goes well) in the proxy state where we're just proxying
 * data between the client and upstream.
 */
static void do_next(client_ctx *cx) {
  int new_state;

  ASSERT(cx->state != s_dead);
  switch (cx->state) {
    case s_handshake:
      new_state = do_handshake(cx);
      break;
    case s_handshake_auth:
      new_state = do_handshake_auth(cx);
      break;
    case s_req_start:
      new_state = do_req_start(cx);
      break;
    case s_req_parse:
      new_state = do_req_parse(cx);
      break;
    case s_req_lookup:
      new_state = do_req_lookup(cx);
      break;
    case s_req_connect:
      new_state = do_req_connect(cx);
      break;
    case s_proxy_start:
      new_state = do_proxy_start(cx);
      break;
    case s_proxy:
      new_state = do_proxy(cx);
      break;
    case s_kill:
      new_state = do_kill(cx);
      break;
    case s_almost_dead_0:
    case s_almost_dead_1:
    case s_almost_dead_2:
    case s_almost_dead_3:
    case s_almost_dead_4:
      new_state = do_almost_dead(cx);
      break;
    default:
      UNREACHABLE();
  }
  cx->state = new_state;

  if (cx->state == s_dead) {
    if (DEBUG_CHECKS) {
      memset(cx, -1, sizeof(*cx));
    }
    free(cx);
  }
}

static int do_handshake(client_ctx *cx) {
  unsigned int methods;
  conn *incoming;
  s5_ctx *parser;
  uint8_t *data;
  size_t size;
  int err;

  parser = &cx->parser;
  incoming = &cx->incoming;
  ASSERT(incoming->rdstate == c_done);
  ASSERT(incoming->wrstate == c_stop);
  incoming->rdstate = c_stop;

  if (incoming->result < 0) {
    pr_err("read error: %s", uv_strerror(incoming->result));
    return do_kill(cx);
  }

  data = (uint8_t *) incoming->t.buf;
  size = (size_t) incoming->result;
  err = s5_parse(parser, &data, &size);
  if (err == s5_ok) {
    conn_read(incoming);
    return s_handshake;  /* Need more data. */
  }

  if (size != 0) {
    /* Could allow a round-trip saving shortcut here if the requested auth
     * method is S5_AUTH_NONE (provided unauthenticated traffic is allowed.)
     * Requires client support however.
     */
    pr_err("junk in handshake");
    return do_kill(cx);
  }

  if (err != s5_auth_select) {
    pr_err("handshake error: %s", s5_strerror(err));
    return do_kill(cx);
  }

  methods = s5_auth_methods(parser);
  if ((methods & S5_AUTH_NONE) && can_auth_none(cx->sx, cx)) {
    s5_select_auth(parser, S5_AUTH_NONE);
    conn_write(incoming, "\5\0", 2);  /* No auth required. */
    return s_req_start;
  }

  if ((methods & S5_AUTH_PASSWD) && can_auth_passwd(cx->sx, cx)) {
    /* TODO(bnoordhuis) Implement username/password auth. */
  }

  conn_write(incoming, "\5\377", 2);  /* No acceptable auth. */
  return s_kill;
}

/* TODO(bnoordhuis) Implement username/password auth. */
static int do_handshake_auth(client_ctx *cx) {
  UNREACHABLE();
  return do_kill(cx);
}

static int do_req_start(client_ctx *cx) {
  conn *incoming;

  incoming = &cx->incoming;
  ASSERT(incoming->rdstate == c_stop);
  ASSERT(incoming->wrstate == c_done);
  incoming->wrstate = c_stop;

  if (incoming->result < 0) {
    pr_err("write error: %s", uv_strerror(incoming->result));
    return do_kill(cx);
  }

  conn_read(incoming);
  return s_req_parse;
}

static int do_req_parse(client_ctx *cx) {
  conn *incoming;
  conn *outgoing;
  s5_ctx *parser;
  uint8_t *data;
  size_t size;
  int err;

  parser = &cx->parser;
  incoming = &cx->incoming;
  outgoing = &cx->outgoing;
  ASSERT(incoming->rdstate == c_done);
  ASSERT(incoming->wrstate == c_stop);
  ASSERT(outgoing->rdstate == c_stop);
  ASSERT(outgoing->wrstate == c_stop);
  incoming->rdstate = c_stop;

  if (incoming->result < 0) {
    pr_err("read error: %s", uv_strerror(incoming->result));
    return do_kill(cx);
  }

  data = (uint8_t *) incoming->t.buf;
  size = (size_t) incoming->result;
  err = s5_parse(parser, &data, &size);
  if (err == s5_ok) {
    conn_read(incoming);
    return s_req_parse;  /* Need more data. */
  }

  if (size != 0) {
    pr_err("junk in request %u", (unsigned) size);
    return do_kill(cx);
  }

  if (err != s5_exec_cmd) {
    pr_err("request error: %s", s5_strerror(err));
    return do_kill(cx);
  }

  if (parser->cmd == s5_cmd_tcp_bind) {
    /* Not supported but relatively straightforward to implement. */
    pr_warn("BIND requests are not supported.");
    return do_kill(cx);
  }

  if (parser->cmd == s5_cmd_udp_assoc) {
    /* Not supported.  Might be hard to implement because libuv has no
     * functionality for detecting the MTU size which the RFC mandates.
     */
    pr_warn("UDP ASSOC requests are not supported.");
    return do_kill(cx);
  }
  ASSERT(parser->cmd == s5_cmd_tcp_connect);

  if (parser->atyp == s5_atyp_host) {
    conn_getaddrinfo(outgoing, (const char *) parser->daddr);
    return s_req_lookup;
  }

  if (parser->atyp == s5_atyp_ipv4) {
    memset(&outgoing->t.addr4, 0, sizeof(outgoing->t.addr4));
    outgoing->t.addr4.sin_family = AF_INET;
    outgoing->t.addr4.sin_port = htons(parser->dport);
    memcpy(&outgoing->t.addr4.sin_addr,
           parser->daddr,
           sizeof(outgoing->t.addr4.sin_addr));
  } else if (parser->atyp == s5_atyp_ipv6) {
    memset(&outgoing->t.addr6, 0, sizeof(outgoing->t.addr6));
    outgoing->t.addr6.sin6_family = AF_INET6;
    outgoing->t.addr6.sin6_port = htons(parser->dport);
    memcpy(&outgoing->t.addr6.sin6_addr,
           parser->daddr,
           sizeof(outgoing->t.addr6.sin6_addr));
  } else {
    UNREACHABLE();
  }

  return do_req_connect_start(cx);
}

static int do_req_lookup(client_ctx *cx) {
  s5_ctx *parser;
  conn *incoming;
  conn *outgoing;

  parser = &cx->parser;
  incoming = &cx->incoming;
  outgoing = &cx->outgoing;
  ASSERT(incoming->rdstate == c_stop);
  ASSERT(incoming->wrstate == c_stop);
  ASSERT(outgoing->rdstate == c_stop);
  ASSERT(outgoing->wrstate == c_stop);

  if (outgoing->result < 0) {
    /* TODO(bnoordhuis) Escape control characters in parser->daddr. */
    pr_err("lookup error for \"%s\": %s",
           parser->daddr,
           uv_strerror(outgoing->result));
    /* Send back a 'Host unreachable' reply. */
    conn_write(incoming, "\5\4\0\1\0\0\0\0\0\0", 10);
    return s_kill;
  }

  /* Don't make assumptions about the offset of sin_port/sin6_port. */
  switch (outgoing->t.addr.sa_family) {
    case AF_INET:
      outgoing->t.addr4.sin_port = htons(parser->dport);
      break;
    case AF_INET6:
      outgoing->t.addr6.sin6_port = htons(parser->dport);
      break;
    default:
      UNREACHABLE();
  }

  return do_req_connect_start(cx);
}

/* Assumes that cx->outgoing.t.sa contains a valid AF_INET/AF_INET6 address. */
static int do_req_connect_start(client_ctx *cx) {
  conn *incoming;
  conn *outgoing;
  int err;

  incoming = &cx->incoming;
  outgoing = &cx->outgoing;
  ASSERT(incoming->rdstate == c_stop);
  ASSERT(incoming->wrstate == c_stop);
  ASSERT(outgoing->rdstate == c_stop);
  ASSERT(outgoing->wrstate == c_stop);

  if (!can_access(cx->sx, cx, &outgoing->t.addr)) {
    pr_warn("connection not allowed by ruleset");
    /* Send a 'Connection not allowed by ruleset' reply. */
    conn_write(incoming, "\5\2\0\1\0\0\0\0\0\0", 10);
    return s_kill;
  }

  err = conn_connect(outgoing);
  if (err != 0) {
    pr_err("connect error: %s\n", uv_strerror(err));
    return do_kill(cx);
  }

  return s_req_connect;
}

static int do_req_connect(client_ctx *cx) {
  const struct sockaddr_in6 *in6;
  const struct sockaddr_in *in;
  char addr_storage[sizeof(*in6)];
  conn *incoming;
  conn *outgoing;
  uint8_t *buf;
  int addrlen;

  incoming = &cx->incoming;
  outgoing = &cx->outgoing;
  ASSERT(incoming->rdstate == c_stop);
  ASSERT(incoming->wrstate == c_stop);
  ASSERT(outgoing->rdstate == c_stop);
  ASSERT(outgoing->wrstate == c_stop);

  /* Build and send the reply.  Not very pretty but gets the job done. */
  buf = (uint8_t *) incoming->t.buf;
  if (outgoing->result == 0) {
    /* The RFC mandates that the SOCKS server must include the local port
     * and address in the reply.  So that's what we do.
     */
    addrlen = sizeof(addr_storage);
    CHECK(0 == uv_tcp_getsockname(&outgoing->handle.tcp,
                                  (struct sockaddr *) addr_storage,
                                  &addrlen));
    buf[0] = 5;  /* Version. */
    buf[1] = 0;  /* Success. */
    buf[2] = 0;  /* Reserved. */
    if (addrlen == sizeof(*in)) {
      buf[3] = 1;  /* IPv4. */
      in = (const struct sockaddr_in *) &addr_storage;
      memcpy(buf + 4, &in->sin_addr, 4);
      memcpy(buf + 8, &in->sin_port, 2);
      conn_write(incoming, buf, 10);
    } else if (addrlen == sizeof(*in6)) {
      buf[3] = 4;  /* IPv6. */
      in6 = (const struct sockaddr_in6 *) &addr_storage;
      memcpy(buf + 4, &in6->sin6_addr, 16);
      memcpy(buf + 20, &in6->sin6_port, 2);
      conn_write(incoming, buf, 22);
    } else {
      UNREACHABLE();
    }
    return s_proxy_start;
  } else {
    pr_err("upstream connection error: %s\n", uv_strerror(outgoing->result));
    /* Send a 'Connection refused' reply. */
    conn_write(incoming, "\5\5\0\1\0\0\0\0\0\0", 10);
    return s_kill;
  }

  UNREACHABLE();
  return s_kill;
}

static int do_proxy_start(client_ctx *cx) {
  conn *incoming;
  conn *outgoing;

  incoming = &cx->incoming;
  outgoing = &cx->outgoing;
  ASSERT(incoming->rdstate == c_stop);
  ASSERT(incoming->wrstate == c_done);
  ASSERT(outgoing->rdstate == c_stop);
  ASSERT(outgoing->wrstate == c_stop);
  incoming->wrstate = c_stop;

  if (incoming->result < 0) {
    pr_err("write error: %s", uv_strerror(incoming->result));
    return do_kill(cx);
  }

  conn_read(incoming);
  conn_read(outgoing);
  return s_proxy;
}

/* Proxy incoming data back and forth. */
static int do_proxy(client_ctx *cx) {
  if (conn_cycle("client", &cx->incoming, &cx->outgoing)) {
    return do_kill(cx);
  }

  if (conn_cycle("upstream", &cx->outgoing, &cx->incoming)) {
    return do_kill(cx);
  }

  return s_proxy;
}

static int do_kill(client_ctx *cx) {
  int new_state;

  if (cx->state >= s_almost_dead_0) {
    return cx->state;
  }

  /* Try to cancel the request. The callback still runs but if the
   * cancellation succeeded, it gets called with status=UV_ECANCELED.
   */
  new_state = s_almost_dead_1;
  if (cx->state == s_req_lookup) {
    new_state = s_almost_dead_0;
    uv_cancel(&cx->outgoing.t.req);
  }

  conn_close(&cx->incoming);
  conn_close(&cx->outgoing);
  return new_state;
}

static int do_almost_dead(client_ctx *cx) {
  ASSERT(cx->state >= s_almost_dead_0);
  return cx->state + 1;  /* Another finalizer completed. */
}

static int conn_cycle(const char *who, conn *a, conn *b) {
  if (a->result < 0) {
    if (a->result != UV_EOF) {
      pr_err("%s error: %s", who, uv_strerror(a->result));
    }
    return -1;
  }

  if (b->result < 0) {
    return -1;
  }

  if (a->wrstate == c_done) {
    a->wrstate = c_stop;
  }

  /* The logic is as follows: read when we don't write and write when we don't
   * read.  That gives us back-pressure handling for free because if the peer
   * sends data faster than we consume it, TCP congestion control kicks in.
   */
  if (a->wrstate == c_stop) {
    if (b->rdstate == c_stop) {
      conn_read(b);
    } else if (b->rdstate == c_done) {
      conn_write(a, b->t.buf, b->result);
      b->rdstate = c_stop;  /* Triggers the call to conn_read() above. */
    }
  }

  return 0;
}

static void conn_timer_reset(conn *c) {
  CHECK(0 == uv_timer_start(&c->timer_handle,
                            conn_timer_expire,
                            c->idle_timeout,
                            0));
}

static void conn_timer_expire(uv_timer_t *handle) {
  conn *c;

  c = CONTAINER_OF(handle, conn, timer_handle);
  c->result = UV_ETIMEDOUT;
  do_next(c->client);
}

static void conn_getaddrinfo(conn *c, const char *hostname) {
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  CHECK(0 == uv_getaddrinfo(c->client->sx->loop,
                            &c->t.addrinfo_req,
                            conn_getaddrinfo_done,
                            hostname,
                            NULL,
                            &hints));
  conn_timer_reset(c);
}

static void conn_getaddrinfo_done(uv_getaddrinfo_t *req,
                                  int status,
                                  struct addrinfo *ai) {
  conn *c;

  c = CONTAINER_OF(req, conn, t.addrinfo_req);
  c->result = status;

  if (status == 0) {
    /* FIXME(bnoordhuis) Should try all addresses. */
    if (ai->ai_family == AF_INET) {
      c->t.addr4 = *(const struct sockaddr_in *) ai->ai_addr;
    } else if (ai->ai_family == AF_INET6) {
      c->t.addr6 = *(const struct sockaddr_in6 *) ai->ai_addr;
    } else {
      UNREACHABLE();
    }
  }

  uv_freeaddrinfo(ai);
  do_next(c->client);
}

/* Assumes that c->t.sa contains a valid AF_INET or AF_INET6 address. */
static int conn_connect(conn *c) {
  ASSERT(c->t.addr.sa_family == AF_INET ||
         c->t.addr.sa_family == AF_INET6);
  conn_timer_reset(c);
  return uv_tcp_connect(&c->t.connect_req,
                        &c->handle.tcp,
                        &c->t.addr,
                        conn_connect_done);
}

static void conn_connect_done(uv_connect_t *req, int status) {
  conn *c;

  if (status == UV_ECANCELED) {
    return;  /* Handle has been closed. */
  }

  c = CONTAINER_OF(req, conn, t.connect_req);
  c->result = status;
  do_next(c->client);
}

static void conn_read(conn *c) {
  ASSERT(c->rdstate == c_stop);
  CHECK(0 == uv_read_start(&c->handle.stream, conn_alloc, conn_read_done));
  c->rdstate = c_busy;
  conn_timer_reset(c);
}

static void conn_read_done(uv_stream_t *handle,
                           ssize_t nread,
                           const uv_buf_t *buf) {
  conn *c;

  c = CONTAINER_OF(handle, conn, handle);
  ASSERT(c->t.buf == buf->base);
  ASSERT(c->rdstate == c_busy);
  c->rdstate = c_done;
  c->result = nread;

  uv_read_stop(&c->handle.stream);
  do_next(c->client);
}

static void conn_alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf) {
  conn *c;

  c = CONTAINER_OF(handle, conn, handle);
  ASSERT(c->rdstate == c_busy);
  buf->base = c->t.buf;
  buf->len = sizeof(c->t.buf);
}

static void conn_write(conn *c, const void *data, unsigned int len) {
  uv_buf_t buf;

  ASSERT(c->wrstate == c_stop || c->wrstate == c_done);
  c->wrstate = c_busy;

  /* It's okay to cast away constness here, uv_write() won't modify the
   * memory.
   */
  buf.base = (char *) data;
  buf.len = len;

  CHECK(0 == uv_write(&c->write_req,
                      &c->handle.stream,
                      &buf,
                      1,
                      conn_write_done));
  conn_timer_reset(c);
}

static void conn_write_done(uv_write_t *req, int status) {
  conn *c;

  if (status == UV_ECANCELED) {
    return;  /* Handle has been closed. */
  }

  c = CONTAINER_OF(req, conn, write_req);
  ASSERT(c->wrstate == c_busy);
  c->wrstate = c_done;
  c->result = status;
  do_next(c->client);
}

static void conn_close(conn *c) {
  ASSERT(c->rdstate != c_dead);
  ASSERT(c->wrstate != c_dead);
  c->rdstate = c_dead;
  c->wrstate = c_dead;
  c->timer_handle.data = c;
  c->handle.handle.data = c;
  uv_close(&c->handle.handle, conn_close_done);
  uv_close((uv_handle_t *) &c->timer_handle, conn_close_done);
}

static void conn_close_done(uv_handle_t *handle) {
  conn *c;

  c = handle->data;
  do_next(c->client);
}
