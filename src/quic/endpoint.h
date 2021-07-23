#ifndef SRC_QUIC_ENDPOINT_H_
#define SRC_QUIC_ENDPOINT_H_
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "quic/buffer.h"
#include "quic/crypto.h"
#include "quic/quic.h"
#include "quic/stats.h"
#include "quic/session.h"
#include "crypto/crypto_context.h"
#include "crypto/crypto_util.h"
#include "aliased_struct.h"
#include "async_signal.h"
#include "async_wrap.h"
#include "base_object.h"
#include "env.h"
#include "handle_wrap.h"
#include "node_sockaddr.h"
#include "node_worker.h"
#include "udp_wrap.h"

#include <ngtcp2/ngtcp2.h>
#include <v8.h>

#include <deque>
#include <unordered_set>
#include <string>

namespace node {
namespace quic {

#define ENDPOINT_STATS(V)                                                      \
  V(CREATED_AT, created_at, "Created at")                                      \
  V(DESTROYED_AT, destroyed_at, "Destroyed at")                                \
  V(BYTES_RECEIVED, bytes_received, "Bytes received")                          \
  V(BYTES_SENT, bytes_sent, "Bytes sent")                                      \
  V(PACKETS_RECEIVED, packets_received, "Packets received")                    \
  V(PACKETS_SENT, packets_sent, "Packets sent")                                \
  V(SERVER_SESSIONS, server_sessions, "Server sessions")                       \
  V(CLIENT_SESSIONS, client_sessions, "Client sessions")                       \
  V(STATELESS_RESET_COUNT, stateless_reset_count, "Stateless reset count")     \
  V(SERVER_BUSY_COUNT, server_busy_count, "Server busy count")

#define ENDPOINT_STATE(V)                                                      \
  V(LISTENING, listening, uint8_t)                                             \
  V(WAITING_FOR_CALLBACKS, waiting_for_callbacks, uint8_t)                     \
  V(PENDING_CALLBACKS, pending_callbacks, size_t)

class Endpoint;
class EndpointWrap;

#define V(name, _, __) IDX_STATS_ENDPOINT_##name,
enum EndpointStatsIdx {
  ENDPOINT_STATS(V)
  IDX_STATS_ENDPOINT_COUNT
};
#undef V

#define V(name, _, __) IDX_STATE_ENDPOINT_##name,
enum EndpointStateIdx {
  ENDPOINT_STATE(V)
  IDX_STATE_ENDPOINT_COUNT
};
#undef V

#define V(_, name, __) uint64_t name;
struct EndpointStats final {
  ENDPOINT_STATS(V)
};
#undef V

using EndpointStatsBase = StatsBase<StatsTraitsImpl<EndpointStats, Endpoint>>;
using UdpSendWrap = ReqWrap<uv_udp_send_t>;

// An Endpoint encapsulates a bound UDP port through which QUIC packets
// are sent and received. An Endpoint is created when a new EndpointWrap
// is created, and may be shared by multiple EndpointWrap instances at
// the same time. The Endpoint, and it's bound UDP port, are associated
// with the libuv event loop and Environment of the EndpointWrap that
// originally created it. All network traffic will be processed within
// the context of that owning event loop for as long as it lasts. If that
// event loop exits, the Endpoint will be closed along with all
// EndpointWrap instances holding a reference to it.
//
// For inbound packets, the Endpoint will perform a preliminary check
// to determine if the UDP packet *looks* like a valid QUIC packet, and
// will perform a handful of checks to see if the packet is acceptable.
// Assuming the packet passes those checks, the Endpoint will extract
// the destination CID from the packet header and will determine whether
// there is an EndpointWrap associated with the CID. If there is, the
// packet will be passed on the EndpointWrap to be processed further.
// If the packet is an "Initial" QUIC packet and the CID is not matched
// to an existing EndpointWrap, the Endpoint will dispatch the packet
// to the first available EndpointWrap instance that has registered
// itself as accepting initial packets ("listening"). If there are
// multiple EndpointWrap instances listening, dispatch is round-robin,
// iterating through each until one accepts the packet or all reject.
// The EndpointWrap that accepts the packet will be moved to the end
// of the list for when the next packet arrives.
//
// The Endpoint can be marked as "busy", which will stop it from accepting
// and dispatching new initial packets to any EndpointWrap, even if those
// are currently idle.
//
// For outbound packets, EndpointWrap instances prepare a SendWrap
// that encapsulates the packet to be sent and the destination address.
// Those are pushed into a queue and processed as a batch on each
// event loop turn. The packets are processed in the order they are added
// to the queue. While the outbound packets can originate from any
// worker thread or context, the actual dispatch of the packet data to
// the UDP port takes place within the context of the Endpoint's owning
// event loop.
//
// The Endpoint encapsulates the uv_udp_t handle directly (via the
// Endpoint::UDPHandle and Endpoint::UDP classes) rather than using
// the node::UDPWrap in order to provide greater control over the
// buffer allocation of inbound packets to ensure that we can safely
// implement zero-copy, thread-safe data sharing across worker
// threads.
class Endpoint final : public MemoryRetainer,
                       public EndpointStatsBase {
 public:
  // Endpoint::Config provides the fundamental configuration options for
  // an Endpoint instance. The configuration property names should be
  // relatively self-explanatory but additional notes are provided in
  // comments where necessary.
  struct Config final : public MemoryRetainer {
    // The local socket address to which the UDP port will be bound.
    // The port may be 0 to have Node.js select an available port.
    // IPv6 or IPv4 addresses may be used. When using IPv6, dual mode
    // will be supported by default.
    std::shared_ptr<SocketAddress> local_address;

    // Retry tokens issued by the Endpoint are time-limited. By default,
    // retry tokens expire after DEFAULT_RETRYTOKEN_EXPIRATION *seconds*.
    // The retry_token_expiration parameter is always expressed in terms
    // of seconds. This is an arbitrary choice that is not mandated by
    // the QUIC specification; so we can choose any value that makes
    // sense here. Retry tokens are sent to the client, which echoes them
    // back to the server in a subsequent set of packets, which means the
    // expiration must be set high enough to allow a reasonable round-trip
    // time for the session TLS handshake to complete.
    uint64_t retry_token_expiration = DEFAULT_RETRYTOKEN_EXPIRATION;

    // Tokens issued using NEW_TOKEN are time-limited. By default,
    // tokens expire after DEFAULT_TOKEN_EXPIRATION *seconds*.
    uint64_t token_expiration = DEFAULT_TOKEN_EXPIRATION;

    // The max_window_override and max_stream_window_override parameters
    // determine the maximum flow control window sizes that will be used.
    // Setting these at zero causes ngtcp2 to use it's defaults, which is
    // ideal. Setting things to any other value will disable the automatic
    // flow control management, which is already optimized. Settings these
    // should be rare, and should only be done if there's a really good
    // reason.
    uint64_t max_window_override = 0;
    uint64_t max_stream_window_override = 0;

    // Each Endpoint places limits on the number of concurrent connections
    // from a single host, and the total number of concurrent connections
    // allowed as a whole. These are set to fairly modest, and arbitrary
    // defaults. We can set these to whatever we'd like.
    uint64_t max_connections_per_host = DEFAULT_MAX_CONNECTIONS_PER_HOST;
    uint64_t max_connections_total = DEFAULT_MAX_CONNECTIONS;

    // A stateless reset in QUIC is a discrete mechanism that one endpoint
    // can use to communicate to a peer that it has lost whatever state
    // it previously held about a session. Because generating a stateless
    // reset consumes resources (even very modestly), they can be a DOS
    // vector in which a malicious peer intentionally sends a large number
    // of stateless reset eliciting packets. To protect against that risk,
    // we limit the number of stateless resets that may be generated for
    // a given remote host within a window of time. This is not mandated
    // by QUIC, and the limit is arbitrary. We can set it to whatever we'd
    // like.
    uint64_t max_stateless_resets = DEFAULT_MAX_STATELESS_RESETS;

    // For tracking the number of connections per host, the number of
    // stateless resets that have been sent, and tracking the path
    // verification status of a remote host, we maintain an LRU cache
    // of the most recently seen hosts.  The address_lru_size parameter
    // determines the size of that cache. The default is set modestly
    // at 10 times the default max connections per host.
    uint64_t address_lru_size = DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE;

    // Similar to stateless resets, we enforce a limit on the number of
    // retry packets that can be generated and sent for a remote host.
    // Generating retry packets consumes a modest amount of resources
    // and it's fairly trivial for a malcious peer to trigger generation
    // of a large number of retries, so limiting them helps prevent a
    // DOS vector.
    uint64_t retry_limit = DEFAULT_MAX_RETRY_LIMIT;

    // The max_payload_size is the maximum size of a serialized QUIC
    // packet. It should always be set small enough to fit within a
    // single MTU without fragmentation. The default is set by the QUIC
    // specification at 1200. This value should not be changed unless
    // you know for sure that the entire path supports a given MTU
    // without fragmenting at any point in the path.
    uint64_t max_payload_size = NGTCP2_DEFAULT_MAX_PKTLEN;

    // The unacknowledged_packet_threshold is the maximum number of
    // unacknowledged packets that an ngtcp2 session will accumulate
    // before sending an acknowledgement. Setting this to 0 uses the
    // ngtcp2 defaults, which is what most will want. The value can
    // be changed to fine tune some of the performance characteristics
    // of the session. This should only be changed if you have a really
    // good reason for doing so.
    uint64_t unacknowledged_packet_threshold = 0;

    // The validate_address parameter instructs the Endpoint to perform
    // explicit address validation using retry tokens. This is strongly
    // recommended and should only be disabled in trusted, closed
    // environments as a performance optimization.
    bool validate_address = true;

    // The stateless reset mechanism can be disabled. This should rarely
    // ever be needed, and should only ever be done in trusted, closed
    // environments as a performance optimization.
    bool disable_stateless_reset = false;

    // The rx_loss and tx_loss parameters are debugging tools that allow
    // the Endpoint to simulate random packet loss. The value for each
    // parameter is a value between 0.0 and 1.0 indicating a probability
    // of packet loss. Each time a packet is sent or received, the packet
    // loss bit is calculated and if true, the packet is silently dropped.
    // This should only ever be used for testing and debugging. There is
    // never a reason why rx_loss and tx_loss should ever be used in a
    // production system.
    double rx_loss = 0.0;
    double tx_loss = 0.0;

    // There are two common congestion control algorithms that ngtcp2 uses
    // to determine how it manages the flow control window: RENO and CUBIC.
    // The details of how each works is not relevant here. The choice of
    // which to use by default is arbitrary and we can choose whichever we'd
    // like. Additional performance profiling will be needed to determine
    // which is the better of the two for our needs.
    ngtcp2_cc_algo cc_algorithm = NGTCP2_CC_ALGO_CUBIC;

    // By default, when Node.js starts, it will generate a reset_token_secret
    // at random. This is a secret used in generating stateless reset tokens.
    // In order for stateless reset to be effective, however, it is necessary
    // to use a deterministic secret that persists across ngtcp2 endpoints and
    // sessions.
    // TODO(@jasnell): Given that this is a secret, it may make sense to use
    // the secure heap to store it (when the heap is available). Doing so
    // will require some refactoring here, however, so we'll defer that to
    // a future iteration.
    uint8_t reset_token_secret[NGTCP2_STATELESS_RESET_TOKENLEN];

    // When the local_address specifies an IPv6 local address to bind
    // to, the ipv6_only parameter determines whether dual stack mode
    // (supporting both IPv6 and IPv4) transparently is supported.
    // This sets the UV_UDP_IPV6ONLY flag on the underlying uv_udp_t.
    bool ipv6_only = false;

    uint32_t udp_receive_buffer_size = 0;
    uint32_t udp_send_buffer_size = 0;

    // The UDP TTL configuration is the number of network hops a packet
    // will be forwarded through. The default is 64. The value is in the
    // range 1 to 255. Setting to 0 uses the default.
    uint8_t udp_ttl = 0;

    Config();
    Config(const Config& other) noexcept;

    Config(Config&& other) = delete;
    Config& operator=(Config&& other) = delete;

    inline Config& operator=(const Config& other) noexcept {
      if (this == &other) return *this;
      this->~Config();
      return *new(this) Config(other);
    }

    inline void GenerateResetTokenSecret();

    SET_NO_MEMORY_INFO()
    SET_MEMORY_INFO_NAME(Endpoint::Config)
    SET_SELF_SIZE(Config)
  };

  // The SendWrap is a persistent ReqWrap instance that encapsulates a
  // QUIC Packet that is to be sent to a remote peer. They are created
  // by the EndpointWrap and queued into the shared Endpoint for processing.
  class SendWrap final : public UdpSendWrap {
   public:
    static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

    static BaseObjectPtr<SendWrap> Create(
        Environment* env,
        const std::shared_ptr<SocketAddress>& destination,
        std::unique_ptr<Packet> packet,
        BaseObjectPtr<EndpointWrap> endpoint = BaseObjectPtr<EndpointWrap>());

    SendWrap(
      Environment* env,
      v8::Local<v8::Object> object,
      const std::shared_ptr<SocketAddress>& destination,
      std::unique_ptr<Packet> packet,
      BaseObjectPtr<EndpointWrap> endpoint);

    inline void Attach(const BaseObjectPtr<BaseObject>& strong_ptr) {
      strong_ptr_ = strong_ptr;
    }

    inline const std::shared_ptr<SocketAddress>& destination() const {
      return destination_;
    }
    inline EndpointWrap* endpoint() const { return endpoint_.get(); }
    inline Packet* packet() const { return packet_.get(); }

    void Done(int status);

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Endpoint::SendWrap)
    SET_SELF_SIZE(SendWrap)

    using Queue = std::deque<BaseObjectPtr<SendWrap>>;

   private:
    std::shared_ptr<SocketAddress> destination_;
    std::unique_ptr<Packet> packet_;
    BaseObjectPtr<EndpointWrap> endpoint_;
    BaseObjectPtr<BaseObject> strong_ptr_;
    BaseObjectPtr<SendWrap> self_ptr_;
  };

  // The UDP class directly encapsulates the uv_udp_t handle. This is
  // very similar to UDPWrap except that it is specific to the QUIC
  // data structures (like Endpoint and Packet), and passes received
  // packet data on using v8::BackingStore instead of uv_buf_t to
  // help eliminate the need for memcpy down the line by making
  // transfer of data ownership more explicit.
  class UDP final : public HandleWrap {
   public:
    static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
        Environment* env);

    static UDP* Create(Environment* env, Endpoint* endpoint);

    UDP(Environment* env,
        v8::Local<v8::Object> object,
        Endpoint* endpoint);

    UDP(const UDP&) = delete;
    UDP(UDP&&) = delete;
    UDP& operator=(const UDP&) = delete;
    UDP& operator=(UDP&&) = delete;

    int Bind(const Endpoint::Config& config);

    void Ref();
    void Unref();
    void Close();
    int StartReceiving();
    void StopReceiving();
    std::shared_ptr<SocketAddress> local_address() const;

    int SendPacket(BaseObjectPtr<SendWrap> req);

    inline bool is_closing() const { return uv_is_closing(GetHandle()); }

    SET_NO_MEMORY_INFO()
    SET_MEMORY_INFO_NAME(Endpoint::UDP)
    SET_SELF_SIZE(UDP)

   private:
    static void ClosedCb(uv_handle_t* handle);
    static void OnAlloc(
        uv_handle_t* handle,
        size_t suggested_size,
        uv_buf_t* buf);

    static void OnReceive(
        uv_udp_t* handle,
        ssize_t nread,
        const uv_buf_t* buf,
        const sockaddr* addr,
        unsigned int flags);

    void MaybeClose();

    uv_udp_t handle_;
    Endpoint* endpoint_;
  };

  // UDPHandle is a helper class that sits between the Endpoint and
  // the UDP the help manage the lifecycle of the UDP class. Essentially,
  // UDPHandle allows the Endpoint to be deconstructed immediately while
  // allowing the UDP class to go through the typical asynchronous cleanup
  // flow with the event loop.
  class UDPHandle final : public MemoryRetainer {
   public:
    UDPHandle(Environment* env, Endpoint* endpoint);

    inline ~UDPHandle() { Close(); }

    inline int Bind(const Endpoint::Config& config) {
      return udp_->Bind(config);
    }

    inline void Ref() { if (udp_) udp_->Ref(); }
    inline void Unref() { if (udp_) udp_->Unref();}
    inline int StartReceiving() {
      return udp_ ? udp_->StartReceiving() : UV_EBADF;
    }
    inline void StopReceiving() {
      if (udp_) udp_->StopReceiving();
    }
    inline std::shared_ptr<SocketAddress> local_address() const {
      return udp_ ? udp_->local_address() : std::make_shared<SocketAddress>();
    }
    void Close();

    inline int SendPacket(BaseObjectPtr<SendWrap> req) {
      return udp_ ? udp_->SendPacket(std::move(req)) : UV_EBADF;
    }

    inline bool closed() const { return !udp_; }

    void MemoryInfo(node::MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Endpoint::UDPHandle)
    SET_SELF_SIZE(UDPHandle)

   private:
    static void CleanupHook(void* data);
    Environment* env_;
    UDP* udp_;
  };

  // The InitialPacketListener is an interface implemented by EndpointWrap
  // and registered with the Endpoint when it is set to listen for new
  // incoming initial packets (that is, when it's acting as a server).
  // The Endpoint passes both the v8::BackingStore and the nread because
  // the nread (the actual number of bytes read and filled in the backing
  // store) may be less than the actual size of the v8::BackingStore.
  // Specifically, nread will always <= v8::BackStore::ByteLength().
  struct InitialPacketListener {
    virtual bool Accept(
        const Session::Config& config,
        std::shared_ptr<v8::BackingStore> store,
        size_t nread,
        const std::shared_ptr<SocketAddress>& local_address,
        const std::shared_ptr<SocketAddress>& remote_address,
        const CID& dcid,
        const CID& scid,
        const CID& ocid) = 0;

    using List = std::deque<InitialPacketListener*>;
  };

  // The PacketListener is an interface implemented by EndpointWrap
  // and registered with the Endpoint to handle received packets
  // intended for a specific session.
  // The Endpoint passes both the v8::BackingStore and the nread because
  // the nread (the actual number of bytes read and filled in the backing
  // store) may be less than the actual size of the v8::BackingStore.
  // Specifically, nread will always <= v8::BackStore::ByteLength().
  struct PacketListener {
    enum class Flags {
      NONE,
      STATELESS_RESET
    };

    virtual bool Receive(
        const CID& dcid,
        const CID& scid,
        std::shared_ptr<v8::BackingStore> store,
        size_t nread,
        const std::shared_ptr<SocketAddress>& local_address,
        const std::shared_ptr<SocketAddress>& remote_address,
        Flags flags = Flags::NONE) = 0;
  };

  // Every EndpointWrap associated with the Endpoint registers a CloseListener
  // that receives notification when the Endpoint is closing, either because
  // it's owning event loop is closing or because of an error.
  struct CloseListener {
    enum class Context {
      CLOSE,
      RECEIVE_FAILURE,
      SEND_FAILURE,
      LISTEN_FAILURE,
    };
    virtual void EndpointClosed(Context context, int status) = 0;

    using Set = std::unordered_set<CloseListener*>;
  };

  Endpoint(Environment* env, const Config& config);

  ~Endpoint() override;

  void AddCloseListener(CloseListener* listener);
  void RemoveCloseListener(CloseListener* listener);

  void AddInitialPacketListener(InitialPacketListener* listener);
  void RemoveInitialPacketListener(InitialPacketListener* listener);

  void AssociateCID(const CID& cid, PacketListener* session);
  void DisassociateCID(const CID& cid);

  void IncrementSocketAddressCounter(
      const std::shared_ptr<SocketAddress>& address);
  void DecrementSocketAddressCounter(
      const std::shared_ptr<SocketAddress>& address);

  void AssociateStatelessResetToken(
      const StatelessResetToken& token,
      PacketListener* session);

  void DisassociateStatelessResetToken(const StatelessResetToken& token);

  v8::Maybe<size_t> GenerateNewToken(
      uint8_t* token,
      const std::shared_ptr<SocketAddress>& remote_address);

  // This version of SendPacket is used to send packets that are not
  // affiliated with a Session (Retry, Version Negotiation, and Early
  // Connection Close packets, for instance).
  bool SendPacket(
    const std::shared_ptr<SocketAddress>& remote_address,
    std::unique_ptr<Packet> packet);

  // This version of SendPacket is used to send packets that are
  // affiliated with a Session.
  void SendPacket(BaseObjectPtr<SendWrap> packet);

  // Shutdown a connection prematurely, before a Session is created.
  // This should only be called at the start of a session before the crypto
  // keys have been established.
  void ImmediateConnectionClose(
      const quic_version version,
      const CID& scid,
      const CID& dcid,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_addr,
      error_code reason = NGTCP2_INVALID_TOKEN);

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
  bool SendRetry(
      const quic_version version,
      const CID& dcid,
      const CID& scid,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address);

  // Sends a version negotiation packet. This is terminal for
  // the connection and is sent only when a QUIC packet is
  // received for an unsupported Node.js version.
  // It is possible that a malicious packet triggered this
  // so we need to be careful not to commit too many resources.
  // Currently, we only support one QUIC version at a time.
  void SendVersionNegotiation(
      const quic_version version,
      const CID& dcid,
      const CID& scid,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address);

  void Ref();
  void Unref();

  // While the busy flag is set, the Endpoint will reject all initial
  // packets with a SERVER_BUSY response, even if there are available
  // listening EndpointWraps. This allows us to build a circuit breaker
  // directly in to the implementation, explicitly signaling that the
  // server is blocked when activity is high.
  inline void set_busy(bool on = true) { busy_ = on; }

  // QUIC strongly recommends the use of flow labels when using IPv6.
  // The GetFlowLabel will deterministically generate a flow label as
  // a function of the given local address, remote address, and connection ID.
  uint32_t GetFlowLabel(
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const CID& cid);

  inline const Config& config() const { return config_; }
  inline Environment* env() const { return env_; }
  std::shared_ptr<SocketAddress> local_address() const;

  int StartReceiving();
  void MaybeStopReceiving();

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Endpoint);
  SET_SELF_SIZE(Endpoint);

  struct Lock {
    Mutex::ScopedLock lock_;
    explicit Lock(Endpoint* endpoint) : lock_(endpoint->mutex_) {}
    explicit Lock(const std::shared_ptr<Endpoint>& endpoint)
        : lock_(endpoint->mutex_) {}
  };

 private:
  static void OnCleanup(void* data);

  void Close(
      CloseListener::Context context = CloseListener::Context::CLOSE,
      int status = 0);

  // Inspects the packet and possibly accepts it as a new
  // initial packet creating a new Session instance.
  // If the packet is not acceptable, it is very important
  // not to commit resources.
  bool AcceptInitialPacket(
      const quic_version version,
      const CID& dcid,
      const CID& scid,
      std::shared_ptr<v8::BackingStore> store,
      size_t nread,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address);

  int MaybeBind();

  PacketListener* FindSession(const CID& cid);

  // When a received packet contains a QUIC short header but cannot be
  // matched to a known Session, it is either (a) garbage,
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
  bool MaybeStatelessReset(
      const CID& dcid,
      const CID& scid,
      std::shared_ptr<v8::BackingStore> store,
      size_t nread,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address);

  uv_buf_t OnAlloc(size_t suggested_size);
  void OnReceive(
      size_t nread,
      const uv_buf_t& buf,
      const std::shared_ptr<SocketAddress>& address);

  void ProcessOutbound();
  void ProcessSendFailure(int status);
  void ProcessReceiveFailure(int status);

  // Possibly generates and sends a stateless reset packet.
  // This is terminal for the connection. It is possible
  // that a malicious packet triggered this so we need to
  // be careful not to commit too many resources.
  bool SendStatelessReset(
      const CID& cid,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address,
      size_t source_len);

  void set_validated_address(const std::shared_ptr<SocketAddress>& addr);
  bool is_validated_address(const std::shared_ptr<SocketAddress>& addr) const;
  void IncrementStatelessResetCounter(
      const std::shared_ptr<SocketAddress>& addr);
  size_t current_socket_address_count(
      const std::shared_ptr<SocketAddress>& addr) const;
  size_t current_stateless_reset_count(
      const std::shared_ptr<SocketAddress>& addr) const;
  bool is_diagnostic_packet_loss(double prob) const;

  Environment* env_;
  UDPHandle udp_;
  const Config config_;

  SendWrap::Queue outbound_;
  AsyncSignalHandle outbound_signal_;
  size_t pending_outbound_ = 0;

  size_t ref_count_ = 0;

  uint8_t token_secret_[kTokenSecretLen];
  ngtcp2_crypto_aead token_aead_;
  ngtcp2_crypto_md token_md_;

  struct SocketAddressInfoTraits final {
    struct Type final {
      size_t active_connections;
      size_t reset_count;
      size_t retry_count;
      uint64_t timestamp;
      bool validated;
    };

    static bool CheckExpired(
        const SocketAddress& address,
        const Type& type);
    static void Touch(
        const SocketAddress& address,
        Type* type);
  };

  SocketAddressLRU<SocketAddressInfoTraits> addrLRU_;
  StatelessResetToken::Map<PacketListener*> token_map_;
  CID::Map<PacketListener*> sessions_;
  InitialPacketListener::List listeners_;
  CloseListener::Set close_listeners_;

  bool busy_ = false;
  bool bound_ = false;
  bool receiving_ = false;

  Mutex mutex_;
};

// The EndpointWrap is the intermediate JavaScript binding object
// that is passed into JavaScript for interacting with the Endpoint.
// Every EndpointWrap wraps a single Endpoint (that may be shared
// with other EndpointWrap instances).
// All EndpointWrap instances are "cloneable" via MessagePort but
// the instances are not true copies. Each "clone" will share a
// reference to the same Endpoint (via std::shared_ptr<Endpoint>),
// but will retain their own separate state in every other regard.
// Specifically, QUIC Sessions are only ever associated with a
// single EndpointWrap at a time.
class EndpointWrap final : public AsyncWrap,
                           public Endpoint::CloseListener,
                           public Endpoint::InitialPacketListener,
                           public Endpoint::PacketListener {
 public:
  struct State final {
#define V(_, name, type) type name;
    ENDPOINT_STATE(V)
#undef V
  };

  // The InboundPacket represents a packet received by the
  // Endpoint and passed on to the EndpointWrap for processing.
  // InboundPackets are stored in a queue and processed in a batch
  // once per event loop turn. They are always processed in the
  // order they were received.
  struct InboundPacket final {
    CID dcid;
    CID scid;
    std::shared_ptr<v8::BackingStore> store;
    size_t nread;
    std::shared_ptr<SocketAddress> local_address;
    std::shared_ptr<SocketAddress> remote_address;
    Endpoint::PacketListener::Flags flags;

    using Queue = std::deque<InboundPacket>;
  };

  // The InitialPacket represents an initial packet to create
  // a new Session received by the Endpoint and passed on to
  // the EndpointWrap for processing. InitialPackets are stored
  // in a queue and processed in a batch once per event loop turn.
  // They are always processed in the order they were received.
  struct InitialPacket final {
    Session::Config config;
    std::shared_ptr<v8::BackingStore> store;
    size_t nread;
    std::shared_ptr<SocketAddress> local_address;
    std::shared_ptr<SocketAddress> remote_address;
    CID dcid;
    CID scid;
    CID ocid;

    using Queue = std::deque<InitialPacket>;
  };

  static bool HasInstance(Environment* env, const v8::Local<v8::Value>& value);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  static BaseObjectPtr<EndpointWrap> Create(
      Environment* env,
      const Endpoint::Config& config);

  static BaseObjectPtr<EndpointWrap> Create(
      Environment* env,
      std::shared_ptr<Endpoint> endpoint);

  static void CreateClientSession(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CreateEndpoint(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StartListen(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StartWaitForPendingCallbacks(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LocalAddress(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Ref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Unref(const v8::FunctionCallbackInfo<v8::Value>& args);

  EndpointWrap(
      Environment* env,
      v8::Local<v8::Object> object,
      const Endpoint::Config& config);

  EndpointWrap(
      Environment* env,
      v8::Local<v8::Object> object,
      std::shared_ptr<Endpoint> inner);

  ~EndpointWrap() override;

  explicit EndpointWrap(const EndpointWrap& other) = delete;
  explicit EndpointWrap(const EndpointWrap&& other) = delete;
  EndpointWrap& operator=(const Endpoint& other) = delete;
  EndpointWrap& operator=(const Endpoint&& other) = delete;

  void EndpointClosed(Endpoint::CloseListener::Context context, int status);

  // Returns the default Session::Options used for new server
  // sessions accepted by this EndpointWrap. The server_options_
  // is set when EndpointWrap::Listen() is called. Until then it
  // will return the defaults.
  inline const std::shared_ptr<Session::Options>& server_config() const {
    return server_options_;
  }

  v8::Maybe<size_t> GenerateNewToken(
      uint8_t* token,
      const std::shared_ptr<SocketAddress>& remote_address);

  inline State* state() { return state_.Data(); }
  inline const Endpoint::Config& config() const { return inner_->config(); }

  // The local UDP address to which the inner Endpoint is bound.
  inline std::shared_ptr<SocketAddress> local_address() const {
    return inner_->local_address();
  }

  // Called by the inner Endpoint when a new initial packet is received.
  // Accept() will return true if the EndpointWrap will handle the initial
  // packet, false otherwise.
  bool Accept(
      const Session::Config& config,
      std::shared_ptr<v8::BackingStore> store,
      size_t nread,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address,
      const CID& dcid,
      const CID& scid,
      const CID& ocid) override;

  // Adds a new Session to this EndpointWrap, associating the
  // session with the given CID. The inner Endpoint is also
  // notified to associate the CID with this EndpointWrap.
  void AddSession(const CID& cid, const BaseObjectPtr<Session>& session);

  // A single session may be associated with multiple CIDs
  // The AssociateCID registers the neceesary mapping both in the
  // EndpointWrap and the inner Endpoint.
  void AssociateCID(const CID& cid, const CID& scid);

  // Associates a given stateless reset token with the session.
  // This allows stateless reset tokens to be recognized and
  // dispatched to the proper EndpointWrap and Session for
  // processing.
  void AssociateStatelessResetToken(
      const StatelessResetToken& token,
      const BaseObjectPtr<Session>& session);

  BaseObjectPtr<Session> CreateClient(
      const std::shared_ptr<SocketAddress>& remote_addr,
      const Session::Config& config,
      const std::shared_ptr<Session::Options>& options,
      const BaseObjectPtr<crypto::SecureContext>& context,
      const crypto::ArrayBufferOrViewContents<uint8_t>& session_ticket =
          crypto::ArrayBufferOrViewContents<uint8_t>(),
      const crypto::ArrayBufferOrViewContents<uint8_t>&
          remote_transport_params =
              crypto::ArrayBufferOrViewContents<uint8_t>());

  // Removes the associated CID from this EndpointWrap and the
  // inner Endpoint.
  void DisassociateCID(const CID& cid);

  // Removes the associated stateless reset token from this EndpointWrap
  // and the inner Endpoint.
  void DisassociateStatelessResetToken(const StatelessResetToken& token);

  // Looks up an existing session by the associated CID. If no matching
  // session is found, returns an empty BaseObjectPtr<Session>.
  BaseObjectPtr<Session> FindSession(const CID& cid);

  // Generates an IPv6 flow label for the given local_address, remote_address,
  // and CID. Both the local_address and remote_address must be IPv6.
  uint32_t GetFlowLabel(
    const std::shared_ptr<SocketAddress>& local_address,
    const std::shared_ptr<SocketAddress>& remote_address,
    const CID& cid);

  // Shutdown a connection prematurely, before a Session is created.
  // This should only be called at the start of a session before the crypto
  // keys have been established.
  void ImmediateConnectionClose(
      const quic_version version,
      const CID& scid,
      const CID& dcid,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address,
      const error_code reason = NGTCP2_INVALID_TOKEN);

  // Registers this EndpointWrap as able to accept incoming initial
  // packets. Whenever an Endpoint receives an initial packet for which
  // there is no associated Session, the Endpoint will iterate through
  // it's registered listening EndpointWrap instances to find one willing
  // to accept the packet.
  void Listen(
      const std::shared_ptr<Session::Options>& options,
      const BaseObjectPtr<crypto::SecureContext>& context);

  void OnSendDone(int status);

  // Receives a packet intended for a session owned by this EndpointWrap
  bool Receive(
      const CID& dcid,
      const CID& scid,
      std::shared_ptr<v8::BackingStore> store,
      size_t nread,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address,
      const PacketListener::Flags flags) override;

  // Removes the given session from from EndpointWrap and removes the
  // registered associations on the inner Endpoint.
  void RemoveSession(
      const CID& cid,
      const std::shared_ptr<SocketAddress>& address);

  // Sends a serialized QUIC packet to the remote_addr on behalf of the
  // given session.
  void SendPacket(
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address,
      std::unique_ptr<Packet> packet,
      const BaseObjectPtr<Session>& session = BaseObjectPtr<Session>());

  // Generates and sends a retry packet. This is terminal
  // for the connection. Retry packets are used to force
  // explicit path validation by issuing a token to the
  // peer that it must thereafter include in all subsequent
  // initial packets. Upon receiving a retry packet, the
  // peer must termination it's initial attempt to
  // establish a connection and start a new attempt.
  //
  // Retry packets will only ever be generated by QUIC servers,
  // and only if the Endpoint is configured for explicit path
  // validation. There is no way for a client to force a retry
  // packet to be created. However, once a client determines that
  // explicit path validation is enabled, it could attempt to
  // DOS by sending a large number of malicious initial packets
  // to intentionally ellicit retry packets (It can do so by
  // intentionally sending initial packets that ignore the retry
  // token). To help mitigate that risk, we limit the number of
  // retries we send to a given remote endpoint.
  bool SendRetry(
      const quic_version version,
      const CID& dcid,
      const CID& scid,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address);

  inline void set_busy(bool on = true) { inner_->set_busy(on); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(EndpointWrap);
  SET_SELF_SIZE(EndpointWrap);

  // An EndpointWrap instance is cloneable over MessagePort.
  // Clones will share the same inner Endpoint instance but
  // will maintain their own state and their own collection
  // of associated sessions.
  class TransferData final : public worker::TransferData {
   public:
    inline TransferData(std::shared_ptr<Endpoint> inner)
        : inner_(inner) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self);

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(EndpointWrap::TransferData)
    SET_SELF_SIZE(TransferData)

   private:
    std::shared_ptr<Endpoint> inner_;
  };

  TransferMode GetTransferMode() const override {
    return TransferMode::kCloneable;
  }
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

 private:
  // The underlying endpoint has been closed. Clean everything up and notify.
  // No further packets will be sent at this point. This can happen abruptly
  // so we have to make sure we cycle out through the JavaScript side to free
  // up everything there.
  void Close();

  // Called after the endpoint has been closed and the final
  // pending send callback has been received. Signals to the
  // JavaScript side that the endpoint is ready to be destroyed.
  void OnEndpointDone();

  // Called when the Endpoint has encountered an error condition
  // Signals to the JavaScript side.
  void OnError(v8::Local<v8::Value> error = v8::Local<v8::Value>());

  // Called when a new Session has been created. Passes the
  // reference to the new session on the JavaScript side for
  // additional processing.
  void OnNewSession(const BaseObjectPtr<Session>& session);

  void ProcessInbound();
  void ProcessInitial();
  void ProcessInitialFailure();

  inline void DecrementPendingCallbacks() { state_->pending_callbacks--; }
  inline void IncrementPendingCallbacks() { state_->pending_callbacks++; }
  inline bool is_done_waiting_for_callbacks() const {
    return state_->waiting_for_callbacks && !state_->pending_callbacks;
  }
  void WaitForPendingCallbacks();

  AliasedStruct<State> state_;
  std::shared_ptr<Endpoint> inner_;

  std::shared_ptr<Session::Options> server_options_;
  BaseObjectPtr<crypto::SecureContext> server_context_;

  StatelessResetToken::Map<BaseObjectPtr<Session>> token_map_;
  CID::Map<BaseObjectPtr<Session>> sessions_;
  CID::Map<CID> dcid_to_scid_;

  InboundPacket::Queue inbound_;
  InitialPacket::Queue initial_;

  Endpoint::CloseListener::Context close_context_ =
      Endpoint::CloseListener::Context::CLOSE;
  int close_status_ = 0;

  AsyncSignalHandle close_signal_;
  AsyncSignalHandle inbound_signal_;
  AsyncSignalHandle initial_signal_;
  Mutex inbound_mutex_;
};

// The ConfigObject is a persistent, cloneable Endpoint::Config.
// It is used to encapsulate all of the fairly complex configuration
// options for an Endpoint.
class ConfigObject final : public BaseObject {
 public:
  static bool HasInstance(Environment* env, const v8::Local<v8::Value>& value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GenerateResetTokenSecret(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetResetTokenSecret(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  ConfigObject(
      Environment* env,
      v8::Local<v8::Object> object,
      std::shared_ptr<Endpoint::Config> config =
          std::make_shared<Endpoint::Config>());

  inline Endpoint::Config* data() { return config_.get(); }
  inline const Endpoint::Config& config() { return *config_.get(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ConfigObject)
  SET_SELF_SIZE(ConfigObject)

 private:
  template <typename T>
  v8::Maybe<bool> SetOption(
      const v8::Local<v8::Object>& object,
      const v8::Local<v8::String>& name,
      T Endpoint::Config::*member);

  std::shared_ptr<Endpoint::Config> config_;
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_QUIC_ENDPOINT_H_
