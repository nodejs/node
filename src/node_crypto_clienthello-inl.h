#ifndef SRC_NODE_CRYPTO_CLIENTHELLO_INL_H_
#define SRC_NODE_CRYPTO_CLIENTHELLO_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util.h"
#include "util-inl.h"

namespace node {

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

  CHECK_NE(onhello_cb, nullptr);

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

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CRYPTO_CLIENTHELLO_INL_H_
