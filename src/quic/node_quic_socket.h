#ifndef SRC_QUIC_NODE_QUIC_SOCKET_H_
#define SRC_QUIC_NODE_QUIC_SOCKET_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_struct.h"
#include "base_object.h"
#include "node.h"
#include "node_crypto.h"
#include "node_internals.h"
#include "ngtcp2/ngtcp2.h"
#include "nghttp3/nghttp3.h"
#include "node_quic_state.h"
#include "node_quic_session.h"
#include "node_quic_util.h"
#include "node_sockaddr.h"
#include "env.h"
#include "udp_wrap.h"
#include "v8.h"
#include "uv.h"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace node {

using v8::Context;
using v8::Local;
using v8::Object;

namespace quic {

class QuicSocket;
class QuicEndpoint;

constexpr size_t DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE = 1000;
constexpr size_t DEFAULT_MAX_RETRY_LIMIT = 10;

#define QUICSOCKET_OPTIONS(V)                                                  \
    V(VALIDATE_ADDRESS, validate_address)

#define V(id, _) QUICSOCKET_OPTIONS_##id,
enum QuicSocketOptions : uint32_t {
  QUICSOCKET_OPTIONS(V)
  QUICSOCKET_OPTIONS_COUNT
};
#undef V

#define QUICSOCKET_SHARED_STATE(V)                                             \
    V(SERVER_LISTENING, server_listening, uint8_t)                             \
    V(SERVER_BUSY, server_busy, uint8_t)                                       \
    V(STATELESS_RESET_DISABLED, stateless_reset_disabled, uint8_t)

#define V(_, name, type) type name;
struct QuicSocketState {
  QUICSOCKET_SHARED_STATE(V)
};
#undef V

#define V(id, name, _)                                                         \
    IDX_QUICSOCKET_STATE_##id = offsetof(QuicSocketState, name),
enum QuicSocketStateFields {
  QUICSOCKET_SHARED_STATE(V)
  IDX_QUICSOCKET_STATE_END
};
#undef V

#define SOCKET_STATS(V)                                                        \
  V(CREATED_AT, created_at, "Created At")                                      \
  V(BOUND_AT, bound_at, "Bound At")                                            \
  V(LISTEN_AT, listen_at, "Listen At")                                         \
  V(DESTROYED_AT, destroyed_at, "Destroyed At")                                \
  V(BYTES_RECEIVED, bytes_received, "Bytes Received")                          \
  V(BYTES_SENT, bytes_sent, "Bytes Sent")                                      \
  V(PACKETS_RECEIVED, packets_received, "Packets Received")                    \
  V(PACKETS_IGNORED, packets_ignored, "Packets Ignored")                       \
  V(PACKETS_SENT, packets_sent, "Packets Sent")                                \
  V(SERVER_SESSIONS, server_sessions, "Server Sessions")                       \
  V(CLIENT_SESSIONS, client_sessions, "Client Sessions")                       \
  V(STATELESS_RESET_COUNT, stateless_reset_count, "Stateless Reset Count")     \
  V(SERVER_BUSY_COUNT, server_busy_count, "Server Busy Count")

#define V(name, _, __) IDX_QUIC_SOCKET_STATS_##name,
enum QuicSocketStatsIdx : int {
  SOCKET_STATS(V)
  IDX_QUIC_SOCKET_STATS_COUNT
};
#undef V

#define V(_, name, __) uint64_t name;
struct QuicSocketStats {
  SOCKET_STATS(V)
};
#undef V

struct QuicSocketStatsTraits {
  using Stats = QuicSocketStats;
  using Base = QuicSocket;

  template <typename Fn>
  static void ToString(const Base& ptr, Fn&& add_field);
};

// This is the generic interface for objects that control QuicSocket
// instances. The default `JSQuicSocketListener` emits events to
// JavaScript
class QuicSocketListener {
 public:
  virtual ~QuicSocketListener();

  virtual void OnError(ssize_t code);
  virtual void OnSessionReady(BaseObjectPtr<QuicSession> session);
  virtual void OnServerBusy();
  virtual void OnEndpointDone(QuicEndpoint* endpoint);
  virtual void OnDestroy();

  QuicSocket* socket() const { return socket_.get(); }

 private:
  BaseObjectWeakPtr<QuicSocket> socket_;
  QuicSocketListener* previous_listener_ = nullptr;
  friend class QuicSocket;
};

class JSQuicSocketListener final : public QuicSocketListener {
 public:
  void OnError(ssize_t code) override;
  void OnSessionReady(BaseObjectPtr<QuicSession> session) override;
  void OnServerBusy() override;
  void OnEndpointDone(QuicEndpoint* endpoint) override;
  void OnDestroy() override;
};

// This is just a formality as the QUIC spec normatively
// defines that the ipv4 max pktlen is always going to be
// larger than the ipv6 max pktlen, but in the off chance
// ever changes (which is unlikely) we check here.
constexpr size_t MAX_PKTLEN =
    std::max<size_t>(NGTCP2_MAX_PKTLEN_IPV4, NGTCP2_MAX_PKTLEN_IPV6);

// A serialized QuicPacket to be sent by a QuicSocket instance.
// QuicPackets are intended to be transient. They are created,
// filled with the contents of a serialized packet, and passed
// off immediately to the QuicSocket to be sent. As soon as
// the packet is sent, it is freed.
class QuicPacket : public MemoryRetainer {
 public:
  // Creates a new QuicPacket. By default the packet will be
  // stack allocated with a max size of NGTCP2_MAX_PKTLEN_IPV4.
  // If a larger packet size is specified, it will be heap
  // allocated. A QUIC packet should never be larger than the
  // current MTU to avoid IP fragmentation.
  //
  // The content of a QuicPacket is provided by ngtcp2 and is
  // opaque for us. The typical use pattern is to create a QuicPacket
  // instance and then pass a pointer to it's internal buffer and max
  // size in to an ngtcp2 function that serializes the data.
  // ngtcp2 will fill the buffer as much as possible then return
  // the number of bytes serialized. User code is then responsible
  // for calling set_length() to set the final length of the
  // QuicPacket prior to sending it off to the QuicSocket.
  //
  // The diagnostic label is used in NODE_DEBUG_NATIVE output
  // to differentiate send operations. This should always be
  // a statically allocated string or nullptr (in which case
  // the value "unspecified" is used in the debug output).
  //
  // Instances of std::unique_ptr<QuicPacket> are moved through
  // QuicSocket and ultimately become the responsibility of the
  // SendWrap instance. When the SendWrap is cleaned up, the
  // QuicPacket instance will be freed.
  static inline std::unique_ptr<QuicPacket> Create(
      const char* diagnostic_label = nullptr,
      size_t len = MAX_PKTLEN);

  // Copy the data of the QuicPacket to a new one. Currently,
  // this is only used when retransmitting close connection
  // packets from a QuicServer.
  static inline std::unique_ptr<QuicPacket> Copy(
      const std::unique_ptr<QuicPacket>& other);

  QuicPacket(const char* diagnostic_label, size_t len);
  QuicPacket(const QuicPacket& other);

  uint8_t* data() { return data_; }

  size_t length() const { return len_; }

  uv_buf_t buf() const {
    return uv_buf_init(
      const_cast<char*>(reinterpret_cast<const char*>(&data_)),
      length());
  }

  inline void set_length(size_t len);

  const char* diagnostic_label() const;

  SET_NO_MEMORY_INFO();
  SET_MEMORY_INFO_NAME(QuicPacket);
  SET_SELF_SIZE(QuicPacket);

 private:
  uint8_t data_[MAX_PKTLEN];
  size_t len_ = MAX_PKTLEN;
  const char* diagnostic_label_ = nullptr;
};

// QuicEndpointListener listens to events generated by a QuicEndpoint.
class QuicEndpointListener {
 public:
  virtual void OnError(QuicEndpoint* endpoint, ssize_t error) = 0;
  virtual void OnReceive(
      ssize_t nread,
      AllocatedBuffer buf,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      unsigned int flags) = 0;
  virtual ReqWrap<uv_udp_send_t>* OnCreateSendWrap(size_t msg_size) = 0;
  virtual void OnSendDone(ReqWrap<uv_udp_send_t>* wrap, int status) = 0;
  virtual void OnBind(QuicEndpoint* endpoint) = 0;
  virtual void OnEndpointDone(QuicEndpoint* endpoint) = 0;
};

// A QuicEndpoint wraps a UDPBaseWrap. A single QuicSocket may
// have multiple QuicEndpoints, the lifecycles of which are
// attached to the QuicSocket.
class QuicEndpoint final : public BaseObject,
                           public UDPListener {
 public:
  static void Initialize(
    Environment* env,
    Local<Object> target,
    Local<Context> context);

  QuicEndpoint(
      QuicState* quic_state,
      Local<Object> wrap,
      QuicSocket* listener,
      Local<Object> udp_wrap);

  ~QuicEndpoint() override;

  SocketAddress local_address() const {
    return udp_->GetSockName();
  }

  // Implementation for UDPListener
  uv_buf_t OnAlloc(size_t suggested_size) override;

  void OnRecv(ssize_t nread,
              const uv_buf_t& buf,
              const sockaddr* addr,
              unsigned int flags) override;

  ReqWrap<uv_udp_send_t>* CreateSendWrap(size_t msg_size) override;

  void OnSendDone(ReqWrap<uv_udp_send_t>* wrap, int status) override;

  void OnAfterBind() override;

  inline int ReceiveStart();

  inline int ReceiveStop();

  inline int Send(
      uv_buf_t* buf,
      size_t len,
      const sockaddr* addr);

  void IncrementPendingCallbacks() { pending_callbacks_++; }
  void DecrementPendingCallbacks() { pending_callbacks_--; }
  bool has_pending_callbacks() const { return pending_callbacks_ > 0; }
  inline void WaitForPendingCallbacks();

  QuicState* quic_state() const { return quic_state_.get(); }

  SET_NO_MEMORY_INFO();
  SET_MEMORY_INFO_NAME(QuicEndpoint)
  SET_SELF_SIZE(QuicEndpoint)

 private:
  BaseObjectWeakPtr<QuicSocket> listener_;
  UDPWrapBase* udp_;
  BaseObjectPtr<AsyncWrap> strong_ptr_;
  size_t pending_callbacks_ = 0;
  bool waiting_for_callbacks_ = false;
  BaseObjectPtr<QuicState> quic_state_;
};

// QuicSocket manages the flow of data from the UDP socket to the
// QuicSession. It is responsible for managing the lifecycle of the
// UDP sockets, listening for new server QuicSession instances, and
// passing data two and from the remote peer.
class QuicSocket : public AsyncWrap,
                   public QuicEndpointListener,
                   public mem::NgLibMemoryManager<QuicSocket, ngtcp2_mem>,
                   public StatsBase<QuicSocketStatsTraits> {
 public:
  static void Initialize(
      Environment* env,
      Local<Object> target,
      Local<Context> context);

  QuicSocket(
      QuicState* quic_state,
      Local<Object> wrap,
      // A retry token should only be valid for a small window of time.
      // The retry_token_expiration specifies the number of seconds a
      // retry token is permitted to be valid.
      uint64_t retry_token_expiration,
      // To prevent malicious clients from opening too many concurrent
      // connections, we limit the maximum number per remote sockaddr.
      size_t max_connections,
      size_t max_connections_per_host,
      size_t max_stateless_resets_per_host
          = DEFAULT_MAX_STATELESS_RESETS_PER_HOST,
      uint32_t options = 0,
      QlogMode qlog = QlogMode::kDisabled,
      const uint8_t* session_reset_secret = nullptr,
      bool disable_session_reset = false);

  ~QuicSocket() override;

  // Returns the default/preferred local address. Additional
  // QuicEndpoint instances may be associated with the
  // QuicSocket bound to other local addresses.
  inline SocketAddress local_address() const;

  void MaybeClose();

  inline void AddSession(
      const QuicCID& cid,
      BaseObjectPtr<QuicSession> session);

  inline void AssociateCID(
      const QuicCID& cid,
      const QuicCID& scid);

  inline void DisassociateCID(
      const QuicCID& cid);

  inline void AssociateStatelessResetToken(
      const StatelessResetToken& token,
      BaseObjectPtr<QuicSession> session);

  inline void DisassociateStatelessResetToken(
      const StatelessResetToken& token);

  void Listen(
      BaseObjectPtr<crypto::SecureContext> context,
      const sockaddr* preferred_address = nullptr,
      const std::string& alpn = NGHTTP3_ALPN_H3,
      uint32_t options = 0);

  inline void ReceiveStart();

  inline void ReceiveStop();

  inline void RemoveSession(
      const QuicCID& cid,
      const SocketAddress& addr);

  inline void ReportSendError(int error);

  int SendPacket(
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      std::unique_ptr<QuicPacket> packet,
      BaseObjectPtr<QuicSession> session = BaseObjectPtr<QuicSession>());

#define V(id, name)                                                            \
  bool has_option_##name() const {                                             \
    return options_ & (1 << QUICSOCKET_OPTIONS_##id); }
  QUICSOCKET_OPTIONS(V)
#undef V

  // Allows the server busy status to be enabled from C++. A notification
  // will be sent to the JavaScript side informing that the status has
  // changed.
  inline void ServerBusy(bool on);

  inline void set_diagnostic_packet_loss(double rx = 0.0, double tx = 0.0);

  BaseObjectPtr<crypto::SecureContext> server_secure_context() const {
    return server_secure_context_;
  }

  QuicState* quic_state() { return quic_state_.get(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicSocket)
  SET_SELF_SIZE(QuicSocket)

  // Implementation for mem::NgLibMemoryManager
  void CheckAllocatedSize(size_t previous_size) const;

  void IncreaseAllocatedSize(size_t size);

  void DecreaseAllocatedSize(size_t size);

  const uint8_t* session_reset_secret() { return reset_token_secret_; }

  // Implementation for QuicListener
  ReqWrap<uv_udp_send_t>* OnCreateSendWrap(size_t msg_size) override;

  // Implementation for QuicListener
  void OnSendDone(ReqWrap<uv_udp_send_t>* wrap, int status) override;

  // Implementation for QuicListener
  void OnBind(QuicEndpoint* endpoint) override;

  // Implementation for QuicListener
  void OnReceive(
      ssize_t nread,
      AllocatedBuffer buf,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      unsigned int flags) override;

  // Implementation for QuicListener
  void OnError(QuicEndpoint* endpoint, ssize_t error) override;

  // Implementation for QuicListener
  void OnEndpointDone(QuicEndpoint* endpoint) override;

  // Serializes and transmits a RETRY packet to the connected peer.
  bool SendRetry(
      const QuicCID& dcid,
      const QuicCID& scid,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr);

  // Serializes and transmits a Stateless Reset to the connected peer.
  bool SendStatelessReset(
      const QuicCID& cid,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      size_t source_len);

  // Serializes and transmits a Version Negotiation packet to the
  // connected peer.
  void SendVersionNegotiation(
      uint32_t version,
      const QuicCID& dcid,
      const QuicCID& scid,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr);

  void PushListener(QuicSocketListener* listener);

  void RemoveListener(QuicSocketListener* listener);

  inline void AddEndpoint(
      BaseObjectPtr<QuicEndpoint> endpoint,
      bool preferred = false);

  void ImmediateConnectionClose(
      const QuicCID& scid,
      const QuicCID& dcid,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      int64_t reason = NGTCP2_INVALID_TOKEN);

 private:
  static void OnAlloc(
      uv_handle_t* handle,
      size_t suggested_size,
      uv_buf_t* buf);

  void OnSend(int status, QuicPacket* packet);

  inline void set_validated_address(const SocketAddress& addr);

  inline bool is_validated_address(const SocketAddress& addr) const;

  bool MaybeStatelessReset(
      const QuicCID& dcid,
      const QuicCID& scid,
      ssize_t nread,
      const uint8_t* data,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      unsigned int flags);

  BaseObjectPtr<QuicSession> AcceptInitialPacket(
      uint32_t version,
      const QuicCID& dcid,
      const QuicCID& scid,
      ssize_t nread,
      const uint8_t* data,
      const SocketAddress& local_addr,
      const SocketAddress& remote_addr,
      unsigned int flags);

  BaseObjectPtr<QuicSession> FindSession(const QuicCID& cid);

  inline void IncrementSocketAddressCounter(const SocketAddress& addr);

  inline void DecrementSocketAddressCounter(const SocketAddress& addr);

  inline void IncrementStatelessResetCounter(const SocketAddress& addr);

  inline size_t GetCurrentSocketAddressCounter(const SocketAddress& addr);

  inline size_t GetCurrentStatelessResetCounter(const SocketAddress& addr);

  // Returns true if, and only if, diagnostic packet loss is enabled
  // and the current packet should be artificially considered lost.
  inline bool is_diagnostic_packet_loss(double prob) const;

  ngtcp2_mem alloc_info_;

  std::vector<BaseObjectPtr<QuicEndpoint>> endpoints_;
  SocketAddress::Map<BaseObjectWeakPtr<QuicEndpoint>> bound_endpoints_;
  BaseObjectWeakPtr<QuicEndpoint> preferred_endpoint_;

  uint32_t flags_ = 0;
  uint32_t options_ = 0;
  uint32_t server_options_;

  AliasedStruct<QuicSocketState> state_;

  size_t max_connections_ = DEFAULT_MAX_CONNECTIONS;
  size_t max_connections_per_host_ = DEFAULT_MAX_CONNECTIONS_PER_HOST;
  size_t current_ngtcp2_memory_ = 0;
  size_t max_stateless_resets_per_host_ = DEFAULT_MAX_STATELESS_RESETS_PER_HOST;

  uint64_t retry_token_expiration_;

  // Used to specify diagnostic packet loss probabilities
  double rx_loss_ = 0.0;
  double tx_loss_ = 0.0;

  QuicSocketListener* listener_;
  JSQuicSocketListener default_listener_;
  QuicSessionConfig server_session_config_;
  QlogMode qlog_ = QlogMode::kDisabled;
  BaseObjectPtr<crypto::SecureContext> server_secure_context_;
  std::string server_alpn_;
  QuicCID::Map<BaseObjectPtr<QuicSession>> sessions_;
  QuicCID::Map<QuicCID> dcid_to_scid_;

  uint8_t token_secret_[kTokenSecretLen];
  uint8_t reset_token_secret_[NGTCP2_STATELESS_RESET_TOKENLEN];

  struct SocketAddressInfo {
    size_t active_connections;
    size_t reset_count;
    size_t retry_count;
    bool validated;
    uint64_t timestamp;
  };

  struct SocketAddressInfoTraits {
    using Type = SocketAddressInfo;

    static bool CheckExpired(const SocketAddress& address, const Type& type);
    static void Touch(const SocketAddress& address, Type* type);
  };

  SocketAddressLRU<SocketAddressInfoTraits> addrLRU_;

  StatelessResetToken::Map<QuicSession> token_map_;

  class SendWrap : public ReqWrap<uv_udp_send_t> {
   public:
    SendWrap(QuicState* quic_state,
             v8::Local<v8::Object> req_wrap_obj,
             size_t total_length_);

    void set_packet(std::unique_ptr<QuicPacket> packet) {
      packet_ = std::move(packet);
    }

    QuicPacket* packet() { return packet_.get(); }

    void set_session(BaseObjectPtr<QuicSession> session) { session_ = session; }

    size_t total_length() const { return total_length_; }

    QuicState* quic_state() { return quic_state_.get(); }

    SET_SELF_SIZE(SendWrap);
    std::string MemoryInfoName() const override;
    void MemoryInfo(MemoryTracker* tracker) const override;

   private:
    BaseObjectPtr<QuicSession> session_;
    std::unique_ptr<QuicPacket> packet_;
    size_t total_length_;
    BaseObjectPtr<QuicState> quic_state_;
  };

  SendWrap* last_created_send_wrap_ = nullptr;
  BaseObjectPtr<QuicState> quic_state_;

  friend class QuicSocketListener;
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_QUIC_NODE_QUIC_SOCKET_H_
