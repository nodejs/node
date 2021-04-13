#include "quic/buffer.h"
#include "quic/crypto.h"
#include "quic/endpoint.h"
#include "quic/quic.h"
#include "quic/qlog.h"
#include "quic/session.h"
#include "quic/stream.h"
#include "crypto/crypto_util.h"
#include "aliased_struct-inl.h"
#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_mem-inl.h"
#include "node_sockaddr-inl.h"
#include "req_wrap-inl.h"
#include "udp_wrap.h"
#include "v8.h"

#include <ngtcp2/ngtcp2_crypto.h>
#include <openssl/evp.h>

namespace node {

using v8::BackingStore;
using v8::BigInt;
using v8::Context;
using v8::EscapableHandleScope;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::PropertyAttribute;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

namespace quic {

namespace {
// The reserved version is a mechanism QUIC endpoints
// can use to ensure correct handling of version
// negotiation. It is defined by the QUIC spec in
// https://tools.ietf.org/html/draft-ietf-quic-transport-24#section-6.3
// Specifically, any version that follows the pattern
// 0x?a?a?a?a may be used to force version negotiation.
inline quic_version GenerateReservedVersion(
    const std::shared_ptr<SocketAddress>& addr,
    const quic_version version) {
  socklen_t addrlen = addr->length();
  quic_version h = 0x811C9DC5u;
  quic_version ver = htonl(version);
  const uint8_t* p = addr->raw();
  const uint8_t* ep = p + addrlen;
  for (; p != ep; ++p) {
    h ^= *p;
    h *= 0x01000193u;
  }
  p =  reinterpret_cast<const uint8_t*>(&ver);
  ep = p + sizeof(version);
  for (; p != ep; ++p) {
    h ^= *p;
    h *= 0x01000193u;
  }
  h &= 0xf0f0f0f0u;
  h |= 0x0a0a0a0au;
  return h;
}

MaybeLocal<Value> GetExceptionForContext(
    Environment* env,
    Endpoint::CloseListener::Context context) {
  EscapableHandleScope scope(env->isolate());
  switch (context) {
    case Endpoint::CloseListener::Context::RECEIVE_FAILURE:
      return scope.Escape(ERR_QUIC_ENDPOINT_RECEIVE_FAILURE(env->isolate()));
    case Endpoint::CloseListener::Context::SEND_FAILURE:
      return scope.Escape(ERR_QUIC_ENDPOINT_SEND_FAILURE(env->isolate()));
    case Endpoint::CloseListener::Context::LISTEN_FAILURE:
      return scope.Escape(ERR_QUIC_ENDPOINT_LISTEN_FAILURE(env->isolate()));
    default:
      UNREACHABLE();
  }
}
}  // namespace

Endpoint::Config::Config() {
  GenerateResetTokenSecret();
}

Endpoint::Config::Config(const Config& other) noexcept
    : local_address(other.local_address),
      retry_token_expiration(other.retry_token_expiration),
      token_expiration(other.token_expiration),
      max_window_override(other.max_window_override),
      max_stream_window_override(other.max_stream_window_override),
      max_connections_per_host(other.max_connections_per_host),
      max_connections_total(other.max_connections_total),
      max_stateless_resets(other.max_stateless_resets),
      address_lru_size(other.address_lru_size),
      retry_limit(other.retry_limit),
      max_payload_size(other.max_payload_size),
      unacknowledged_packet_threshold(
          other.unacknowledged_packet_threshold),
      validate_address(other.validate_address),
      disable_stateless_reset(other.disable_stateless_reset),
      rx_loss(other.rx_loss),
      tx_loss(other.tx_loss),
      cc_algorithm(other.cc_algorithm),
      ipv6_only(other.ipv6_only),
      udp_receive_buffer_size(other.udp_receive_buffer_size),
      udp_send_buffer_size(other.udp_send_buffer_size),
      udp_ttl(other.udp_ttl) {
  memcpy(
      reset_token_secret,
      other.reset_token_secret,
      NGTCP2_STATELESS_RESET_TOKENLEN);
}

void Endpoint::Config::GenerateResetTokenSecret() {
  crypto::EntropySource(
      reinterpret_cast<unsigned char*>(&reset_token_secret),
      NGTCP2_STATELESS_RESET_TOKENLEN);
}

bool Endpoint::SocketAddressInfoTraits::CheckExpired(
    const SocketAddress& address,
    const Type& type) {
  return (uv_hrtime() - type.timestamp) > kSocketAddressInfoTimeout;
}

void Endpoint::SocketAddressInfoTraits::Touch(
    const SocketAddress& address,
    Type* type) {
  type->timestamp = uv_hrtime();
}

template<>
void StatsTraitsImpl<EndpointStats, Endpoint>::ToString(
    const Endpoint& ptr,
    AddStatsField add_field) {
#define V(_n, name, label) add_field(label, ptr.GetStat(&EndpointStats::name));
  ENDPOINT_STATS(V)
#undef V
}

bool ConfigObject::HasInstance(Environment* env, const Local<Value>& value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> ConfigObject::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state->endpoint_config_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "ConfigObject"));
    env->SetProtoMethod(
        tmpl,
        "generateResetTokenSecret",
        GenerateResetTokenSecret);
    env->SetProtoMethod(
        tmpl,
        "setResetTokenSecret",
        SetResetTokenSecret);
    state->set_endpoint_config_constructor_template(tmpl);
  }
  return tmpl;
}

void ConfigObject::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "ConfigObject",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);
}

template <> Maybe<bool> ConfigObject::SetOption<uint64_t>(
    const Local<Object>& object,
    const Local<String>& name,
    uint64_t Endpoint::Config::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined())
    return Just(false);

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
  config_.get()->*member = val;
  return Just(true);
}

template <> Maybe<bool> ConfigObject::SetOption<uint32_t>(
    const Local<Object>& object,
    const Local<String>& name,
    uint32_t Endpoint::Config::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined())
    return Just(false);

  CHECK(value->IsUint32());

  uint32_t val = value.As<Uint32>()->Value();
  config_.get()->*member = val;
  return Just(true);
}

template <> Maybe<bool> ConfigObject::SetOption<uint8_t>(
    const Local<Object>& object,
    const Local<String>& name,
    uint8_t Endpoint::Config::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined())
    return Just(false);

  CHECK(value->IsUint32());

  uint32_t val = value.As<Uint32>()->Value();
  if (val > 255) return Just(false);
  config_.get()->*member = static_cast<uint8_t>(val);
  return Just(true);
}

template <> Maybe<bool> ConfigObject::SetOption<double>(
    const Local<Object>& object,
    const Local<String>& name,
    double Endpoint::Config::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined())
    return Just(false);

  CHECK(value->IsNumber());
  double val = value.As<Number>()->Value();
  config_.get()->*member = val;
  return Just(true);
}

template <> Maybe<bool> ConfigObject::SetOption<ngtcp2_cc_algo>(
    const Local<Object>& object,
    const Local<String>& name,
    ngtcp2_cc_algo Endpoint::Config::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();

  if (value->IsUndefined())
    return Just(false);

  ngtcp2_cc_algo val = static_cast<ngtcp2_cc_algo>(value.As<Int32>()->Value());
  switch (val) {
    case NGTCP2_CC_ALGO_CUBIC:
      // Fall through
    case NGTCP2_CC_ALGO_RENO:
      config_.get()->*member = val;
      break;
    default:
      UNREACHABLE();
  }

  return Just(true);
}

template <> Maybe<bool> ConfigObject::SetOption<bool>(
    const Local<Object>& object,
    const Local<String>& name,
    bool Endpoint::Config::*member) {
  Local<Value> value;
  if (UNLIKELY(!object->Get(env()->context(), name).ToLocal(&value)))
    return Nothing<bool>();
  if (value->IsUndefined())
    return Just(false);
  CHECK(value->IsBoolean());
  config_.get()->*member = value->IsTrue();
  return Just(true);
}

void ConfigObject::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);

  ConfigObject* config = new ConfigObject(env, args.This());
  config->data()->GenerateResetTokenSecret();

  CHECK(SocketAddressBase::HasInstance(env, args[0]));
  SocketAddressBase* address;
  ASSIGN_OR_RETURN_UNWRAP(&address, args[0]);

  config->data()->local_address = address->address();

  if (LIKELY(args[1]->IsObject())) {
    BindingState* state = BindingState::Get(env);
    Local<Object> object = args[1].As<Object>();
    if (UNLIKELY(config->SetOption(
            object,
            state->retry_token_expiration_string(),
            &Endpoint::Config::retry_token_expiration).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->token_expiration_string(),
            &Endpoint::Config::token_expiration).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->max_window_override_string(),
            &Endpoint::Config::max_window_override).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->max_stream_window_override_string(),
            &Endpoint::Config::max_stream_window_override).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->max_connections_per_host_string(),
            &Endpoint::Config::max_connections_per_host).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->max_connections_total_string(),
            &Endpoint::Config::max_connections_total).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->max_stateless_resets_string(),
            &Endpoint::Config::max_stateless_resets).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->address_lru_size_string(),
            &Endpoint::Config::address_lru_size).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->retry_limit_string(),
            &Endpoint::Config::retry_limit).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->max_payload_size_string(),
            &Endpoint::Config::max_payload_size).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->unacknowledged_packet_threshold_string(),
            &Endpoint::Config::unacknowledged_packet_threshold).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->validate_address_string(),
            &Endpoint::Config::validate_address).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->disable_stateless_reset_string(),
            &Endpoint::Config::disable_stateless_reset).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->rx_packet_loss_string(),
            &Endpoint::Config::rx_loss).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->tx_packet_loss_string(),
            &Endpoint::Config::tx_loss).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->cc_algorithm_string(),
            &Endpoint::Config::cc_algorithm).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->ipv6_only_string(),
            &Endpoint::Config::ipv6_only).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->udp_receive_buffer_size_string(),
            &Endpoint::Config::udp_receive_buffer_size).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->udp_send_buffer_size_string(),
            &Endpoint::Config::udp_send_buffer_size).IsNothing()) ||
        UNLIKELY(config->SetOption(
            object,
            state->udp_ttl_string(),
            &Endpoint::Config::udp_ttl).IsNothing())) {
      // The if block intentionally does nothing. The code is structured
      // like this to shortcircuit if any of the SetOptions() returns Nothing.
    }
  }
}

void ConfigObject::GenerateResetTokenSecret(
    const FunctionCallbackInfo<Value>& args) {
  ConfigObject* config;
  ASSIGN_OR_RETURN_UNWRAP(&config, args.Holder());
  config->data()->GenerateResetTokenSecret();
}

void ConfigObject::SetResetTokenSecret(
    const FunctionCallbackInfo<Value>& args) {
  ConfigObject* config;
  ASSIGN_OR_RETURN_UNWRAP(&config, args.Holder());

  crypto::ArrayBufferOrViewContents<uint8_t> secret(args[0]);
  CHECK_EQ(secret.size(), sizeof(config->data()->reset_token_secret));
  memcpy(config->data()->reset_token_secret, secret.data(), secret.size());
}

ConfigObject::ConfigObject(
    Environment* env,
    Local<Object> object,
    std::shared_ptr<Endpoint::Config> config)
    : BaseObject(env, object),
      config_(config) {
  MakeWeak();
}

void ConfigObject::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("config", config_);
}

Local<FunctionTemplate> Endpoint::SendWrap::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = BindingState::Get(env);
  CHECK_NOT_NULL(state);
  Local<FunctionTemplate> tmpl = state->send_wrap_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = FunctionTemplate::New(env->isolate());
    tmpl->Inherit(UdpSendWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        SendWrap::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "QuicSendWrap"));
    state->set_send_wrap_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<Endpoint::SendWrap> Endpoint::SendWrap::Create(
    Environment* env,
    const std::shared_ptr<SocketAddress>& destination,
    std::unique_ptr<Packet> packet,
    BaseObjectPtr<EndpointWrap> endpoint) {
  Local<Object> obj;
  if (UNLIKELY(!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj))) {
    return BaseObjectPtr<SendWrap>();
  }

  return MakeBaseObject<SendWrap>(
      env,
      obj,
      destination,
      std::move(packet),
      std::move(endpoint));
}

Endpoint::SendWrap::SendWrap(
    Environment* env,
    v8::Local<v8::Object> object,
    const std::shared_ptr<SocketAddress>& destination,
    std::unique_ptr<Packet> packet,
    BaseObjectPtr<EndpointWrap> endpoint)
    : UdpSendWrap(env, object, AsyncWrap::PROVIDER_QUICSENDWRAP),
      destination_(destination),
      packet_(std::move(packet)),
      endpoint_(std::move(endpoint)),
      self_ptr_(this) {
  Debug(this, "Created");
  MakeWeak();
}

void Endpoint::SendWrap::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("destination", destination_);
  tracker->TrackField("packet", packet_);
  if (endpoint_)
    tracker->TrackField("endpoint", endpoint_);
}

void Endpoint::SendWrap::Done(int status) {
  Debug(this, "Done sending packet");
  if (endpoint_)
    endpoint_->OnSendDone(status);
  strong_ptr_.reset();
  self_ptr_.reset();
  endpoint_.reset();
}

Endpoint::Endpoint(Environment* env, const Config& config)
    : EndpointStatsBase(env),
      env_(env),
      udp_(env, this),
      config_(config),
      outbound_signal_(env, [this]() { this->ProcessOutbound(); }),
      token_aead_(CryptoAeadAes128GCM()),
      token_md_(CryptoMDSha256()),
      addrLRU_(config.address_lru_size) {
  crypto::EntropySource(
      reinterpret_cast<unsigned char*>(token_secret_),
      kTokenSecretLen);
  env->AddCleanupHook(OnCleanup, this);
}

Endpoint::~Endpoint() {
  // There should be no more sessions and all queues and lists should be empty.
  CHECK(sessions_.empty());
  CHECK(token_map_.empty());
  CHECK(outbound_.empty());
  CHECK(listeners_.empty());
  outbound_signal_.Close();
  env()->RemoveCleanupHook(OnCleanup, this);
}

void Endpoint::OnCleanup(void* data) {
  Endpoint* endpoint = static_cast<Endpoint*>(data);
  endpoint->Close();
}

void Endpoint::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("udp", udp_);
  tracker->TrackField("outbound", outbound_);
  tracker->TrackField("addrLRU", addrLRU_);
}

void Endpoint::ProcessReceiveFailure(int status) {
  Close(CloseListener::Context::RECEIVE_FAILURE, status);
}

void Endpoint::AddCloseListener(CloseListener* listener) {
  close_listeners_.insert(listener);
}

void Endpoint::RemoveCloseListener(CloseListener* listener) {
  close_listeners_.erase(listener);
}

void Endpoint::Close(CloseListener::Context context, int status) {
  RecordTimestamp(&EndpointStats::destroyed_at);

  // Cancel any remaining outbound packets. Ideally there wouldn't
  // be any, but at this point there's nothing else we can do.
  SendWrap::Queue outbound;
  outbound_.swap(outbound);
  for (const auto& packet : outbound)
    packet->Done(UV_ECANCELED);
  outbound.clear();
  pending_outbound_ = 0;

  // Notify all of the registered EndpointWrap instances that
  // this shared endpoint is closed.
  for (const auto listener : close_listeners_)
    listener->EndpointClosed(context, status);

  udp_.CloseHandle();
}

bool Endpoint::AcceptInitialPacket(
    const quic_version version,
    const CID& dcid,
    const CID& scid,
    std::shared_ptr<v8::BackingStore> store,
    size_t nread,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address) {
  if (listeners_.empty()) return false;

  ngtcp2_pkt_hd hd;
  CID ocid;

  switch (ngtcp2_accept(&hd, static_cast<uint8_t*>(store->Data()), nread)) {
    case 1:
      // Send Version Negotiation
      Debug(env_, DebugCategory::QUICENDPOINT,
            "Requested version %llu is not supported.\n", version);
      SendVersionNegotiation(
          version,
          dcid,
          scid,
          local_address,
          remote_address);
      // Fall through
    case -1:
      // Either a version negotiation packet was sent or the packet is
      // an invalid initial packet. Either way, there's nothing more we
      // can do here and we will consider this an ignored packet.
      return false;
  }

  // If the server is busy, of the number of connections total for this
  // server, and this remote addr, new connections will be shut down
  // immediately.
  if (UNLIKELY(busy_) ||
      sessions_.size() >= config_.max_connections_total ||
      current_socket_address_count(remote_address) >=
          config_.max_connections_per_host) {
    // Endpoint is busy or the connection count is exceeded
    Debug(env_, DebugCategory::QUICENDPOINT,
          "Server is busy. Connection refused\n");
    IncrementStat(&EndpointStats::server_busy_count);
    ImmediateConnectionClose(
      version,
      scid,
      dcid,
      local_address,
      remote_address,
      NGTCP2_CONNECTION_REFUSED);
    return BaseObjectPtr<Session>();
  }

  Session::Config config(this, dcid, scid, version);

  // QUIC has address validation built in to the handshake but allows for
  // an additional explicit validation request using RETRY frames. If we
  // are using explicit validation, we check for the existence of a valid
  // retry token in the packet. If one does not exist, we send a retry with
  // a new token. If it does exist, and if it's valid, we grab the original
  // cid and continue.
  if (!is_validated_address(remote_address)) {
    switch (hd.type) {
      case NGTCP2_PKT_INITIAL:
        if (LIKELY(config_.validate_address) || hd.token.len > 0) {
          // Perform explicit address validation
          if (hd.token.len == 0) {
            // No retry token was detected. Generate one.
            SendRetry(version, dcid, scid, local_address, remote_address);
            return BaseObjectPtr<Session>();
          }

          if (hd.token.base[0] != kRetryTokenMagic &&
              hd.dcid.datalen < NGTCP2_MIN_INITIAL_DCIDLEN) {
            ImmediateConnectionClose(
                version,
                scid,
                dcid,
                local_address,
                remote_address);
            return BaseObjectPtr<Session>();
          }

          switch (hd.token.base[0]) {
            case kRetryTokenMagic: {
              if (!ValidateRetryToken(
                      hd.token,
                      remote_address,
                      dcid,
                      token_secret_,
                      config_.retry_token_expiration,
                      token_aead_,
                      token_md_).To(&ocid)) {
                Debug(env_, DebugCategory::QUICENDPOINT,
                      "Invalid retry token. Connection refused.\n");
                // Invalid retry token was detected. Close the connection.
                ImmediateConnectionClose(
                    version,
                    scid,
                    dcid,
                    local_address,
                    remote_address,
                    NGTCP2_CONNECTION_REFUSED);
                return BaseObjectPtr<Session>();
              }
              break;
            }
            case kTokenMagic: {
              if (!ValidateToken(
                      hd.token,
                      remote_address,
                      token_secret_,
                      config_.token_expiration,
                      token_aead_,
                      token_md_)) {
                SendRetry(version, dcid, scid, local_address, remote_address);
                return BaseObjectPtr<Session>();
              }
              hd.token.base = nullptr;
              hd.token.len = 0;
              break;
            }
            default: {
              if (LIKELY(config_.validate_address)) {
                SendRetry(version, dcid, scid, local_address, remote_address);
                return BaseObjectPtr<Session>();
              }
              hd.token.base = nullptr;
              hd.token.len = 0;
            }
          }
          set_validated_address(remote_address);
          config.token = hd.token;
        }
        break;
      case NGTCP2_PKT_0RTT:
        SendRetry(version, dcid, scid, local_address, remote_address);
        return BaseObjectPtr<Session>();
    }
  }

  // Iterate through the available listeners, if any. If a listener
  // accepts the packet, that listener will be moved to the end of
  // the list so that another listener has the option of picking
  // up the next one.
  {
    Lock lock(this);
    for (auto it = listeners_.begin(); it != listeners_.end(); it++) {
      InitialPacketListener* listener = *it;
      if (listener->Accept(
              config,
              store,
              nread,
              local_address,
              remote_address,
              dcid,
              scid,
              ocid)) {
        listeners_.erase(it);
        listeners_.emplace_back(listener);
        return true;
      }
    }
  }

  return false;
}

void Endpoint::AssociateCID(const CID& cid, PacketListener* listener) {
  sessions_[cid] = listener;
  int err = StartReceiving();
  if (err && err != UV_EALREADY)
    Close(CloseListener::Context::LISTEN_FAILURE, err);
}

void Endpoint::DisassociateCID(const CID& cid) {
  sessions_.erase(cid);
  MaybeStopReceiving();
}

void Endpoint::AddInitialPacketListener(InitialPacketListener* listener) {
  listeners_.emplace_back(listener);
  int err = StartReceiving();
  if (err && err != UV_EALREADY)
    Close(CloseListener::Context::LISTEN_FAILURE, err);
}

void Endpoint::ImmediateConnectionClose(
    const quic_version version,
    const CID& scid,
    const CID& dcid,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    error_code reason) {
  std::unique_ptr<Packet> packet =
      std::make_unique<Packet>("immediate connection close");

  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Sending immediate connection close\n"
        "    dcid: %s\n"
        "    scid: %s\n"
        "    local address: %s\n"
        "    remote address: %s\n"
        "    reason: %llu\n",
        dcid,
        scid,
        local_address->ToString(),
        remote_address->ToString(),
        reason);

  ssize_t nwrite = ngtcp2_crypto_write_connection_close(
      packet->data(),
      packet->length(),
      version,
      scid.cid(),
      dcid.cid(),
      reason);
  if (nwrite <= 0) return;
  packet->set_length(static_cast<size_t>(nwrite));
  SendPacket(remote_address, std::move(packet));
}

void Endpoint::RemoveInitialPacketListener(
    InitialPacketListener* listener) {
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end())
    listeners_.erase(it);
  MaybeStopReceiving();
}

Endpoint::PacketListener* Endpoint::FindSession(const CID& cid) {
  auto session_it = sessions_.find(cid);
  if (session_it != std::end(sessions_))
    return session_it->second;
  return nullptr;
}

void Endpoint::DisassociateStatelessResetToken(
    const StatelessResetToken& token) {
  token_map_.erase(token);
}

void Endpoint::AssociateStatelessResetToken(
    const StatelessResetToken& token,
    PacketListener* listener) {
  token_map_[token] = listener;
}

int Endpoint::MaybeBind() {
  if (bound_) return 0;
  bound_ = true;
  return udp_.Bind(config_);
}

bool Endpoint::MaybeStatelessReset(
    const CID& dcid,
    const CID& scid,
    std::shared_ptr<BackingStore> store,
    size_t nread,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address) {
  if (UNLIKELY(config_.disable_stateless_reset) ||
      nread < NGTCP2_STATELESS_RESET_TOKENLEN) {
    return false;
  }
  uint8_t* ptr = static_cast<uint8_t*>(store->Data());
  ptr += nread;
  ptr -= NGTCP2_STATELESS_RESET_TOKENLEN;
  StatelessResetToken possible_token(ptr);
  Lock lock(this);
  auto it = token_map_.find(possible_token);
  if (it == token_map_.end())
    return false;
  return it->second->Receive(
      dcid,
      scid,
      std::move(store),
      nread,
      local_address,
      remote_address,
      PacketListener::Flags::STATELESS_RESET);
}

uv_buf_t Endpoint::OnAlloc(size_t suggested_size) {
  return AllocatedBuffer::AllocateManaged(env(), suggested_size).release();
}

void Endpoint::OnReceive(
    size_t nread,
    const uv_buf_t& buf,
    const std::shared_ptr<SocketAddress>& remote_address) {

  AllocatedBuffer buffer(env(), buf);
  // When diagnostic packet loss is enabled, the packet will be randomly
  // dropped based on the rx_loss probability.
  if (UNLIKELY(is_diagnostic_packet_loss(config_.rx_loss))) {
    Debug(env_, DebugCategory::QUICENDPOINT,
          "Endpoint: Simulating rx packet loss\n");
    return;
  }

  // TODO(@jasnell): Implement blocklist support
  // if (UNLIKELY(block_list_->Apply(remote_address))) {
  //   Debug(this, "Ignoring blocked remote address: %s", remote_address);
  //   return;
  // }

  IncrementStat(&EndpointStats::bytes_received, nread);

  std::shared_ptr<BackingStore> store = buffer.ReleaseBackingStore();
  if (UNLIKELY(!store)) {
    // TODO(@jasnell): Send immediate close?
    ProcessReceiveFailure(UV_ENOMEM);
    return;
  }

  const uint8_t* data = reinterpret_cast<const uint8_t*>(store->Data());

  CHECK_LE(nread, store->ByteLength());

  quic_version pversion;
  const uint8_t* pdcid;
  size_t pdcidlen;
  const uint8_t* pscid;
  size_t pscidlen;

  // This is our first check to see if the received data can be
  // processed as a QUIC packet. If this fails, then the QUIC packet
  // header is invalid and cannot be processed; all we can do is ignore
  // it. If it succeeds, we have a valid QUIC header but there is still
  // no guarantee that the packet can be successfully processed.
  if (ngtcp2_pkt_decode_version_cid(
        &pversion,
        &pdcid,
        &pdcidlen,
        &pscid,
        &pscidlen,
        data,
        nread,
        NGTCP2_MAX_CIDLEN) < 0) {
    return;  // Ignore the packet!
  }

  // QUIC currently requires CID lengths of max NGTCP2_MAX_CIDLEN. The
  // ngtcp2 API allows non-standard lengths, and we may want to allow
  // non-standard lengths later. But for now, we're going to ignore any
  // packet with a non-standard CID length.
  if (pdcidlen > NGTCP2_MAX_CIDLEN || pscidlen > NGTCP2_MAX_CIDLEN)
    return;  // Ignore the packet!

  CID dcid(pdcid, pdcidlen);
  CID scid(pscid, pscidlen);

  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Receiving packet\n"
        "    dcid: %s\n"
        "    scid: %s\n",
        dcid, scid);

  PacketListener* listener = nullptr;
  {
    Lock lock(this);
    listener = FindSession(dcid);
  }

  // If a session is not found, there are four possible reasons:
  // 1. The session has not been created yet
  // 2. The session existed once but we've lost the local state for it
  // 3. The packet is a stateless reset sent by the peer
  // 4. This is a malicious or malformed packet.
  if (listener == nullptr) {
    Debug(env_, DebugCategory::QUICENDPOINT, "Endpoint: No existing session\n");
    bool is_short_header = (pversion == NGTCP2_PROTO_VER_MAX && !scid);

    // Handle possible reception of a stateless reset token...
    // If it is a stateless reset, the packet will be handled with
    // no additional action necessary here. We want to return immediately
    // without committing any further resources.
    if (is_short_header &&
        MaybeStatelessReset(
            dcid,
            scid,
            store,
            nread,
            local_address(),
            remote_address)) {
      Debug(env_, DebugCategory::QUICENDPOINT, "Endpoint: Stateless reset\n");
      return;  // Ignore the packet!
    }

    if (AcceptInitialPacket(
          pversion,
          dcid,
          scid,
          store,
          nread,
          local_address(),
          remote_address)) {
      Debug(env_, DebugCategory::QUICENDPOINT,
            "Endpoint: Initial packet accepted\n");
      return IncrementStat(&EndpointStats::packets_received);
    }
    return;  // Ignore the packet!
  }

  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Passing packet to session\n");
  if (listener->Receive(
          dcid,
          scid,
          std::move(store),
          nread,
          local_address(),
          remote_address)) {
    IncrementStat(&EndpointStats::packets_received);
  }
}

void Endpoint::ProcessOutbound() {
  Debug(env_,
        DebugCategory::QUICENDPOINT,
        "Endpoint: Processing outbound queue\n");
  SendWrap::Queue queue;
  {
    Lock lock(this);
    outbound_.swap(queue);
  }

  int err = 0;
  while (!queue.empty()) {
    auto& packet = queue.front();
    queue.pop_front();
    err = udp_.SendPacket(packet);
    if (err) {
      packet->Done(err);
      break;
    }
  }

  // If there was a fatal error sending, the Endpoint
  // will be destroyed along with all associated sessions.
  // Go ahead and cancel the remaining pending sends.
  if (err) {
    while (!queue.empty()) {
      auto& packet = queue.front();
      queue.pop_front();
      packet->Done(UV_ECANCELED);
    }
    ProcessSendFailure(err);
  }
}

void Endpoint::ProcessSendFailure(int status) {
  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Processing send failure (%d)",
        status);
  Close(CloseListener::Context::SEND_FAILURE, status);
}

void Endpoint::Ref() {
  ref_count_++;
  udp_.Ref();
}

bool Endpoint::SendPacket(
    const std::shared_ptr<SocketAddress>& remote_address,
    std::unique_ptr<Packet> packet) {
  HandleScope scope(env()->isolate());
  BaseObjectPtr<SendWrap> wrap(
      SendWrap::Create(
          env(),
          remote_address,
          std::move(packet)));
  if (!wrap) return false;
  SendPacket(std::move(wrap));
  return true;
}

void Endpoint::SendPacket(BaseObjectPtr<SendWrap> packet) {
  IncrementStat(&EndpointStats::bytes_sent, packet->packet()->length());
  IncrementStat(&EndpointStats::packets_sent);
  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Enqueing outbound packet\n");
  {
    Lock lock(this);
    outbound_.emplace_back(std::move(packet));
  }
  outbound_signal_.Send();
}

bool Endpoint::SendRetry(
    const quic_version version,
    const CID& dcid,
    const CID& scid,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address) {
  CHECK(remote_address);

  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Sending retry\n"
        "   dcid: %s\n"
        "   scid: %s\n"
        "   local address: %s\n"
        "   remote address: %s\n",
        dcid,
        scid,
        local_address->ToString(),
        remote_address->ToString());

  auto info = addrLRU_.Upsert(*remote_address.get());
  if (++(info->retry_count) > config_.retry_limit) {
    Debug(env_, DebugCategory::QUICENDPOINT,
          "Endpoint: Retry limit reached for %s",
          remote_address->ToString());
    return true;
  }
  std::unique_ptr<Packet> packet =
      GenerateRetryPacket(
          version,
          token_secret_,
          dcid,
          scid,
          local_address,
          remote_address,
          token_aead_,
          token_md_);
  if (UNLIKELY(!packet)) return false;
  return SendPacket(remote_address, std::move(packet));
}

bool Endpoint::SendStatelessReset(
    const CID& cid,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    size_t source_len) {
  if (UNLIKELY(config_.disable_stateless_reset))
    return false;

  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Sending stateless reset\n"
        "    cid: %s\n"
        "    local address: %s\n"
        "    remote address: %s\n",
        cid,
        local_address->ToString(),
        remote_address->ToString());

  constexpr static size_t kRandlen = NGTCP2_MIN_STATELESS_RESET_RANDLEN * 5;
  constexpr static size_t kMinStatelessResetLen = 41;
  uint8_t random[kRandlen];

  // Per the QUIC spec, we need to protect against sending too
  // many stateless reset tokens to an endpoint to prevent
  // endless looping.
  if (current_stateless_reset_count(remote_address) >=
          config_.max_stateless_resets) {
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

  StatelessResetToken token(config_.reset_token_secret, cid);
  crypto::EntropySource(random, kRandlen);

  std::unique_ptr<Packet> packet =
      std::make_unique<Packet>(pktlen, "stateless reset");
  ssize_t nwrite =
      ngtcp2_pkt_write_stateless_reset(
        packet->data(),
        NGTCP2_MAX_PKTLEN_IPV4,
        const_cast<uint8_t*>(token.data()),
        random,
        kRandlen);
  if (nwrite >= static_cast<ssize_t>(kMinStatelessResetLen)) {
    packet->set_length(nwrite);
    IncrementStatelessResetCounter(remote_address);
    return SendPacket(remote_address, std::move(packet));
  }
  return false;
}

Maybe<size_t> Endpoint::GenerateNewToken(
    uint8_t* token,
    const std::shared_ptr<SocketAddress>& remote_address) {
  size_t tokenlen = kMaxTokenLen;
  Debug(env_, DebugCategory::QUICENDPOINT, "Endpoint: Generating new token\n");
  if (!GenerateToken(
          token,
          &tokenlen,
          remote_address,
          token_secret_,
          token_aead_,
          token_md_)) {
    return Nothing<size_t>();
  }
  return Just(tokenlen);
}

uint32_t Endpoint::GetFlowLabel(
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const CID& cid) {
  return GenerateFlowLabel(
      local_address,
      remote_address,
      cid,
      token_secret_,
      NGTCP2_STATELESS_RESET_TOKENLEN);
}

void Endpoint::SendVersionNegotiation(
      const quic_version version,
      const CID& dcid,
      const CID& scid,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address) {
  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Sending version negotiation\n"
        "    dcid: %s\n"
        "    scid: %s\n"
        "    local address: %s\n"
        "    remote address: %s\n",
        dcid,
        scid,
        local_address->ToString(),
        remote_address->ToString());
  uint32_t sv[2];
  sv[0] = GenerateReservedVersion(remote_address, version);
  sv[1] = NGTCP2_PROTO_VER_MAX;

  uint8_t unused_random;
  crypto::EntropySource(&unused_random, 1);

  size_t pktlen = dcid.length() + scid.length() + (sizeof(sv)) + 7;

  std::unique_ptr<Packet> packet =
      std::make_unique<Packet>(pktlen, "version negotiation");
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
  if (nwrite > 0) {
    packet->set_length(nwrite);
    SendPacket(remote_address, std::move(packet));
  }
}

int Endpoint::StartReceiving() {
  if (receiving_) return UV_EALREADY;
  receiving_ = true;
  int err = MaybeBind();
  if (err) return err;
  Debug(env_, DebugCategory::QUICENDPOINT, "Endpoint: Start receiving\n");
  return udp_.StartReceiving();
}

void Endpoint::MaybeStopReceiving() {
  if (!sessions_.empty() || !listeners_.empty())
    return;
  receiving_ = false;
  Debug(env_, DebugCategory::QUICENDPOINT, "Endpoint: Stop receiving\n");
  udp_.StopReceiving();
}

void Endpoint::Unref() {
  ref_count_--;

  // Only Unref if the ref_count_ actually falls below
  if (!ref_count_) udp_.Unref();
}

bool Endpoint::is_diagnostic_packet_loss(double prob) const {
  // TODO(@jasnell): This impl could be improved by caching entropy
  if (LIKELY(prob == 0.0)) return false;
  unsigned char c = 255;
  crypto::EntropySource(&c, 1);
  return (static_cast<double>(c) / 255) < prob;
}

void Endpoint::set_validated_address(
    const std::shared_ptr<SocketAddress>& addr) {
  CHECK(addr);
  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Setting validated address (%s)\n", addr->ToString());
  addrLRU_.Upsert(*(addr.get()))->validated = true;
}

bool Endpoint::is_validated_address(
    const std::shared_ptr<SocketAddress>& addr) const {
  CHECK(addr);
  auto info = addrLRU_.Peek(*(addr.get()));
  return info != nullptr ? info->validated : false;
}

void Endpoint::IncrementStatelessResetCounter(
    const std::shared_ptr<SocketAddress>& addr) {
  CHECK(addr);
  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Increment stateless reset counter (%s)\n",
        addr->ToString());
  addrLRU_.Upsert(*(addr.get()))->reset_count++;
}

void Endpoint::IncrementSocketAddressCounter(
    const std::shared_ptr<SocketAddress>& addr) {
  CHECK(addr);
  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Increment socket address counter (%s)\n",
        addr->ToString());
  addrLRU_.Upsert(*(addr.get()))->active_connections++;
}

void Endpoint::DecrementSocketAddressCounter(
    const std::shared_ptr<SocketAddress>& addr) {
  CHECK(addr);
  Debug(env_, DebugCategory::QUICENDPOINT,
        "Endpoint: Decrement socket address counter (%s)\n",
        addr->ToString());
  SocketAddressInfoTraits::Type* counts = addrLRU_.Peek(*(addr.get()));
  if (counts != nullptr && counts->active_connections > 0)
    counts->active_connections--;
}

size_t Endpoint::current_socket_address_count(
    const std::shared_ptr<SocketAddress>& addr) const {
  CHECK(addr);
  SocketAddressInfoTraits::Type* counts = addrLRU_.Peek(*(addr.get()));
  return counts != nullptr ? counts->active_connections : 0;
}

size_t Endpoint::current_stateless_reset_count(
    const std::shared_ptr<SocketAddress>& addr) const {
  CHECK(addr);
  SocketAddressInfoTraits::Type* counts = addrLRU_.Peek(*(addr.get()));
  return counts != nullptr ? counts->reset_count : 0;
}

std::shared_ptr<SocketAddress> Endpoint::local_address() const {
  return udp_.local_address();
}

Local<FunctionTemplate> Endpoint::UDP::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state->udp_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = FunctionTemplate::New(env->isolate());
    tmpl->Inherit(HandleWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        HandleWrap::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "Session::UDP"));
    state->set_udp_constructor_template(tmpl);
  }
  return tmpl;
}

Endpoint::UDP* Endpoint::UDP::Create(
    Environment* env,
    Endpoint* endpoint) {
  Local<Object> obj;
  if (UNLIKELY(!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj))) {
    return nullptr;
  }

  return new Endpoint::UDP(env, obj, endpoint);
}

Endpoint::UDP::UDP(
    Environment* env,
    Local<Object> obj,
    Endpoint* endpoint)
    : HandleWrap(
          env,
          obj,
          reinterpret_cast<uv_handle_t*>(&handle_),
          AsyncWrap::PROVIDER_QUICENDPOINTUDP),
      endpoint_(endpoint) {
  Debug(this, "Created");
  CHECK_EQ(uv_udp_init(env->event_loop(), &handle_), 0);
  handle_.data = this;
}

std::shared_ptr<SocketAddress> Endpoint::UDP::local_address() const {
  return SocketAddress::FromSockName(handle_);
}

int Endpoint::UDP::Bind(const Endpoint::Config& config) {
  Debug(this, "Binding...");
  int flags = 0;
  if (config.local_address->family() == AF_INET6 && config.ipv6_only)
    flags |= UV_UDP_IPV6ONLY;

  int err = uv_udp_bind(
      &handle_,
      config.local_address->data(), flags);
  int size;

  if (!err) {
    size = static_cast<int>(config.udp_receive_buffer_size);
    if (size > 0) {
      err = uv_recv_buffer_size(
          reinterpret_cast<uv_handle_t*>(&handle_),
          &size);
      if (err) return err;
    }

    size = static_cast<int>(config.udp_send_buffer_size);
    if (size > 0) {
      err = uv_send_buffer_size(
          reinterpret_cast<uv_handle_t*>(&handle_),
          &size);
      if (err) return err;
    }

    size = static_cast<int>(config.udp_ttl);
    if (size > 0) {
      err = uv_udp_set_ttl(&handle_, size);
      if (err) return err;
    }
  }

  return err;
}

void Endpoint::UDP::CloseHandle() {
  Debug(this, "Closing...");
  if (is_closing()) return;
  env()->CloseHandle(reinterpret_cast<uv_handle_t*>(&handle_), ClosedCb);
}

void Endpoint::UDP::ClosedCb(uv_handle_t* handle) {
  std::unique_ptr<UDP> ptr(
      ContainerOf(&Endpoint::UDP::handle_,
                  reinterpret_cast<uv_udp_t*>(handle)));
}

void Endpoint::UDP::Ref() {
  uv_ref(reinterpret_cast<uv_handle_t*>(&handle_));
}

void Endpoint::UDP::Unref() {
  uv_unref(reinterpret_cast<uv_handle_t*>(&handle_));
}

int Endpoint::UDP::StartReceiving() {
  Debug(this, "Starting to receive packets");
  if (IsHandleClosing()) return UV_EBADF;
  int err = uv_udp_recv_start(&handle_, OnAlloc, OnReceive);
  if (err == UV_EALREADY)
    err = 0;
  return err;
}

void Endpoint::UDP::StopReceiving() {
  if (!IsHandleClosing()) {
    Debug(this, "Stop receiving packets");
    USE(uv_udp_recv_stop(&handle_));
  }
}

int Endpoint::UDP::SendPacket(BaseObjectPtr<SendWrap> req) {
  CHECK(req);
  // Attach a strong pointer to the UDP instance to
  // ensure that it is not freed until all of the
  // dispatched SendWraps are freed.
  req->Attach(BaseObjectPtr<BaseObject>(this));
  uv_buf_t buf = req->packet()->buf();
  Debug(this, "Sending UDP packet. %llu bytes.", buf.len);
  const sockaddr* dest = req->destination()->data();
  return req->Dispatch(
      uv_udp_send,
      &handle_,
      &buf, 1,
      dest,
      uv_udp_send_cb{[](uv_udp_send_t* req, int status) {
        std::unique_ptr<SendWrap> ptr(
          static_cast<SendWrap*>(UdpSendWrap::from_req(req)));
        ptr->Done(status);
      }});
}

void Endpoint::UDP::OnAlloc(
    uv_handle_t* handle,
    size_t suggested_size,
    uv_buf_t* buf) {
  UDP* udp =
      ContainerOf(
        &Endpoint::UDP::handle_,
        reinterpret_cast<uv_udp_t*>(handle));
  *buf = udp->endpoint_->OnAlloc(suggested_size);
}

void Endpoint::UDP::OnReceive(
    uv_udp_t* handle,
    ssize_t nread,
    const uv_buf_t* buf,
    const sockaddr* addr,
    unsigned int flags) {
  UDP* udp = ContainerOf(&Endpoint::UDP::handle_, handle);
  if (nread < 0) {
    udp->endpoint_->ProcessReceiveFailure(static_cast<int>(nread));
    return;
  }

  // Nothing to do it in this case.
  if (nread == 0) return;

  Debug(udp, "Receiving UDP packet. %llu bytes.", nread);

  CHECK_NOT_NULL(addr);

  if (UNLIKELY(flags & UV_UDP_PARTIAL)) {
    udp->endpoint_->ProcessReceiveFailure(UV_ENOBUFS);
    return;
  }

  udp->endpoint_->OnReceive(
      static_cast<size_t>(nread),
      *buf,
      std::make_shared<SocketAddress>(addr));
}

Endpoint::UDPHandle::UDPHandle(Environment* env, Endpoint* endpoint)
    : env_(env),
      udp_(Endpoint::UDP::Create(env, endpoint)) {
  CHECK_NOT_NULL(udp_);
  env->AddCleanupHook(CleanupHook, this);
}

void Endpoint::UDPHandle::CloseHandle() {
  if (udp_ != nullptr) {
    env_->RemoveCleanupHook(CleanupHook, this);
    udp_->CloseHandle();
  }
  udp_ = nullptr;
}

void Endpoint::UDPHandle::MemoryInfo(MemoryTracker* tracker) const {
  if (udp_)
    tracker->TrackField("udp", udp_);
}

void Endpoint::UDPHandle::CleanupHook(void* data) {
  static_cast<UDPHandle*>(data)->CloseHandle();
}

bool EndpointWrap::HasInstance(Environment* env, const Local<Value>& value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> EndpointWrap::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state->endpoint_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(IllegalConstructor);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Endpoint"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        EndpointWrap::kInternalFieldCount);
    env->SetProtoMethod(
        tmpl,
        "listen",
        StartListen);
    env->SetProtoMethod(
        tmpl,
        "waitForPendingCallbacks",
        StartWaitForPendingCallbacks);
    env->SetProtoMethod(
        tmpl,
        "createClientSession",
        CreateClientSession);
    env->SetProtoMethodNoSideEffect(
        tmpl,
        "address",
        LocalAddress);
    env->SetProtoMethod(tmpl, "ref", Ref);
    env->SetProtoMethod(tmpl, "unref", Unref);
    state->set_endpoint_constructor_template(tmpl);
  }
  return tmpl;
}

void EndpointWrap::Initialize(Environment* env, Local<Object> target) {
  env->SetMethod(target, "createEndpoint", CreateEndpoint);

  ConfigObject::Initialize(env, target);

#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_##name);
  ENDPOINT_STATS(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATS_ENDPOINT_COUNT);
#undef V
#define V(name, _, __) NODE_DEFINE_CONSTANT(target, IDX_STATE_ENDPOINT_##name);
  ENDPOINT_STATE(V)
  NODE_DEFINE_CONSTANT(target, IDX_STATE_ENDPOINT_COUNT);
#undef V

#define CLOSE_CONSTANT(name)                                                   \
  do {                                                                         \
    constexpr int ENDPOINT_CLOSE_CONTEXT_##name =                              \
        static_cast<int>(Endpoint::CloseListener::Context::name);              \
    NODE_DEFINE_CONSTANT(target, ENDPOINT_CLOSE_CONTEXT_##name);               \
  } while (0);

  CLOSE_CONSTANT(CLOSE);
  CLOSE_CONSTANT(RECEIVE_FAILURE);
  CLOSE_CONSTANT(SEND_FAILURE);
  CLOSE_CONSTANT(LISTEN_FAILURE);
#undef CLOSE_CONSTANT
}

void EndpointWrap::CreateClientSession(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  EndpointWrap* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());

  CHECK(SocketAddressBase::HasInstance(env, args[0]));
  CHECK(OptionsObject::HasInstance(env, args[1]));
  CHECK(crypto::SecureContext::HasInstance(env, args[2]));

  SocketAddressBase* address;
  OptionsObject* options;
  crypto::SecureContext* context;

  ASSIGN_OR_RETURN_UNWRAP(&address, args[0]);
  ASSIGN_OR_RETURN_UNWRAP(&options, args[1]);
  ASSIGN_OR_RETURN_UNWRAP(&context, args[2]);

  Session::Config config(
    endpoint->inner_.get(),
    NGTCP2_PROTO_VER_MAX);

  if (options->options()->qlog)
    config.EnableQLog();

  crypto::ArrayBufferOrViewContents<uint8_t> session_ticket;
  crypto::ArrayBufferOrViewContents<uint8_t> remote_transport_params;
  CHECK_IMPLIES(args[3]->IsUndefined(), args[4]->IsUndefined());
  if (!args[3]->IsUndefined()) {
    session_ticket = crypto::ArrayBufferOrViewContents<uint8_t>(args[3]);
    remote_transport_params =
        crypto::ArrayBufferOrViewContents<uint8_t>(args[4]);
  }

  BaseObjectPtr<Session> session = endpoint->CreateClient(
    address->address(),
    config,
    options->options(),
    BaseObjectPtr<crypto::SecureContext>(context),
    session_ticket,
    remote_transport_params);

  if (UNLIKELY(!session)) {
    return THROW_ERR_QUIC_INTERNAL_ERROR(
        env, "Failure to create new client session");
  }

  args.GetReturnValue().Set(session->object());
}

void EndpointWrap::CreateEndpoint(const FunctionCallbackInfo<Value>& args) {
  CHECK(!args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  CHECK(ConfigObject::HasInstance(env, args[0]));
  ConfigObject* config;
  ASSIGN_OR_RETURN_UNWRAP(&config, args[0]);

  BaseObjectPtr<EndpointWrap> endpoint = Create(env, config->config());
  if (LIKELY(endpoint))
    args.GetReturnValue().Set(endpoint->object());
}

void EndpointWrap::StartListen(const FunctionCallbackInfo<Value>& args) {
  EndpointWrap* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  Environment* env = Environment::GetCurrent(args);
  CHECK(OptionsObject::HasInstance(env, args[0]));
  OptionsObject* options;
  ASSIGN_OR_RETURN_UNWRAP(&options, args[0].As<Object>());
  CHECK(crypto::SecureContext::HasInstance(env, args[1]));
  crypto::SecureContext* context;
  ASSIGN_OR_RETURN_UNWRAP(&context, args[1]);
  endpoint->Listen(
      options->options(),
      BaseObjectPtr<crypto::SecureContext>(context));
}

void EndpointWrap::StartWaitForPendingCallbacks(
    const FunctionCallbackInfo<Value>& args) {
  EndpointWrap* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
  endpoint->WaitForPendingCallbacks();
}

Maybe<size_t> EndpointWrap::GenerateNewToken(
    uint8_t* token,
    const std::shared_ptr<SocketAddress>& remote_address) {
  return inner_->GenerateNewToken(token, remote_address);
}

void EndpointWrap::LocalAddress(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    EndpointWrap* endpoint;
    ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.Holder());
    BaseObjectPtr<SocketAddressBase> addr;
    std::shared_ptr<SocketAddress> address = endpoint->inner_->local_address();
    if (address)
      addr = SocketAddressBase::Create(env, address);
    if (addr)
      args.GetReturnValue().Set(addr->object());
}

void EndpointWrap::Ref(const FunctionCallbackInfo<Value>& args) {
  EndpointWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  Endpoint::Lock lock(wrap->inner_);
  wrap->inner_->Ref();
}

void EndpointWrap::Unref(const FunctionCallbackInfo<Value>& args) {
  EndpointWrap* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.Holder());
  Endpoint::Lock lock(wrap->inner_);
  wrap->inner_->Unref();
}

BaseObjectPtr<EndpointWrap> EndpointWrap::Create(
    Environment* env,
    const Endpoint::Config& config) {
  Local<Object> obj;
  if (UNLIKELY(!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj))) {
    return BaseObjectPtr<EndpointWrap>();
  }

  return MakeBaseObject<EndpointWrap>(env, obj, config);
}

BaseObjectPtr<EndpointWrap> EndpointWrap::Create(
    Environment* env,
    std::shared_ptr<Endpoint> endpoint) {
  Local<Object> obj;
  if (UNLIKELY(!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj))) {
    return BaseObjectPtr<EndpointWrap>();
  }

  return MakeBaseObject<EndpointWrap>(env, obj, std::move(endpoint));
}

EndpointWrap::EndpointWrap(
    Environment* env,
    Local<Object> object,
    const Endpoint::Config& config)
    : EndpointWrap(
          env,
          object,
          std::make_shared<Endpoint>(env, config)) {}

EndpointWrap::EndpointWrap(
    Environment* env,
    Local<Object> object,
    std::shared_ptr<Endpoint> inner)
    : AsyncWrap(env, object, AsyncWrap::PROVIDER_QUICENDPOINT),
      state_(env),
      inner_(std::move(inner)),
      close_signal_(env, [this]() { Close(); }),
      inbound_signal_(env, [this]() { ProcessInbound(); }),
      initial_signal_(env, [this]() { ProcessInitial(); }) {
  Debug(this, "Created");
  MakeWeak();

  object->DefineOwnProperty(
      env->context(),
      env->state_string(),
      state_.GetArrayBuffer(),
      PropertyAttribute::ReadOnly).Check();

  object->DefineOwnProperty(
      env->context(),
      env->stats_string(),
      inner_->ToBigUint64Array(env),
      PropertyAttribute::ReadOnly).Check();

  {
    Endpoint::Lock lock(inner_);
    inner_->AddCloseListener(this);
  }
}

EndpointWrap::~EndpointWrap() {
  CHECK(sessions_.empty());
  if (LIKELY(inner_)) {
    Endpoint::Lock lock(inner_);
    inner_->RemoveCloseListener(this);
    inner_->RemoveInitialPacketListener(this);
  }
  Debug(this, "Destroyed");
}

void EndpointWrap::EndpointClosed(
    Endpoint::CloseListener::Context context,
    int status) {
  Debug(this, "Closed (%d, %d)", static_cast<int>(context), status);
  close_context_ = context;
  close_status_ = status;
  close_signal_.Send();
}


void EndpointWrap::Close() {
  if (close_context_ == Endpoint::CloseListener::Context::CLOSE) {
    OnEndpointDone();
    return;
  }

  BindingState* state = BindingState::Get(env());
  if (state == nullptr || !env()->can_call_into_js())
    return;

  HandleScope scope(env()->isolate());
  Local<Value> error;
  if (!GetExceptionForContext(env(), close_context_).ToLocal(&error))
    error = Undefined(env()->isolate());

  OnError(error);
}

void EndpointWrap::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("endpoint", inner_);
  tracker->TrackField("sessions", sessions_);
}

void EndpointWrap::AddSession(
    const CID& cid,
    const BaseObjectPtr<Session>& session) {
  Debug(this, "Adding session for CID %s\n", cid);
  sessions_[cid] = session;
  {
    Endpoint::Lock lock(inner_);
    inner_->AssociateCID(cid, this);
    inner_->IncrementSocketAddressCounter(session->remote_address());
    inner_->IncrementStat(
        session->is_server()
            ? &EndpointStats::server_sessions
            : &EndpointStats::client_sessions);
    ClearWeak();
  }
  if (session->is_server())
    OnNewSession(session);
}

void EndpointWrap::AssociateCID(const CID& cid, const CID& scid) {
  if (LIKELY(cid && scid)) {
    Debug(this, "Associating cid %s with %s", cid, scid);
    dcid_to_scid_[cid] = scid;
    Endpoint::Lock lock(inner_);
    inner_->AssociateCID(cid, this);
  }
}

void EndpointWrap::AssociateStatelessResetToken(
    const StatelessResetToken& token,
    const BaseObjectPtr<Session>& session) {
  Debug(this, "Associating stateless reset token %s", token);
  token_map_[token] = session;
  Endpoint::Lock lock(inner_);
  inner_->AssociateStatelessResetToken(token, this);
}

void EndpointWrap::DisassociateCID(const CID& cid) {
  if (LIKELY(cid)) {
    Debug(this, "Removing association for cid %s", cid);
    dcid_to_scid_.erase(cid);
    Endpoint::Lock lock(inner_);
    inner_->DisassociateCID(cid);
  }
}

void EndpointWrap::DisassociateStatelessResetToken(
    const StatelessResetToken& token) {
  Debug(this, "Removing stateless reset token %s", token);
  Endpoint::Lock lock(inner_);
  inner_->DisassociateStatelessResetToken(token);
}

BaseObjectPtr<Session> EndpointWrap::FindSession(const CID& cid) {
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

uint32_t EndpointWrap::GetFlowLabel(
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const CID& cid) {
  return inner_->GetFlowLabel(local_address, remote_address, cid);
}

void EndpointWrap::ImmediateConnectionClose(
    const quic_version version,
    const CID& scid,
    const CID& dcid,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const error_code reason) {
  Debug(this, "Sending stateless connection close to %s", scid);
  inner_->ImmediateConnectionClose(
      version,
      scid,
      dcid,
      local_address,
      remote_address,
      reason);
}

BaseObjectPtr<Session> EndpointWrap::CreateClient(
    const std::shared_ptr<SocketAddress>& address,
    const Session::Config& config,
    const std::shared_ptr<Session::Options>& options,
    const BaseObjectPtr<crypto::SecureContext>& context,
    const crypto::ArrayBufferOrViewContents<uint8_t>& session_ticket,
    const crypto::ArrayBufferOrViewContents<uint8_t>& remote_transport_params) {
  {
    Endpoint::Lock lock(inner_);
    inner_->StartReceiving();
  }

  return Session::CreateClient(
      this,
      this->local_address(),
      address,
      config,
      options,
      context,
      session_ticket,
      remote_transport_params);
}

void EndpointWrap::Listen(
    const std::shared_ptr<Session::Options>& options,
    const BaseObjectPtr<crypto::SecureContext>& context) {
  if (state_->listening == 1) return;
  CHECK(context);
  Debug(this, "Listening");
  server_options_ = options;
  server_context_ = context;
  state_->listening = 1;
  Endpoint::Lock lock(inner_);
  inner_->AddInitialPacketListener(this);
  // While listening, this shouldn't be weak
  this->ClearWeak();
}

void EndpointWrap::OnEndpointDone() {
  MakeWeak();
  state_->listening = 0;

  Debug(this, "Calling endpoint_done callback");

  BindingState* state = BindingState::Get(env());
  if (UNLIKELY(state == nullptr || !env()->can_call_into_js()))
    return;

  HandleScope scope(env()->isolate());
  v8::Context::Scope context_scope(env()->context());
  BaseObjectPtr<EndpointWrap> ptr(this);
  MakeCallback(state->endpoint_done_callback(), 0, nullptr);
}

void EndpointWrap::OnError(Local<Value> error) {
  MakeWeak();
  state_->listening = 0;

  Debug(this, "Calling on_error callback");

  BindingState* state = BindingState::Get(env());
  if (UNLIKELY(state == nullptr || !env()->can_call_into_js()))
    return;

  if (UNLIKELY(error.IsEmpty()))
    error = Undefined(env()->isolate());

  v8::Context::Scope context_scope(env()->context());
  BaseObjectPtr<EndpointWrap> ptr(this);
  MakeCallback(state->endpoint_error_callback(), 1, &error);
}

void EndpointWrap::OnNewSession(const BaseObjectPtr<Session>& session) {
  BindingState* state = BindingState::Get(env());

  Debug(this, "Calling session_new callback");

  Local<Value> arg = session->object();
  v8::Context::Scope context_scope(env()->context());
  BaseObjectPtr<EndpointWrap> ptr(this);
  MakeCallback(state->session_new_callback(), 1, &arg);
}

void EndpointWrap::OnSendDone(int status) {
  DecrementPendingCallbacks();
  if (is_done_waiting_for_callbacks())
    OnEndpointDone();
}

bool EndpointWrap::Accept(
    const Session::Config& config,
    std::shared_ptr<v8::BackingStore> store,
    size_t nread,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const CID& dcid,
    const CID& scid,
    const CID& ocid) {

  Debug(this, "Adding new initial packet to inbound queue");
  {
    Mutex::ScopedLock lock(inbound_mutex_);
    initial_.emplace_back(InitialPacket {
      config,
      std::move(store),
      nread,
      local_address,
      remote_address,
      dcid,
      scid,
      ocid
    });
  }
  initial_signal_.Send();
  return true;
}

void EndpointWrap::ProcessInitial() {
  InitialPacket::Queue queue;
  {
    Mutex::ScopedLock lock(inbound_mutex_);
    initial_.swap(queue);
  }

  Debug(this, "Processing queued initial packets");

  while (!queue.empty()) {
    InitialPacket packet = queue.front();
    queue.pop_front();

    if (server_options_->qlog)
      packet.config.EnableQLog(packet.dcid);

    BaseObjectPtr<Session> session =
        Session::CreateServer(
            this,
            packet.local_address,
            packet.remote_address,
            packet.scid,
            packet.dcid,
            packet.ocid,
            packet.config,
            server_options_,
            server_context_);

    if (UNLIKELY(!session))
      return ProcessInitialFailure();

    Debug(this, "New server session created");

    session->Receive(
        packet.nread,
        std::move(packet.store),
        packet.local_address,
        packet.remote_address);
  }
}

void EndpointWrap::ProcessInitialFailure() {
  Debug(this, "Failure processing initial packet");
  OnError(ERR_QUIC_ENDPOINT_INITIAL_PACKET_FAILURE(env()->isolate()));
}

bool EndpointWrap::Receive(
    const CID& dcid,
    const CID& scid,
    std::shared_ptr<v8::BackingStore> store,
    size_t nread,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const Endpoint::PacketListener::Flags flags) {

  Debug(this, "Receiving and queuing a inbound packet");
  {
    Mutex::ScopedLock lock(inbound_mutex_);
    inbound_.emplace_back(InboundPacket{
      dcid,
      scid,
      std::move(store),
      nread,
      local_address,
      remote_address,
      flags
    });
  }
  inbound_signal_.Send();
  return true;
}

void EndpointWrap::ProcessInbound() {
  InboundPacket::Queue queue;
  {
    Mutex::ScopedLock lock(inbound_mutex_);
    inbound_.swap(queue);
  }

  Debug(this, "Processing queued inbound packets");

  while (!queue.empty()) {
    InboundPacket packet = queue.front();
    queue.pop_front();

    inner_->IncrementStat(&EndpointStats::bytes_received, packet.nread);
    BaseObjectPtr<Session> session = FindSession(packet.dcid);
    if (session && !session->is_destroyed()) {
      session->Receive(
        packet.nread,
        std::move(packet.store),
        packet.local_address,
        packet.remote_address);
    }
  }
}

void EndpointWrap::RemoveSession(
    const CID& cid,
    const std::shared_ptr<SocketAddress>& addr) {
  Debug(this, "Removing session %s", cid);
  {
    Endpoint::Lock lock(inner_);
    inner_->DisassociateCID(cid);
    inner_->DecrementSocketAddressCounter(addr);
  }
  sessions_.erase(cid);
  if (!state_->listening && sessions_.empty())
    MakeWeak();
}

void EndpointWrap::SendPacket(
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    std::unique_ptr<Packet> packet,
    const BaseObjectPtr<Session>& session) {
  if (UNLIKELY(packet->length() == 0))
    return;

  // Make certain we're in a handle scope.
  HandleScope scope(env()->isolate());

  Debug(this, "Sending %" PRIu64 " bytes to %s from %s (label: %s)",
        packet->length(),
        remote_address->ToString(),
        local_address->ToString(),
        packet->diagnostic_label());

  BaseObjectPtr<Endpoint::SendWrap> wrap =
      Endpoint::SendWrap::Create(
          env(),
          remote_address,
          std::move(packet),
          BaseObjectPtr<EndpointWrap>(this));
  if (UNLIKELY(!wrap))
    return OnError(ERR_QUIC_ENDPOINT_SEND_FAILURE(env()->isolate()));

  IncrementPendingCallbacks();
  inner_->SendPacket(std::move(wrap));
}

bool EndpointWrap::SendRetry(
    const quic_version version,
    const CID& dcid,
    const CID& scid,
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address) {
  Debug(this, "Sending retry for %s from %s to %s\n",
        dcid, local_address.get(), remote_address.get());
  return inner_->SendRetry(version, dcid, scid, local_address, remote_address);
}

void EndpointWrap::WaitForPendingCallbacks() {
  // If this EndpointWrap is listening for incoming initial packets,
  // unregister the listener now so that the Endpoint does not try
  // to forward on new initial packets while we're waiting for the
  // existing writes to clear.
  Debug(this, "Waiting for pending callbacks");
  inner_->RemoveInitialPacketListener(this);
  state_->listening = 0;

  if (!is_done_waiting_for_callbacks()) {
    OnEndpointDone();
    return;
  }
  state_->waiting_for_callbacks = 1;
}

std::unique_ptr<worker::TransferData> EndpointWrap::CloneForMessaging() const {
  return std::make_unique<TransferData>(inner_);
}

BaseObjectPtr<BaseObject> EndpointWrap::TransferData::Deserialize(
    Environment* env,
    v8::Local<v8::Context> context,
    std::unique_ptr<worker::TransferData> self) {
  return EndpointWrap::Create(env, std::move(inner_));
}

void EndpointWrap::TransferData::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("inner", inner_);
}

}  // namespace quic
}  // namespace node
