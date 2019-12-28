#ifndef SRC_NODE_QUIC_SESSION_INL_H_
#define SRC_NODE_QUIC_SESSION_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_quic_session.h"
#include "node_quic_socket.h"


namespace node {

namespace quic {

void QuicSession::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_ngtcp2_memory_, previous_size);
}

void QuicSession::IncreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ += size;
}

void QuicSession::DecreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ -= size;
}

size_t QuicSession::GetMaxPacketLength() const {
  return max_pktlen_;
}

uint64_t QuicSession::GetMaxDataLeft() {
  return ngtcp2_conn_get_max_data_left(Connection());
}

uint64_t QuicSession::GetMaxLocalStreamsUni() {
  return ngtcp2_conn_get_max_local_streams_uni(Connection());
}

void QuicSession::SetLastError(QuicError error) {
  last_error_ = error;
}

void QuicSession::SetLastError(int32_t family, uint64_t code) {
  SetLastError({ family, code });
}

void QuicSession::SetLastError(int32_t family, int code) {
  SetLastError({ family, code });
}

bool QuicSession::IsInClosingPeriod() {
  return ngtcp2_conn_is_in_closing_period(Connection());
}

bool QuicSession::IsInDrainingPeriod() {
  return ngtcp2_conn_is_in_draining_period(Connection());
}

bool QuicSession::HasStream(int64_t id) {
  return streams_.find(id) != std::end(streams_);
}

QuicError QuicSession::GetLastError() const { return last_error_; }

bool QuicSession::IsGracefullyClosing() const {
  return IsFlagSet(QUICSESSION_FLAG_GRACEFUL_CLOSING);
}

bool QuicSession::IsDestroyed() const {
  return IsFlagSet(QUICSESSION_FLAG_DESTROYED);
}

bool QuicSession::IsServer() const {
  return crypto_context_->Side() == NGTCP2_CRYPTO_SIDE_SERVER;
}

void QuicSession::StartGracefulClose() {
  SetFlag(QUICSESSION_FLAG_GRACEFUL_CLOSING);
  session_stats_.closing_at = uv_hrtime();
}

QuicSocket* QuicSession::Socket() const {
  return socket_.get();
}

Environment* QuicApplication::env() const {
  return Session()->env();
}

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_QUIC_SESSION_INL_H_
