#ifndef SRC_QUIC_NODE_QUIC_SOCKET_INL_H_
#define SRC_QUIC_NODE_QUIC_SOCKET_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_quic_socket.h"
#include "node_sockaddr-inl.h"
#include "node_quic_session.h"
#include "node_crypto.h"
#include "debug_utils-inl.h"

namespace node {

using crypto::EntropySource;

namespace quic {

std::unique_ptr<QuicPacket> QuicPacket::Create(
    const char* diagnostic_label,
    size_t len) {
  CHECK_LE(len, MAX_PKTLEN);
  return std::make_unique<QuicPacket>(diagnostic_label, len);
}

std::unique_ptr<QuicPacket> QuicPacket::Copy(
    const std::unique_ptr<QuicPacket>& other) {
  return std::make_unique<QuicPacket>(*other.get());
}

void QuicPacket::set_length(size_t len) {
  CHECK_LE(len, MAX_PKTLEN);
  len_ = len;
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

SocketAddress QuicSocket::local_address() const {
  DCHECK(preferred_endpoint_);
  return preferred_endpoint_->local_address();
}

void QuicSocket::DisassociateStatelessResetToken(
    const StatelessResetToken& token) {
  Debug(this, "Removing stateless reset token %s", token);
  token_map_.erase(token);
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
  addrLRU_.Upsert(addr)->reset_count++;
}

void QuicSocket::IncrementSocketAddressCounter(const SocketAddress& addr) {
  addrLRU_.Upsert(addr)->active_connections++;
}

void QuicSocket::DecrementSocketAddressCounter(const SocketAddress& addr) {
  SocketAddressInfo* counts = addrLRU_.Peek(addr);
  if (counts != nullptr && counts->active_connections > 0)
    counts->active_connections--;
}

size_t QuicSocket::GetCurrentSocketAddressCounter(const SocketAddress& addr) {
  SocketAddressInfo* counts = addrLRU_.Peek(addr);
  return counts != nullptr ? counts->active_connections : 0;
}

size_t QuicSocket::GetCurrentStatelessResetCounter(const SocketAddress& addr) {
  SocketAddressInfo* counts = addrLRU_.Peek(addr);
  return counts != nullptr ? counts->reset_count : 0;
}

void QuicSocket::ServerBusy(bool on) {
  Debug(this, "Turning Server Busy Response %s", on ? "on" : "off");
  state_->server_busy = on ? 1 : 0;
  listener_->OnServerBusy();
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

void QuicSocket::set_validated_address(const SocketAddress& addr) {
  addrLRU_.Upsert(addr)->validated = true;
}

bool QuicSocket::is_validated_address(const SocketAddress& addr) const {
  auto info = addrLRU_.Peek(addr);
  return info != nullptr ? info->validated : false;
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
  if (state_->server_listening)
    endpoint_->ReceiveStart();
}

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_SOCKET_INL_H_
