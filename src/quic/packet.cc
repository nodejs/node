#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <crypto/crypto_util.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node_sockaddr-inl.h>
#include <uv.h>
#include <cstring>
#include <string>
#include "cid.h"
#include "defs.h"
#include "endpoint.h"
#include "ncrypto.h"
#include "packet.h"
#include "tokens.h"

namespace node::quic {

namespace {
static constexpr size_t kRandlen = NGTCP2_MIN_STATELESS_RESET_RANDLEN * 5;
static constexpr size_t kMinStatelessResetLen = 41;
}  // namespace

std::string PathDescriptor::ToString() const {
  DebugIndentScope indent;
  auto prefix = indent.Prefix();
  std::string res = "{";
  res += prefix + "version: " + std::to_string(version);
  res += prefix + "dcid: " + dcid.ToString();
  res += prefix + "scid: " + scid.ToString();
  res += prefix + "local address: " + local_address.ToString();
  res += prefix + "remote address: " + remote_address.ToString();
  res += indent.Close();
  return res;
}

// ============================================================================
// Packet

Packet::Packet(uint8_t* data,
               size_t capacity,
               Listener* listener,
               const SocketAddress& destination)
    : data_(data),
      capacity_(capacity),
      length_(capacity),
      listener_(listener),
      destination_(destination),
      req_{} {}

std::string Packet::ToString() const {
  std::string res = "Packet(";
#ifdef DEBUG
  if (diagnostic_label_) {
    res += diagnostic_label_;
    res += ", ";
  }
#endif
  res += std::to_string(length_);
  res += ")";
  return res;
}

// ============================================================================
// Static factory methods

Packet::Ptr Packet::CreateRetryPacket(Endpoint& endpoint,
                                      const PathDescriptor& path_descriptor,
                                      const TokenSecret& token_secret) {
  auto& random = CID::Factory::random();
  CID cid = random.Generate();
  RetryToken token(path_descriptor.version,
                   path_descriptor.remote_address,
                   cid,
                   path_descriptor.dcid,
                   token_secret);
  if (!token) return Ptr();

  const ngtcp2_vec& vec = token;

  size_t pktlen =
      vec.len + (2 * NGTCP2_MAX_CIDLEN) + path_descriptor.scid.length() + 8;

  auto packet =
      endpoint.CreatePacket(path_descriptor.remote_address, pktlen, "retry");
  if (!packet) return packet;

  ngtcp2_vec dest = *packet;

  ssize_t nwrite = ngtcp2_crypto_write_retry(dest.base,
                                             pktlen,
                                             path_descriptor.version,
                                             path_descriptor.scid,
                                             cid,
                                             path_descriptor.dcid,
                                             vec.base,
                                             vec.len);
  if (nwrite <= 0) return Ptr();
  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

Packet::Ptr Packet::CreateConnectionClosePacket(
    Endpoint& endpoint,
    const SocketAddress& destination,
    ngtcp2_conn* conn,
    const QuicError& error) {
  auto packet = endpoint.CreatePacket(
      destination, kDefaultMaxPacketLength, "connection close");
  if (!packet) return packet;
  ngtcp2_vec vec = *packet;

  ssize_t nwrite = ngtcp2_conn_write_connection_close(
      conn, nullptr, nullptr, vec.base, vec.len, error, uv_hrtime());
  if (nwrite < 0) return Ptr();
  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

Packet::Ptr Packet::CreateImmediateConnectionClosePacket(
    Endpoint& endpoint,
    const PathDescriptor& path_descriptor,
    const QuicError& reason) {
  auto packet = endpoint.CreatePacket(path_descriptor.remote_address,
                                      kDefaultMaxPacketLength,
                                      "immediate connection close (endpoint)");
  if (!packet) return packet;
  ngtcp2_vec vec = *packet;
  ssize_t nwrite = ngtcp2_crypto_write_connection_close(vec.base,
                                                        vec.len,
                                                        path_descriptor.version,
                                                        path_descriptor.dcid,
                                                        path_descriptor.scid,
                                                        reason.code(),
                                                        nullptr,
                                                        0);
  if (nwrite <= 0) return Ptr();
  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

Packet::Ptr Packet::CreateStatelessResetPacket(
    Endpoint& endpoint,
    const PathDescriptor& path_descriptor,
    const TokenSecret& token_secret,
    size_t source_len) {
  // Per the QUIC spec, a stateless reset token must be strictly smaller than
  // the packet that triggered it.
  size_t pktlen = source_len - 1;
  if (pktlen < kMinStatelessResetLen) return Ptr();

  StatelessResetToken token(token_secret, path_descriptor.dcid);
  uint8_t random[kRandlen];
  CHECK(ncrypto::CSPRNG(random, kRandlen));

  auto packet = endpoint.CreatePacket(path_descriptor.remote_address,
                                      kDefaultMaxPacketLength,
                                      "stateless reset");
  if (!packet) return packet;
  ngtcp2_vec vec = *packet;

  ssize_t nwrite = ngtcp2_pkt_write_stateless_reset(
      vec.base, pktlen, token, random, kRandlen);
  if (nwrite <= static_cast<ssize_t>(kMinStatelessResetLen)) return Ptr();

  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

Packet::Ptr Packet::CreateVersionNegotiationPacket(
    Endpoint& endpoint, const PathDescriptor& path_descriptor) {
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

  auto packet = endpoint.CreatePacket(path_descriptor.remote_address,
                                      kDefaultMaxPacketLength,
                                      "version negotiation");
  if (!packet) return packet;
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
  if (nwrite <= 0) return Ptr();
  packet->Truncate(static_cast<size_t>(nwrite));
  return packet;
}

}  // namespace node::quic

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
