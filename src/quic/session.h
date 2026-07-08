#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <async_wrap.h>
#include <base_object.h>
#include <env.h>
#include <memory_tracker.h>
#include <ngtcp2/ngtcp2.h>
#include <node_http_common.h>
#include <node_sockaddr.h>
#include <timer_wrap.h>
#include <util.h>
#include <optional>
#include "bindingdata.h"
#include "cid.h"
#include "data.h"
#include "defs.h"
#include "packet.h"
#include "preferredaddress.h"
#include "sessionticket.h"
#include "streams.h"
#include "tlscontext.h"
#include "transportparams.h"

namespace node::quic {

class Endpoint;

// A Session represents one half of a persistent connection between two QUIC
// peers. Every Session is established first by performing a TLS handshake in
// which the client sends an initial packet to the server containing a TLS
// client hello. Once the TLS handshake has been completed, the Session can be
// used to open one or more Streams for the actual data flow back and forth.
//
// While client and server Sessions are created in slightly different ways,
// their lifecycles are generally identical:
//
// A Session is either acting as a Client or as a Server.
//
// Client Sessions are always created using Endpoint::Connect()
//
// Server Sessions are always created by an Endpoint receiving a valid initial
// request received from a remote client.
//
// As soon as Sessions of either type are created, they will immediately start
// working through the TLS handshake to establish the crypographic keys used to
// secure the communication. Once those keys are established, the Session can be
// used to open Streams. Based on how the Session is configured, any number of
// Streams can exist concurrently on a single Session.
//
// The Session wraps an ngtcp2_conn that is initialized when the session object
// is created. This ngtcp2_conn is destroyed when the session object is freed.
// However, the session can be in a closed/destroyed state and still have a
// valid ngtcp2_conn pointer. This is important because the ngtcp2 still might
// be processing data within the scope of an ngtcp2_conn after the session
// object itself is closed/destroyed by user code.
class Session final : public AsyncWrap, private SessionTicket::AppData::Source {
 public:
  SessionTicket::AppData::Source& ticket_app_data_source() { return *this; }
  // For simplicity, we use the same Application::Options struct for all
  // Application types. This may change in the future. Not all of the options
  // are going to be relevant for all Application types.
  struct Application_Options final : public MemoryRetainer {
    // The maximum number of header pairs permitted for a Stream.
    // Only relevant if the selected application supports headers.
    uint64_t max_header_pairs = DEFAULT_MAX_HEADER_LIST_PAIRS;

    // The maximum total number of header bytes (including header
    // name and value) permitted for a Stream.
    // Only relevant if the selected application supports headers.
    uint64_t max_header_length = DEFAULT_MAX_HEADER_LENGTH;

    // HTTP/3 specific options.
    // The maximum header section size advertised to the peer in SETTINGS.
    // Defaults to match max_header_length so the SETTINGS frame accurately
    // reflects the enforcement limit. A value of 0 would incorrectly tell
    // the peer not to send any headers at all.
    uint64_t max_field_section_size = DEFAULT_MAX_HEADER_LENGTH;
    uint64_t qpack_max_dtable_capacity = 4096;
    uint64_t qpack_encoder_max_dtable_capacity = 4096;
    uint64_t qpack_blocked_streams = 100;

    bool enable_connect_protocol = true;
    bool enable_datagrams = true;

    operator const nghttp3_settings() const;

    SET_NO_MEMORY_INFO()
    SET_MEMORY_INFO_NAME(Application::Options)
    SET_SELF_SIZE(Options)

    static v8::Maybe<Application_Options> From(Environment* env,
                                               v8::Local<v8::Value> value);

    std::string ToString() const;

    v8::MaybeLocal<v8::Object> ToObject(Environment* env) const;

    static const Application_Options kDefault;
  };

  // An Application implements the ALPN-protocol specific semantics on behalf
  // of a QUIC Session.
  class Application;

  // Decode the first ALPN protocol name from wire format (length-prefixed).
  static std::string_view DecodeAlpn(std::string_view wire);

  // Select the Application implementation based on the negotiated ALPN.
  // h3 (and h3-XX variants) map to Http3ApplicationImpl; all others map
  // to DefaultApplication. Sets the application_type state field.
  std::unique_ptr<Application> SelectApplicationFromAlpn(std::string_view alpn);

  // Install the Application on the session. Called at construction for
  // clients (ALPN known upfront) or from OnSelectAlpn for servers
  // (ALPN negotiated during handshake). Must be called before any
  // application data is received.
  void SetApplication(std::unique_ptr<Application> app);
  // Controls which datagram to drop when the pending datagram queue is full.
  enum class DatagramDropPolicy : uint8_t {
    DROP_OLDEST = 0,  // Drop the oldest queued datagram (default).
    DROP_NEWEST = 1,  // Drop the incoming datagram.
  };

  // The options used to configure a session. Most of these deal directly with
  // the transport parameters that are exchanged with the remote peer during
  // handshake.
  struct Options final : public MemoryRetainer {
    // The QUIC protocol version requested for the session.
    uint32_t version = NGTCP2_PROTO_VER_MAX;

    // Te minimum QUIC protocol version supported by this session.
    uint32_t min_version = NGTCP2_PROTO_VER_MIN;

    // By default a client session will ignore the preferred address
    // advertised by the the server. This option is only relevant for
    // client sessions.
    PreferredAddress::Policy preferred_address_strategy =
        PreferredAddress::Policy::IGNORE_PREFERRED;

    TransportParams::Options transport_params =
        TransportParams::Options::kDefault;
    TLSContext::Options tls_options = TLSContext::Options::kDefault;
    std::unordered_map<std::string, TLSContext::Options> sni;

    // A reference to the CID::Factory used to generate CID instances
    // for this session.
    const CID::Factory* cid_factory = &CID::Factory::random();
    // If the CID::Factory is a base object, we keep a reference to it
    // so that it cannot be garbage collected.
    BaseObjectPtr<BaseObject> cid_factory_ref;

    // Application-specific options (used for HTTP/3 if the negotiated
    // ALPN selects Http3ApplicationImpl).
    Application_Options application_options = Application_Options::kDefault;

    // When true, QLog output will be enabled for the session.
    bool qlog = false;

    // The amount of time (in milliseconds) that the endpoint will wait for the
    // completion of the TLS handshake. If the handshake does not complete
    // within this time, the session is closed. This prevents a peer from
    // holding a session open indefinitely in the handshake state, consuming
    // server resources (ngtcp2 connection, TLS state, JS objects) without
    // ever completing the connection. The default of 10 seconds is generous
    // enough to accommodate slow networks with retransmissions while still
    // bounding resource exposure. Set to UINT64_MAX to disable.
    static constexpr uint64_t DEFAULT_HANDSHAKE_TIMEOUT = 10'000;
    uint64_t handshake_timeout = DEFAULT_HANDSHAKE_TIMEOUT;

    // The initial round-trip time estimate in milliseconds. ngtcp2 uses this
    // for PTO computation, initial pacing, and early loss detection before
    // the first RTT sample is collected. The default of 0 uses ngtcp2's
    // built-in default of 333ms, which is appropriate for the general
    // internet. For low-latency environments (e.g., loopback or same-rack
    // deployments), setting a value closer to the actual RTT avoids
    // unnecessarily conservative initial behavior.
    uint64_t initial_rtt = 0;

    // The keep-alive timeout in milliseconds. When set to a non-zero value,
    // ngtcp2 will automatically send PING frames to keep the connection alive
    // before the idle timeout fires. Set to 0 to disable (default).
    uint64_t keep_alive_timeout = 0;

    // Maximum initial flow control window size for a stream.
    uint64_t max_stream_window = 0;

    // Maximum initial flow control window size for the connection.
    uint64_t max_window = 0;

    // The max_payload_size is the maximum size of a serialized QUIC packet. It
    // should always be set small enough to fit within a single MTU without
    // fragmentation. The default is set by the QUIC specification at 1200. This
    // value should not be changed unless you know for sure that the entire path
    // supports a given MTU without fragmenting at any point in the path.
    uint64_t max_payload_size = kDefaultMaxPacketLength;

    // The unacknowledged_packet_threshold is the maximum number of
    // unacknowledged packets that an ngtcp2 session will accumulate before
    // sending an acknowledgement. Setting this to 0 uses the ngtcp2 defaults,
    // which is what most will want. The value can be changed to fine tune some
    // of the performance characteristics of the session. This should only be
    // changed if you have a really good reason for doing so.
    uint64_t unacknowledged_packet_threshold = 0;

    // There are several common congestion control algorithms that ngtcp2 uses
    // to determine how it manages the flow control window: RENO, CUBIC, and
    // BBR. The details of how each works is not relevant here. The choice of
    // which to use by default is arbitrary and we can choose whichever we'd
    // like. Additional performance profiling will be needed to determine which
    // is the better of the two for our needs.
    ngtcp2_cc_algo cc_algorithm = CC_ALGO_CUBIC;

    // Controls which datagram to drop when the pending queue is full.
    DatagramDropPolicy datagram_drop_policy = DatagramDropPolicy::DROP_OLDEST;

    // Maximum number of SendPendingData attempts before a datagram is
    // abandoned. When a datagram cannot be sent due to congestion control
    // or packet size constraints, it remains in the queue and the counter
    // is incremented. Once the limit is reached, the datagram is dropped
    // and reported as abandoned. Range: 1-255. Default: 5.
    uint8_t max_datagram_send_attempts = 5;

    // Multiplier for the Probe Timeout (PTO) used to compute the draining
    // period duration after receiving CONNECTION_CLOSE. RFC 9000 Section
    // 10.2 requires at least 3x PTO. Range: 3-255. Default: 3.
    uint8_t draining_period_multiplier = 3;

    // The amount of time (in milliseconds) that a stream can be idle
    // (no data received) before it is automatically destroyed. This
    // protects against slowloris-style attacks where a peer opens streams
    // but never sends data, holding server resources indefinitely.
    // Only applies to peer-initiated streams. Set to 0 to disable.
    static constexpr uint64_t DEFAULT_STREAM_IDLE_TIMEOUT = 30'000;
    uint64_t stream_idle_timeout = DEFAULT_STREAM_IDLE_TIMEOUT;

    // An optional NEW_TOKEN from a previous connection to the same
    // server. When set, the token is included in the Initial packet
    // to skip address validation. Client-side only.
    std::optional<Store> token;

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Session::Options)
    SET_SELF_SIZE(Options)

    static v8::Maybe<Options> From(Environment* env,
                                   v8::Local<v8::Value> value);

    std::string ToString() const;
  };

  // The additional configuration settings used to create a specific session.
  // while the Options above can be used to configure multiple sessions, a
  // single Config is used to create a single session, which is why they are
  // kept separate.
  struct Config final : MemoryRetainer {
    // Is the Session acting as a client or a server?
    Side side;

    // The options to use for this session.
    Options options;

    // The actual QUIC version identified for this session.
    uint32_t version;

    SocketAddress local_address;
    SocketAddress remote_address;

    // The destination CID, identifying the remote peer. This value is always
    // provided by the remote peer.
    CID dcid = CID::kInvalid;

    // The source CID, identifying this session. This value is always created
    // locally.
    CID scid = CID::kInvalid;

    // Used only by client sessions to identify the original DCID
    // used to initiate the connection.
    CID ocid = CID::kInvalid;
    CID retry_scid = CID::kInvalid;
    CID preferred_address_cid = CID::kInvalid;

    ngtcp2_settings settings = {};
    operator ngtcp2_settings*() { return &settings; }
    operator const ngtcp2_settings*() const { return &settings; }

    Config(Environment* env,
           Side side,
           const Options& options,
           uint32_t version,
           const SocketAddress& local_address,
           const SocketAddress& remote_address,
           const CID& dcid,
           const CID& scid,
           const CID& ocid = CID::kInvalid);

    Config(Environment* env,
           const Options& options,
           const SocketAddress& local_address,
           const SocketAddress& remote_address,
           const CID& ocid = CID::kInvalid);

    void set_token(const uint8_t* token,
                   size_t len,
                   ngtcp2_token_type type = NGTCP2_TOKEN_TYPE_UNKNOWN);
    void set_token(const RetryToken& token);
    void set_token(const RegularToken& token);

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Session::Config)
    SET_SELF_SIZE(Config)

    std::string ToString() const;
  };

  JS_CONSTRUCTOR(Session);
  JS_BINDING_INIT_BOILERPLATE();

  static BaseObjectPtr<Session> Create(
      Endpoint* endpoint,
      const Config& config,
      TLSContext* tls_context,
      const std::optional<SessionTicket>& ticket);

  // Really should be private but MakeDetachedBaseObject needs visibility.
  Session(Endpoint* endpoint,
          v8::Local<v8::Object> object,
          const Config& config,
          TLSContext* tls_context,
          const std::optional<SessionTicket>& ticket);
  DISALLOW_COPY_AND_MOVE(Session)
  ~Session() override;

  bool is_destroyed() const;
  bool is_server() const;

  uint32_t version() const;
  Endpoint& endpoint() const;
  TLSSession& tls_session() const;
  bool has_application() const;
  Application& application() const;
  const Config& config() const;
  const Options& options() const;
  const SocketAddress& remote_address() const;
  const SocketAddress& local_address() const;

  std::string diagnostic_name() const override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Session)
  SET_SELF_SIZE(Session)

  operator ngtcp2_conn*() const;

  // Ensures that the session/application sends pending data when the scope
  // exits. Scopes can be nested. When nested, pending data will be sent
  // only when the outermost scope is exited.
  struct SendPendingDataScope final {
    Session* session;
    explicit SendPendingDataScope(Session* session);
    explicit SendPendingDataScope(const BaseObjectPtr<Session>& session);
    ~SendPendingDataScope();
    DISALLOW_COPY_AND_MOVE(SendPendingDataScope)
  };

  struct State;
  struct Stats;

  void HandleQlog(uint32_t flags, const void* data, size_t len);
  void EmitQlog(uint32_t flags, std::string_view data);

 private:
  struct Impl;

  using StreamsMap = std::unordered_map<stream_id, BaseObjectPtr<Stream>>;
  using QuicConnectionPointer = DeleteFnPtr<ngtcp2_conn, ngtcp2_conn_del>;

  struct PathValidationFlags final {
    bool preferredAddress = false;
  };

  struct DatagramReceivedFlags final {
    bool early = false;
  };

  bool Receive(const uint8_t* data,
               size_t len,
               const SocketAddress& local_address,
               const SocketAddress& remote_address,
               const PacketInfo& pkt_info = PacketInfo(),
               uint64_t ts = 0);

  // ReadPacket processes a single inbound packet through ngtcp2 without
  // triggering SendPendingData. This is the building block for batched
  // receive processing: the caller (Endpoint::Receive) accumulates
  // dirty sessions and a uv_check callback flushes them after all
  // packets in the I/O burst have been read.
  // Receive() is kept as a convenience wrapper that calls ReadPacket()
  // then triggers SendPendingData (for paths like Connect that need
  // immediate response).
  // The data pointer is used synchronously — ngtcp2_conn_read_pkt does
  // not retain a reference after returning, so the caller's buffer can
  // be reused immediately.
  // When ts is 0 (the default), uv_hrtime() is called internally.
  // The batched receive path caches a timestamp and passes it to all
  // ReadPacket() calls in the same I/O burst.
  bool ReadPacket(const uint8_t* data,
                  size_t len,
                  const SocketAddress& local_address,
                  const SocketAddress& remote_address,
                  const PacketInfo& pkt_info = PacketInfo(),
                  uint64_t ts = 0);

  // Called by BindingData's flush callback to trigger SendPendingData
  // on this session. Encapsulates the application() access so that
  // bindingdata.cc doesn't need the full Application type definition.
  void FlushPendingData();

  // Send a batch of packets accumulated by SendPendingData. Uses
  // Endpoint::SendBatch (uv_udp_try_send2 / sendmmsg) for synchronous
  // batched delivery when called from the deferred flush path.
  // Handles per-packet path updates and cross-endpoint redirects.
  // All Ptr entries are consumed (released or moved) on return.
  void SendBatch(Packet::Ptr* packets, PathStorage* paths, size_t count);

  void Send(Packet::Ptr packet);
  void Send(Packet::Ptr packet, const PathStorage& path);
  datagram_id SendDatagram(Store&& data);

  // Pending datagram accessors for use by SendPendingData.
  struct PendingDatagram {
    datagram_id id;
    Store data;
    uint8_t send_attempts = 0;
  };
  bool HasPendingDatagrams() const;
  PendingDatagram& PeekPendingDatagram();
  void PopPendingDatagram();
  size_t PendingDatagramCount() const;
  void DatagramSent(datagram_id id);

  // A non-const variation to allow certain modifications.
  Config& config();

  enum class CreateStreamOption : uint8_t {
    NOTIFY,
    DO_NOT_NOTIFY,
  };
  BaseObjectPtr<Stream> FindStream(stream_id id) const;
  // Returns a copy of the streams map (safe for iteration while streams
  // are being destroyed).
  StreamsMap streams() const;
  BaseObjectPtr<Stream> CreateStream(
      stream_id id,
      CreateStreamOption option = CreateStreamOption::NOTIFY,
      std::shared_ptr<DataQueue> data_source = nullptr);
  void AddStream(BaseObjectPtr<Stream> stream,
                 CreateStreamOption option = CreateStreamOption::NOTIFY);
  void RemoveStream(stream_id id);
  void ResumeStream(stream_id id);
  void StreamDataBlocked(stream_id id);
  void ShutdownStream(stream_id id, QuicError error = QuicError());
  void ShutdownStreamWrite(stream_id id, QuicError code = QuicError());

  // Use the configured CID::Factory to generate a new CID.
  CID new_cid(size_t len = CID::kMaxLength) const;

  const TransportParams local_transport_params() const;
  const TransportParams remote_transport_params() const;

  bool is_destroyed_or_closing() const;
  size_t max_packet_size() const;
  void set_priority_supported(bool on = true);

  // Open a new locally-initialized stream with the specified directionality.
  // If the session is not yet in a state where the stream can be openen --
  // such as when the handshake is not yet sufficiently far along and ORTT
  // session resumption is not being used -- then the stream will be created
  // in a pending state where actually opening the stream will be deferred.
  v8::MaybeLocal<v8::Object> OpenStream(
      Direction direction, std::shared_ptr<DataQueue> data_source = nullptr);

  void ExtendStreamOffset(stream_id id, size_t amount);
  void ExtendOffset(size_t amount);
  void SetLastError(QuicError&& error);
  uint64_t max_data_left() const;

  PendingStream::PendingStreamQueue& pending_bidi_stream_queue() const;
  PendingStream::PendingStreamQueue& pending_uni_stream_queue() const;

  // Implementation of SessionTicket::AppData::Source
  void CollectSessionTicketAppData(
      SessionTicket::AppData* app_data) const override;
  SessionTicket::AppData::Status ExtractSessionTicketAppData(
      const SessionTicket::AppData& app_data,
      SessionTicket::AppData::Source::Flag flag) override;

  // Returns true if the Session has entered the closing period after sending a
  // CONNECTION_CLOSE. While true, the Session is only permitted to transmit
  // CONNECTION_CLOSE frames until either the idle timeout period elapses or
  // until the Session is explicitly destroyed.
  bool is_in_closing_period() const;

  // Returns true if the Session has received a CONNECTION_CLOSE frame from the
  // peer. Once in the draining period, the Session is not permitted to send any
  // frames to the peer. The Session will be silently closed after either the
  // idle timeout period elapses or until the Session is explicitly destroyed.
  bool is_in_draining_period() const;

  // Returns false if the Session is currently in a state where it is unable to
  // transmit any packets.
  bool can_send_packets() const;

  // Returns false if the Session is currently in a state where it cannot create
  // new streams. Specifically, a stream is not in a state to create streams if
  // it has been destroyed or is closing.
  bool can_create_streams() const;

  // Returns false if the Session is currently in a state where it cannot open
  // a new locally-initiated stream. When using 0RTT session resumption, this
  // will become true immediately after the session ticket and transport params
  // have been configured. Otherwise, it becomes true after the remote transport
  // params and tx keys have been installed.
  bool can_open_streams() const;

  uint64_t max_local_streams_uni() const;
  uint64_t max_local_streams_bidi() const;

  bool wants_session_ticket() const;
  void SetStreamOpenAllowed();

  // Populate state buffer fields from the 0-RTT transport params.
  // Called after ngtcp2_conn_decode_and_set_0rtt_transport_params
  // succeeds, so that values like maxDatagramSize are available
  // before the handshake completes.
  void PopulateEarlyTransportParamsState();

  void set_hello_processed() { hello_processed_ = true; }

  // True once the negotiated TLS parameters (SNI, ALPN) are final.
  bool tls_info_ready() const;

  // It's a terrible name but "wrapped" here means that the Session has been
  // passed out to JavaScript and should be "wrapped" by whatever handler is
  // defined there to manage it.
  void set_wrapped();

  // True while JS emits must be held for later replay, before the handshake
  // is complete and the server session event has been emitted.
  bool must_defer_emits() const;

  // Replays, in order, any emits held while must_defer_emits() was true.
  // Called synchronously right after the new-session emit.
  void ReplayDeferredEmits();

  // Queues fn to be replayed by ReplayDeferredEmits(). Out-of-line so the
  // header does not need the full Impl definition.
  void QueueDeferredEmit(std::function<void()> fn);

  template <typename F>
  bool DeferEmit(F&& fn) {
    if (!must_defer_emits()) return false;
    QueueDeferredEmit(std::forward<F>(fn));
    return true;
  }

  enum class CloseMethod : uint8_t {
    // Immediate close with a roundtrip through JavaScript, causing all
    // currently opened streams to be closed. An attempt will be made to
    // send a CONNECTION_CLOSE frame to the peer. If closing while within
    // the ngtcp2 callback scope, sending the CONNECTION_CLOSE will be
    // deferred until the scope exits.
    DEFAULT,
    // Same as DEFAULT except that no attempt to notify the peer will be
    // made.
    SILENT,
    // Closing gracefully disables the ability to open or accept new streams
    // for this Session. Existing streams are allowed to close naturally on
    // their own.
    // Once called, the Session will be immediately closed once there are no
    // remaining streams. No notification is given to the connected peer that
    // we are in a graceful closing state. A CONNECTION_CLOSE will be sent
    // only once FinishClose() is called.
    GRACEFUL
  };
  // Initiate closing of the session.
  void Close(CloseMethod method = CloseMethod::DEFAULT);

  void FinishClose();
  void Destroy();

  // Close the session and send a connection close packet to the peer.
  // If creating the packet fails the session will be silently closed.
  // The connection close packet will use the value of last_error_ as
  // the error code transmitted to the peer.
  void SendConnectionClose();
  void OnTimeout();

  void UpdateTimer();
  // Has to be called after certain operations that generate packets.
  void UpdatePacketTxTime();
  void UpdateDataStats();
  void CheckStreamIdleTimeout(uint64_t now);
  void UpdatePath(const PathStorage& path);

  void ProcessPendingBidiStreams();
  void ProcessPendingUniStreams();

  // JavaScript callouts

  void EmitClose(const QuicError& error = QuicError());
  void EmitGoaway(stream_id last_stream_id);

  // Sets the max datagram payload size in the shared state. Used by
  // Http3ApplicationImpl to block datagram sends when the peer's
  // SETTINGS_H3_DATAGRAM=0 (RFC 9297 §3).
  void set_max_datagram_size(uint16_t size);
  void EmitDatagram(Store&& datagram, DatagramReceivedFlags flag);
  void EmitDatagramStatus(datagram_id id, DatagramStatus status);
  void EmitHandshakeComplete();
  void EmitKeylog(const char* line);
  void EmitOrigins(std::vector<std::string>&& origins);

  struct ValidatedPath {
    std::shared_ptr<SocketAddress> local;
    std::shared_ptr<SocketAddress> remote;
  };

  void EmitPathValidation(PathValidationResult result,
                          PathValidationFlags flags,
                          const ValidatedPath& newPath,
                          const std::optional<ValidatedPath>& oldPath);
  void EmitSessionTicket(Store&& ticket);
  void EmitNewToken(const uint8_t* token, size_t len);
  void EmitEarlyDataRejected();
  void DestroyAllStreams(const QuicError& error);
  void EmitStream(const BaseObjectWeakPtr<Stream>& stream);
  void EmitVersionNegotiation(const ngtcp2_pkt_hd& hd,
                              const uint32_t* sv,
                              size_t nsv);
  void EmitApplication();
  void DatagramStatus(datagram_id datagramId, DatagramStatus status);
  void DatagramReceived(const uint8_t* data,
                        size_t datalen,
                        DatagramReceivedFlags flag);
  void GenerateNewConnectionId(ngtcp2_cid* cid,
                               size_t len,
                               ngtcp2_stateless_reset_token* token);
  bool HandshakeCompleted();
  void HandshakeConfirmed();
  void SelectPreferredAddress(PreferredAddress* preferredAddress);

  QuicConnectionPointer InitConnection();

  Side side_;
  const ngtcp2_mem* allocator_;
  std::unique_ptr<Impl> impl_;

  struct Flags {
    // These flags live on Session (not Impl) so that the NgTcp2CallbackScope
    // and NgHttp3CallbackScope destructors can safely clear them even after
    // Impl has been destroyed via MakeCallback re-entrancy during a callback.
    // The scope is placed at the ngtcp2/nghttp3 entry point (e.g. Receive,
    // OnTimeout) rather than on individual callbacks, so the deferred destroy
    // only fires after all callbacks for that entry point have completed.
    uint8_t in_ngtcp2_callback_scope : 1 = 0;
    uint8_t in_nghttp3_callback_scope : 1 = 0;
    uint8_t destroy_deferred : 1 = 0;
    // Set when this session is in BindingData's pending_flush_sessions_ vector.
    // Cleared by the flush callback before calling SendPendingData.
    // Provides O(1) dedup so a session receiving multiple packets in one I/O
    // burst is only scheduled for flush once.
    uint8_t pending_flush : 1 = 0;
    // When true, Session::Send prefers synchronous delivery via
    // Endpoint::SendOrTrySend (uv_udp_try_send with async fallback).
    // Set during FlushPendingData to avoid the one-tick latency of
    // async-only sends from the uv_check callback.
    uint8_t prefer_try_send : 1 = 0;
  };
  Flags flags_;

  bool hello_processed_ = false;

  QuicConnectionPointer connection_;
  std::unique_ptr<TLSSession> tls_session_;
  friend struct NgTcp2CallbackScope;
  friend struct NgHttp3CallbackScope;
  friend class Application;
  friend class BindingData;
  friend class DefaultApplication;
  friend class Http3ApplicationImpl;
  friend class Endpoint;
  friend class SessionManager;
  friend class Stream;
  friend class PendingStream;
  friend class TLSContext;
  friend class TLSSession;
  friend class TransportParams;
  friend struct Impl;
  friend struct SendPendingDataScope;
};

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
