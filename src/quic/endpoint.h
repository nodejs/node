#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <aliased_struct.h>
#include <async_wrap.h>
#include <env.h>
#include <node_sockaddr.h>
#include <timer_wrap.h>
#include <uv.h>
#include <v8.h>
#include <algorithm>
#include <optional>
#include "arena.h"
#include "bindingdata.h"
#include "packet.h"
#include "session.h"
#include "session_manager.h"
#include "sessionticket.h"
#include "tokens.h"

namespace node::quic {

// An Endpoint encapsulates the UDP local port binding and is responsible for
// sending and receiving QUIC packets. A single endpoint can act as both a QUIC
// client and server simultaneously.
class Endpoint final : public AsyncWrap, public Packet::Listener {
 public:
  // The socket address LRU is used for tracking validated remote addresses.
  static constexpr uint64_t DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE = 1024;

  // Default rate limits for stateless responses. These are global token
  // bucket limits that cap the total rate of each response type regardless
  // of source address. This prevents spoofed-source floods from bypassing
  // per-host limits (which are keyed by source IP and trivially defeated
  // by rotating spoofed addresses). The rate is in responses per second
  // and the burst is the maximum tokens the bucket can hold.
  static constexpr double DEFAULT_RETRY_RATE = 100;
  static constexpr double DEFAULT_RETRY_BURST = 200;
  static constexpr double DEFAULT_STATELESS_RESET_RATE = 100;
  static constexpr double DEFAULT_STATELESS_RESET_BURST = 200;
  static constexpr double DEFAULT_VERSION_NEGOTIATION_RATE = 100;
  static constexpr double DEFAULT_VERSION_NEGOTIATION_BURST = 200;
  static constexpr double DEFAULT_IMMEDIATE_CLOSE_RATE = 100;
  static constexpr double DEFAULT_IMMEDIATE_CLOSE_BURST = 200;

  // Endpoint configuration options
  struct Options final : public MemoryRetainer {
    // The local socket address to which the UDP port will be bound. The port
    // may be 0 to have Node.js select an available port. IPv6 or IPv4 addresses
    // may be used. When using IPv6, dual mode will be supported by default.
    std::shared_ptr<SocketAddress> local_address;

    // Retry tokens issued by the Endpoint are time-limited. By default, retry
    // tokens expire after DEFAULT_RETRYTOKEN_EXPIRATION *seconds*. This is an
    // arbitrary choice that is not mandated by the QUIC specification; so we
    // can choose any value that makes sense here. Retry tokens are sent to the
    // client, which echoes them back to the server in a subsequent set of
    // packets, which means the expiration must be set high enough to allow a
    // reasonable round-trip time for the session TLS handshake to complete.
    uint64_t retry_token_expiration =
        RetryToken::QUIC_DEFAULT_RETRYTOKEN_EXPIRATION / NGTCP2_SECONDS;

    // Tokens issued using NEW_TOKEN are time-limited. By default, tokens expire
    // after QUIC_DEFAULT_REGULARTOKEN_EXPIRATION *seconds*.
    uint64_t token_expiration =
        RegularToken::QUIC_DEFAULT_REGULARTOKEN_EXPIRATION / NGTCP2_SECONDS;

    // For tracking the path verification status of remote hosts, we maintain
    // an LRU cache of the most recently seen hosts.
    uint64_t address_lru_size = DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE;

    // Global token bucket rate limits for stateless responses. These cap
    // the total rate of each response type regardless of source address,
    // preventing spoofed-source floods. Rate is in responses per second,
    // burst is the maximum number of responses that can be sent in a burst.
    double retry_rate = DEFAULT_RETRY_RATE;
    double retry_burst = DEFAULT_RETRY_BURST;
    double stateless_reset_rate = DEFAULT_STATELESS_RESET_RATE;
    double stateless_reset_burst = DEFAULT_STATELESS_RESET_BURST;
    double version_negotiation_rate = DEFAULT_VERSION_NEGOTIATION_RATE;
    double version_negotiation_burst = DEFAULT_VERSION_NEGOTIATION_BURST;
    double immediate_close_rate = DEFAULT_IMMEDIATE_CLOSE_RATE;
    double immediate_close_burst = DEFAULT_IMMEDIATE_CLOSE_BURST;

    // The validate_address parameter instructs the Endpoint to perform explicit
    // address validation using retry tokens. This is strongly recommended and
    // should only be disabled in trusted, closed environments as a performance
    // optimization.
    bool validate_address = true;

    // The stateless reset mechanism can be disabled. This should rarely ever be
    // needed, and should only ever be done in trusted, closed environments as a
    // performance optimization.
    bool disable_stateless_reset = false;

#ifdef DEBUG
    // The rx_loss and tx_loss parameters are debugging tools that allow the
    // Endpoint to simulate random packet loss. The value for each parameter is
    // a value between 0.0 and 1.0 indicating a probability of packet loss. Each
    // time a packet is sent or received, the packet loss bit is calculated and
    // if true, the packet is silently dropped. This should only ever be used
    // for testing and debugging. There is never a reason why rx_loss and
    // tx_loss should ever be used in a production system.
    double rx_loss = 0.0;
    double tx_loss = 0.0;
#endif  // DEBUG

    // By default, when the endpoint is created, it will generate a
    // reset_token_secret at random. This is a secret used in generating
    // stateless reset tokens. In order for stateless reset to be effective,
    // however, it is necessary to use a deterministic secret that persists
    // across ngtcp2 endpoints and sessions. This means that the endpoint
    // configuration really should have a reset token secret passed in.
    TokenSecret reset_token_secret;

    // The secret used for generating new regular tokens.
    TokenSecret token_secret;

    // When the local_address specifies an IPv6 local address to bind to, the
    // ipv6_only parameter determines whether dual stack mode (supporting both
    // IPv6 and IPv4) transparently is supported. This sets the UV_UDP_IPV6ONLY
    // flag on the underlying uv_udp_t.
    bool ipv6_only = false;

    // When true, multiple endpoints (across separate processes) can bind to
    // the same address:port and the kernel will load-balance incoming UDP
    // datagrams across them. This sets the UV_UDP_REUSEPORT flag on the
    // underlying uv_udp_t. Supported on Linux 3.9+ and DragonFlyBSD 3.6+.
    bool reuse_port = false;

    uint32_t udp_receive_buffer_size = 0;
    uint32_t udp_send_buffer_size = 0;

    // The UDP TTL configuration is the number of network hops a packet will be
    // forwarded through. The default is 64. The value is in the range 1 to 255.
    // Setting to 0 uses the default.
    uint8_t udp_ttl = 0;

    // When an endpoint becomes idle (not listening and no primary sessions),
    // it will be destroyed after this many seconds. A value of 0 means
    // destroy immediately when idle (default, preserves pre-SessionManager
    // behavior). A positive value keeps the endpoint alive for potential
    // reuse by future connect() or listen() calls.
    static constexpr uint64_t DEFAULT_IDLE_TIMEOUT = 0;
    uint64_t idle_timeout = DEFAULT_IDLE_TIMEOUT;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Endpoint::Config)
    SET_SELF_SIZE(Options)

    static v8::Maybe<Options> From(Environment* env,
                                   v8::Local<v8::Value> value);

    std::string ToString() const;
  };

  JS_CONSTRUCTOR(Endpoint);
  JS_BINDING_INIT_BOILERPLATE();

  Endpoint(Environment* env,
           v8::Local<v8::Object> object,
           const Endpoint::Options& options);

  inline operator Packet::Listener*() {
    return this;
  }

  inline const Options& options() const {
    return options_;
  }

  // While the busy flag is set, the Endpoint will reject all initial packets
  // with a SERVER_BUSY response. This allows us to build a circuit breaker
  // directly into the implementation, explicitly signaling that the server is
  // blocked when activity is too high.
  void MarkAsBusy(bool on = true);

  // Use the endpoint's token secret to generate a new token.
  RegularToken GenerateNewToken(uint32_t version,
                                const SocketAddress& remote_address);

  // Use the endpoint's reset token secret to generate a new stateless reset.
  StatelessResetToken GenerateNewStatelessResetToken(uint8_t* token,
                                                     const CID& cid) const;

  void AddSession(const CID& cid, BaseObjectPtr<Session> session);
  void RemoveSession(const CID& cid, const SocketAddress& remote_address);
  BaseObjectPtr<Session> FindSession(const CID& cid);

  // A single session may be associated with multiple CIDs.
  // AssociateCID registers the mapping both in the Endpoint and the inner
  // Endpoint.
  void AssociateCID(const CID& cid, const CID& scid);
  void DisassociateCID(const CID& cid);

  // Associates a given stateless reset token with the session. This allows
  // stateless reset tokens to be recognized and dispatched to the proper
  // Endpoint and Session for processing.
  void AssociateStatelessResetToken(const StatelessResetToken& token,
                                    Session* session);
  void DisassociateStatelessResetToken(const StatelessResetToken& token);

  void Send(Packet::Ptr packet);

  // Attempt synchronous send via uv_udp_try_send. If the socket is
  // writable, the packet is sent immediately and the Ptr is released.
  // If the socket is not writable (UV_EAGAIN), falls back to the
  // async Send path. Used by the deferred flush callback to avoid
  // the one-tick latency of async uv_udp_send.
  void SendOrTrySend(Packet::Ptr packet);

  // Send a batch of packets using uv_udp_try_send2 (sendmmsg) for
  // synchronous batched delivery. Packets successfully sent are released
  // immediately. On EAGAIN or partial send, remaining packets fall back
  // to async uv_udp_send. The Packet::Ptr array is consumed: all entries
  // will be empty (released or moved) on return.
  void SendBatch(Packet::Ptr* packets, size_t count);

  // Acquire a Packet from the pool. length sets the initial working
  // size (must be <= pool capacity). The slot is always allocated at
  // full capacity to avoid fragmentation.
  Packet::Ptr CreatePacket(const SocketAddress& destination,
                           size_t length = kDefaultMaxPacketLength,
                           const char* diagnostic_label = nullptr);

  // Generates and sends a retry packet. This is terminal for the connection.
  // Retry packets are used to force explicit path validation by issuing a token
  // to the peer that it must thereafter include in all subsequent initial
  // packets. Upon receiving a retry packet, the peer must termination it's
  // initial attempt to establish a connection and start a new attempt.
  //
  // Retry packets will only ever be generated by QUIC servers, and only if the
  // QuicSocket is configured for explicit path validation. There is no way for
  // a client to force a retry packet to be created. However, once a client
  // determines that explicit path validation is enabled, it could attempt to
  // DOS by sending a large number of malicious initial packets to intentionally
  // ellicit retry packets (It can do so by intentionally sending initial
  // packets that ignore the retry token). To help mitigate that risk, we limit
  // the number of retries we send to a given remote endpoint.
  void SendRetry(const PathDescriptor& options);

  // Sends a version negotiation packet. This is terminal for the connection and
  // is sent only when a QUIC packet is received for an unsupported QUIC
  // version. It is possible that a malicious packet triggered this so we need
  // to be careful not to commit too many resources.
  void SendVersionNegotiation(const PathDescriptor& options);

  // Possibly generates and sends a stateless reset packet. This is terminal for
  // the connection. It is possible that a malicious packet triggered this so we
  // need to be careful not to commit too many resources.
  bool SendStatelessReset(const PathDescriptor& options, size_t source_len);

  // Shutdown a connection prematurely, before a Session is created. This should
  // only be called at the start of a session before the crypto keys have been
  // established.
  void SendImmediateConnectionClose(const PathDescriptor& options,
                                    QuicError error);

  // Listen for connections (act as a server).
  void Listen(const Session::Options& options);

  // Create a new client-side Session.
  BaseObjectPtr<Session> Connect(
      const SocketAddress& remote_address,
      const Session::Options& options,
      std::optional<SessionTicket> sessionTicket = std::nullopt);

  // Returns the local address only if the endpoint has been bound. Before
  // the endpoint is bound, or after it is closed, this will abort due to
  // a failed check so it is important to check `is_closed()` before calling.
  SocketAddress local_address() const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Endpoint)
  SET_SELF_SIZE(Endpoint)

  struct Stats;
  struct State;

 private:
  class UDP final : public MemoryRetainer {
   public:
    explicit UDP(Endpoint* endpoint);
    ~UDP() override;

    int Bind(const Endpoint::Options& config);
    int Start();
    void Stop();
    void Close();
    int Send(Packet::Ptr packet);

    // Synchronous send using uv_udp_try_send. Returns the number of
    // bytes sent on success, UV_EAGAIN if the socket is not writable
    // or the send queue is non-empty, or another negative error code.
    // The Ptr is not consumed — the caller manages the lifecycle.
    int TrySend(const Packet::Ptr& packet);

    // Synchronous batched send using uv_udp_try_send2 (sendmmsg).
    // Takes pre-built libuv argument arrays. Returns the number of
    // messages successfully sent (>= 0), or a negative error code.
    int TrySendBatch(uv_buf_t* bufs[],
                     unsigned int nbufs[],
                     struct sockaddr* addrs[],
                     size_t count);

    // Returns the local UDP socket address to which we are bound,
    // or fail with an assert if we are not bound.
    SocketAddress local_address() const;

    bool is_bound() const;
    bool is_closed() const;
    bool is_closed_or_closing() const;
    operator bool() const;

    void Ref();
    void Unref();

    void MemoryInfo(node::MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Endpoint::UDP)
    SET_SELF_SIZE(UDP)

   private:
    class Impl;

    BaseObjectWeakPtr<Impl> impl_;
    struct Flags {
      uint8_t is_bound : 1 = 0;
      uint8_t is_started : 1 = 0;
      uint8_t is_closed : 1 = 0;
    };
    Flags flags_;
  };

  bool is_closed() const;
  bool is_closing() const;
  bool is_listening() const;

  bool Start();

  // Destroy the endpoint if...
  // * There are no sessions,
  // * There are no sent packets with pending done callbacks, and
  // * We're not listening for new initial packets.
  void MaybeDestroy();

  // Specifies the general reason the endpoint is being destroyed.
  enum class CloseContext : uint8_t {
    CLOSE,
    BIND_FAILURE,
    START_FAILURE,
    RECEIVE_FAILURE,
    SEND_FAILURE,
    LISTEN_FAILURE,
  };

  void Destroy(CloseContext context = CloseContext::CLOSE, int status = 0);

  // A graceful close will destroy the endpoint once all existing sessions
  // have ended normally. Creating new sessions (inbound or outbound) will
  // be prevented.
  void CloseGracefully();

  void PacketDone(int status) override;

  void EmitNewSession(const BaseObjectPtr<Session>& session);
  void EmitClose(CloseContext context, int status);

  // JavaScript API

  // Create a new Endpoint.
  // @param Endpoint::Options options - Options to configure the Endpoint.
  JS_METHOD(New);

  // Methods on the Endpoint instance:

  // Create a new client Session on this endpoint.
  // @param node::SocketAddress local_address - The local address to bind to.
  // @param Session::Options options - Options to configure the Session.
  // @param v8::ArrayBufferView session_ticket - The session ticket to use for
  // the Session.
  // @param v8::ArrayBufferView remote_transport_params - The remote transport
  // params.
  JS_METHOD(DoConnect);

  // Start listening as a QUIC server
  // @param Session::Options options - Options to configure the Session.
  JS_METHOD(DoListen);

  // Mark the Endpoint as busy, temporarily pausing handling of new initial
  // packets.
  // @param bool on - If true, mark the Endpoint as busy.
  JS_METHOD(MarkBusy);

  // DoCloseGracefully is the signal that endpoint should close. Any packets
  // that are already in the queue or in flight will be allowed to finish, but
  // the EndpoingWrap will be otherwise no longer able to receive or send
  // packets.
  JS_METHOD(DoCloseGracefully);

  JS_METHOD(DoSetSNIContexts);

  // Get the local address of the Endpoint.
  // @return node::SocketAddress - The local address of the Endpoint.
  JS_METHOD(LocalAddress);

  // Ref() causes a listening Endpoint to keep the event loop active.
  JS_METHOD(Ref);

  void Receive(const uint8_t* data, size_t len, const SocketAddress& from);

  AliasedStruct<Stats> stats_;
  AliasedStruct<State> state_;
  const Options options_;
  ArenaPool<Packet> packet_pool_;
  UDP udp_;

  // Idle timer: started when the endpoint becomes idle (not listening,
  // no primary sessions). When it fires, the endpoint is destroyed.
  // Stopped when a new session is added or listening begins.
  TimerWrapHandle idle_timer_;

  struct ServerState {
    Session::Options options;
    std::shared_ptr<TLSContext> tls_context;
  };
  // Set if/when the endpoint is configured to listen.
  std::optional<ServerState> server_state_ = std::nullopt;

  // Count of sessions for which this endpoint is the primary endpoint.
  // Drives ref/unref and idle timer logic. The actual session-to-endpoint
  // mapping is maintained by the SessionManager.
  size_t primary_session_count_ = 0;

  // Per-endpoint CID -> SCID mapping for peer-chosen CIDs from connection
  // establishment (config.dcid, config.ocid). These are kept per-endpoint
  // because peer-chosen values can collide across endpoints (e.g., a
  // client's random outgoing DCID matching an incoming DCID on the server
  // endpoint). Locally-generated CIDs that need cross-endpoint routing
  // (preferred address, multipath) go in SessionManager::dcid_to_scid_.
  //
  // Endpoint::FindSession does a three-tier lookup:
  //   1. SessionManager::sessions_[cid]          (direct SCID match)
  //   2. SessionManager::dcid_to_scid_[cid]      (cross-endpoint CID)
  //   3. Endpoint::dcid_to_scid_[cid]            (peer-chosen CID)
  // Each tier resolves to an SCID and looks up SessionManager::sessions_.
  CID::Map<CID> dcid_to_scid_;

  SessionManager& session_manager() const;

  struct SocketAddressInfoTraits final {
    struct Type final {
      uint64_t timestamp;
      bool validated;
    };

    static bool CheckExpired(const SocketAddress& address, const Type& type);
    static void Touch(const SocketAddress& address, Type* type);
  };

  SocketAddressLRU<SocketAddressInfoTraits> addr_validation_lru_;

  // Global token buckets for stateless response rate limiting.
  // These cap the total server-wide rate of each response type,
  // regardless of source address.
  TokenBucket retry_bucket_;
  TokenBucket stateless_reset_bucket_;
  TokenBucket version_negotiation_bucket_;
  TokenBucket immediate_close_bucket_;

  // Per-IP connection counts for maxConnectionsPerHost enforcement.
  // Only populated when max_connections_per_host > 0. Entries are
  // added in AddSession and removed when the count reaches 0 in
  // RemoveSession. The map size is bounded by the number of active
  // sessions (each entry has count >= 1).
  SocketAddress::IpMap<uint16_t> conn_counts_per_host_;

  CloseContext close_context_ = CloseContext::CLOSE;
  int close_status_ = 0;

  friend class UDP;
  friend class Packet;
  friend class Session;
};

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
