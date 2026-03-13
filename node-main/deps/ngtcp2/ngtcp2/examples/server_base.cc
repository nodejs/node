/*
 * ngtcp2
 *
 * Copyright (c) 2020 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "server_base.h"

#include <cassert>
#include <array>
#include <iostream>

#include "debug.h"

using namespace ngtcp2;

extern Config config;

Buffer::Buffer(const uint8_t *data, size_t datalen)
  : buf{data, data + datalen}, begin(buf.data()), tail(begin + datalen) {}
Buffer::Buffer(size_t datalen) : buf(datalen), begin(buf.data()), tail(begin) {}

static ngtcp2_conn *get_conn(ngtcp2_crypto_conn_ref *conn_ref) {
  auto h = static_cast<HandlerBase *>(conn_ref->user_data);
  return h->conn();
}

HandlerBase::HandlerBase() : conn_ref_{get_conn, this}, conn_(nullptr) {
  ngtcp2_ccerr_default(&last_error_);
}

HandlerBase::~HandlerBase() {
  if (conn_) {
    ngtcp2_conn_del(conn_);
  }
}

ngtcp2_conn *HandlerBase::conn() const { return conn_; }

ngtcp2_crypto_conn_ref *HandlerBase::conn_ref() { return &conn_ref_; }
