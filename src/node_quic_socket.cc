#include "async_wrap-inl.h"
#include "debug_utils.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "nghttp2/nghttp2.h"
#include "node.h"
#include "node_buffer.h"
#include "node_crypto.h"
#include "node_internals.h"
#include "node_mem-inl.h"
#include "node_quic_crypto.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket.h"
#include "node_quic_util-inl.h"
#include "node_sockaddr-inl.h"
#include "req_wrap-inl.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <random>

namespace node {

using crypto::EntropySource;
using crypto::SecureContext;

using v8::ArrayBufferView;
using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::PropertyAttribute;
using v8::String;
using v8::Value;

namespace quic {

namespace {
inline uint32_t GenerateReservedVersion(
    const sockaddr* addr,
    uint32_t version) {
  socklen_t addrlen = SocketAddress::GetLength(addr);
  uint32_t h = 0x811C9DC5u;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(addr);
  const uint8_t* ep = p + addrlen;
  for (; p != ep; ++p) {
    h ^= *p;
    h *= 0x01000193u;
  }
  version = htonl(version);
  p =  reinterpret_cast<const uint8_t*>(&version);
  ep = p + sizeof(version);
  for (; p != ep; ++p) {
    h ^= *p;
    h *= 0x01000193u;
  }
  h &= 0xf0f0f0f0u;
  h |= 0x0a0a0a0au;
  return h;
}
}  // namespace

QuicSocketListener::~QuicSocketListener() {
  if (socket_ != nullptr)
    socket_->RemoveListener(this);
}

void QuicSocketListener::OnError(ssize_t code) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnError(code);
}

void QuicSocketListener::OnError(int code) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnError(code);
}

void QuicSocketListener::OnSessionReady(BaseObjectPtr<QuicSession> session) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnSessionReady(session);
}

void QuicSocketListener::OnServerBusy(bool busy) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnServerBusy(busy);
}

void QuicSocketListener::OnDone() {
  if (previous_listener_ != nullptr)
    previous_listener_->OnDone();
}

void QuicSocketListener::OnDestroy() {
  if (previous_listener_ != nullptr)
    previous_listener_->OnDestroy();
}

void JSQuicSocketListener::OnError(ssize_t code) {
  Environment* env = Socket()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> arg = Number::New(env->isolate(), static_cast<double>(code));
  Socket()->MakeCallback(env->quic_on_socket_error_function(), 1, &arg);
}

void JSQuicSocketListener::OnError(int code) {
  Environment* env = Socket()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> arg = Integer::New(env->isolate(), code);
  Socket()->MakeCallback(env->quic_on_socket_error_function(), 1, &arg);
}

void JSQuicSocketListener::OnSessionReady(BaseObjectPtr<QuicSession> session) {
  Environment* env = Socket()->env();
  Local<Value> arg = session->object();
  Context::Scope context_scope(env->context());
  Socket()->MakeCallback(env->quic_on_session_ready_function(), 1, &arg);
}

void JSQuicSocketListener::OnServerBusy(bool busy) {
  Environment* env = Socket()->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> arg = Boolean::New(env->isolate(), busy);
  Socket()->MakeCallback(env->quic_on_socket_server_busy_function(), 1, &arg);
}

void JSQuicSocketListener::OnDone() {
  Environment* env = Socket()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  Socket()->MakeCallback(env->ondone_string(), 0, nullptr);
}

void JSQuicSocketListener::OnDestroy() {
  // Do nothing here.
}

QuicEndpoint::QuicEndpoint(
    Environment* env,
    QuicEndpointListener* listener,
    Local<Object> udp_wrap) :
    env_(env),
    listener_(listener) {
  udp_ = static_cast<UDPWrapBase*>(
      udp_wrap->GetAlignedPointerFromInternalField(
          UDPWrapBase::kUDPWrapBaseField));
  CHECK_NOT_NULL(udp_);
  udp_->set_listener(this);
  strong_ptr_.reset(udp_->GetAsyncWrap());
}

int QuicEndpoint::Send(
    uv_buf_t* buf,
    size_t len,
    const sockaddr* addr) {
  return udp_->Send(buf, len, addr);
}

int QuicEndpoint::ReceiveStart() {
  return udp_->RecvStart();
}

int QuicEndpoint::ReceiveStop() {
  return udp_->RecvStop();
}

uv_buf_t QuicEndpoint::OnAlloc(size_t suggested_size) {
  return env_->AllocateManaged(suggested_size).release();
}

void QuicEndpoint::OnRecv(
    ssize_t nread,
    const uv_buf_t& buf_,
    const sockaddr* addr,
    unsigned int flags) {
  AllocatedBuffer buf(env_, buf_);

  if (nread <= 0) {
    if (nread < 0)
      listener_->OnError(this, nread);
    return;
  }

  listener_->OnReceive(
      nread,
      std::move(buf),
      GetLocalAddress(),
      addr,
      flags);
}

ReqWrap<uv_udp_send_t>* QuicEndpoint::CreateSendWrap(size_t msg_size) {
  return listener_->OnCreateSendWrap(msg_size);
}

void QuicEndpoint::OnSendDone(ReqWrap<uv_udp_send_t>* wrap, int status) {
  listener_->OnSendDone(wrap, status);
}

void QuicEndpoint::OnAfterBind() {
  listener_->OnBind(this);
}

QuicSocket::QuicSocket(
    Environment* env,
    Local<Object> wrap,
    Local<Object> udp_base_wrap,
    uint64_t retry_token_expiration,
    size_t max_connections_per_host,
    uint32_t options,
    QlogMode qlog,
    const uint8_t* session_reset_secret)
  : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_QUICSOCKET),
    alloc_info_(MakeAllocator()),
    options_(options),
    max_connections_per_host_(max_connections_per_host),
    retry_token_expiration_(retry_token_expiration),
    qlog_(qlog),
    server_alpn_(NGTCP2_ALPN_H3),
    stats_buffer_(
        env->isolate(),
        sizeof(socket_stats_) / sizeof(uint64_t),
        reinterpret_cast<uint64_t*>(&socket_stats_)) {
  MakeWeak();
  PushListener(&default_listener_);

  AddEndpoint(udp_base_wrap, true);

  Debug(this, "New QuicSocket created.");

  EntropySource(token_secret_.data(), token_secret_.size());
  socket_stats_.created_at = uv_hrtime();

  // Set the session reset secret to the one provided or random.
  // Note that a random secret is going to make it exceedingly
  // difficult for the session reset token to be useful.
  if (session_reset_secret != nullptr) {
    memcpy(reset_token_secret_.data(),
           session_reset_secret,
           reset_token_secret_.size());
  } else {
    EntropySource(
        reset_token_secret_.data(),
        reset_token_secret_.size());
  }

  USE(wrap->DefineOwnProperty(
      env->context(),
      env->stats_string(),
      stats_buffer_.GetJSArray(),
      PropertyAttribute::ReadOnly));
}

QuicSocket::~QuicSocket() {
  uint64_t now = uv_hrtime();
  Debug(this,
        "QuicSocket destroyed.\n"
        "  Duration: %" PRIu64 "\n"
        "  Bound Duration: %" PRIu64 "\n"
        "  Listen Duration: %" PRIu64 "\n"
        "  Bytes Received: %" PRIu64 "\n"
        "  Bytes Sent: %" PRIu64 "\n"
        "  Packets Received: %" PRIu64 "\n"
        "  Packets Sent: %" PRIu64 "\n"
        "  Packets Ignored: %" PRIu64 "\n"
        "  Server Sessions: %" PRIu64 "\n"
        "  Client Sessions: %" PRIu64 "\n"
        "  Stateless Resets: %" PRIu64 "\n",
        now - socket_stats_.created_at,
        socket_stats_.bound_at > 0 ? now - socket_stats_.bound_at : 0,
        socket_stats_.listen_at > 0 ? now - socket_stats_.listen_at : 0,
        socket_stats_.bytes_received,
        socket_stats_.bytes_sent,
        socket_stats_.packets_received,
        socket_stats_.packets_sent,
        socket_stats_.packets_ignored,
        socket_stats_.server_sessions,
        socket_stats_.client_sessions,
        socket_stats_.stateless_reset_count);
  QuicSocketListener* listener = listener_;
  listener_->OnDestroy();
  // Remove the listener if it didn't remove itself already.
  if (listener == listener_)
    RemoveListener(listener_);
}

void QuicSocket::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("sessions", sessions_);
  tracker->TrackField("dcid_to_scid", dcid_to_scid_);
  tracker->TrackFieldWithSize("addr_counts",
      addr_counts_.size() * (sizeof(sockaddr*) + sizeof(size_t)));
  tracker->TrackField("validated_addrs", validated_addrs_);
  tracker->TrackField("stats_buffer", stats_buffer_);
  tracker->TrackFieldWithSize("current_ngtcp2_memory", current_ngtcp2_memory_);
}

void QuicSocket::AddEndpoint(Local<Object> endpoint_, bool preferred) {
  auto endpoint = std::make_unique<QuicEndpoint>(env(), this, endpoint_);
  Debug(this, "Adding %sendpoint.", preferred ? "preferred " : "");
  if (preferred)
    preferred_endpoint_ = endpoint.get();
  endpoints_.push_back(std::move(endpoint));
}

void QuicSocket::AddSession(
    const QuicCID& cid,
    BaseObjectPtr<QuicSession> session) {
  sessions_[cid.ToStr()] = session;
  IncrementSocketAddressCounter(session->GetRemoteAddress());
  IncrementSocketStat(
      1, &socket_stats_,
      session->IsServer() ?
          &socket_stats::server_sessions :
          &socket_stats::client_sessions);
}

void QuicSocket::AssociateCID(
    const QuicCID& cid,
    const QuicCID& scid) {
  dcid_to_scid_.emplace(cid.ToStr(), scid.ToStr());
}

void QuicSocket::DisassociateCID(const QuicCID& cid) {
  Debug(this, "Removing associations for cid %s", cid.ToHex().c_str());
  dcid_to_scid_.erase(cid.ToStr());
}

void QuicSocket::Listen(
    SecureContext* sc,
    const sockaddr* preferred_address,
    const std::string& alpn,
    uint32_t options) {
  CHECK_NOT_NULL(sc);
  CHECK_NULL(server_secure_context_);
  CHECK(!IsFlagSet(QUICSOCKET_FLAGS_SERVER_LISTENING));
  Debug(this, "Starting to listen.");
  server_session_config_.Set(env(), preferred_address);
  server_secure_context_ = sc;
  server_alpn_ = alpn;
  server_options_ = options;
  SetFlag(QUICSOCKET_FLAGS_SERVER_LISTENING);
  socket_stats_.listen_at = uv_hrtime();
  ReceiveStart();
}

// StopListening is called when the QuicSocket is no longer
// accepting new server connections. Typically, this is called
// when the QuicSocket enters a graceful closing state where
// existing sessions are allowed to close naturally but new
// sessions are rejected.
void QuicSocket::StopListening() {
  if (!IsFlagSet(QUICSOCKET_FLAGS_SERVER_LISTENING))
    return;
  Debug(this, "Stop listening.");
  SetFlag(QUICSOCKET_FLAGS_SERVER_LISTENING, false);
}

void QuicSocket::WaitForPendingCallbacks() {
  if (!HasPendingCallbacks()) {
    Debug(this, "No pending callbacks, calling ondone immediately");
    listener_->OnDone();
    return;
  }
  SetFlag(QUICSOCKET_FLAGS_WAITING_FOR_CALLBACKS);
  Debug(this, "Waiting for pending callbacks");
}

namespace {
bool IsShortHeader(
    uint32_t version,
    const uint8_t* pscid,
    size_t pscidlen) {
  return version == NGTCP2_PROTO_VER &&
         pscid == nullptr &&
         pscidlen == 0;
}
}  // namespace

void QuicSocket::OnError(QuicEndpoint* endpoint, ssize_t error) {
  Debug(this, "Reading data from UDP socket failed. Error %" PRId64, error);
  listener_->OnError(error);
}

ReqWrap<uv_udp_send_t>* QuicSocket::OnCreateSendWrap(size_t msg_size) {
  HandleScope handle_scope(env()->isolate());
  Local<Object> obj;
  if (!env()->quicsocketsendwrap_constructor_template()
          ->NewInstance(env()->context()).ToLocal(&obj)) return nullptr;
  return last_created_send_wrap_ = new SendWrap(env(), obj, msg_size);
}

void QuicSocket::OnBind(QuicEndpoint* endpoint) {
  auto& local_address = endpoint->GetLocalAddress();
  Debug(this, "Endpoint %s:%d bound",
        local_address.GetAddress(),
        local_address.GetPort());
  socket_stats_.bound_at = uv_hrtime();
}

void QuicSocket::OnReceive(
    ssize_t nread,
    AllocatedBuffer buf,
    const SocketAddress& local_addr,
    const sockaddr* remote_addr,
    unsigned int flags) {
  Debug(this, "Receiving %d bytes from the UDP socket.", nread);

  // When diagnostic packet loss is enabled, the packet will be randomly
  // dropped based on the rx_loss_ probability.
  if (UNLIKELY(IsDiagnosticPacketLoss(rx_loss_))) {
    Debug(this, "Simulating received packet loss.");
    return;
  }

  IncrementSocketStat(nread, &socket_stats_, &socket_stats::bytes_received);

  const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.data());

  uint32_t pversion;
  const uint8_t* pdcid;
  size_t pdcidlen;
  const uint8_t* pscid;
  size_t pscidlen;

  if (ngtcp2_pkt_decode_version_cid(
        &pversion,
        &pdcid,
        &pdcidlen,
        &pscid,
        &pscidlen,
        data, nread, NGTCP2_SV_SCIDLEN) < 0) {
    // There's nothing we can do here but ignore the packet. The packet
    // is likely not a QUIC packet or is malformed in some way.
    IncrementSocketStat(1, &socket_stats_, &socket_stats::packets_ignored);
    return;
  }

  if (pdcidlen > NGTCP2_MAX_CIDLEN || pscidlen > NGTCP2_MAX_CIDLEN) {
    // QUIC currently requires CID lengths of max NGTCP2_MAX_CIDLEN. The
    // ngtcp2 API allows non-standard lengths, and we may want to allow
    // non-standard lengths later. But for now, we're going to ignore any
    // packet with a non-standard CID length.
    IncrementSocketStat(1, &socket_stats_, &socket_stats::packets_ignored);
    return;
  }

  QuicCID dcid(pdcid, pdcidlen);
  QuicCID scid(pscid, pscidlen);

  std::string dcid_hex = dcid.ToHex();
  std::string dcid_str = dcid.ToStr();
  Debug(this, "Received a QUIC packet for dcid %s", dcid_hex.c_str());

  // Grabbing a shared pointer to prevent the QuicSession from
  // desconstructing while we're still using it. The session may
  // end up being destroyed, however, so we have to make sure
  // we're checking for that.
  BaseObjectPtr<QuicSession> session;

  // Identify the appropriate handler
  auto session_it = sessions_.find(dcid_str);
  if (session_it == std::end(sessions_)) {
    auto scid_it = dcid_to_scid_.find(dcid_str);
    if (scid_it == std::end(dcid_to_scid_)) {
      Debug(this, "There is no existing session for dcid %s", dcid_hex.c_str());
      // AcceptInitialPacket will first validate that the packet can be
      // accepted, then create a new server QuicSession instance if able
      // to do so. If a new instance cannot be created (for any reason),
      // the session shared_ptr will be empty on return.
      session = AcceptInitialPacket(
          pversion,
          dcid,
          scid,
          nread,
          data,
          local_addr,
          remote_addr,
          flags);

      // There are many reasons why a server QuicSession could not be
      // created. The most common will be invalid packets or incorrect
      // QUIC version. In any of these cases, however, to prevent a
      // potential attacker from causing us to consume resources,
      // we're just going to ignore the packet. It is possible that
      // the AcceptInitialPacket sent a version negotiation packet,
      // or (in the future) a CONNECTION_CLOSE packet.
      if (!session) {
        Debug(this, "Could not initialize a new server QuicSession.");

        // Ignore the packet if it's an unacceptable long header (initial)
        if (!IsShortHeader(pversion, pscid, pscidlen)) {
          IncrementSocketStat(
              1, &socket_stats_,
              &socket_stats::packets_ignored);
        } else {
          // Attempt to send a stateless reset. If it fails, we just ignore
          // TODO(@jasnell): Need to verify that stateless reset is occurring
          // correctly. Also need to determine how to test.
          if (!SendStatelessReset(dcid, local_addr, remote_addr)) {
            IncrementSocketStat(
                1, &socket_stats_,
                &socket_stats::packets_ignored);
          } else {
            IncrementSocketStat(
                1, &socket_stats_,
                &socket_stats::stateless_reset_count);
          }
        }

        return;
      }
    } else {
      session_it = sessions_.find(scid_it->second);
      session = session_it->second;
      CHECK_NE(session_it, std::end(sessions_));
    }
  } else {
    session = session_it->second;
  }

  CHECK(session);

  // If the packet could not successfully processed for any reason (possibly
  // due to being malformed or malicious in some way) we ignore it completely.
  if (!session->Receive(nread, data, local_addr, remote_addr, flags)) {
    IncrementSocketStat(1, &socket_stats_, &socket_stats::packets_ignored);
    return;
  }

  IncrementSocketStat(1, &socket_stats_, &socket_stats::packets_received);
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
  sessions_.erase(cid.ToStr());
  DecrementSocketAddressCounter(addr);
}

void QuicSocket::ReportSendError(int error) {
  listener_->OnError(error);
}

void QuicSocket::SendVersionNegotiation(
      uint32_t version,
      const QuicCID& dcid,
      const QuicCID& scid,
      const SocketAddress& local_addr,
      const sockaddr* remote_addr) {
  std::array<uint32_t, 2> sv;
  sv[0] = GenerateReservedVersion(remote_addr, version);
  sv[1] = NGTCP2_PROTO_VER;

  uint8_t unused_random;
  EntropySource(&unused_random, 1);

  auto packet = QuicPacket::Create("version negotiation");
  ssize_t nwrite = ngtcp2_pkt_write_version_negotiation(
      packet->data(),
      NGTCP2_MAX_PKTLEN_IPV6,
      unused_random,
      dcid.data(),
      dcid.length(),
      scid.data(),
      scid.length(),
      sv.data(),
      sv.size());
  if (nwrite <= 0)
    return;
  packet->SetLength(nwrite);
  SocketAddress remote_address(remote_addr);
  SendPacket(local_addr, remote_address, std::move(packet));
}

bool QuicSocket::SendStatelessReset(
    const QuicCID& cid,
    const SocketAddress& local_addr,
    const sockaddr* remote_addr) {
  constexpr static size_t RANDLEN = NGTCP2_MIN_STATELESS_RESET_RANDLEN * 5;
  uint8_t token[NGTCP2_STATELESS_RESET_TOKENLEN];
  uint8_t random[RANDLEN];

  GenerateResetToken(
      token,
      reset_token_secret_.data(),
      reset_token_secret_.size(), cid.cid());
  EntropySource(random, RANDLEN);

  auto packet = QuicPacket::Create("stateless reset");
  ssize_t nwrite =
      ngtcp2_pkt_write_stateless_reset(
        reinterpret_cast<uint8_t*>(packet->data()),
        NGTCP2_MAX_PKTLEN_IPV4,
        token,
        random,
        RANDLEN);
    if (nwrite <= 0)
      return false;
    packet->SetLength(nwrite);
    SocketAddress remote_address(remote_addr);
    return SendPacket(local_addr, remote_address, std::move(packet)) == 0;
}

bool QuicSocket::SendRetry(
    uint32_t version,
    const QuicCID& dcid,
    const QuicCID& scid,
    const SocketAddress& local_addr,
    const sockaddr* remote_addr) {
  std::array<uint8_t, 256> token;
  size_t tokenlen = token.size();

  if (!GenerateRetryToken(
          token.data(),
          &tokenlen,
          remote_addr,
          dcid.cid(),
          token_secret_)) {
    return false;
  }

  ngtcp2_pkt_hd hd;
  hd.version = version;
  hd.flags = NGTCP2_PKT_FLAG_LONG_FORM;
  hd.type = NGTCP2_PKT_RETRY;
  hd.pkt_num = 0;
  hd.token = nullptr;
  hd.tokenlen = 0;
  hd.len = 0;
  hd.dcid = *scid.cid();
  hd.scid.datalen = NGTCP2_SV_SCIDLEN;

  EntropySource(hd.scid.data, NGTCP2_SV_SCIDLEN);

  auto packet = QuicPacket::Create("retry");
  ssize_t nwrite =
      ngtcp2_pkt_write_retry(
          reinterpret_cast<uint8_t*>(packet->data()),
          NGTCP2_MAX_PKTLEN_IPV4,
          &hd,
          dcid.cid(),
          token.data(),
          tokenlen);
  if (nwrite <= 0)
    return false;
  packet->SetLength(nwrite);
  SocketAddress remote_address(remote_addr);
  return SendPacket(local_addr, remote_address, std::move(packet)) == 0;
}

namespace {
  SocketAddress::Hash addr_hash;
};

void QuicSocket::SetValidatedAddress(const sockaddr* addr) {
  if (IsOptionSet(QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU)) {
    // Remove the oldest item if we've hit the LRU limit
    validated_addrs_.push_back(addr_hash(addr));
    if (validated_addrs_.size() > MAX_VALIDATE_ADDRESS_LRU)
      validated_addrs_.pop_front();
  }
}

bool QuicSocket::IsValidatedAddress(const sockaddr* addr) const {
  if (IsOptionSet(QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU)) {
    auto res = std::find(std::begin(validated_addrs_),
                         std::end(validated_addrs_),
                         addr_hash(addr));
    return res != std::end(validated_addrs_);
  }
  return false;
}

BaseObjectPtr<QuicSession> QuicSocket::AcceptInitialPacket(
    uint32_t version,
    const QuicCID& dcid,
    const QuicCID& scid,
    ssize_t nread,
    const uint8_t* data,
    const SocketAddress& local_addr,
    const sockaddr* remote_addr,
    unsigned int flags) {
  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  ngtcp2_pkt_hd hd;
  ngtcp2_cid ocid;
  ngtcp2_cid* ocid_ptr = nullptr;
  uint64_t initial_connection_close = NGTCP2_NO_ERROR;

  if (!IsFlagSet(QUICSOCKET_FLAGS_SERVER_LISTENING)) {
    Debug(this, "QuicSocket is not listening");
    return {};
  }

  // Perform some initial checks on the packet to see if it is an
  // acceptable initial packet with the right QUIC version.
  switch (QuicSession::Accept(&hd, version, data, nread)) {
    case QuicSession::InitialPacketResult::PACKET_VERSION:
      SendVersionNegotiation(
          version,
          dcid,
          scid,
          local_addr,
          remote_addr);
      return {};
    case QuicSession::InitialPacketResult::PACKET_RETRY:
      Debug(this, "0RTT Packet. Sending retry.");
      SendRetry(version, dcid, scid, local_addr, remote_addr);
      return {};
    case QuicSession::InitialPacketResult::PACKET_IGNORE:
      return {};
    case QuicSession::InitialPacketResult::PACKET_OK:
      break;
  }

  // If the server is busy, new connections will be shut down immediately
  // after the initial keys are installed.
  if (IsFlagSet(QUICSOCKET_FLAGS_SERVER_BUSY)) {
    Debug(this, "QuicSocket is busy");
    initial_connection_close = NGTCP2_SERVER_BUSY;
  }

  // Check to see if the number of connections for this peer has been exceeded.
  // If the count has been exceeded, shutdown the connection immediately
  // after the initial keys are installed.
  if (GetCurrentSocketAddressCounter(remote_addr) >=
      max_connections_per_host_) {
    Debug(this, "Connection count for address exceeded");
    initial_connection_close = NGTCP2_SERVER_BUSY;
  }

  // QUIC has address validation built in to the handshake but allows for
  // an additional explicit validation request using RETRY frames. If we
  // are using explicit validation, we check for the existence of a valid
  // retry token in the packet. If one does not exist, we send a retry with
  // a new token. If it does exist, and if it's valid, we grab the original
  // cid and continue.
  //
  // If initial_connection_close is not NGTCP2_NO_ERROR, skip address
  // validation since we're going to reject the connection anyway.
  if (initial_connection_close == NGTCP2_NO_ERROR &&
      IsOptionSet(QUICSOCKET_OPTIONS_VALIDATE_ADDRESS) &&
      hd.type == NGTCP2_PKT_INITIAL) {
      // If the VALIDATE_ADDRESS_LRU option is set, IsValidatedAddress
      // will check to see if the given address is in the validated_addrs_
      // LRU cache. If it is, we'll skip the validation step entirely.
      // The VALIDATE_ADDRESS_LRU option is disable by default.
    if (!IsValidatedAddress(remote_addr)) {
      Debug(this, "Performing explicit address validation.");
      if (InvalidRetryToken(
              env(),
              &ocid,
              &hd,
              remote_addr,
              token_secret_,
              retry_token_expiration_)) {
        Debug(this, "A valid retry token was not found. Sending retry.");
        SendRetry(version, dcid, scid, local_addr, remote_addr);
        return {};
      }
      Debug(this, "A valid retry token was found. Continuing.");
      SetValidatedAddress(remote_addr);
      ocid_ptr = &ocid;
    } else {
      Debug(this, "Skipping validation for recently validated address.");
    }
  }

  BaseObjectPtr<QuicSession> session =
      QuicSession::CreateServer(
          this,
          &server_session_config_,
          dcid.cid(),
          local_addr,
          remote_addr,
          scid.cid(),
          ocid_ptr,
          version,
          server_alpn_,
          server_options_,
          initial_connection_close,
          qlog_);

  listener_->OnSessionReady(session);

  return session;
}

void QuicSocket::IncrementSocketAddressCounter(const SocketAddress& addr) {
  addr_counts_[addr.data()]++;
}

void QuicSocket::DecrementSocketAddressCounter(const SocketAddress& addr) {
  auto it = addr_counts_.find(addr.data());
  if (it == std::end(addr_counts_))
    return;
  it->second--;
  // Remove the address if the counter reaches zero again.
  if (it->second == 0)
    addr_counts_.erase(addr.data());
}

size_t QuicSocket::GetCurrentSocketAddressCounter(const sockaddr* addr) {
  auto it = addr_counts_.find(addr);
  if (it == std::end(addr_counts_))
    return 0;
  return it->second;
}

void QuicSocket::SetServerBusy(bool on) {
  Debug(this, "Turning Server Busy Response %s", on ? "on" : "off");
  SetFlag(QUICSOCKET_FLAGS_SERVER_BUSY, on);
  listener_->OnServerBusy(on);
}

QuicSocket::SendWrap::SendWrap(
    Environment* env,
    Local<Object> req_wrap_obj,
    size_t total_length)
  : ReqWrap(env, req_wrap_obj, PROVIDER_QUICSOCKET),
    total_length_(total_length) {
}

std::string QuicSocket::SendWrap::MemoryInfoName() const {
  return "QuicSendWrap";
}

void QuicSocket::SendWrap::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("session", session_);
  tracker->TrackField("packet", packet_);
}

int QuicSocket::SendPacket(
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    std::unique_ptr<QuicPacket> packet,
    BaseObjectPtr<QuicSession> session) {
  // If the packet is empty, there's nothing to do
  if (packet->length() == 0)
    return 0;

  Debug(this, "Sending %" PRIu64 " bytes to %s:%d from %s:%d (label: %s)",
        packet->length(),
        remote_addr.GetAddress().c_str(),
        remote_addr.GetPort(),
        local_addr.GetAddress().c_str(),
        remote_addr.GetPort(),
        packet->diagnostic_label());

  // If DiagnosticPacketLoss returns true, it will call Done() internally
  if (UNLIKELY(IsDiagnosticPacketLoss(tx_loss_))) {
    Debug(this, "Simulating transmitted packet loss.");
    return 0;
  }

  last_created_send_wrap_ = nullptr;
  uv_buf_t buf = uv_buf_init(
      reinterpret_cast<char*>(packet->data()),
      packet->length());
  CHECK_NOT_NULL(preferred_endpoint_);
  int err = preferred_endpoint_->Send(&buf, 1, remote_addr.data());

  if (err != 0) {
    if (err > 0) err = 0;
    OnSend(err, packet.get());
  } else {
    IncrementPendingCallbacks();

    CHECK_NOT_NULL(last_created_send_wrap_);
    last_created_send_wrap_->set_packet(std::move(packet));
    if (session)
      last_created_send_wrap_->set_session(session);
  }
  return err;
}

void QuicSocket::OnSend(int status, QuicPacket* packet) {
  if (status == 0) {
    Debug(this, "Sent %" PRIu64 " bytes (label: %s)",
          packet->length(),
          packet->diagnostic_label());
    IncrementSocketStat(
        packet->length(),
        &socket_stats_,
        &socket_stats::bytes_sent);
    IncrementSocketStat(
        1,
        &socket_stats_,
        &socket_stats::packets_sent);
  } else {
    Debug(this, "Failed to send %" PRIu64 " bytes (status: %d, label: %s)",
          packet->length(),
          status,
          packet->diagnostic_label());
  }

  if (!HasPendingCallbacks() &&
      IsFlagSet(QUICSOCKET_FLAGS_WAITING_FOR_CALLBACKS)) {
    listener_->OnDone();
  }
}

void QuicSocket::OnSendDone(ReqWrap<uv_udp_send_t>* wrap, int status) {
  std::unique_ptr<SendWrap> req_wrap(static_cast<SendWrap*>(wrap));
  DecrementPendingCallbacks();
  OnSend(status, req_wrap->get_packet());
}

bool QuicSocket::IsDiagnosticPacketLoss(double prob) {
  if (LIKELY(prob == 0.0)) return false;
  unsigned char c = 255;
  EntropySource(&c, 1);
  return (static_cast<double>(c) / 255) < prob;
}

void QuicSocket::SetDiagnosticPacketLoss(double rx, double tx) {
  rx_loss_ = rx;
  tx_loss_ = tx;
}

void QuicSocket::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_ngtcp2_memory_, previous_size);
}

void QuicSocket::IncreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ += size;
}

void QuicSocket::DecreaseAllocatedSize(size_t size) {
  current_ngtcp2_memory_ -= size;
}

void QuicSocket::PushListener(QuicSocketListener* listener) {
  CHECK_NOT_NULL(listener);
  CHECK_NULL(listener->socket_);

  listener->previous_listener_ = listener_;
  listener->socket_ = this;

  listener_ = listener;
}

void QuicSocket::RemoveListener(QuicSocketListener* listener) {
  CHECK_NOT_NULL(listener);

  QuicSocketListener* previous;
  QuicSocketListener* current;

  for (current = listener_, previous = nullptr;
       /* No loop condition because we want a crash if listener is not found */
       ; previous = current, current = current->previous_listener_) {
    CHECK_NOT_NULL(current);
    if (current == listener) {
      if (previous != nullptr)
        previous->previous_listener_ = current->previous_listener_;
      else
        listener_ = listener->previous_listener_;
      break;
    }
  }

  listener->socket_ = nullptr;
  listener->previous_listener_ = nullptr;
}

// JavaScript API
namespace {
void NewQuicSocket(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsObject());
  CHECK_GE(args[0].As<Object>()->InternalFieldCount(),
           UDPWrapBase::kUDPWrapBaseField);

  uint32_t options;
  uint32_t retry_token_expiration;
  uint32_t max_connections_per_host;

  if (!args[1]->Uint32Value(env->context()).To(&options) ||
      !args[2]->Uint32Value(env->context()).To(&retry_token_expiration) ||
      !args[3]->Uint32Value(env->context()).To(&max_connections_per_host)) {
    return;
  }
  CHECK_GE(retry_token_expiration, MIN_RETRYTOKEN_EXPIRATION);
  CHECK_LE(retry_token_expiration, MAX_RETRYTOKEN_EXPIRATION);

  const uint8_t* session_reset_secret = nullptr;
  if (args[5]->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> buf(args[5].As<ArrayBufferView>());
    CHECK_EQ(buf.length(), TOKEN_SECRETLEN);
    session_reset_secret = buf.data();
  }

  QuicSocket* socket =
      new QuicSocket(
          env,
          args.This(),
          args[0].As<Object>(),
          retry_token_expiration,
          max_connections_per_host,
          options,
          args[4]->IsTrue() ? QlogMode::kEnabled : QlogMode::kDisabled,
          session_reset_secret);
}

// Enabling diagnostic packet loss enables a mode where the QuicSocket
// instance will randomly ignore received packets in order to simulate
// packet loss. This is not an API that should be enabled in production
// but is useful when debugging and diagnosing performance issues.
// Diagnostic packet loss is enabled by setting either the tx or rx
// arguments to a value between 0.0 and 1.0. Setting both values to 0.0
// disables the mechanism.
void QuicSocketSetDiagnosticPacketLoss(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder());
  double rx, tx;
  if (!args[0]->NumberValue(env->context()).To(&rx) ||
      !args[1]->NumberValue(env->context()).To(&tx)) return;
  CHECK_GE(rx, 0.0f);
  CHECK_GE(tx, 0.0f);
  CHECK_LE(rx, 1.0f);
  CHECK_LE(tx, 1.0f);
  socket->SetDiagnosticPacketLoss(rx, tx);
}

void QuicSocketDestroy(const FunctionCallbackInfo<Value>& args) {
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder());
  socket->ReceiveStop();
}

void QuicSocketListen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  CHECK(args[0]->IsObject() &&
        env->secure_context_constructor_template()->HasInstance(args[0]));
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args[0].As<Object>(),
                          args.GetReturnValue().Set(UV_EBADF));

  sockaddr_storage preferred_address_storage;
  const sockaddr* preferred_address = nullptr;
  if (args[1]->IsString()) {
    node::Utf8Value preferred_address_host(args.GetIsolate(), args[1]);
    int32_t preferred_address_family;
    uint32_t preferred_address_port;
    if (!args[2]->Int32Value(env->context()).To(&preferred_address_family) ||
        !args[3]->Uint32Value(env->context()).To(&preferred_address_port))
      return;
    if (SocketAddress::ToSockAddr(
            preferred_address_family,
            *preferred_address_host,
            preferred_address_port,
            &preferred_address_storage) != nullptr) {
      preferred_address =
          reinterpret_cast<const sockaddr*>(&preferred_address_storage);
    }
  }

  std::string alpn(NGTCP2_ALPN_H3);
  if (args[4]->IsString()) {
    Utf8Value val(env->isolate(), args[4]);
    alpn = val.length();
    alpn += *val;
  }

  uint32_t options = 0;
  if (!args[5]->Uint32Value(env->context()).To(&options)) return;

  socket->Listen(sc, preferred_address, alpn, options);
}

void QuicSocketStopListening(const FunctionCallbackInfo<Value>& args) {
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder());
  socket->StopListening();
}

void QuicSocketReceiveStart(const FunctionCallbackInfo<Value>& args) {
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  socket->ReceiveStart();
}

void QuicSocketReceiveStop(const FunctionCallbackInfo<Value>& args) {
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder(),
                          args.GetReturnValue().Set(UV_EBADF));
  socket->ReceiveStop();
}

void QuicSocketSetServerBusy(const FunctionCallbackInfo<Value>& args) {
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder());
  CHECK_EQ(args.Length(), 1);
  socket->SetServerBusy(args[0]->IsTrue());
}

void QuicSocketWaitForPendingCallbacks(
    const FunctionCallbackInfo<Value>& args) {
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder());
  socket->WaitForPendingCallbacks();
}

}  // namespace

void QuicSocket::Initialize(
    Environment* env,
    Local<Object> target,
    Local<Context> context) {
  Isolate* isolate = env->isolate();
  Local<String> class_name = FIXED_ONE_BYTE_STRING(isolate, "QuicSocket");
  Local<FunctionTemplate> socket = env->NewFunctionTemplate(NewQuicSocket);
  socket->SetClassName(class_name);
  socket->InstanceTemplate()->SetInternalFieldCount(1);
  socket->InstanceTemplate()->Set(env->owner_symbol(), Null(isolate));
  env->SetProtoMethod(socket,
                      "destroy",
                      QuicSocketDestroy);
  env->SetProtoMethod(socket,
                      "listen",
                      QuicSocketListen);
  env->SetProtoMethod(socket,
                      "receiveStart",
                      QuicSocketReceiveStart);
  env->SetProtoMethod(socket,
                      "receiveStop",
                      QuicSocketReceiveStop);
  env->SetProtoMethod(socket,
                      "setDiagnosticPacketLoss",
                      QuicSocketSetDiagnosticPacketLoss);
  env->SetProtoMethod(socket,
                      "setServerBusy",
                      QuicSocketSetServerBusy);
  env->SetProtoMethod(socket,
                      "stopListening",
                      QuicSocketStopListening);
  env->SetProtoMethod(socket,
                      "waitForPendingCallbacks",
                      QuicSocketWaitForPendingCallbacks);
  socket->Inherit(HandleWrap::GetConstructorTemplate(env));
  target->Set(context, class_name,
              socket->GetFunction(env->context()).ToLocalChecked()).FromJust();

  // TODO(addaleax): None of these templates actually are constructor templates.
  Local<ObjectTemplate> sendwrap_template = ObjectTemplate::New(isolate);
  sendwrap_template->SetInternalFieldCount(1);
  env->set_quicsocketsendwrap_constructor_template(sendwrap_template);
}

}  // namespace quic
}  // namespace node
