#include "endpoint.h"
#include <aliased_struct-inl.h>
#include <async_wrap-inl.h>
#include <base_object-inl.h>
#include <debug_utils-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/crypto/shared.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node_errors.h>
#include <node_sockaddr-inl.h>
#include <req_wrap-inl.h>
#include <util-inl.h>
#include <uv.h>
#include <v8.h>
#include <atomic>
#include "crypto.h"
#include "crypto/crypto_util.h"
#include "quic/defs.h"
#include "quic/quic.h"
#include "v8-local-handle.h"

namespace node {

using crypto::CSPRNG;
using v8::BackingStore;
using v8::BigInt;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::PropertyAttribute;
using v8::String;
using v8::Uint32;
using v8::Value;

namespace quic {

// ======================================================================================
// Endpoint::UDP and Endpoint::UDP::Impl

class Endpoint::UDP::Impl final : public HandleWrap {
 public:
  static Local<FunctionTemplate> GetConstructorTemplate(Environment* env) {
    auto& state = BindingState::Get(env);
    Local<FunctionTemplate> tmpl = state.udp_constructor_template();
    if (tmpl.IsEmpty()) {
      tmpl = NewFunctionTemplate(env->isolate(), IllegalConstructor);
      tmpl->Inherit(HandleWrap::GetConstructorTemplate(env));
      tmpl->InstanceTemplate()->SetInternalFieldCount(
          HandleWrap::kInternalFieldCount);
      tmpl->SetClassName(state.endpoint_udp_string());
      state.set_udp_constructor_template(tmpl);
    }
    return tmpl;
  }

  static BaseObjectPtr<Impl> Create(Environment* env, Endpoint* endpoint) {
    Local<Object> obj;
    if (UNLIKELY(!GetConstructorTemplate(env)
                      ->InstanceTemplate()
                      ->NewInstance(env->context())
                      .ToLocal(&obj))) {
      return BaseObjectPtr<Impl>();
    }

    return MakeBaseObject<Impl>(env, obj, endpoint);
  }

  static Impl* From(uv_handle_t* handle) {
    Impl* impl =
        ContainerOf(&Impl::handle_, reinterpret_cast<uv_udp_t*>(handle));
    return impl;
  }

  Impl(Environment* env, v8::Local<v8::Object> object, Endpoint* endpoint)
      : HandleWrap(env,
                   object,
                   reinterpret_cast<uv_handle_t*>(&handle_),
                   AsyncWrap::PROVIDER_QUICENDPOINT_UDP),
        endpoint_(endpoint) {
    CHECK_EQ(uv_udp_init(env->event_loop(), &handle_), 0);
    handle_.data = this;
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Endpoint::UDP)
  SET_SELF_SIZE(UDP)

 private:
  static void ClosedCb(uv_handle_t* handle) {
    std::unique_ptr<Impl> ptr(From(handle));
  }

  static void OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf) {
    *buf = From(handle)->env()->allocate_managed_buffer(suggested_size);
  }

  static void OnReceive(uv_udp_t* handle,
                        ssize_t nread,
                        const uv_buf_t* buf,
                        const sockaddr* addr,
                        unsigned int flags) {
    // Nothing to do it in this case.
    if (nread == 0) return;

    Impl* impl = ContainerOf(&Impl::handle_, handle);

    CHECK_NOT_NULL(impl);
    CHECK_NOT_NULL(impl->endpoint_);

    if (nread < 0) {
      impl->endpoint_->Destroy(CloseContext::RECEIVE_FAILURE,
                               static_cast<int>(nread));
      return;
    }

    if (UNLIKELY(flags & UV_UDP_PARTIAL)) {
      impl->endpoint_->Destroy(CloseContext::RECEIVE_FAILURE, UV_ENOBUFS);
      return;
    }

    impl->endpoint_->Receive(
        static_cast<size_t>(nread), *buf, SocketAddress(addr));
  }

  uv_udp_t handle_;
  Endpoint* endpoint_;

  friend class UDP;
};

Endpoint::UDP::UDP(Environment* env, Endpoint* endpoint)
    : impl_(Impl::Create(env, endpoint)) {
  env->AddCleanupHook(CleanupHook, this);
}

Endpoint::UDP::~UDP() {
  Close();
}

int Endpoint::UDP::Bind(const Endpoint::Options& options) {
  if (is_closed()) return UV_EBADF;

  DEBUG(impl_.get(), "Binding");

  int flags = 0;
  if (options.local_address.family() == AF_INET6 && options.ipv6_only)
    flags |= UV_UDP_IPV6ONLY;
  int err = uv_udp_bind(&impl_->handle_, options.local_address.data(), flags);
  int size;

  if (!err) {
    size = static_cast<int>(options.udp_receive_buffer_size);
    if (size > 0) {
      err = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(&impl_->handle_),
                                &size);
      if (err) return err;
    }

    size = static_cast<int>(options.udp_send_buffer_size);
    if (size > 0) {
      err = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(&impl_->handle_),
                                &size);
      if (err) return err;
    }

    size = static_cast<int>(options.udp_ttl);
    if (size > 0) {
      err = uv_udp_set_ttl(&impl_->handle_, size);
      if (err) return err;
    }
  }

  return err;
}

void Endpoint::UDP::Ref() {
  if (!is_closed()) uv_ref(reinterpret_cast<uv_handle_t*>(&impl_->handle_));
}

void Endpoint::UDP::Unref() {
  if (!is_closed()) uv_unref(reinterpret_cast<uv_handle_t*>(&impl_->handle_));
}

int Endpoint::UDP::Start() {
  if (is_closed()) return UV_EBADF;
  if (impl_->IsHandleClosing()) return UV_EBADF;
  DEBUG(impl_.get(), "Waiting for packets");
  int err = uv_udp_recv_start(&impl_->handle_, Impl::OnAlloc, Impl::OnReceive);
  return err == UV_EALREADY ? 0 : err;
}

void Endpoint::UDP::Stop() {
  if (is_closed()) return;
  if (!impl_->IsHandleClosing()) {
    DEBUG(impl_.get(), "No longer waiting for packets");
    USE(uv_udp_recv_stop(&impl_->handle_));
  }
}

void Endpoint::UDP::Close() {
  if (is_closed()) return;
  Stop();
  impl_->env()->RemoveCleanupHook(CleanupHook, this);
  impl_->Close();
  impl_.reset();
}

bool Endpoint::UDP::is_closed() const {
  return impl_.get() == nullptr;
}
Endpoint::UDP::operator bool() const {
  return !is_closed();
}

Maybe<SocketAddress> Endpoint::UDP::local_address() const {
  if (is_closed()) return Nothing<SocketAddress>();
  return Just(SocketAddress::FromSockName(impl_->handle_));
}

int Endpoint::UDP::Send(BaseObjectPtr<Packet> req) {
  if (is_closed()) return UV_EBADF;
  CHECK(req && !req->is_pending());
  // Attach this UDP::Impl instance to the packet to that we are sure it will
  // stay alive until the callback is invoked...
  DEBUG_ARGS(impl_.get(), "Sending a packet [%s]", req->ToString());
  req->Attach(impl_);
  uv_buf_t buf = *req;
  // The packet maintains a strong reference to itself to keep it from being
  // gc'd until the callback is invoked.

  return req->Dispatch(uv_udp_send,
                       &impl_->handle_,
                       &buf,
                       1,
                       req->destination().data(),
                       uv_udp_send_cb{[](uv_udp_send_t* req, int status) {
                         BaseObjectPtr<Packet> ptr(
                             static_cast<Packet*>(PacketReq::from_req(req)));
                         ptr->Done(status);
                       }});
}

void Endpoint::UDP::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("impl", impl_);
}

void Endpoint::UDP::CleanupHook(void* data) {
  static_cast<UDP*>(data)->Close();
}

// ======================================================================================
// Endpoint::Options

Endpoint::Options::Options() {
  GenerateResetTokenSecret();
}

void Endpoint::Options::GenerateResetTokenSecret() {
  CHECK(CSPRNG(reinterpret_cast<unsigned char*>(&reset_token_secret),
               NGTCP2_STATELESS_RESET_TOKENLEN)
            .is_ok());
}

// ======================================================================================
// Endpoint

Local<FunctionTemplate> Endpoint::GetConstructorTemplate(Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.endpoint_constructor_template();
  if (tmpl.IsEmpty()) {
    auto isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, IllegalConstructor);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->SetClassName(state.endpoint_string());
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Endpoint::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "listen", DoListen);
    SetProtoMethod(isolate, tmpl, "closeGracefully", DoCloseGracefully);
    SetProtoMethod(isolate, tmpl, "connect", DoConnect);
    SetProtoMethod(isolate, tmpl, "markBusy", MarkBusy);
    SetProtoMethod(isolate, tmpl, "ref", Ref);
    SetProtoMethod(isolate, tmpl, "unref", Unref);
    SetProtoMethodNoSideEffect(isolate, tmpl, "address", LocalAddress);
    state.set_endpoint_constructor_template(tmpl);
  }
  return tmpl;
}

void Endpoint::Initialize(Environment* env, Local<Object> target) {
  SetMethod(env->context(), target, "createEndpoint", CreateEndpoint);

  OptionsObject::Initialize(env, target);

#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_##name);
  ENDPOINT_STATS(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_COUNT);
#undef V
#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_ENDPOINT_##name);
  ENDPOINT_STATE(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATE_ENDPOINT_COUNT);
#undef V
}

void Endpoint::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(IllegalConstructor);
  registry->Register(CreateEndpoint);
  registry->Register(DoConnect);
  registry->Register(DoListen);
  registry->Register(DoCloseGracefully);
  registry->Register(LocalAddress);
  registry->Register(Ref);
  registry->Register(Unref);
  OptionsObject::RegisterExternalReferences(registry);
}

void Endpoint::OptionsObject::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(GenerateResetTokenSecret);
  registry->Register(SetResetTokenSecret);
}

BaseObjectPtr<Endpoint> Endpoint::Create(Environment* env,
                                         const Endpoint::Options& options) {
  Local<Object> obj;
  if (UNLIKELY(!GetConstructorTemplate(env)
                    ->InstanceTemplate()
                    ->NewInstance(env->context())
                    .ToLocal(&obj))) {
    return BaseObjectPtr<Endpoint>();
  }

  return MakeBaseObject<Endpoint>(env, obj, options);
}

Endpoint::Endpoint(Environment* env,
                   Local<Object> object,
                   const Endpoint::Options& options)
    : EndpointStatsBase(env),
      AsyncWrap(env, object, AsyncWrap::PROVIDER_QUICENDPOINT),
      state_(env->isolate()),
      options_(options),
      udp_(env, this),
      addrLRU_(options.address_lru_size) {
  MakeWeak();

  DEBUG(this, "Created");

  CHECK(CSPRNG(reinterpret_cast<unsigned char*>(token_secret_), kTokenSecretLen)
            .is_ok());

  const auto defineProperty = [&](auto name, auto value) {
    object
        ->DefineOwnProperty(
            env->context(), name, value, PropertyAttribute::ReadOnly)
        .Check();
  };

  defineProperty(env->state_string(), state_.GetArrayBuffer());
  defineProperty(env->stats_string(), ToBigUint64Array(env));
}

Endpoint::~Endpoint() {
  DEBUG(this, "Destroyed");
  if (udp_) udp_.Close();
  CHECK_EQ(state_->pending_callbacks, 0);
  CHECK(sessions_.empty());
  CHECK(is_closed());
}

v8::Maybe<SocketAddress> Endpoint::local_address() const {
  if (is_closed()) return v8::Nothing<SocketAddress>();
  return udp_.local_address();
}

void Endpoint::MarkAsBusy(bool on) {
  DEBUG_ARGS(this, "Marked as busy? %s", on ? "yes" : "no");
  if (!is_closed()) state_->busy = on ? 1 : 0;
}

Maybe<size_t> Endpoint::GenerateNewToken(quic_version version,
                                         uint8_t* token,
                                         const SocketAddress& remote_address) {
  if (is_closed() || is_closing()) return Nothing<size_t>();
  DEBUG(this, "Creating a new regular token");
  return GenerateToken(version, token, remote_address, token_secret_);
}

void Endpoint::AddSession(const CID& cid, BaseObjectPtr<Session> session) {
  if (is_closed() || is_closing()) return;
  DEBUG_ARGS(this,
             "Adding a new %s session [%s]",
             session->is_server() ? "server" : "client",
             cid);
  sessions_[cid] = session;
  IncrementSocketAddressCounter(session->remote_address());
  IncrementStat(session->is_server() ? &EndpointStats::server_sessions
                                     : &EndpointStats::client_sessions);
  if (session->is_server()) EmitNewSession(session);
}

void Endpoint::RemoveSession(const CID& cid, const SocketAddress& addr) {
  if (is_closed()) return;

  DEBUG_ARGS(this, "Removing a session [%s]", cid);

  DecrementSocketAddressCounter(addr);
  sessions_.erase(cid);

  if (state_->waiting_for_callbacks == 1) MaybeDestroy();
}

BaseObjectPtr<Session> Endpoint::FindSession(const CID& cid) {
  BaseObjectPtr<Session> session;
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

void Endpoint::AssociateCID(const CID& cid, const CID& scid) {
  if (!is_closed() && !is_closing() && cid && scid && cid != scid &&
      dcid_to_scid_[cid] != scid) {
    DEBUG_ARGS(this, "Associating %s to %s", cid, scid);
    dcid_to_scid_[cid] = scid;
  }
}

void Endpoint::DisassociateCID(const CID& cid) {
  if (!cid) return;
  if (!is_closed() && cid) {
    DEBUG_ARGS(this, "Disassociating %s", cid);
    dcid_to_scid_.erase(cid);
  }
}

void Endpoint::AssociateStatelessResetToken(const StatelessResetToken& token,
                                            Session* session) {
  if (is_closed() || is_closing()) return;
  DEBUG(this, "Associating stateless reset token");
  token_map_[token] = session;
}

void Endpoint::DisassociateStatelessResetToken(
    const StatelessResetToken& token) {
  DEBUG(this, "Disassociating stateless reset token");
  if (!is_closed()) token_map_.erase(token);
}

void Endpoint::Send(BaseObjectPtr<Packet> packet) {
  if (is_closed() || is_closing() || packet->length() == 0) return;
  state_->pending_callbacks++;
  int err = udp_.Send(packet);

  if (err != 0) {
    packet->Done(err);
    Destroy(CloseContext::SEND_FAILURE, err);
  }
  IncrementStat(&EndpointStats::bytes_sent, packet->length());
  IncrementStat(&EndpointStats::packets_sent);
  DEBUG_ARGS(
      this, "Packets sent: %" PRIu64, GetStat(&EndpointStats::packets_sent));
  DEBUG_ARGS(this, "Bytes sent: %" PRIu64, GetStat(&EndpointStats::bytes_sent));
}

void Endpoint::SendRetry(const quic_version version,
                         const CID& dcid,
                         const CID& scid,
                         const SocketAddress& local_address,
                         const SocketAddress& remote_address) {
  auto info = addrLRU_.Upsert(remote_address);
  if (++(info->retry_count) <= options_.retry_limit) {
    DEBUG(this, "Sending retry packet");
    // A retry packet will never be larger than the default 1200 so we're safe
    // not providing a size here...
    auto packet = Packet::Create(env(), this, remote_address, "retry");
    if (GenerateRetryPacket(
            packet, version, token_secret_, dcid, scid, remote_address))
      Send(std::move(packet));
    // If creating the retry is unsuccessful, we just drop things on the floor.
    // It's not worth committing any further resources to this one packet. We
    // might want to log the failure at some point tho.
  }
}

void Endpoint::SendVersionNegotiation(const quic_version version,
                                      const CID& dcid,
                                      const CID& scid,
                                      const SocketAddress& local_address,
                                      const SocketAddress& remote_address) {
  const auto generateReservedVersion = [&] {
    socklen_t addrlen = remote_address.length();
    quic_version h = 0x811C9DC5u;
    quic_version ver = htonl(version);
    const uint8_t* p = remote_address.raw();
    const uint8_t* ep = p + addrlen;
    for (; p != ep; ++p) {
      h ^= *p;
      h *= 0x01000193u;
    }
    p = reinterpret_cast<const uint8_t*>(&ver);
    ep = p + sizeof(version);
    for (; p != ep; ++p) {
      h ^= *p;
      h *= 0x01000193u;
    }
    h &= 0xf0f0f0f0u;
    h |= NGTCP2_RESERVED_VERSION_MASK;
    return h;
  };

  uint32_t sv[2] = {generateReservedVersion(), NGTCP2_PROTO_VER_MAX};

  DEBUG(this, "Sending version negotiation packet");

  uint8_t unused_random;
  CHECK(CSPRNG(&unused_random, 1).is_ok());
  size_t pktlen = dcid.length() + scid.length() + (sizeof(sv)) + 7;

  auto packet =
      Packet::Create(env(), this, remote_address, "version negotiation");
  ngtcp2_vec vec = *packet;

  ssize_t nwrite = ngtcp2_pkt_write_version_negotiation(vec.base,
                                                        pktlen,
                                                        unused_random,
                                                        dcid,
                                                        dcid.length(),
                                                        scid,
                                                        scid.length(),
                                                        sv,
                                                        arraysize(sv));
  if (nwrite > 0) {
    packet->Truncate(nwrite);
    Send(std::move(packet));
  }
}

bool Endpoint::SendStatelessReset(const CID& cid,
                                  const SocketAddress& local_address,
                                  const SocketAddress& remote_address,
                                  size_t source_len) {
  if (UNLIKELY(options_.disable_stateless_reset)) return false;

  static constexpr size_t kRandlen = NGTCP2_MIN_STATELESS_RESET_RANDLEN * 5;
  static constexpr size_t kMinStatelessResetLen = 41;
  uint8_t random[kRandlen];

  const auto exceeds_limits = [&] {
    SocketAddressInfoTraits::Type* counts = addrLRU_.Peek(remote_address);
    auto count = counts != nullptr ? counts->reset_count : 0;
    return count >= options_.max_stateless_resets;
  };

  // Per the QUIC spec, we need to protect against sending too many stateless
  // reset tokens to an endpoint to prevent endless looping.
  if (exceeds_limits()) return false;

  DEBUG(this, "Sending stateless reset packet");

  // Per the QUIC spec, a stateless reset token must be strictly smaller than
  // the packet that triggered it. This is one of the mechanisms to prevent
  // infinite looping exchange of stateless tokens with the peer. An endpoint
  // should never send a stateless reset token smaller than 41 bytes per the
  // QUIC spec. The reason is that packets less than 41 bytes may allow an
  // observer to reliably determine that it's a stateless reset.
  size_t pktlen = source_len - 1;
  if (pktlen < kMinStatelessResetLen) return false;

  StatelessResetToken token(options_.reset_token_secret, cid);
  CHECK(CSPRNG(random, kRandlen).is_ok());

  auto packet = Packet::Create(env(), this, remote_address, "stateless reset");
  ngtcp2_vec vec = *packet;

  ssize_t nwrite = ngtcp2_pkt_write_stateless_reset(
      vec.base, pktlen, const_cast<uint8_t*>(token.data()), random, kRandlen);
  if (nwrite >= static_cast<ssize_t>(kMinStatelessResetLen)) {
    addrLRU_.Upsert(remote_address)->reset_count++;
    packet->Truncate(nwrite);
    Send(std::move(packet));
    return true;
  }
  return false;
}

void Endpoint::SendImmediateConnectionClose(const quic_version version,
                                            const CID& dcid,
                                            const CID& scid,
                                            const SocketAddress& local_address,
                                            const SocketAddress& remote_address,
                                            QuicError reason) {
  DEBUG(this, "Sending immediate connection close");
  auto packet = Packet::Create(
      env(), this, remote_address, "immediate connection close (endpoint)");
  ngtcp2_vec vec = *packet;
  ssize_t nwrite = ngtcp2_crypto_write_connection_close(
      vec.base,
      vec.len,
      version,
      dcid,
      scid,
      reason.code(),
      // We do not bother sending a reason string here, even if
      // there is one in the QuicError
      nullptr,
      0);
  if (nwrite > 0) {
    packet->Truncate(static_cast<size_t>(nwrite));
    Send(std::move(packet));
  }
}

bool Endpoint::Start() {
  if (is_closed() || is_closing()) return false;
  if (state_->receiving == 1) return true;

  DEBUG(this, "Starting");

  int err = 0;
  if (state_->bound == 0) {
    err = udp_.Bind(options_);
    if (err != 0) {
      // If we failed to bind, destroy the endpoint. There's nothing we can do.
      Destroy(CloseContext::BIND_FAILURE, err);
      return false;
    }
    state_->bound = 1;
  }

  err = udp_.Start();
  if (err != 0) {
    // If we failed to start listening, destroy the endpoint. There's nothing we
    // can do.
    Destroy(CloseContext::START_FAILURE, err);
    return false;
  }

  BindingState::Get(env()).listening_endpoints_[this] =
      BaseObjectPtr<Endpoint>(this);
  state_->receiving = 1;
  return true;
}

bool Endpoint::Listen(const Session::Options& options) {
  if (is_closed() || is_closing()) return false;
  if (state_->listening == 1) return true;

  server_options_ = options;

  if (Start()) {
    state_->listening = 1;

    DEBUG(this, "Listening as a server");

    return true;
  }
  return false;
}

BaseObjectPtr<Session> Endpoint::Connect(const SocketAddress& remote_address,
                                         const Session::Options& options,
                                         SessionTicket* sessionTicket) {
  // If starting fails, the endpoint will be destroyed.
  if (!Start()) return BaseObjectPtr<Session>();

  DEBUG_ARGS(this, "Connecting to %s", remote_address);

  auto local = local_address().ToChecked();

  auto config = NewSessionConfig(
      // For client sessions, we always randomly generate an intial CID for the
      // server. This is generally just a throwaway. The server will generate
      // it's own CID and send that back to us.
      CIDFactory::random().Generate(NGTCP2_MIN_INITIAL_DCIDLEN),
      local,
      remote_address,
      NGTCP2_PROTO_VER_MAX,
      NGTCP2_CRYPTO_SIDE_CLIENT);

  DEBUG_ARGS(this, "QLOG?? %s", options.qlog ? "Yes" : "No");
  if (options.qlog) config.EnableQLog();

  if (sessionTicket != nullptr)
    config.session_ticket = BaseObjectPtr<SessionTicket>(sessionTicket);

  DEBUG_ARGS(this, "Client CID %s", config.scid);
  DEBUG_ARGS(this, "Server CID (random) %s", config.dcid);

  auto session =
      Session::Create(BaseObjectPtr<Endpoint>(this), config, options);
  if (!session) return BaseObjectPtr<Session>();

  DEBUG(this, "Client connection created");

  session->set_wrapped();

  Session::SendPendingDataScope send(session);
  return session;
}

void Endpoint::MaybeDestroy() {
  if (!is_closing() && sessions_.empty() && state_->pending_callbacks == 0 &&
      state_->listening == 0) {
    Destroy();
  }
}

void Endpoint::Destroy(CloseContext context, int status) {
  if (is_closed() || is_closing()) return;

  DEBUG_ARGS(this, "Destroying [%d, %d]", static_cast<int>(context), status);

  RecordTimestamp(&EndpointStats::destroyed_at);

  state_->closing = true;

  // Stop listening for new connections while we shut things down.
  state_->listening = 0;

  // If there are open sessions still, shut them down. As those clean themselves
  // up, they will remove themselves. The cleanup here will be synchronous and
  // no attempt will be made to communicate further with the peer.
  if (!sessions_.empty()) {
    close_context_ = context;
    close_status_ = status;
    for (auto& session : sessions_) session.second->CloseSilently();
  }
  CHECK(sessions_.empty());

  state_->closing = false;

  udp_.Close();
  state_->bound = 0;
  state_->receiving = 0;
  token_map_.clear();
  dcid_to_scid_.clear();
  BindingState::Get(env()).listening_endpoints_.erase(this);

  return context == CloseContext::CLOSE
             ? EmitEndpointDone()
             : EmitError(close_context_, close_status_);
}

void Endpoint::CloseGracefully() {
  DEBUG(this, "Close gracefully");

  if (!is_closed() && !is_closing() && state_->waiting_for_callbacks == 0) {
    state_->listening = 0;
    state_->waiting_for_callbacks = 1;
  }

  MaybeDestroy();
}

Session::Config Endpoint::NewSessionConfig(const CID& scid,
                                           const SocketAddress& local_address,
                                           const SocketAddress& remote_address,
                                           quic_version version,
                                           ngtcp2_crypto_side side) {
  return Session::Config(
      *this, scid, local_address, remote_address, version, side);
}

void Endpoint::Receive(size_t nread,
                       const uv_buf_t& buf,
                       const SocketAddress& remote_address) {
  const auto is_diagnostic_packet_loss = [](auto probability) {
    if (LIKELY(probability == 0.0)) return false;
    unsigned char c = 255;
    CHECK(CSPRNG(&c, 1).is_ok());
    return (static_cast<double>(c) / 255) < probability;
  };

  const auto receive = [&](Store&& store,
                           const SocketAddress& local_address,
                           const SocketAddress& remote_address,
                           const CID& dcid,
                           const CID& scid) {
    IncrementStat(&EndpointStats::bytes_received, store.length());
    BaseObjectPtr<Session> session = FindSession(dcid);
    return session && !session->is_destroyed()
               ? session->Receive(
                     std::move(store), local_address, remote_address)
               : false;
  };

  const auto accept = [&](const Session::Config& config, Store&& store) {
    if (is_closed() || is_closing() || !is_listening()) return false;

    DEBUG(this, "Accepting initial packet");

    BaseObjectPtr<Session> session =
        Session::Create(BaseObjectPtr<Endpoint>(this), config, server_options_);

    CHECK(session);

    DEBUG_ARGS(this, "Client CID %s", config.dcid);
    DEBUG_ARGS(this, "Server CID %s", config.scid);
    if (config.ocid.IsJust())
      DEBUG_ARGS(this, "Original Server CID %s", config.ocid.FromJust());

    return session
               ? session->Receive(
                     std::move(store), config.local_addr, config.remote_addr)
               : false;
  };

  const auto acceptInitialPacket = [&](const quic_version version,
                                       const CID& dcid,
                                       const CID& scid,
                                       Store&& store,
                                       const SocketAddress& local_address,
                                       const SocketAddress& remote_address) {
    // Conditionally accept an initial packet to create a new session.

    // If we're not listening, do not accept.
    if (state_->listening == 0) return false;

    DEBUG(this, "Evaluating initial packet");

    ngtcp2_pkt_hd hd;

    // This is our first condition check... A minimal check to see if ngtcp2 can
    // even recognize this packet as a quic packet with the correct version.
    ngtcp2_vec vec = store;
    switch (ngtcp2_accept(&hd, vec.base, vec.len)) {
      case 1:
        // The requested QUIC protocol version is not supported
        SendVersionNegotiation(
            version, dcid, scid, local_address, remote_address);
        // The packet was successfully processed, even if we did refuse the
        // connection and send a version negotiation in response.
        return true;
      case -1:
        // The packet is invalid and we're just going to ignore it.
        return false;
    }

    // This is the second condition check... If the server has been marked busy
    // or the remote peer has exceeded their maximum number of concurrent
    // connections, any new connections will be shut down immediately.
    const auto limits_exceeded = [&] {
      if (sessions_.size() >= options_.max_connections_total) return true;

      SocketAddressInfoTraits::Type* counts = addrLRU_.Peek(remote_address);
      auto count = counts != nullptr ? counts->active_connections : 0;
      return count >= options_.max_connections_per_host;
    };

    if (state_->busy || limits_exceeded()) {
      if (state_->busy) IncrementStat(&EndpointStats::server_busy_count);
      // Endpoint is busy or the connection count is exceeded. The connection is
      // refused.
      IncrementStat(&EndpointStats::server_busy_count);
      DEBUG(
          this,
          "Immediately closing because server is busy or limits are exceeded");
      SendImmediateConnectionClose(
          version,
          scid,
          dcid,
          local_address,
          remote_address,
          QuicError::ForTransport(NGTCP2_CONNECTION_REFUSED));
      // The packet was successfully processed, even if we did refuse the
      // connection.
      return true;
    }

    // At this point, we start to set up the configuration for our local
    // session. The second argument to the Config constructor here is the dcid.
    // We pass the received scid here as the value because that is the value
    // *this* session will use as it's outbound dcid.
    auto config = NewSessionConfig(scid,
                                   local_address,
                                   remote_address,
                                   version,
                                   NGTCP2_CRYPTO_SIDE_SERVER);

    // The this point, the config.scid and config.dcid represent *our* views of
    // the CIDs. Specifically, config.dcid identifies the peer and config.scid
    // identifies us. config.dcid should equal scid. config.scid should *not*
    // equal dcid.

    const auto is_remote_address_validated = [&] {
      auto info = addrLRU_.Peek(remote_address);
      return info != nullptr ? info->validated : false;
    };

    config.ocid = Just(dcid);

    // QUIC has address validation built in to the handshake but allows for
    // an additional explicit validation request using RETRY frames. If we
    // are using explicit validation, we check for the existence of a valid
    // retry token in the packet. If one does not exist, we send a retry with
    // a new token. If it does exist, and if it's valid, we grab the original
    // cid and continue.
    if (!is_remote_address_validated()) {
      switch (hd.type) {
        case NGTCP2_PKT_INITIAL:
          // First, let's see if we need to do anything here.

          if (options_.validate_address) {
            // If there is no token, generate and send one.
            if (hd.token.len == 0) {
              SendRetry(version, dcid, scid, local_address, remote_address);
              return true;
            }

            // We have two kinds of tokens, each prefixed with a different magic
            // byte.
            switch (hd.token.base[0]) {
              case kRetryTokenMagic: {
                CID ocid;
                if (!ValidateRetryToken(
                         version,
                         hd.token,
                         remote_address,
                         dcid,
                         token_secret_,
                         options_.retry_token_expiration * NGTCP2_SECONDS)
                         .To(&ocid)) {
                  // Invalid retry token was detected. Close the connection.
                  DEBUG(
                      this,
                      "Immediately closing because the retry token was invalid")
                  SendImmediateConnectionClose(
                      version,
                      scid,
                      dcid,
                      local_address,
                      remote_address,
                      QuicError::ForTransport(NGTCP2_CONNECTION_REFUSED));
                  return true;
                }

                // The ocid is the original dcid that was encoded into the
                // original retry packet sent to the client. We use it for
                // validation.
                config.ocid = Just(ocid);
                config.retry_scid = Just(dcid);
                break;
              }
              case kTokenMagic: {
                if (!ValidateToken(
                        version,
                        hd.token,
                        remote_address,
                        token_secret_,
                        options_.token_expiration * NGTCP2_SECONDS)) {
                  SendRetry(version, dcid, scid, local_address, remote_address);
                  return true;
                }
                hd.token.base = nullptr;
                hd.token.len = 0;
                break;
              }
              default: {
                SendRetry(version, dcid, scid, local_address, remote_address);
                return true;
              }
            }

            // Ok! If we've got this far, our token is valid! Which means our
            // path to the remote address is valid (for now). Let's record that
            // so we don't have to do this dance again for this endpoint
            // instance.
            addrLRU_.Upsert(remote_address)->validated = true;
            config.token = hd.token;
          } else if (hd.token.len > 0) {
            DEBUG(this,
                  "Address validation is disabled but there is a token! "
                  "Ignoring");
            // If validation is turned off and there is a token, that's weird.
            // The peer should only have a token if we sent it to them and we
            // wouldn't have sent it unless validation was turned on. Let's
            // assume the peer is buggy or malicious and drop the packet on the
            // floor.
            return false;
          } else {
            DEBUG(
                this,
                "Warning: Address validation is turned off for this endpoint");
          }
          break;
        case NGTCP2_PKT_0RTT:
          // If it's a 0RTT packet, we're always going to perform path
          // validation no matter what.
          SendRetry(version, dcid, scid, local_address, remote_address);
          return true;
      }
    }

    return accept(config, std::move(store));
  };

  // When a received packet contains a QUIC short header but cannot be matched
  // to a known Session, it is either (a) garbage, (b) a valid packet for a
  // connection we no longer have state for, or (c) a stateless reset. Because
  // we do not yet know if we are going to process the packet, we need to try to
  // quickly determine -- with as little cost as possible -- whether the packet
  // contains a reset token. We do so by checking the final
  // NGTCP2_STATELESS_RESET_TOKENLEN bytes in the packet to see if they match
  // one of the known reset tokens previously given by the remote peer. If
  // there's a match, then it's a reset token, if not, we move on the to the
  // next check. It is very important that this check be as inexpensive as
  // possible to avoid a DOS vector.
  const auto maybeStatelessReset = [&](const CID& dcid,
                                       const CID& scid,
                                       Store& store,
                                       const SocketAddress& local_address,
                                       const SocketAddress& remote_address) {
    if (options_.disable_stateless_reset ||
        store.length() < NGTCP2_STATELESS_RESET_TOKENLEN)
      return false;

    ngtcp2_vec vec = store;
    vec.base += vec.len;
    vec.base -= NGTCP2_STATELESS_RESET_TOKENLEN;

    Session* session = nullptr;
    auto it = token_map_.find(StatelessResetToken(vec.base));
    if (it != token_map_.end()) session = it->second;

    DEBUG(this, "Received stateless reset");

    return session != nullptr ? receive(std::move(store),
                                        local_address,
                                        remote_address,
                                        dcid,
                                        scid)
                              : false;
  };

  CHECK_LE(nread, buf.len);

  // When diagnostic packet loss is enabled, the packet will be randomly
  // dropped.
  if (UNLIKELY(is_diagnostic_packet_loss(options_.rx_loss))) {
    // Simulating rx packet loss
    return;
  }

  // TODO(@jasnell): Implement blocklist support
  // if (UNLIKELY(block_list_->Apply(remote_address))) {
  //   Debug(this, "Ignoring blocked remote address: %s", remote_address);
  //   return;
  // }

  std::shared_ptr<BackingStore> backing = env()->release_managed_buffer(buf);
  if (UNLIKELY(!backing))
    return Destroy(CloseContext::RECEIVE_FAILURE, UV_ENOMEM);

  Store store(backing, nread, 0);

  ngtcp2_vec vec = store;
  ngtcp2_version_cid pversion_cid;

  // This is our first check to see if the received data can be processed as a
  // QUIC packet. If this fails, then the QUIC packet header is invalid and
  // cannot be processed; all we can do is ignore it. If it succeeds, we have a
  // valid QUIC header but there is still no guarantee that the packet can be
  // successfully processed.
  if (ngtcp2_pkt_decode_version_cid(
          &pversion_cid, vec.base, vec.len, NGTCP2_MAX_CIDLEN) < 0) {
    return;  // Ignore the packet!
  }

  // QUIC currently requires CID lengths of max NGTCP2_MAX_CIDLEN. The ngtcp2
  // API allows non-standard lengths, and we may want to allow non-standard
  // lengths later. But for now, we're going to ignore any packet with a
  // non-standard CID length.
  if (pversion_cid.dcidlen > NGTCP2_MAX_CIDLEN ||
      pversion_cid.scidlen > NGTCP2_MAX_CIDLEN)
    return;  // Ignore the packet!

  // Each QUIC peer has two CIDs: The Source Connection ID (or scid), and the
  // Destination Connection ID (or dcid). For each peer, the dcid is the CID
  // identifying the other peer, and the scid is the CID identifying itself.
  // That is, the client's scid is the server dcid; likewise the server's scid
  // is the client's dcid.
  //
  // The dcid and scid below are the values sent from the peer received in the
  // current packet, so in this case, dcid represents who the peer sent the
  // packet too (this endpoint) and the scid represents who sent the packet.
  CID dcid(pversion_cid.dcid, pversion_cid.dcidlen);
  CID scid(pversion_cid.scid, pversion_cid.scidlen);

  DEBUG_ARGS(this, "Received packet DCID %s (destination)", dcid);
  DEBUG_ARGS(this, "Received packet SCID %s (source)", scid)

  // We index the current sessions by the dcid of the client. For initial
  // packets, the dcid is some random value and the scid is omitted from the
  // header (it uses what quic calls a "short header"). It is unlikely (but not
  // impossible) that this randomly selected dcid will be in our index. If we do
  // happen to have a collision, as unlikely as it is, ngtcp2 will do the right
  // thing when it tries to process the packet so we really don't have to worry
  // about it here. If the dcid is not known, the listener here will be nullptr.
  //
  // When the session is established, this peer will create it's own scid and
  // will send that back to the remote peer to use as it's new dcid on
  // subsequent packets. When that session is added, we will index it by the
  // local scid, so as long as the client sends the subsequent packets with the
  // right dcid, everything will just work.

  auto session = FindSession(dcid);
  auto addr = local_address().ToChecked();

  // Once we start receiving, it's likely that we'll create a bunch of JS
  // objects, so let's go ahead and create a v8::HandleScope here.
  HandleScope handle_scope(env()->isolate());

  // If a session is not found, there are four possible reasons:
  // 1. The session has not been created yet
  // 2. The session existed once but we've lost the local state for it
  // 3. The packet is a stateless reset sent by the peer
  // 4. This is a malicious or malformed packet.
  if (!session) {
    // No existing session.

    // For the current version of QUIC, it is a short header if there is no
    // scid.
    bool is_short_header =
        (pversion_cid.version == NGTCP2_PROTO_VER_MAX && !scid);

    // Handle possible reception of a stateless reset token... If it is a
    // stateless reset, the packet will be handled with no additional action
    // necessary here. We want to return immediately without committing any
    // further resources.
    if (is_short_header &&
        maybeStatelessReset(dcid, scid, store, addr, remote_address))
      return;  // Stateless reset! Don't do any further processing.

    if (acceptInitialPacket(pversion_cid.version,
                            dcid,
                            scid,
                            std::move(store),
                            addr,
                            remote_address)) {
      // Packet was successfully received.
      return IncrementStat(&EndpointStats::packets_received);
    }
    return;  // Ignore the packet!
  }

  DEBUG_ARGS(this, "Forwarding to session (dcid: %s, scid: %s)", dcid, scid);
  // If we got here, the dcid matched the scid of a known local session. Yay!
  if (receive(std::move(store), addr, remote_address, dcid, scid))
    IncrementStat(&EndpointStats::packets_received);  // Success!
}

void Endpoint::OnSendDone(int status) {
  if (is_closed()) return;
  DEBUG_ARGS(this, "Packet sent [%d]", status);
  state_->pending_callbacks--;
  // Can we go ahead and close now? Yes, so long as there are no pending
  // callbacks and no sessions open.
  if (state_->waiting_for_callbacks == 1) {
    HandleScope scope(env()->isolate());
    MaybeDestroy();
  }
}

void Endpoint::IncrementSocketAddressCounter(const SocketAddress& addr) {
  addrLRU_.Upsert(addr)->active_connections++;
}

void Endpoint::DecrementSocketAddressCounter(const SocketAddress& addr) {
  auto* counts = addrLRU_.Peek(addr);
  if (counts != nullptr && counts->active_connections > 0)
    counts->active_connections--;
}

void Endpoint::Ref() {
  if (!is_closed()) udp_.Ref();
}

void Endpoint::Unref() {
  if (!is_closed()) udp_.Unref();
}

// ======================================================================================
// Endpoint::SocketAddressInfoTraits

bool Endpoint::SocketAddressInfoTraits::CheckExpired(
    const SocketAddress& address, const Type& type) {
  return (uv_hrtime() - type.timestamp) > kSocketAddressInfoTimeout;
}

void Endpoint::SocketAddressInfoTraits::Touch(const SocketAddress& address,
                                              Type* type) {
  type->timestamp = uv_hrtime();
}

// ======================================================================================
// JavaScript call outs

void Endpoint::EmitNewSession(const BaseObjectPtr<Session>& session) {
  if (!env()->can_call_into_js()) return;
  CallbackScope scope(this);
  session->set_wrapped();
  Local<Value> arg = session->object();

  DEBUG(this, "Emitting new session callback");

  MakeCallback(BindingState::Get(env()).session_new_callback(), 1, &arg);
}

void Endpoint::EmitEndpointDone() {
  if (!env()->can_call_into_js()) return;
  CallbackScope scope(this);

  DEBUG(this, "Emitting endpoint done callback");

  MakeCallback(BindingState::Get(env()).endpoint_done_callback(), 0, nullptr);
}

void Endpoint::EmitError(CloseContext context, int status) {
  if (!env()->can_call_into_js()) return;
  CallbackScope scope(this);
  DEBUG(this, "Emitting error callback");
  auto isolate = env()->isolate();
  Local<Value> argv[] = {Integer::New(isolate, static_cast<int>(context)),
                         Integer::New(isolate, static_cast<int>(status))};

  MakeCallback(BindingState::Get(env()).endpoint_error_callback(),
               arraysize(argv),
               argv);
}

void Endpoint::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
  tracker->TrackField("udp", udp_);
  tracker->TrackField("server_options", server_options_);
  tracker->TrackField("token_map", token_map_);
  tracker->TrackField("sessions", sessions_);
  tracker->TrackField("cid_map", dcid_to_scid_);
  tracker->TrackField("address LRU", addrLRU_);
}

// ======================================================================================
// Endpoint JavaScript API

void Endpoint::CreateEndpoint(const FunctionCallbackInfo<Value>& args) {
  CHECK(!args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  CHECK(OptionsObject::HasInstance(env, args[0]));
  OptionsObject* options;
  ASSIGN_OR_RETURN_UNWRAP(&options, args[0]);

  BaseObjectPtr<Endpoint> endpoint = Create(env, *options);
  if (endpoint) args.GetReturnValue().Set(endpoint->object());
}

void Endpoint::DoConnect(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());

  CHECK(SocketAddressBase::HasInstance(env, args[0]));
  CHECK(Session::OptionsObject::HasInstance(env, args[1]));
  CHECK_IMPLIES(!args[2]->IsUndefined(),
                SessionTicket::HasInstance(env, args[2]));

  SocketAddressBase* address;
  Session::OptionsObject* options;
  SessionTicket* sessionTicket = nullptr;

  ASSIGN_OR_RETURN_UNWRAP(&address, args[0]);
  ASSIGN_OR_RETURN_UNWRAP(&options, args[1]);
  if (!args[2]->IsUndefined()) ASSIGN_OR_RETURN_UNWRAP(&sessionTicket, args[2]);

  BaseObjectPtr<Session> session =
      endpoint->Connect(*address->address(), *options, sessionTicket);
  if (session) args.GetReturnValue().Set(session->object());
}

void Endpoint::DoListen(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  Environment* env = Environment::GetCurrent(args);
  CHECK(Session::OptionsObject::HasInstance(env, args[0]));
  Session::OptionsObject* options;
  ASSIGN_OR_RETURN_UNWRAP(&options, args[0].As<Object>());
  args.GetReturnValue().Set(endpoint->Listen(options->options()));
}

void Endpoint::MarkBusy(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  endpoint->MarkAsBusy(args[0]->IsTrue());
}

void Endpoint::DoCloseGracefully(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  endpoint->CloseGracefully();
}

void Endpoint::LocalAddress(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  SocketAddress address;
  if (endpoint->local_address().To(&address)) {
    auto addr = SocketAddressBase::Create(
        env, std::make_shared<SocketAddress>(address));
    if (addr) args.GetReturnValue().Set(addr->object());
  }
}

void Endpoint::Ref(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  endpoint->Ref();
}

void Endpoint::Unref(const FunctionCallbackInfo<Value>& args) {
  Endpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  endpoint->Unref();
}

// ======================================================================================
// Endpoint::OptionsObject

Local<FunctionTemplate> Endpoint::OptionsObject::GetConstructorTemplate(
    Environment* env) {
  auto& state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state.endpoint_config_constructor_template();
  if (tmpl.IsEmpty()) {
    auto isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(kInternalFieldCount);
    tmpl->SetClassName(state.endpoint_options_string());
    SetProtoMethod(
        isolate, tmpl, "generateResetTokenSecret", GenerateResetTokenSecret);
    SetProtoMethod(isolate, tmpl, "setResetTokenSecret", SetResetTokenSecret);
    state.set_endpoint_config_constructor_template(tmpl);
  }
  return tmpl;
}

void Endpoint::OptionsObject::Initialize(Environment* env,
                                         Local<Object> target) {
  SetConstructorFunction(env->context(),
                         target,
                         "EndpointOptions",
                         GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);
}

template <>
Maybe<bool> Endpoint::OptionsObject::SetOption<uint64_t>(
    const Local<Object>& object,
    const Local<String>& name,
    uint64_t Endpoint::Options::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined()) return Just(false);

  CHECK_IMPLIES(!value->IsBigInt(), value->IsNumber());

  uint64_t val = 0;
  if (value->IsBigInt()) {
    bool lossless = true;
    val = value.As<BigInt>()->Uint64Value(&lossless);
    if (!lossless) {
      Utf8Value label(env()->isolate(), name);
      THROW_ERR_OUT_OF_RANGE(
          env(),
          (std::string("options.") + (*label) + " is out of range").c_str());
      return Nothing<bool>();
    }
  } else {
    val = static_cast<int64_t>(value.As<Number>()->Value());
  }
  options_.*member = val;
  return Just(true);
}

template <>
Maybe<bool> Endpoint::OptionsObject::SetOption<uint32_t>(
    const Local<Object>& object,
    const Local<String>& name,
    uint32_t Endpoint::Options::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined()) return Just(false);

  CHECK(value->IsUint32());

  uint32_t val = value.As<Uint32>()->Value();
  options_.*member = val;
  return Just(true);
}

template <>
Maybe<bool> Endpoint::OptionsObject::SetOption<uint8_t>(
    const Local<Object>& object,
    const Local<String>& name,
    uint8_t Endpoint::Options::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined()) return Just(false);

  CHECK(value->IsUint32());

  uint32_t val = value.As<Uint32>()->Value();
  if (val > 255) return Just(false);
  options_.*member = static_cast<uint8_t>(val);
  return Just(true);
}

template <>
Maybe<bool> Endpoint::OptionsObject::SetOption<double>(
    const Local<Object>& object,
    const Local<String>& name,
    double Endpoint::Options::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined()) return Just(false);

  CHECK(value->IsNumber());
  double val = value.As<Number>()->Value();
  options_.*member = val;
  return Just(true);
}

template <>
Maybe<bool> Endpoint::OptionsObject::SetOption<ngtcp2_cc_algo>(
    const Local<Object>& object,
    const Local<String>& name,
    ngtcp2_cc_algo Endpoint::Options::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined()) return Just(false);

  ngtcp2_cc_algo val = static_cast<ngtcp2_cc_algo>(value.As<Int32>()->Value());
  switch (val) {
    case NGTCP2_CC_ALGO_CUBIC:
      // Fall through
    case NGTCP2_CC_ALGO_RENO:
      // Fall through
    case NGTCP2_CC_ALGO_BBR:
      // Fall through
    case NGTCP2_CC_ALGO_BBR2:
      options_.*member = val;
      break;
    default:
      UNREACHABLE();
  }

  return Just(true);
}

template <>
Maybe<bool> Endpoint::OptionsObject::SetOption<bool>(
    const Local<Object>& object,
    const Local<String>& name,
    bool Endpoint::Options::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();
  if (value->IsUndefined()) return Just(false);
  CHECK(value->IsBoolean());
  options_.*member = value->IsTrue();
  return Just(true);
}

void Endpoint::OptionsObject::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);

  OptionsObject* options =
      new OptionsObject(env, args.This(), Endpoint::Options{});
  options->options().GenerateResetTokenSecret();

  CHECK(SocketAddressBase::HasInstance(env, args[0]));
  SocketAddressBase* address;
  ASSIGN_OR_RETURN_UNWRAP(&address, args[0]);

  options->options().local_address = *address->address();

  if (LIKELY(args[1]->IsObject())) {
    auto& state = BindingState::Get(env);
    Local<Object> object = args[1].As<Object>();
    if (UNLIKELY(options
                     ->SetOption(object,
                                 state.retry_token_expiration_string(),
                                 &Endpoint::Options::retry_token_expiration)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.token_expiration_string(),
                                 &Endpoint::Options::token_expiration)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.max_window_override_string(),
                                 &Endpoint::Options::max_window_override)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.max_stream_window_override_string(),
                                 &Endpoint::Options::max_stream_window_override)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.max_connections_per_host_string(),
                                 &Endpoint::Options::max_connections_per_host)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.max_connections_total_string(),
                                 &Endpoint::Options::max_connections_total)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.max_stateless_resets_string(),
                                 &Endpoint::Options::max_stateless_resets)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.address_lru_size_string(),
                                 &Endpoint::Options::address_lru_size)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.retry_limit_string(),
                                 &Endpoint::Options::retry_limit)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.max_payload_size_string(),
                                 &Endpoint::Options::max_payload_size)
                     .IsNothing()) ||
        UNLIKELY(
            options
                ->SetOption(object,
                            state.unacknowledged_packet_threshold_string(),
                            &Endpoint::Options::unacknowledged_packet_threshold)
                .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.validate_address_string(),
                                 &Endpoint::Options::validate_address)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.disable_stateless_reset_string(),
                                 &Endpoint::Options::disable_stateless_reset)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.rx_packet_loss_string(),
                                 &Endpoint::Options::rx_loss)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.tx_packet_loss_string(),
                                 &Endpoint::Options::tx_loss)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.cc_algorithm_string(),
                                 &Endpoint::Options::cc_algorithm)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.ipv6_only_string(),
                                 &Endpoint::Options::ipv6_only)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.udp_receive_buffer_size_string(),
                                 &Endpoint::Options::udp_receive_buffer_size)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.udp_send_buffer_size_string(),
                                 &Endpoint::Options::udp_send_buffer_size)
                     .IsNothing()) ||
        UNLIKELY(options
                     ->SetOption(object,
                                 state.udp_ttl_string(),
                                 &Endpoint::Options::udp_ttl)
                     .IsNothing())) {
      // The if block intentionally does nothing. The code is structured like
      // this to shortcircuit if any of the SetOptions() returns Nothing.
    }
  }
}

void Endpoint::OptionsObject::GenerateResetTokenSecret(
    const FunctionCallbackInfo<Value>& args) {
  OptionsObject* options;
  ASSIGN_OR_RETURN_UNWRAP(&options, args.Holder());
  options->options().GenerateResetTokenSecret();
}

void Endpoint::OptionsObject::SetResetTokenSecret(
    const FunctionCallbackInfo<Value>& args) {
  OptionsObject* options;
  ASSIGN_OR_RETURN_UNWRAP(&options, args.Holder());

  crypto::ArrayBufferOrViewContents<uint8_t> secret(args[0]);
  CHECK_EQ(secret.size(), kTokenSecretLen);
  memcpy(options->options().reset_token_secret, secret.data(), secret.size());
}

Endpoint::OptionsObject::OptionsObject(Environment* env,
                                       Local<Object> object,
                                       Endpoint::Options options)
    : BaseObject(env, object), options_(options) {
  MakeWeak();
}

void Endpoint::OptionsObject::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
}

}  // namespace quic
}  // namespace node
