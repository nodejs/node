// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_CRYPTO_CLIENTHELLO_INL_H_
#define SRC_NODE_CRYPTO_CLIENTHELLO_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_crypto_clienthello.h"
#include "util-inl.h"

namespace node {
namespace crypto {

inline ClientHelloParser::ClientHelloParser()
    : state_(kEnded),
      onhello_cb_(nullptr),
      onend_cb_(nullptr),
      cb_arg_(nullptr) {
  Reset();
}

inline void ClientHelloParser::Reset() {
  frame_len_ = 0;
  body_offset_ = 0;
  extension_offset_ = 0;
  session_size_ = 0;
  session_id_ = nullptr;
  tls_ticket_size_ = -1;
  tls_ticket_ = nullptr;
  servername_size_ = 0;
  servername_ = nullptr;
}

inline void ClientHelloParser::Start(ClientHelloParser::OnHelloCb onhello_cb,
                                     ClientHelloParser::OnEndCb onend_cb,
                                     void* onend_arg) {
  if (!IsEnded())
    return;
  Reset();

  CHECK_NOT_NULL(onhello_cb);

  state_ = kWaiting;
  onhello_cb_ = onhello_cb;
  onend_cb_ = onend_cb;
  cb_arg_ = onend_arg;
}

inline void ClientHelloParser::End() {
  if (state_ == kEnded)
    return;
  state_ = kEnded;
  if (onend_cb_ != nullptr) {
    onend_cb_(cb_arg_);
    onend_cb_ = nullptr;
  }
}

inline bool ClientHelloParser::IsEnded() const {
  return state_ == kEnded;
}

inline bool ClientHelloParser::IsPaused() const {
  return state_ == kPaused;
}

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CRYPTO_CLIENTHELLO_INL_H_
