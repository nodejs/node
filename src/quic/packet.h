#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <ngtcp2/ngtcp2.h>
#include <node_sockaddr.h>
#include <uv.h>
#include <string>
#include "arena.h"
#include "cid.h"
#include "data.h"
#include "defs.h"
#include "tokens.h"

namespace node::quic {

class Endpoint;

struct PathDescriptor final {
  uint32_t version;
  const CID& dcid;
  const CID& scid;
  const SocketAddress& local_address;
  const SocketAddress& remote_address;
  std::string ToString() const;
};

// A Packet encapsulates serialized outbound QUIC data.
//
// Packets are allocated from an ArenaPool owned by the Endpoint, which
// allocates fixed-size slots from contiguous memory blocks. This avoids
// heap fragmentation and the overhead of per-packet V8 object allocation.
//
// Each Packet contains:
// - A uv_udp_send_t for the libuv send operation
// - A destination socket address
// - A data buffer (trailing memory in the arena slot)
//
// Packets are always encrypted; their content is opaque. We leave it
// entirely up to ngtcp2 how to encode QUIC frames into the packet.
//
// Member layout is ordered so that fields touched on the hot path
// (data_, capacity_, length_, listener_) share the first cache line.
// The uv_udp_send_t (320 bytes, only touched by libuv) is placed last.
class Packet final {
 public:
  using Ptr = ArenaPool<Packet>::Ptr;

  class Listener {
   public:
    virtual void PacketDone(int status) = 0;
  };

  // Constructor takes the trailing data buffer and capacity from the
  // arena slot, plus the listener and destination. The data/capacity
  // are injected by ArenaPool::AcquireExtra().
  Packet(uint8_t* data,
         size_t capacity,
         Listener* listener,
         const SocketAddress& destination);

  DISALLOW_COPY_AND_MOVE(Packet)

  // --- Inline accessors (hot path) ---

  uint8_t* data() { return data_; }
  const uint8_t* data() const { return data_; }
  size_t length() const { return length_; }
  size_t capacity() const { return capacity_; }
  const SocketAddress& destination() const { return destination_; }
  Listener* listener() const { return listener_; }
  uv_udp_send_t* req() { return &req_; }

  operator uv_buf_t() const {
    return uv_buf_init(reinterpret_cast<char*>(data_), length_);
  }
  operator ngtcp2_vec() const { return ngtcp2_vec{data_, length_}; }

  // Modify the logical size of the packet after ngtcp2 has written
  // to it. len must be <= capacity().
  void Truncate(size_t len) {
    DCHECK_LE(len, capacity_);
    length_ = len;
  }

  // Recover Packet* from a uv_udp_send_t* in the libuv send callback.
  static Packet* FromReq(uv_udp_send_t* req) {
    return ContainerOf(&Packet::req_, req);
  }

  // --- Static factory methods ---
  // These create fully-formed packets for specific QUIC operations.
  // They acquire from the endpoint's packet pool and return Ptr.
  // An empty Ptr indicates failure.

  [[nodiscard]] static Ptr CreateRetryPacket(
      Endpoint& endpoint,
      const PathDescriptor& path_descriptor,
      const TokenSecret& token_secret);

  [[nodiscard]] static Ptr CreateConnectionClosePacket(
      Endpoint& endpoint,
      const SocketAddress& destination,
      ngtcp2_conn* conn,
      const QuicError& error);

  [[nodiscard]] static Ptr CreateImmediateConnectionClosePacket(
      Endpoint& endpoint,
      const PathDescriptor& path_descriptor,
      const QuicError& reason);

  [[nodiscard]] static Ptr CreateStatelessResetPacket(
      Endpoint& endpoint,
      const PathDescriptor& path_descriptor,
      const TokenSecret& token_secret,
      size_t source_len);

  [[nodiscard]] static Ptr CreateVersionNegotiationPacket(
      Endpoint& endpoint, const PathDescriptor& path_descriptor);

  // --- Diagnostic label: zero cost in release builds ---
#ifdef DEBUG
  void set_diagnostic_label(const char* label) {
    diagnostic_label_ = label;
  }
  const char* diagnostic_label() const {
    return diagnostic_label_;
  }
#else
  void set_diagnostic_label(const char*) {}
#endif
  std::string ToString() const;

 private:
  // Hot fields first — all on cache line 0 during the fill loop.
  uint8_t* data_;
  size_t capacity_;
  size_t length_;
  Listener* listener_;

  // Touched at send time.
  SocketAddress destination_;

  // Only touched by libuv during uv_udp_send and in the send callback.
  uv_udp_send_t req_;

#ifdef DEBUG
  const char* diagnostic_label_ = nullptr;
#endif
};

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
