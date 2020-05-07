#ifndef SRC_QUIC_NODE_QUIC_SOCKET_INL_H_
#define SRC_QUIC_NODE_QUIC_SOCKET_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_quic_socket.h"
#include "node_sockaddr.h"
#include "node_quic_session.h"
#include "node_crypto.h"
#include "debug_utils-inl.h"

namespace node {

using crypto::EntropySource;

namespace quic {

std::unique_ptr<QuicPacket> QuicPacket::Create(
    const char* diagnostic_label,
    size_t len) {
  return std::make_unique<QuicPacket>(diagnostic_label, len);
}

std::unique_ptr<QuicPacket> QuicPacket::Copy(
    const std::unique_ptr<QuicPacket>& other) {
  return std::make_unique<QuicPacket>(*other.get());
}

void QuicPacket::set_length(size_t len) {
  CHECK_LE(len, data_.size());
  data_.resize(len);
}

int QuicEndpoint::Send(
    uv_buf_t* buf,
    size_t len,
    const sockaddr* addr) {
  int ret = static_cast<int>(udp_->Send(buf, len, addr));
  if (ret == 0)
    IncrementPendingCallbacks();
  return ret;
}

int QuicEndpoint::ReceiveStart() {
  return udp_->RecvStart();
}

int QuicEndpoint::ReceiveStop() {
  return udp_->RecvStop();
}

void QuicEndpoint::WaitForPendingCallbacks() {
  if (!has_pending_callbacks()) {
    listener_->OnEndpointDone(this);
    return;
  }
  waiting_for_callbacks_ = true;
}

void QuicSocket::AssociateCID(
    const QuicCID& cid,
    const QuicCID& scid) {
  if (cid && scid)
    dcid_to_scid_[cid] = scid;
}

void QuicSocket::DisassociateCID(const QuicCID& cid) {
  if (cid) {
    Debug(this, "Removing association for cid %s", cid);
    dcid_to_scid_.erase(cid);
  }
}

void QuicSocket::AssociateStatelessResetToken(
    const StatelessResetToken& token,
    BaseObjectPtr<QuicSession> session) {
  Debug(this, "Associating stateless reset token %s", token);
  token_map_[token] = session;
}

const SocketAddress& QuicSocket::local_address() {
  CHECK(preferred_endpoint_);
  return preferred_endpoint_->local_address();
}

void QuicSocket::DisassociateStatelessResetToken(
    const StatelessResetToken& token) {
  Debug(this, "Removing stateless reset token %s", token);
  token_map_.erase(token);
}

// StopListening is called when the QuicSocket is no longer
// accepting new server connections. Typically, this is called
// when the QuicSocket enters a graceful closing state where
// existing sessions are allowed to close naturally but new
// sessions are rejected.
void QuicSocket::StopListening() {
  if (is_flag_set(QUICSOCKET_FLAGS_SERVER_LISTENING)) {
    Debug(this, "Stop listening");
    set_flag(QUICSOCKET_FLAGS_SERVER_LISTENING, false);
    // It is important to not call ReceiveStop here as there
    // is ongoing traffic being exchanged by the peers.
  }
}

void QuicSocket::ReceiveStart() {
  for (const auto& endpoint : endpoints_)
    CHECK_EQ(endpoint->ReceiveStart(), 0);
}

void QuicSocket::ReceiveStop() {
  for (const auto& endpoint : endpoints_)
    CHECK_EQ(endpoint->ReceiveStop(), 0);
}

void QuicSocket::RemoveSession(
    const QuicCID& cid,
    const SocketAddress& addr) {
  DecrementSocketAddressCounter(addr);
  sessions_.erase(cid);
}

void QuicSocket::ReportSendError(int error) {
  listener_->OnError(error);
}

void QuicSocket::IncrementStatelessResetCounter(const SocketAddress& addr) {
  reset_counts_[addr]++;
}

void QuicSocket::IncrementSocketAddressCounter(const SocketAddress& addr) {
  addr_counts_[addr]++;
}

void QuicSocket::DecrementSocketAddressCounter(const SocketAddress& addr) {
  auto it = addr_counts_.find(addr);
  if (it == std::end(addr_counts_))
    return;
  it->second--;
  // Remove the address if the counter reaches zero again.
  if (it->second == 0) {
    addr_counts_.erase(addr);
    reset_counts_.erase(addr);
  }
}

size_t QuicSocket::GetCurrentSocketAddressCounter(const SocketAddress& addr) {
  auto it = addr_counts_.find(addr);
  return it == std::end(addr_counts_) ? 0 : it->second;
}

size_t QuicSocket::GetCurrentStatelessResetCounter(const SocketAddress& addr) {
  auto it = reset_counts_.find(addr);
  return it == std::end(reset_counts_) ? 0 : it->second;
}

void QuicSocket::set_server_busy(bool on) {
  Debug(this, "Turning Server Busy Response %s", on ? "on" : "off");
  set_flag(QUICSOCKET_FLAGS_SERVER_BUSY, on);
  listener_->OnServerBusy(on);
}

bool QuicSocket::is_diagnostic_packet_loss(double prob) const {
  if (LIKELY(prob == 0.0)) return false;
  unsigned char c = 255;
  EntropySource(&c, 1);
  return (static_cast<double>(c) / 255) < prob;
}

void QuicSocket::set_diagnostic_packet_loss(double rx, double tx) {
  rx_loss_ = rx;
  tx_loss_ = tx;
}

bool QuicSocket::ToggleStatelessReset() {
  set_flag(
      QUICSOCKET_FLAGS_DISABLE_STATELESS_RESET,
      !is_flag_set(QUICSOCKET_FLAGS_DISABLE_STATELESS_RESET));
  return !is_flag_set(QUICSOCKET_FLAGS_DISABLE_STATELESS_RESET);
}

void QuicSocket::set_validated_address(const SocketAddress& addr) {
  if (is_option_set(QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU)) {
    // Remove the oldest item if we've hit the LRU limit
    validated_addrs_.push_back(SocketAddress::Hash()(addr));
    if (validated_addrs_.size() > kMaxValidateAddressLru)
      validated_addrs_.pop_front();
  }
}

bool QuicSocket::is_validated_address(const SocketAddress& addr) const {
  if (is_option_set(QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU)) {
    auto res = std::find(std::begin(validated_addrs_),
                         std::end(validated_addrs_),
                         SocketAddress::Hash()(addr));
    return res != std::end(validated_addrs_);
  }
  return false;
}

void QuicSocket::AddSession(
    const QuicCID& cid,
    BaseObjectPtr<QuicSession> session) {
  sessions_[cid] = session;
  IncrementSocketAddressCounter(session->remote_address());
  IncrementStat(
      session->is_server() ?
          &QuicSocketStats::server_sessions :
          &QuicSocketStats::client_sessions);
}

void QuicSocket::AddEndpoint(
    BaseObjectPtr<QuicEndpoint> endpoint_,
    bool preferred) {
  Debug(this, "Adding %sendpoint", preferred ? "preferred " : "");
  if (preferred || endpoints_.empty())
    preferred_endpoint_ = endpoint_;
  endpoints_.emplace_back(endpoint_);
  if (is_flag_set(QUICSOCKET_FLAGS_SERVER_LISTENING))
    endpoint_->ReceiveStart();
}

void QuicSocket::SessionReady(BaseObjectPtr<QuicSession> session) {
  listener_->OnSessionReady(session);
}

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_SOCKET_INL_H_
