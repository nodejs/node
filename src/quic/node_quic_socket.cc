#include "node_quic_socket-inl.h"  // NOLINT(build/include)
#include "aliased_struct-inl.h"
#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "nghttp2/nghttp2.h"
#include "nghttp3/nghttp3.h"
#include "node.h"
#include "node_buffer.h"
#include "node_crypto.h"
#include "node_internals.h"
#include "node_mem-inl.h"
#include "node_quic_crypto.h"
#include "node_quic_session-inl.h"
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
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
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
// The reserved version is a mechanism QUIC endpoints
// can use to ensure correct handling of version
// negotiation. It is defined by the QUIC spec in
// https://tools.ietf.org/html/draft-ietf-quic-transport-24#section-6.3
// Specifically, any version that follows the pattern
// 0x?a?a?a?a may be used to force version negotiation.
inline uint32_t GenerateReservedVersion(
    const SocketAddress& addr,
    uint32_t version) {
  socklen_t addrlen = addr.length();
  uint32_t h = 0x811C9DC5u;
  const uint8_t* p = addr.raw();
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

bool IsShortHeader(
    uint32_t version,
    const uint8_t* pscid,
    size_t pscidlen) {
  return version == NGTCP2_PROTO_VER &&
         pscid == nullptr &&
         pscidlen == 0;
}
}  // namespace

QuicPacket::QuicPacket(const char* diagnostic_label, size_t len)
    : data_{0},
      len_(len),
      diagnostic_label_(diagnostic_label) {
  CHECK_LE(len, MAX_PKTLEN);
}

QuicPacket::QuicPacket(const QuicPacket& other) :
  QuicPacket(other.diagnostic_label_, other.len_) {
  memcpy(&data_, &other.data_, other.len_);
}

const char* QuicPacket::diagnostic_label() const {
  return diagnostic_label_ != nullptr ?
      diagnostic_label_ : "unspecified";
}

QuicSocketListener::~QuicSocketListener() {
  if (socket_)
    socket_->RemoveListener(this);
}

void QuicSocketListener::OnError(ssize_t code) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnError(code);
}

void QuicSocketListener::OnSessionReady(BaseObjectPtr<QuicSession> session) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnSessionReady(session);
}

void QuicSocketListener::OnServerBusy() {
  if (previous_listener_ != nullptr)
    previous_listener_->OnServerBusy();
}

void QuicSocketListener::OnEndpointDone(QuicEndpoint* endpoint) {
  if (previous_listener_ != nullptr)
    previous_listener_->OnEndpointDone(endpoint);
}

void QuicSocketListener::OnDestroy() {
  if (previous_listener_ != nullptr)
    previous_listener_->OnDestroy();
}

void JSQuicSocketListener::OnError(ssize_t code) {
  Environment* env = socket()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Value> arg = Number::New(env->isolate(), static_cast<double>(code));
  socket()->MakeCallback(env->quic_on_socket_close_function(), 1, &arg);
}

void JSQuicSocketListener::OnSessionReady(BaseObjectPtr<QuicSession> session) {
  Environment* env = socket()->env();
  Local<Value> arg = session->object();
  Context::Scope context_scope(env->context());
  socket()->MakeCallback(env->quic_on_session_ready_function(), 1, &arg);
}

void JSQuicSocketListener::OnServerBusy() {
  Environment* env = socket()->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  socket()->MakeCallback(
      env->quic_on_socket_server_busy_function(), 0, nullptr);
}

void JSQuicSocketListener::OnEndpointDone(QuicEndpoint* endpoint) {
  Environment* env = socket()->env();
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  MakeCallback(
      env->isolate(),
      endpoint->object(),
      env->ondone_string(),
      0, nullptr);
}

void JSQuicSocketListener::OnDestroy() {
  // Do nothing here.
}

QuicEndpoint::QuicEndpoint(
    QuicState* quic_state,
    Local<Object> wrap,
    QuicSocket* listener,
    Local<Object> udp_wrap)
    : BaseObject(quic_state->env(), wrap),
      listener_(listener),
      quic_state_(quic_state) {
  MakeWeak();
  udp_ = static_cast<UDPWrapBase*>(
      udp_wrap->GetAlignedPointerFromInternalField(
          UDPWrapBase::kUDPWrapBaseField));
  CHECK_NOT_NULL(udp_);
  udp_->set_listener(this);
  strong_ptr_.reset(udp_->GetAsyncWrap());
}

QuicEndpoint::~QuicEndpoint() {
  udp_->set_listener(nullptr);
}

uv_buf_t QuicEndpoint::OnAlloc(size_t suggested_size) {
  return AllocatedBuffer::AllocateManaged(env(), suggested_size).release();
}

void QuicEndpoint::OnRecv(
    ssize_t nread,
    const uv_buf_t& buf_,
    const sockaddr* addr,
    unsigned int flags) {
  AllocatedBuffer buf(env(), buf_);

  if (nread <= 0) {
    if (nread < 0)
      listener_->OnError(this, nread);
    return;
  }

  listener_->OnReceive(
      nread,
      std::move(buf),
      local_address(),
      SocketAddress(addr),
      flags);
}

ReqWrap<uv_udp_send_t>* QuicEndpoint::CreateSendWrap(size_t msg_size) {
  return listener_->OnCreateSendWrap(msg_size);
}

void QuicEndpoint::OnSendDone(ReqWrap<uv_udp_send_t>* wrap, int status) {
  DecrementPendingCallbacks();
  listener_->OnSendDone(wrap, status);
  if (!has_pending_callbacks() && waiting_for_callbacks_)
    listener_->OnEndpointDone(this);
}

void QuicEndpoint::OnAfterBind() {
  listener_->OnBind(this);
}

template <typename Fn>
void QuicSocketStatsTraits::ToString(const QuicSocket& ptr, Fn&& add_field) {
#define V(_n, name, label)                                                     \
  add_field(label, ptr.GetStat(&QuicSocketStats::name));
  SOCKET_STATS(V)
#undef V
}

QuicSocket::QuicSocket(
    QuicState* quic_state,
    Local<Object> wrap,
    uint64_t retry_token_expiration,
    size_t max_connections,
    size_t max_connections_per_host,
    size_t max_stateless_resets_per_host,
    uint32_t options,
    QlogMode qlog,
    const uint8_t* session_reset_secret,
    bool disable_stateless_reset)
    : AsyncWrap(quic_state->env(), wrap, AsyncWrap::PROVIDER_QUICSOCKET),
      StatsBase(quic_state->env(), wrap),
      alloc_info_(MakeAllocator()),
      options_(options),
      state_(quic_state->env()->isolate()),
      max_connections_(max_connections),
      max_connections_per_host_(max_connections_per_host),
      max_stateless_resets_per_host_(max_stateless_resets_per_host),
      retry_token_expiration_(retry_token_expiration),
      qlog_(qlog),
      server_alpn_(NGHTTP3_ALPN_H3),
      addrLRU_(DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE),
      quic_state_(quic_state) {
  MakeWeak();
  PushListener(&default_listener_);

  Debug(this, "New QuicSocket created");

  EntropySource(token_secret_, kTokenSecretLen);

  wrap->DefineOwnProperty(
      env()->context(),
      env()->state_string(),
      state_.GetArrayBuffer(),
      PropertyAttribute::ReadOnly).Check();

  if (disable_stateless_reset)
    state_->stateless_reset_disabled = 1;

  // Set the session reset secret to the one provided or random.
  // Note that a random secret is going to make it exceedingly
  // difficult for the session reset token to be useful.
  if (session_reset_secret != nullptr) {
    memcpy(reset_token_secret_,
           session_reset_secret,
           NGTCP2_STATELESS_RESET_TOKENLEN);
  } else {
    EntropySource(reset_token_secret_, NGTCP2_STATELESS_RESET_TOKENLEN);
  }
}

QuicSocket::~QuicSocket() {
  QuicSocketListener* listener = listener_;
  listener_->OnDestroy();
  if (listener == listener_)
    RemoveListener(listener_);

  // In a clean shutdown, all QuicSessions associated with the QuicSocket
  // would have been destroyed explicitly. However, if the QuicSocket is
  // garbage collected / freed before Destroy having been called, there
  // may be sessions remaining. This is not really a good thing.
  Debug(this, "Destroying with %d sessions remaining", sessions_.size());

  DebugStats();
}

void QuicSocket::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("endpoints", endpoints_);
  tracker->TrackField("sessions", sessions_);
  tracker->TrackField("dcid_to_scid", dcid_to_scid_);
  tracker->TrackField("address_counts", addrLRU_);
  tracker->TrackField("token_map", token_map_);
  StatsBase::StatsMemoryInfo(tracker);
  tracker->TrackFieldWithSize(
      "current_ngtcp2_memory",
      current_ngtcp2_memory_);
}

void QuicSocket::Listen(
    BaseObjectPtr<SecureContext> sc,
    const sockaddr* preferred_address,
    const std::string& alpn,
    uint32_t options) {
  CHECK(sc);
  CHECK_NE(state_->server_listening, 1);
  Debug(this, "Starting to listen");
  server_session_config_.Set(quic_state(), preferred_address);
  server_secure_context_ = sc;
  server_alpn_ = alpn;
  server_options_ = options;
  state_->server_listening = 1;
  RecordTimestamp(&QuicSocketStats::listen_at);
  ReceiveStart();
}

void QuicSocket::OnError(QuicEndpoint* endpoint, ssize_t error) {
  // TODO(@jasnell): What should we do with the endpoint?
  Debug(this, "Reading data from UDP socket failed. Error %" PRId64, error);
  listener_->OnError(error);
}

ReqWrap<uv_udp_send_t>* QuicSocket::OnCreateSendWrap(size_t msg_size) {
  HandleScope handle_scope(env()->isolate());
  Local<Object> obj;
  if (!env()->quicsocketsendwrap_instance_template()
          ->NewInstance(env()->context()).ToLocal(&obj)) return nullptr;
  return last_created_send_wrap_ = new SendWrap(quic_state(), obj, msg_size);
}

void QuicSocket::OnEndpointDone(QuicEndpoint* endpoint) {
  Debug(this, "Endpoint has no pending callbacks");
  listener_->OnEndpointDone(endpoint);
}

void QuicSocket::OnBind(QuicEndpoint* endpoint) {
  SocketAddress local_address = endpoint->local_address();
  bound_endpoints_[local_address] =
      BaseObjectWeakPtr<QuicEndpoint>(endpoint);
  Debug(this, "Endpoint %s bound", local_address);
  RecordTimestamp(&QuicSocketStats::bound_at);
}

BaseObjectPtr<QuicSession> QuicSocket::FindSession(const QuicCID& cid) {
  BaseObjectPtr<QuicSession> session;
  auto session_it = sessions_.find(cid);
  if (session_it == std::end(sessions_)) {
    auto scid_it = dcid_to_scid_.find(cid);
    if (scid_it != std::end(dcid_to_scid_)) {
      session_it = sessions_.find(scid_it->second);
      CHECK_NE(session_it, std::end(sessions_));
      session = session_it->second;
    }
  } else {
    session = session_it->second;
  }
  return session;
}

// When a received packet contains a QUIC short header but cannot be
// matched to a known QuicSession, it is either (a) garbage,
// (b) a valid packet for a connection we no longer have state
// for, or (c) a stateless reset. Because we do not yet know if
// we are going to process the packet, we need to try to quickly
// determine -- with as little cost as possible -- whether the
// packet contains a reset token. We do so by checking the final
// NGTCP2_STATELESS_RESET_TOKENLEN bytes in the packet to see if
// they match one of the known reset tokens previously given by
// the remote peer. If there's a match, then it's a reset token,
// if not, we move on the to the next check. It is very important
// that this check be as inexpensive as possible to avoid a DOS
// vector.
bool QuicSocket::MaybeStatelessReset(
    const QuicCID& dcid,
    const QuicCID& scid,
    ssize_t nread,
    const uint8_t* data,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    unsigned int flags) {
  if (UNLIKELY(state_->stateless_reset_disabled || nread < 16))
    return false;
  StatelessResetToken possible_token(
      data + nread - NGTCP2_STATELESS_RESET_TOKENLEN);
  Debug(this, "Possible stateless reset token: %s", possible_token);
  auto it = token_map_.find(possible_token);
  if (it == token_map_.end())
    return false;
  Debug(this, "Received a stateless reset token %s", possible_token);
  return it->second->Receive(nread, data, local_addr, remote_addr, flags);
}

// When a packet is received here, we do not yet know if we can
// process it successfully as a QUIC packet or not. Given the
// nature of UDP, we may receive a great deal of garbage here
// so it is extremely important not to commit resources until
// we're certain we can process the data we received as QUIC
// packet.
// Any packet we choose not to process must be ignored.
void QuicSocket::OnReceive(
    ssize_t nread,
    AllocatedBuffer buf,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    unsigned int flags) {
  Debug(this, "Receiving %d bytes from the UDP socket", nread);

  // When diagnostic packet loss is enabled, the packet will be randomly
  // dropped based on the rx_loss_ probability.
  if (UNLIKELY(is_diagnostic_packet_loss(rx_loss_))) {
    Debug(this, "Simulating received packet loss");
    return;
  }

  IncrementStat(&QuicSocketStats::bytes_received, nread);

  const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.data());

  uint32_t pversion;
  const uint8_t* pdcid;
  size_t pdcidlen;
  const uint8_t* pscid;
  size_t pscidlen;

  // This is our first check to see if the received data can be
  // processed as a QUIC packet. If this fails, then the QUIC packet
  // header is invalid and cannot be processed; all we can do is ignore
  // it. It's questionable whether we should even increment the
  // packets_ignored statistic here but for now we do. If it succeeds,
  // we have a valid QUIC header but there's still no guarantee that
  // the packet can be successfully processed.
  if (ngtcp2_pkt_decode_version_cid(
        &pversion,
        &pdcid,
        &pdcidlen,
        &pscid,
        &pscidlen,
        data, nread, kScidLen) < 0) {
    IncrementStat(&QuicSocketStats::packets_ignored);
    return;
  }

  // QUIC currently requires CID lengths of max NGTCP2_MAX_CIDLEN. The
  // ngtcp2 API allows non-standard lengths, and we may want to allow
  // non-standard lengths later. But for now, we're going to ignore any
  // packet with a non-standard CID length.
  if (pdcidlen > NGTCP2_MAX_CIDLEN || pscidlen > NGTCP2_MAX_CIDLEN) {
    IncrementStat(&QuicSocketStats::packets_ignored);
    return;
  }

  QuicCID dcid(pdcid, pdcidlen);
  QuicCID scid(pscid, pscidlen);

  // TODO(@jasnell): It would be fantastic if Debug() could be
  // modified to accept objects with a ToString-like capability
  // similar to what we can do with TraceEvents... that would
  // allow us to pass the QuicCID directly to Debug and have it
  // converted to hex only if the category is enabled so we can
  // skip committing resources here.
  std::string dcid_hex = dcid.ToString();
  Debug(this, "Received a QUIC packet for dcid %s", dcid_hex.c_str());

  BaseObjectPtr<QuicSession> session = FindSession(dcid);

  // If a session is not found, there are four possible reasons:
  // 1. The session has not been created yet
  // 2. The session existed once but we've lost the local state for it
  // 3. The packet is a stateless reset sent by the peer
  // 4. This is a malicious or malformed packet.
  if (!session) {
    Debug(this, "There is no existing session for dcid %s", dcid_hex.c_str());
    bool is_short_header = IsShortHeader(pversion, pscid, pscidlen);

    // Handle possible reception of a stateless reset token...
    // If it is a stateless reset, the packet will be handled with
    // no additional action necessary here. We want to return immediately
    // without committing any further resources.
    if (is_short_header &&
        MaybeStatelessReset(
            dcid,
            scid,
            nread,
            data,
            local_addr,
            remote_addr,
            flags)) {
      Debug(this, "Handled stateless reset");
      return;
    }

    // AcceptInitialPacket will first validate that the packet can be
    // accepted, then create a new server QuicSession instance if able
    // to do so. If a new instance cannot be created (for any reason),
    // the session BaseObjectPtr will be empty on return.
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
    // or a CONNECTION_CLOSE packet.
    if (!session) {
      Debug(this, "Unable to create a new server QuicSession");
      // If the packet contained a short header, we might need to send
      // a stateless reset. The stateless reset contains a token derived
      // from the received destination connection ID.
      //
      // TODO(@jasnell): Stateless resets are generated programmatically
      // using HKDF with the sender provided dcid and a locally provided
      // secret as input. It is entirely possible that a malicious
      // peer could send multiple stateless reset eliciting packets
      // with the specific intent of using the returned stateless
      // reset to guess the stateless reset token secret used by
      // the server. Once guessed, the malicious peer could use
      // that secret as a DOS vector against other peers. We currently
      // implement some mitigations for this by limiting the number
      // of stateless resets that can be sent to a specific remote
      // address but there are other possible mitigations, such as
      // including the remote address as input in the generation of
      // the stateless token.
      if (is_short_header &&
          SendStatelessReset(dcid, local_addr, remote_addr, nread)) {
        Debug(this, "Sent stateless reset");
        IncrementStat(&QuicSocketStats::stateless_reset_count);
        return;
      }
      IncrementStat(&QuicSocketStats::packets_ignored);
      return;
    }
  }

  CHECK(session);
  // If the QuicSession is already destroyed, there's nothing to do.
  if (session->is_destroyed())
    return IncrementStat(&QuicSocketStats::packets_ignored);

  // If the packet could not successfully processed for any reason (possibly
  // due to being malformed or malicious in some way) we mark it ignored.
  if (!session->Receive(nread, data, local_addr, remote_addr, flags)) {
    IncrementStat(&QuicSocketStats::packets_ignored);
    return;
  }

  IncrementStat(&QuicSocketStats::packets_received);
}

// Generates and sends a version negotiation packet. This is
// terminal for the connection and is sent only when a QUIC
// packet is received for an unsupported Node.js version.
// It is possible that a malicious packet triggered this
// so we need to be careful not to commit too many resources.
// Currently, we only support one QUIC version at a time.
void QuicSocket::SendVersionNegotiation(
      uint32_t version,
      const QuicCID& dcid,
      const QuicCID& scid,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr) {
  uint32_t sv[2];
  sv[0] = GenerateReservedVersion(remote_addr, version);
  sv[1] = NGTCP2_PROTO_VER;

  uint8_t unused_random;
  EntropySource(&unused_random, 1);

  size_t pktlen = dcid.length() + scid.length() + (sizeof(sv)) + 7;

  auto packet = QuicPacket::Create("version negotiation", pktlen);
  ssize_t nwrite = ngtcp2_pkt_write_version_negotiation(
      packet->data(),
      NGTCP2_MAX_PKTLEN_IPV6,
      unused_random,
      dcid.data(),
      dcid.length(),
      scid.data(),
      scid.length(),
      sv,
      arraysize(sv));
  if (nwrite <= 0)
    return;
  packet->set_length(nwrite);
  SocketAddress remote_address(remote_addr);
  SendPacket(local_addr, remote_address, std::move(packet));
}

// Possible generates and sends a stateless reset packet.
// This is terminal for the connection. It is possible
// that a malicious packet triggered this so we need to
// be careful not to commit too many resources.
bool QuicSocket::SendStatelessReset(
    const QuicCID& cid,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    size_t source_len) {
  if (UNLIKELY(state_->stateless_reset_disabled))
    return false;
  constexpr static size_t kRandlen = NGTCP2_MIN_STATELESS_RESET_RANDLEN * 5;
  constexpr static size_t kMinStatelessResetLen = 41;
  uint8_t random[kRandlen];

  // Per the QUIC spec, we need to protect against sending too
  // many stateless reset tokens to an endpoint to prevent
  // endless looping.
  if (GetCurrentStatelessResetCounter(remote_addr) >=
          max_stateless_resets_per_host_) {
    return false;
  }
  // Per the QUIC spec, a stateless reset token must be strictly
  // smaller than the packet that triggered it. This is one of the
  // mechanisms to prevent infinite looping exchange of stateless
  // tokens with the peer.
  // An endpoint should never send a stateless reset token smaller than
  // 41 bytes per the QUIC spec. The reason is that packets less than
  // 41 bytes may allow an observer to determine that it's a stateless
  // reset.
  size_t pktlen = source_len - 1;
  if (pktlen < kMinStatelessResetLen)
    return false;

  StatelessResetToken token(reset_token_secret_, cid);
  EntropySource(random, kRandlen);

  auto packet = QuicPacket::Create("stateless reset", pktlen);
  ssize_t nwrite =
      ngtcp2_pkt_write_stateless_reset(
        packet->data(),
        NGTCP2_MAX_PKTLEN_IPV4,
        const_cast<uint8_t*>(token.data()),
        random,
        kRandlen);
    if (nwrite < static_cast<ssize_t>(kMinStatelessResetLen))
      return false;
    packet->set_length(nwrite);
    SocketAddress remote_address(remote_addr);
    IncrementStatelessResetCounter(remote_address);
    return SendPacket(local_addr, remote_address, std::move(packet)) == 0;
}

// Generates and sends a retry packet. This is terminal
// for the connection. Retry packets are used to force
// explicit path validation by issuing a token to the
// peer that it must thereafter include in all subsequent
// initial packets. Upon receiving a retry packet, the
// peer must termination it's initial attempt to
// establish a connection and start a new attempt.
//
// Retry packets will only ever be generated by QUIC servers,
// and only if the QuicSocket is configured for explicit path
// validation. There is no way for a client to force a retry
// packet to be created. However, once a client determines that
// explicit path validation is enabled, it could attempt to
// DOS by sending a large number of malicious initial packets
// to intentionally ellicit retry packets (It can do so by
// intentionally sending initial packets that ignore the retry
// token). To help mitigate that risk, we limit the number of
// retries we send to a given remote endpoint.
bool QuicSocket::SendRetry(
    const QuicCID& dcid,
    const QuicCID& scid,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr) {
  auto info = addrLRU_.Upsert(remote_addr);
  // Do not send a retry if the retry count is greater
  // than the retry limit.
  // TODO(@jasnell): Make the retry limit configurable.
  if (++(info->retry_count) > DEFAULT_MAX_RETRY_LIMIT)
    return true;
  std::unique_ptr<QuicPacket> packet =
      GenerateRetryPacket(token_secret_, dcid, scid, local_addr, remote_addr);
  return packet ?
      SendPacket(local_addr, remote_addr, std::move(packet)) == 0 : false;
}

// Shutdown a connection prematurely, before a QuicSession is created.
// This should only be called t the start of a session before the crypto
// keys have been established.
void QuicSocket::ImmediateConnectionClose(
    const QuicCID& scid,
    const QuicCID& dcid,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    int64_t reason) {
  Debug(this, "Sending stateless connection close to %s", scid);
  auto packet = QuicPacket::Create("immediate connection close");
  ssize_t nwrite = ngtcp2_crypto_write_connection_close(
      packet->data(),
      packet->length(),
      scid.cid(),
      dcid.cid(),
      reason);
  if (nwrite > 0) {
    packet->set_length(nwrite);
    SendPacket(local_addr, remote_addr, std::move(packet));
  }
}

// Inspects the packet and possibly accepts it as a new
// initial packet creating a new QuicSession instance.
// If the packet is not acceptable, it is very important
// not to commit resources.
BaseObjectPtr<QuicSession> QuicSocket::AcceptInitialPacket(
    uint32_t version,
    const QuicCID& dcid,
    const QuicCID& scid,
    ssize_t nread,
    const uint8_t* data,
    const SocketAddress& local_addr,
    const SocketAddress& remote_addr,
    unsigned int flags) {
  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());
  ngtcp2_pkt_hd hd;
  QuicCID ocid;

  // If the QuicSocket is not listening, the paket will be ignored.
  if (!state_->server_listening) {
    Debug(this, "QuicSocket is not listening");
    return {};
  }

  switch (ngtcp2_accept(&hd, data, static_cast<size_t>(nread))) {
    case 1:
      // Send Version Negotiation
      SendVersionNegotiation(version, dcid, scid, local_addr, remote_addr);
      // Fall through
    case -1:
      // Either a version negotiation packet was sent or the packet is
      // an invalid initial packet. Either way, there's nothing more we
      // can do here.
      return {};
  }

  // If the server is busy, new connections will be shut down immediately
  // after the initial keys are installed. The busy state is controlled
  // entirely by local user code. It is important to understand that
  // a QuicSession is created and resources are committed even though
  // the QuicSession will be torn down as quickly as possible.
  // Else, check to see if the number of connections total for this QuicSocket
  // has been exceeded. If the count has been exceeded, shutdown the connection
  // immediately after the initial keys are installed.
  if (UNLIKELY(state_->server_busy == 1) ||
      sessions_.size() >= max_connections_ ||
      GetCurrentSocketAddressCounter(remote_addr) >=
          max_connections_per_host_) {
    Debug(this, "QuicSocket is busy or connection count exceeded");
    IncrementStat(&QuicSocketStats::server_busy_count);
    ImmediateConnectionClose(
      QuicCID(hd.scid),
      QuicCID(hd.dcid),
      local_addr,
      remote_addr,
      NGTCP2_CONNECTION_REFUSED);
    return {};
  }

  // QUIC has address validation built in to the handshake but allows for
  // an additional explicit validation request using RETRY frames. If we
  // are using explicit validation, we check for the existence of a valid
  // retry token in the packet. If one does not exist, we send a retry with
  // a new token. If it does exist, and if it's valid, we grab the original
  // cid and continue.
  if (!is_validated_address(remote_addr)) {
    switch (hd.type) {
      case NGTCP2_PKT_INITIAL:
        if (has_option_validate_address() || hd.token.len > 0) {
          Debug(this, "Performing explicit address validation");
          if (hd.token.len == 0) {
            Debug(this, "No retry token was detected. Generating one");
            SendRetry(dcid, scid, local_addr, remote_addr);
            // Sending a retry token terminates this connection attempt.
            return {};
          }
          if (InvalidRetryToken(
                  hd.token,
                  remote_addr,
                  &ocid,
                  token_secret_,
                  retry_token_expiration_)) {
            Debug(this, "Invalid retry token was detected. Failing");
            ImmediateConnectionClose(
                QuicCID(hd.scid),
                QuicCID(hd.dcid),
                local_addr,
                remote_addr);
            return {};
          }
        }
        break;
      case NGTCP2_PKT_0RTT:
        SendRetry(dcid, scid, local_addr, remote_addr);
        return {};
    }
  }

  BaseObjectPtr<QuicSession> session =
      QuicSession::CreateServer(
          this,
          server_session_config_,
          local_addr,
          remote_addr,
          scid,
          dcid,
          ocid,
          version,
          server_alpn_,
          server_options_,
          qlog_);
  CHECK(session);

  listener_->OnSessionReady(session);

  // It's possible that the session was destroyed while processing
  // the ready callback. If it was, then we need to send an early
  // CONNECTION_CLOSE.
  if (session->is_destroyed()) {
    ImmediateConnectionClose(
        QuicCID(hd.scid),
        QuicCID(hd.dcid),
        local_addr,
        remote_addr,
        NGTCP2_CONNECTION_REFUSED);
  } else {
    session->set_wrapped();
  }

  return session;
}

QuicSocket::SendWrap::SendWrap(
    QuicState* quic_state,
    Local<Object> req_wrap_obj,
    size_t total_length)
    : ReqWrap(quic_state->env(), req_wrap_obj, PROVIDER_QUICSOCKET),
      total_length_(total_length),
      quic_state_(quic_state) {
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

  Debug(this, "Sending %" PRIu64 " bytes to %s from %s (label: %s)",
        packet->length(),
        remote_addr,
        local_addr,
        packet->diagnostic_label());

  // If DiagnosticPacketLoss returns true, it will call Done() internally
  if (UNLIKELY(is_diagnostic_packet_loss(tx_loss_))) {
    Debug(this, "Simulating transmitted packet loss");
    return 0;
  }

  last_created_send_wrap_ = nullptr;
  uv_buf_t buf = packet->buf();

  auto endpoint = bound_endpoints_.find(local_addr);
  CHECK_NE(endpoint, bound_endpoints_.end());
  int err = endpoint->second->Send(&buf, 1, remote_addr.data());

  if (err != 0) {
    if (err > 0) err = 0;
    OnSend(err, packet.get());
  } else {
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
    IncrementStat(&QuicSocketStats::bytes_sent, packet->length());
    IncrementStat(&QuicSocketStats::packets_sent);
  } else {
    Debug(this, "Failed to send %" PRIu64 " bytes (status: %d, label: %s)",
          packet->length(),
          status,
          packet->diagnostic_label());
  }
}

void QuicSocket::OnSendDone(ReqWrap<uv_udp_send_t>* wrap, int status) {
  std::unique_ptr<SendWrap> req_wrap(static_cast<SendWrap*>(wrap));
  OnSend(status, req_wrap->packet());
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
  CHECK(!listener->socket_);

  listener->previous_listener_ = listener_;
  listener->socket_.reset(this);

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

  listener->socket_.reset();
  listener->previous_listener_ = nullptr;
}

bool QuicSocket::SocketAddressInfoTraits::CheckExpired(
    const SocketAddress& address,
    const Type& type) {
  return (uv_hrtime() - type.timestamp) > 1e10;  // 10 seconds.
}

void QuicSocket::SocketAddressInfoTraits::Touch(
    const SocketAddress& address,
    Type* type) {
  type->timestamp = uv_hrtime();
}

// JavaScript API
namespace {
void NewQuicEndpoint(const FunctionCallbackInfo<Value>& args) {
  QuicState* state = Environment::GetBindingData<QuicState>(args);
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsObject());
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args[0].As<Object>());
  CHECK(args[1]->IsObject());
  CHECK_GE(args[1].As<Object>()->InternalFieldCount(),
           UDPWrapBase::kUDPWrapBaseField);
  new QuicEndpoint(state, args.This(), socket, args[1].As<Object>());
}

void NewQuicSocket(const FunctionCallbackInfo<Value>& args) {
  QuicState* state = Environment::GetBindingData<QuicState>(args);
  Environment* env = state->env();
  CHECK(args.IsConstructCall());

  uint32_t options;
  uint32_t retry_token_expiration;
  uint32_t max_connections;
  uint32_t max_connections_per_host;
  uint32_t max_stateless_resets_per_host;

  if (!args[0]->Uint32Value(env->context()).To(&options) ||
      !args[1]->Uint32Value(env->context()).To(&retry_token_expiration) ||
      !args[2]->Uint32Value(env->context()).To(&max_connections) ||
      !args[3]->Uint32Value(env->context()).To(&max_connections_per_host) ||
      !args[4]->Uint32Value(env->context())
          .To(&max_stateless_resets_per_host)) {
    return;
  }
  CHECK_GE(retry_token_expiration, MIN_RETRYTOKEN_EXPIRATION);
  CHECK_LE(retry_token_expiration, MAX_RETRYTOKEN_EXPIRATION);

  const uint8_t* session_reset_secret = nullptr;
  if (args[6]->IsArrayBufferView()) {
    ArrayBufferViewContents<uint8_t> buf(args[6].As<ArrayBufferView>());
    CHECK_EQ(buf.length(), kTokenSecretLen);
    session_reset_secret = buf.data();
  }

  new QuicSocket(
      state,
      args.This(),
      retry_token_expiration,
      max_connections,
      max_connections_per_host,
      max_stateless_resets_per_host,
      options,
      args[5]->IsTrue() ? QlogMode::kEnabled : QlogMode::kDisabled,
      session_reset_secret,
      args[7]->IsTrue());
}

void QuicSocketAddEndpoint(const FunctionCallbackInfo<Value>& args) {
  QuicSocket* socket;
  ASSIGN_OR_RETURN_UNWRAP(&socket, args.Holder());
  CHECK(args[0]->IsObject());
  QuicEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args[0].As<Object>());
  socket->AddEndpoint(
      BaseObjectPtr<QuicEndpoint>(endpoint),
      args[1]->IsTrue());
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
  socket->set_diagnostic_packet_loss(rx, tx);
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
            &preferred_address_storage)) {
      preferred_address =
          reinterpret_cast<const sockaddr*>(&preferred_address_storage);
    }
  }

  std::string alpn(NGHTTP3_ALPN_H3);
  if (args[4]->IsString()) {
    Utf8Value val(env->isolate(), args[4]);
    alpn = val.length();
    alpn += *val;
  }

  uint32_t options = 0;
  if (!args[5]->Uint32Value(env->context()).To(&options)) return;

  socket->Listen(
      BaseObjectPtr<SecureContext>(sc),
      preferred_address,
      alpn,
      options);
}

void QuicEndpointWaitForPendingCallbacks(
    const FunctionCallbackInfo<Value>& args) {
  QuicEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  endpoint->WaitForPendingCallbacks();
}

}  // namespace

void QuicEndpoint::Initialize(
    Environment* env,
    Local<Object> target,
    Local<Context> context) {
  Isolate* isolate = env->isolate();
  Local<String> class_name = FIXED_ONE_BYTE_STRING(isolate, "QuicEndpoint");
  Local<FunctionTemplate> endpoint = env->NewFunctionTemplate(NewQuicEndpoint);
  endpoint->Inherit(BaseObject::GetConstructorTemplate(env));
  endpoint->SetClassName(class_name);
  endpoint->InstanceTemplate()->SetInternalFieldCount(
      QuicEndpoint::kInternalFieldCount);
  env->SetProtoMethod(endpoint,
                      "waitForPendingCallbacks",
                      QuicEndpointWaitForPendingCallbacks);
  endpoint->InstanceTemplate()->Set(env->owner_symbol(), Null(isolate));

  target->Set(
      context,
      class_name,
      endpoint->GetFunction(context).ToLocalChecked())
          .FromJust();
}

void QuicSocket::Initialize(
    Environment* env,
    Local<Object> target,
    Local<Context> context) {
  Isolate* isolate = env->isolate();
  Local<String> class_name = FIXED_ONE_BYTE_STRING(isolate, "QuicSocket");
  Local<FunctionTemplate> socket = env->NewFunctionTemplate(NewQuicSocket);
  socket->Inherit(AsyncWrap::GetConstructorTemplate(env));
  socket->SetClassName(class_name);
  socket->InstanceTemplate()->SetInternalFieldCount(
      QuicSocket::kInternalFieldCount);
  socket->InstanceTemplate()->Set(env->owner_symbol(), Null(isolate));
  env->SetProtoMethod(socket,
                      "addEndpoint",
                      QuicSocketAddEndpoint);
  env->SetProtoMethod(socket,
                      "destroy",
                      QuicSocketDestroy);
  env->SetProtoMethod(socket,
                      "listen",
                      QuicSocketListen);
  env->SetProtoMethod(socket,
                      "setDiagnosticPacketLoss",
                      QuicSocketSetDiagnosticPacketLoss);
  socket->Inherit(HandleWrap::GetConstructorTemplate(env));
  target->Set(context, class_name,
              socket->GetFunction(env->context()).ToLocalChecked()).FromJust();

  Local<FunctionTemplate> sendwrap_ctor = FunctionTemplate::New(isolate);
  sendwrap_ctor->Inherit(AsyncWrap::GetConstructorTemplate(env));
  sendwrap_ctor->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "SendWrap"));
  Local<ObjectTemplate> sendwrap_template = sendwrap_ctor->InstanceTemplate();
  sendwrap_template->SetInternalFieldCount(SendWrap::kInternalFieldCount);
  env->set_quicsocketsendwrap_instance_template(sendwrap_template);
}

}  // namespace quic
}  // namespace node
