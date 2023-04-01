#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include "packet.h"
#include <base_object-inl.h>
#include <crypto/crypto_util.h>
#include <env-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node_sockaddr-inl.h>
#include <req_wrap-inl.h>
#include <uv.h>
#include <v8.h>
#include <string>
#include "bindingdata.h"
#include "cid.h"
#include "tokens.h"

namespace node {

using v8::FunctionTemplate;
using v8::Local;
using v8::Object;

namespace quic {

namespace {
static constexpr size_t kRandlen = NGTCP2_MIN_STATELESS_RESET_RANDLEN * 5;
static constexpr size_t kMinStatelessResetLen = 41;
static constexpr size_t kMaxFreeList = 100;
}  // namespace

struct Packet::Data final : public MemoryRetainer {
  MaybeStackBuffer<uint8_t, kDefaultMaxPacketLength> data_;

  // The diagnostic_label_ is used only as a debugging tool when
  // logging debug information about the packet. It identifies
  // the purpose of the packet.
  const std::string diagnostic_label_;

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize("data", data_.length());
  }
  SET_MEMORY_INFO_NAME(Data)
  SET_SELF_SIZE(Data)

  Data(size_t length, std::string_view diagnostic_label)
      : diagnostic_label_(diagnostic_label) {
    data_.AllocateSufficientStorage(length);
  }

  size_t length() const { return data_.length(); }
  operator uv_buf_t() {
    return uv_buf_init(reinterpret_cast<char*>(data_.out()), data_.length());
  }
  operator ngtcp2_vec() { return ngtcp2_vec{data_.out(), data_.length()}; }

  std::string ToString() const {
    return diagnostic_label_ + ", " + std::to_string(length());
  }
};

const SocketAddress& Packet::destination() const {
  return destination_;
}

bool Packet::is_sending() const {
  return !!handle_;
}

size_t Packet::length() const {
  return data_ ? data_->length() : 0;
}

Packet::operator uv_buf_t() const {
  return !data_ ? uv_buf_init(nullptr, 0) : *data_;
}

Packet::operator ngtcp2_vec() const {
  return !data_ ? ngtcp2_vec{nullptr, 0} : *data_;
}

void Packet::Truncate(size_t len) {
  DCHECK(data_);
  DCHECK_LE(len, data_->length());
  data_->data_.SetLength(len);
}

Local<FunctionTemplate> Packet::GetConstructorTemplate(Environment* env) {
  auto& state = BindingData::Get(env);
  Local<FunctionTemplate> tmpl = state.packet_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = NewFunctionTemplate(env->isolate(), IllegalConstructor);
    tmpl->Inherit(ReqWrap<uv_udp_send_t>::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Packet::kInternalFieldCount);
    tmpl->SetClassName(state.packetwrap_string());
    state.set_packet_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<Packet> Packet::Create(Environment* env,
                                     Listener* listener,
                                     const SocketAddress& destination,
                                     size_t length,
                                     const char* diagnostic_label) {
  auto& binding = BindingData::Get(env);
  if (binding.packet_freelist.empty()) {
    Local<Object> obj;
    if (UNLIKELY(!GetConstructorTemplate(env)
                      ->InstanceTemplate()
                      ->NewInstance(env->context())
                      .ToLocal(&obj))) {
      return BaseObjectPtr<Packet>();
    }

    return MakeBaseObject<Packet>(
        env, listener, obj, destination, length, diagnostic_label);
  }

  return FromFreeList(env,
                      std::make_shared<Data>(length, diagnostic_label),
                      listener,
                      destination);
}

BaseObjectPtr<Packet> Packet::Clone() const {
  auto& binding = BindingData::Get(env());
  if (binding.packet_freelist.empty()) {
    Local<Object> obj;
    if (UNLIKELY(!GetConstructorTemplate(env())
                      ->InstanceTemplate()
                      ->NewInstance(env()->context())
                      .ToLocal(&obj))) {
      return BaseObjectPtr<Packet>();
    }

    return MakeBaseObject<Packet>(env(), listener_, obj, destination_, data_);
  }

  return FromFreeList(env(), data_, listener_, destination_);
}

BaseObjectPtr<Packet> Packet::FromFreeList(Environment* env,
                                           std::shared_ptr<Data> data,
                                           Listener* listener,
                                           const SocketAddress& destination) {
  auto& binding = BindingData::Get(env);
  auto obj = binding.packet_freelist.back();
  binding.packet_freelist.pop_back();
  DCHECK_EQ(env, obj->env());
  auto packet = static_cast<Packet*>(obj.get());
  packet->data_ = std::move(data);
  packet->destination_ = destination;
  packet->listener_ = listener;
  return BaseObjectPtr<Packet>(packet);
}

Packet::Packet(Environment* env,
               Listener* listener,
               Local<Object> object,
               const SocketAddress& destination,
               std::shared_ptr<Data> data)
    : ReqWrap<uv_udp_send_t>(env, object, AsyncWrap::PROVIDER_QUIC_PACKET),
      listener_(listener),
      destination_(destination),
      data_(std::move(data)) {}

Packet::Packet(Environment* env,
               Listener* listener,
               Local<Object> object,
               const SocketAddress& destination,
               size_t length,
               const char* diagnostic_label)
    : Packet(env,
             listener,
             object,
             destination,
             std::make_shared<Data>(length, diagnostic_label)) {}

int Packet::Send(uv_udp_t* handle, BaseObjectPtr<BaseObject> ref) {
  if (is_sending()) return UV_EALREADY;
  if (data_ == nullptr) return UV_EINVAL;
  DCHECK(!is_sending());
  handle_ = std::move(ref);
  uv_buf_t buf = *this;
  return Dispatch(
      uv_udp_send,
      handle,
      &buf,
      1,
      destination().data(),
      uv_udp_send_cb{[](uv_udp_send_t* req, int status) {
        auto ptr = static_cast<Packet*>(ReqWrap<uv_udp_send_t>::from_req(req));
        ptr->Done(status);
        // Do not try accessing ptr after this. We don't know if it
        // was freelisted or destroyed. Either way, done means done.
      }});
}

void Packet::Done(int status) {
  DCHECK_NOT_NULL(listener_);
  listener_->PacketDone(status);
  handle_.reset();
  data_.reset();
  listener_ = nullptr;
  Reset();

  // As a performance optimization, we add this packet to a freelist
  // rather than deleting it but only if the freelist isn't too
  // big, we don't want to accumulate these things forever.
  auto& binding = BindingData::Get(env());
  if (binding.packet_freelist.size() < kMaxFreeList) {
    binding.packet_freelist.emplace_back(this);
  } else {
    delete this;
  }
}

std::string Packet::ToString() const {
  if (!data_) return "Packet (<empty>)";
  return "Packet (" + data_->ToString() + ")";
}

void Packet::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("destination", destination_);
  tracker->TrackField("data", data_);
  tracker->TrackField("handle", handle_);
}

BaseObjectPtr<Packet> Packet::CreateRetryPacket(
    Environment* env,
    Listener* listener,
    const PathDescriptor& path_descriptor,
    const TokenSecret& token_secret) {
  auto& random = CID::Factory::random();
  CID cid = random.Generate();
  RetryToken token(path_descriptor.version,
                   path_descriptor.remote_address,
                   cid,
                   path_descriptor.dcid,
                   token_secret);
  if (!token) return BaseObjectPtr<Packet>();

  const ngtcp2_vec& vec = token;

  size_t pktlen =
      vec.len + (2 * NGTCP2_MAX_CIDLEN) + path_descriptor.scid.length() + 8;

  auto packet =
      Create(env, listener, path_descriptor.remote_address, pktlen, "retry");
  if (!packet) return BaseObjectPtr<Packet>();

  ngtcp2_vec dest = *packet;

  ssize_t nwrite = ngtcp2_crypto_write_retry(dest.base,
                                             pktlen,
                                             path_descriptor.version,
                                             path_descriptor.scid,
                                             cid,
                                             path_descriptor.dcid,
                                             vec.base,
                                             vec.len);
  if (nwrite <= 0) return BaseObjectPtr<Packet>();
  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

BaseObjectPtr<Packet> Packet::CreateConnectionClosePacket(
    Environment* env,
    Listener* listener,
    const SocketAddress& destination,
    ngtcp2_conn* conn,
    const QuicError& error) {
  auto packet = Packet::Create(
      env, listener, destination, kDefaultMaxPacketLength, "connection close");
  ngtcp2_vec vec = *packet;

  ssize_t nwrite = ngtcp2_conn_write_connection_close(
      conn, nullptr, nullptr, vec.base, vec.len, error, uv_hrtime());
  if (nwrite < 0) return BaseObjectPtr<Packet>();
  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

BaseObjectPtr<Packet> Packet::CreateImmediateConnectionClosePacket(
    Environment* env,
    Listener* listener,
    const SocketAddress& destination,
    const PathDescriptor& path_descriptor,
    const QuicError& reason) {
  auto packet = Packet::Create(env,
                               listener,
                               path_descriptor.remote_address,
                               kDefaultMaxPacketLength,
                               "immediate connection close (endpoint)");
  ngtcp2_vec vec = *packet;
  ssize_t nwrite = ngtcp2_crypto_write_connection_close(
      vec.base,
      vec.len,
      path_descriptor.version,
      path_descriptor.dcid,
      path_descriptor.scid,
      reason.code(),
      // We do not bother sending a reason string here, even if
      // there is one in the QuicError
      nullptr,
      0);
  if (nwrite <= 0) return BaseObjectPtr<Packet>();
  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

BaseObjectPtr<Packet> Packet::CreateStatelessResetPacket(
    Environment* env,
    Listener* listener,
    const PathDescriptor& path_descriptor,
    const TokenSecret& token_secret,
    size_t source_len) {
  // Per the QUIC spec, a stateless reset token must be strictly smaller than
  // the packet that triggered it. This is one of the mechanisms to prevent
  // infinite looping exchange of stateless tokens with the peer. An endpoint
  // should never send a stateless reset token smaller than 41 bytes per the
  // QUIC spec. The reason is that packets less than 41 bytes may allow an
  // observer to reliably determine that it's a stateless reset.
  size_t pktlen = source_len - 1;
  if (pktlen < kMinStatelessResetLen) return BaseObjectPtr<Packet>();

  StatelessResetToken token(token_secret, path_descriptor.dcid);
  uint8_t random[kRandlen];
  CHECK(crypto::CSPRNG(random, kRandlen).is_ok());

  auto packet = Packet::Create(env,
                               listener,
                               path_descriptor.remote_address,
                               kDefaultMaxPacketLength,
                               "stateless reset");
  ngtcp2_vec vec = *packet;

  ssize_t nwrite = ngtcp2_pkt_write_stateless_reset(
      vec.base, pktlen, token, random, kRandlen);
  if (nwrite <= static_cast<ssize_t>(kMinStatelessResetLen)) {
    return BaseObjectPtr<Packet>();
  }

  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

BaseObjectPtr<Packet> Packet::CreateVersionNegotiationPacket(
    Environment* env,
    Listener* listener,
    const PathDescriptor& path_descriptor) {
  const auto generateReservedVersion = [&] {
    socklen_t addrlen = path_descriptor.remote_address.length();
    uint32_t h = 0x811C9DC5u;
    uint32_t ver = htonl(path_descriptor.version);
    const uint8_t* p = path_descriptor.remote_address.raw();
    const uint8_t* ep = p + addrlen;
    for (; p != ep; ++p) {
      h ^= *p;
      h *= 0x01000193u;
    }
    p = reinterpret_cast<const uint8_t*>(&ver);
    ep = p + sizeof(path_descriptor.version);
    for (; p != ep; ++p) {
      h ^= *p;
      h *= 0x01000193u;
    }
    h &= 0xf0f0f0f0u;
    h |= NGTCP2_RESERVED_VERSION_MASK;
    return h;
  };

  uint32_t sv[3] = {
      generateReservedVersion(), NGTCP2_PROTO_VER_MIN, NGTCP2_PROTO_VER_MAX};

  size_t pktlen = path_descriptor.dcid.length() +
                  path_descriptor.scid.length() + (sizeof(sv)) + 7;

  auto packet = Packet::Create(env,
                               listener,
                               path_descriptor.remote_address,
                               kDefaultMaxPacketLength,
                               "version negotiation");
  ngtcp2_vec vec = *packet;

  ssize_t nwrite =
      ngtcp2_pkt_write_version_negotiation(vec.base,
                                           pktlen,
                                           0,
                                           path_descriptor.dcid,
                                           path_descriptor.dcid.length(),
                                           path_descriptor.scid,
                                           path_descriptor.scid.length(),
                                           sv,
                                           arraysize(sv));
  if (nwrite <= 0) return BaseObjectPtr<Packet>();
  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
