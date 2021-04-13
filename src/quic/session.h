#ifndef SRC_QUIC_SESSION_H_
#define SRC_QUIC_SESSION_H_
#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "quic/buffer.h"
#include "quic/crypto.h"
#include "quic/stats.h"
#include "quic/stream.h"
#include "quic/quic.h"
#include "aliased_struct.h"
#include "async_wrap.h"
#include "base_object.h"
#include "crypto/crypto_context.h"
#include "env.h"
#include "node_http_common.h"
#include "node_sockaddr.h"
#include "node_worker.h"
#include "timer_wrap.h"

#include <ngtcp2/ngtcp2.h>
#include <v8.h>
#include <uv.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace node {
namespace quic {

#define SESSION_STATS(V)                                                       \
  V(CREATED_AT, created_at, "Created at")                                      \
  V(HANDSHAKE_COMPLETED_AT, handshake_completed_at, "Handshake completed")     \
  V(HANDSHAKE_CONFIRMED_AT, handshake_confirmed_at, "Handshake confirmed")     \
  V(SENT_AT, sent_at, "Last sent at")                                          \
  V(RECEIVED_AT, received_at, "Last received at")                              \
  V(CLOSING_AT, closing_at, "Closing")                                         \
  V(DESTROYED_AT, destroyed_at, "Destroyed at")                                \
  V(BYTES_RECEIVED, bytes_received, "Bytes received")                          \
  V(BYTES_SENT, bytes_sent, "Bytes sent")                                      \
  V(BIDI_STREAM_COUNT, bidi_stream_count, "Bidi stream count")                 \
  V(UNI_STREAM_COUNT, uni_stream_count, "Uni stream count")                    \
  V(STREAMS_IN_COUNT, streams_in_count, "Streams in count")                    \
  V(STREAMS_OUT_COUNT, streams_out_count, "Streams out count")                 \
  V(KEYUPDATE_COUNT, keyupdate_count, "Key update count")                      \
  V(LOSS_RETRANSMIT_COUNT, loss_retransmit_count, "Loss retransmit count")     \
  V(MAX_BYTES_IN_FLIGHT, max_bytes_in_flight, "Max bytes in flight")           \
  V(BLOCK_COUNT, block_count, "Block count")                                   \
  V(BYTES_IN_FLIGHT, bytes_in_flight, "Bytes in flight")                       \
  V(CONGESTION_RECOVERY_START_TS,                                              \
    congestion_recovery_start_ts,                                              \
    "Congestion recovery start time")                                          \
  V(CWND, cwnd, "Size of the congestion window")                               \
  V(DELIVERY_RATE_SEC, delivery_rate_sec, "Delivery bytes/sec")                \
  V(FIRST_RTT_SAMPLE_TS, first_rtt_sample_ts, "First RTT sample time")         \
  V(INITIAL_RTT, initial_rtt, "Initial RTT")                                   \
  V(LAST_TX_PKT_TS, last_tx_pkt_ts, "Last TX packet time")                     \
  V(LATEST_RTT, latest_rtt, "Latest RTT")                                      \
  V(LOSS_DETECTION_TIMER,                                                      \
    loss_detection_timer,                                                      \
    "Loss detection timer deadline")                                           \
  V(LOSS_TIME, loss_time, "Loss time")                                         \
  V(MAX_UDP_PAYLOAD_SIZE, max_udp_payload_size, "Max UDP payload size")        \
  V(MIN_RTT, min_rtt, "Minimum RTT so far")                                    \
  V(PTO_COUNT, pto_count, "PTO count")                                         \
  V(RTTVAR, rttvar, "Mean deviation of observed RTT")                          \
  V(SMOOTHED_RTT, smoothed_rtt, "Smoothed RTT")                                \
  V(SSTHRESH, ssthresh, "Slow start threshold")                                \
  V(RECEIVE_RATE, receive_rate, "Receive Rate / Sec")                          \
  V(SEND_RATE, send_rate, "Send Rate  Sec")

// Every Session instance maintains an AliasedStruct that is used to quickly
// toggle certain settings back and forth or to access various details with
// lower cost.
#define SESSION_STATE(V)                                                       \
  V(CLIENT_HELLO, client_hello, uint8_t)                                       \
  V(CLIENT_HELLO_DONE, client_hello_done, uint8_t)                             \
  V(CLOSING, closing, uint8_t)                                                 \
  V(CLOSING_TIMER_ENABLED, closing_timer_enabled, uint8_t)                     \
  V(CONNECTION_CLOSE_SCOPE, in_connection_close_scope, uint8_t)                \
  V(DESTROYED, destroyed, uint8_t)                                             \
  V(GRACEFUL_CLOSING, graceful_closing, uint8_t)                               \
  V(HANDSHAKE_CONFIRMED, handshake_confirmed, uint8_t)                         \
  V(IDLE_TIMEOUT, idle_timeout, uint8_t)                                       \
  V(NGTCP2_CALLBACK, in_ngtcp2_callback, uint8_t)                              \
  V(OCSP, ocsp, uint8_t)                                                       \
  V(OCSP_DONE, ocsp_done, uint8_t)                                             \
  V(STATELESS_RESET, stateless_reset, uint8_t)                                 \
  V(SILENT_CLOSE, silent_close, uint8_t)                                       \
  V(STREAM_OPEN_ALLOWED, stream_open_allowed, uint8_t)                         \
  V(TRANSPORT_PARAMS_SET, transport_params_set, uint8_t)                       \
  V(USING_PREFERRED_ADDRESS, using_preferred_address, uint8_t)                 \
  V(WRAPPED, wrapped, uint8_t)

class Endpoint;
class EndpointWrap;
class LogStream;
class Session;

using StreamsMap = std::unordered_map<stream_id, BaseObjectPtr<Stream>>;

using PreferredAddressStrategy = void(*)(Session*, const PreferredAddress&);
using ConnectionCloseFn =
    ssize_t(*)(
        ngtcp2_conn* conn,
        ngtcp2_path* path,
        ngtcp2_pkt_info* pi,
        uint8_t* dest,
        size_t destlen,
        uint64_t error_code,
        ngtcp2_tstamp ts);


static const int kInitialClientBufferLength = 4096;

#define V(name, _, __) IDX_STATS_SESSION_##name,
enum SessionStatsIdx : int {
  SESSION_STATS(V)
  IDX_STATS_SESSION_COUNT
};
#undef V

#define V(name, _, __) IDX_STATE_SESSION_##name,
enum SessionStateIdx {
  SESSION_STATE(V)
  IDX_STATE_SESSION_COUNT
};
#undef V


#define V(_, name, __) uint64_t name;
struct SessionStats final {
  SESSION_STATS(V)
};
#undef V

using SessionStatsBase = StatsBase<StatsTraitsImpl<SessionStats, Session>>;

// A Session is a persistent connection between two QUIC peers,
// one acting as a server, the other acting as a client. Every
// Session is established first by performing a TLS 1.3 handshake
// in which the client sends an initial packet to the server
// containing a TLS client hello. Once the TLS handshake has
// been completed, the Session can be used to open one or more
// Streams for the actual data flow back and forth.
class Session final : public AsyncWrap,
                      public SessionStatsBase {
 public:
  class Application;

  // Used only by client Sessions, this PreferredAddressStrategy
  // ignores the server provided preference communicated via the
  // transport parameters.
  static void IgnorePreferredAddressStrategy(
      Session* session,
      const PreferredAddress& preferred_address);

  // Used only by client Sessions, this PreferredAddressStrategy
  // uses the server provided preference that matches the local
  // port type (IPv4 or IPv6) used by the Endpoint. That is, if
  // the Endpoint is IPv4, and the server advertises an IPv4
  // preferred address, then that preference will be used. Otherwise,
  // the preferred address preference is ignored.
  static void UsePreferredAddressStrategy(
      Session* session,
      const PreferredAddress& preferred_address);

  // An Application encapsulates the ALPN-identified application
  // specific semantics associated with the Session. The Application
  // class itself is an abstract base class that must be specialized.
  // Every Session has exactly one associated Application that is
  // selected using the ALPN identifier when the Session is created.
  // Once selected, the Session will defer many actions to be handled
  // by the Application.
  class Application : public MemoryRetainer {
   public:
    // A base class for configuring the Application. Specific
    // Application subclasses may extend this with additional
    // configuration properties.
    struct Options {
      // The maximum number of header pairs permitted for a Stream.
      uint64_t max_header_pairs = 65535;

      // The maximum total number of header bytes (including header
      // name and value) permitted for a Stream.
      uint64_t max_header_length = 65535;

      Options() = default;
      Options(const Options& other) noexcept;
    };

    Application(
        Session* session,
        const std::shared_ptr<Options>& options);

    Application(const Application& other) = delete;
    Application(Application&& other) = delete;
    Application& operator=(const Application& other) = delete;
    Application& operator=(Application&& other) = delete;

    virtual ~Application() = default;

    // The session will call initialize as soon as the TLS secrets
    // have been set.
    virtual bool Initialize();

    // Session will forward all received stream data immediately
    // on to the Application. The only additional processing the
    // Session does is to automatically adjust the session-level
    // flow control window. It is up to the Application to do
    // the same for the Stream-level flow control.
    //
    // flags are passed on directly from ngtcp2. The most important
    // of which here is NGTCP2_STREAM_DATA_FLAG_FIN, which indicates
    // that this is the final chunk of data that the peer will send
    // for this stream.
    //
    // It is also possible for the NGTCP2_STREAM_DATA_FLAG_EARLY flag
    // to be set, indicating that this chunk of data was received in
    // a 0RTT packet before the TLS handshake completed. This would
    // indicate that it is not as secure and could be replayed by
    // an attacker. We're not currently making use of that flag.
    virtual bool ReceiveStreamData(
        uint32_t flags,
        stream_id id,
        const uint8_t* data,
        size_t datalen,
        uint64_t offset) = 0;

    // Session will forward all data acknowledgements for a stream to
    // the Application.
    virtual void AcknowledgeStreamData(
        stream_id id,
        uint64_t offset,
        size_t datalen) {
      Acknowledge(id, offset, datalen);
    }

    // Called to determine if a Header can be added to this application.
    // Applications that do not support headers will always return false.
    virtual bool CanAddHeader(
        size_t current_count,
        size_t current_headers_length,
        size_t this_header_length) {
      return false;
    }

    // Called to mark the identified stream as being blocked. Not all
    // Application types will support blocked streams, and those that
    // do will do so differently. The default implementation here is
    // to simply acknowledge the notification.
    virtual bool BlockStream(stream_id id) { return true; }

    // Called when the Session determines that the maximum number of
    // remotely-initiated unidirectional streams has been extended.
    // Not all Application types will require this notification so
    // the default is to do nothing.
    virtual void ExtendMaxStreamsRemoteUni(uint64_t max_streams) {}

    // Called when the Session determines that the maximum number of
    // remotely-initiated bidirectional streams has been extended.
    // Not all Application types will require this notification so
    // the default is to do nothing.
    virtual void ExtendMaxStreamsRemoteBidi(uint64_t max_streams) {}

    // Called when the Session determines that the flow control window
    // for the given stream has been expanded. Not all Application types
    // will require this notification so the default is to do nothing.
    virtual void ExtendMaxStreamData(stream_id id, uint64_t max_data) {}

    // Called when the session determines that there is outbound data
    // available to send for the given stream.
    virtual void ResumeStream(stream_id id) {}

    // Different Applications may wish to set some application data in
    // the session ticket (e.g. http/3 would set server settings in the
    // application data). By default, there's nothing to set.
    virtual void SetSessionTicketAppData(
        const SessionTicketAppData& app_data) {}

    // Different Applications may set some application data in
    // the session ticket (e.g. http/3 would set server settings in the
    // application data). By default, there's nothing to get.
    virtual SessionTicketAppData::Status GetSessionTicketAppData(
        const SessionTicketAppData& app_data,
        SessionTicketAppData::Flag flag) {
      return flag == SessionTicketAppData::Flag::STATUS_RENEW ?
        SessionTicketAppData::Status::TICKET_USE_RENEW :
        SessionTicketAppData::Status::TICKET_USE;
    }

    // Notifies the Application that the identified stream has
    // been closed.
    virtual void StreamClose(stream_id id, error_code app_error_code);

    // Notifies the Application that the identified stream has
    // been reset.
    virtual void StreamReset(stream_id id, error_code app_error_code);

    // Submits an outbound block of headers for the given stream.
    // Not all Application types will support headers, in which
    // case this function should return false.
    virtual bool SendHeaders(
        stream_id id,
        Stream::HeadersKind kind,
        const v8::Local<v8::Array>& headers,
        Stream::SendHeadersFlags flags) {
      return false;
    }

    inline Environment* env() const { return session_->env(); }
    inline Session* session() const { return session_.get(); }

    inline const Options& options() const { return *(options_.get()); }

    // Signals to the Application that it should serialize and transmit
    // any pending session and stream packets it has accumulated.
    bool SendPendingData();

   protected:
    std::unique_ptr<Packet> CreateStreamDataPacket();

    struct StreamData final {
      size_t count = 0;
      size_t remaining = 0;
      stream_id id = -1;
      int fin = 0;
      ngtcp2_vec data[kMaxVectorCount] {};
      ngtcp2_vec* buf = nullptr;
      BaseObjectPtr<Stream> stream;
      StreamData() { buf = data; }
    };

    void Acknowledge(stream_id id, uint64_t offset, size_t datalen);
    virtual int GetStreamData(StreamData* data) = 0;
    virtual bool StreamCommit(StreamData* data, size_t datalen) = 0;
    virtual bool ShouldSetFin(const StreamData& data) = 0;

    ssize_t WriteVStream(
        PathStorage* path,
        uint8_t* buf,
        ssize_t* ndatalen,
        const StreamData& stream_data);

   private:
    BaseObjectWeakPtr<Session> session_;
    std::shared_ptr<Options> options_;
    bool needs_init_ = true;
  };

  // A utility that wraps the configuration settings for the
  // Session and the underlying ngtcp2_conn. This struct is
  // created when a new Client or Server session is created.
  struct Config final : public ngtcp2_settings {
    // The QUIC protocol version requested for the Session.
    quic_version version;

    // The initial destination CID.
    CID dcid;

    // The locally selected source CID.
    CID scid;

    // The original CID (if any).
    CID ocid;

    Config(
        Endpoint* endpoint,
        const CID& dcid,
        const CID& scid,
        quic_version version = NGTCP2_PROTO_VER_MAX);

    Config(
        Endpoint* endpoint,
        quic_version version = NGTCP2_PROTO_VER_MAX);

    void EnableQLog(const CID& ocid = CID());
  };

  // The Options struct contains all of the usercode specified
  // options for the session. Most of the options correlate to
  // the transport parameters that are communicated to the remote
  // peer once the session is created.
  struct Options final : public MemoryRetainer {
    // The protocol identifier to be used by this Session.
    std::string alpn = NGHTTP3_ALPN_H3;

    // The SNI hostname to be used. This is used only by client
    // Sessions to identify the SNI host in the TLS client hello
    // message.
    std::string hostname = "";

    RoutableConnectionIDConfig* cid_strategy;
    BaseObjectPtr<BaseObject> cid_strategy_strong_ref;

    CID dcid {};
    CID scid {};

    PreferredAddressStrategy preferred_address_strategy =
        UsePreferredAddressStrategy;

    bool qlog = false;

    // Set only on server Sessions, the preferred address communicates
    // the IP address and port that the server would prefer the client
    // to use when communicating with it. See the QUIC specification for
    // more detail on how the preferred address mechanism works.
    std::shared_ptr<SocketAddress> preferred_address_ipv4;
    std::shared_ptr<SocketAddress> preferred_address_ipv6;

    // The initial size of the flow control window of locally initiated
    // streams. This is the maximum number of bytes that the *remote*
    // endpoint can send when the connection is started.
    uint64_t initial_max_stream_data_bidi_local =
        DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL;

    // The initial size of the flow control window of remotely initiated
    // streams. This is the maximum number of bytes that the remote endpoint
    // can send when the connection is started.
    uint64_t initial_max_stream_data_bidi_remote =
        DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE;

    // The initial size of the flow control window of remotely initiated
    // unidirectional streams. This is the maximum number of bytes that
    // the remote endpoint can send when the connection is started.
    uint64_t initial_max_stream_data_uni = DEFAULT_MAX_STREAM_DATA_UNI;

    // The initial size of the session-level flow control window.
    uint64_t initial_max_data = DEFAULT_MAX_DATA;

    // The initial maximum number of concurrent bidirectional streams
    // the remote endpoint is permitted to open.
    uint64_t initial_max_streams_bidi = DEFAULT_MAX_STREAMS_BIDI;

    // The initial maximum number of concurrent unidirectional streams
    // the remote endpoint is permitted to open.
    uint64_t initial_max_streams_uni = DEFAULT_MAX_STREAMS_UNI;

    // The maximum amount of time that a Session is permitted to remain
    // idle before it is silently closed and state is discarded.
    uint64_t max_idle_timeout = DEFAULT_MAX_IDLE_TIMEOUT;

    // The maximum number of Connection IDs that the peer can store.
    // A single Session may have several connection IDs over it's lifetime.
    uint64_t active_connection_id_limit = DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;

    // Establishes the exponent used in ACK Delay field in the ACK frame.
    // See the QUIC specification for details. This is an advanced option
    // that should rarely be modified and only if there is really good reason.
    uint64_t ack_delay_exponent = NGTCP2_DEFAULT_ACK_DELAY_EXPONENT;

    // The maximum amount of time by which the endpoint will delay sending
    // acknowledgements. This is an advanced option that should rarely be
    // modified and only if there is a really good reason. It is used to
    // determine how long a Session will wait to determine that a packet
    // has been lost.
    uint64_t max_ack_delay = NGTCP2_DEFAULT_MAX_ACK_DELAY;

    // The maximum size of DATAGRAM frames that the endpoint will accept.
    // Setting the value to 0 will disable DATAGRAM support.
    uint64_t max_datagram_frame_size = NGTCP2_DEFAULT_MAX_PKTLEN;

    // When true, TLS keylog data will be emitted to the JavaScript
    // session object.
    bool keylog = false;

    // When true, communicates that the Session does not support active
    // connection migration. See the QUIC specification for more details
    // on connection migration.
    bool disable_active_migration = false;

    // When set, the peer certificate is verified against
    // the list of supplied CAs. If verification fails, the
    // connection will be refused.
    bool reject_unauthorized = true;

    // When set, the clienthello event will be emitted on the
    // Session to allow user code an opportunity to provide a
    // different SecureContext based on alpn, SNI servername,
    // and ciphers.
    bool client_hello = false;

    // When set, enables TLS tracing for the session.
    // This should only be used for debugging.
    bool enable_tls_trace = false;

    // Options only used by server sessions:

    // When set, instructs the server session to request a
    // client authentication certificate.
    bool request_peer_certificate = false;

    // Options pnly used by client sessions:

    // When set, instructs the client session to include an
    // OCSP request in the initial TLS handshake. For server
    // sessions, instructs the session not to ignore ocsp requests.
    bool ocsp = false;

    // When set, instructs the client session to verify the
    // hostname default. This is required by QUIC and enabled
    // by default. We allow disabling it only for debugging.
    bool verify_hostname_identity = true;

    // True if a PSK callback was given.
    bool psk_callback_present = false;

    // The TLS session ID context (only used on the server)
    std::string session_id_ctx = "node.js quic server";

    std::shared_ptr<Application::Options> application;

    Options() = default;
    Options(const Options& other) noexcept;

    inline Options& operator=(const Options& other) noexcept {
      if (this == &other) return *this;
      this->~Options();
      return *new(this) Options(other);
    }

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Session::Options)
    SET_SELF_SIZE(Options)
  };

  #define V(_, name, type) type name;
  struct State final {
    SESSION_STATE(V)
  };
  #undef V

  // A utility struct used to prepare the ngtcp2_transport_params
  // when creating a new Session.
  struct TransportParams final : public ngtcp2_transport_params {
    TransportParams(
        const std::shared_ptr<Options>& options,
        const CID& scid = CID(),
        const CID& ocid = CID());

    void SetPreferredAddress(const std::shared_ptr<SocketAddress>& address);
    void GenerateStatelessResetToken(EndpointWrap* endpoint, const CID& cid);
    void GeneratePreferredAddressToken(
        RoutableConnectionIDStrategy* connection_id_strategy,
        Session* session,
        CID* pscid);
  };

  // Every Session has exactly one CryptoContext that maintains the
  // state of the TLS handshake and negotiated cipher keys after the
  // handshake has been completed. It is separated out from the main
  // Session class only as a convenience to help make the code more
  // maintainable and understandable.
  class CryptoContext final : public MemoryRetainer {
   public:
    CryptoContext(
        Session* session,
        const std::shared_ptr<Options>& options,
        const BaseObjectPtr<crypto::SecureContext>& context,
        ngtcp2_crypto_side side);
    ~CryptoContext() override;

    // Outgoing crypto data must be retained in memory until it is
    // explicitly acknowledged. AcknowledgeCryptoData will be invoked
    // when ngtcp2 determines that it has received an acknowledgement
    // for crypto data at the specified level. This is our indication
    // that the data for that level can be released.
    void AcknowledgeCryptoData(ngtcp2_crypto_level level, size_t datalen);

    // Cancels the TLS handshake and returns the number of unprocessed
    // bytes that were still in the queue when canceled.
    size_t Cancel();

    void Initialize();

    // Returns the server's prepared OCSP response for transmission
    // (if any). The shared_ptr will be empty if there was an error
    // or if no OCSP response was provided. If release is true, the
    // internal std::shared_ptr will be reset.
    std::shared_ptr<v8::BackingStore> ocsp_response(bool release = true);

    // Returns ngtcp2's understanding of the current inbound crypto level
    ngtcp2_crypto_level read_crypto_level() const;

    // Returns ngtcp2's understanding of the current outbound crypto level
    ngtcp2_crypto_level write_crypto_level() const;

    // TLS Keylogging is enabled per-Session by attaching an handler to the
    // "keylog" event. Each keylog line is emitted to JavaScript where it can
    // be routed to whatever destination makes sense. Typically, this will be
    // to a keylog file that can be consumed by tools like Wireshark to
    // intercept and decrypt QUIC network traffic.
    void Keylog(const char* line);

    int OnClientHello();

    void OnClientHelloDone(BaseObjectPtr<crypto::SecureContext> context);

    // The OnCert callback provides an opportunity to prompt the server to
    // perform on OCSP request on behalf of the client (when the client
    // requests it). If there is a listener for the 'OCSPRequest' event
    // on the JavaScript side, the IDX_QUIC_SESSION_STATE_CERT_ENABLED
    // session state slot will equal 1, which will cause the callback to
    // be invoked. The callback will be given a reference to a JavaScript
    // function that must be called in order for the TLS handshake to
    // continue.
    int OnOCSP();

    // The OnOCSP function is called by the OnOCSPDone
    // function when usercode is done handling the OCSP request
    void OnOCSPDone(std::shared_ptr<v8::BackingStore> ocsp_response);

    // At this point in time, the TLS handshake secrets have been
    // generated by openssl for this end of the connection and are
    // ready to be used. Within this function, we need to install
    // the secrets into the ngtcp2 connection object, store the
    // remote transport parameters, and begin initialization of
    // the Application that was selected.
    bool OnSecrets(
        ngtcp2_crypto_level level,
        const uint8_t* rx_secret,
        const uint8_t* tx_secret,
        size_t secretlen);

    // When the client has requested OSCP, this function will be called to
    // provide the OSCP response. The OnOSCP() callback should have already
    // been called by this point if any data is to be provided. If it hasn't,
    // and ocsp_response_ is empty, no OCSP response will be sent.
    int OnTLSStatus();

    // Called by ngtcp2 when a chunk of peer TLS handshake data is received.
    // For every chunk, we move the TLS handshake further along until it
    // is complete.
    int Receive(
        ngtcp2_crypto_level crypto_level,
        uint64_t offset,
        const uint8_t* data,
        size_t datalen);

    void ResumeHandshake();

    v8::MaybeLocal<v8::Object> cert(Environment* env) const;
    v8::MaybeLocal<v8::Object> peer_cert(Environment* env) const;
    v8::MaybeLocal<v8::Value> cipher_name(Environment* env) const;
    v8::MaybeLocal<v8::Value> cipher_version(Environment* env) const;
    v8::MaybeLocal<v8::Object> ephemeral_key(Environment* env) const;
    v8::MaybeLocal<v8::Array> hello_ciphers(Environment* env) const;
    v8::MaybeLocal<v8::Value> hello_servername(Environment* env) const;
    v8::MaybeLocal<v8::Value> hello_alpn(Environment* env) const;
    std::string servername() const;

    std::string selected_alpn() const;

    void set_tls_alert(int err);

    // Write outbound TLS handshake data into the ngtcp2 connection
    // to prepare it to be serialized. The outbound data must be
    // stored in the handshake_ until it is acknowledged by the
    // remote peer. It's important to keep in mind that there is
    // a potential security risk here -- that is, a malicious peer
    // can cause the local session to keep sent handshake data in
    // memory by failing to acknowledge it or slowly acknowledging
    // it. We currently do not track how much data is being buffered
    // here but we do record statistics on how long the handshake
    // data is foreced to be kept in memory.
    void WriteHandshake(
        ngtcp2_crypto_level level,
        const uint8_t* data,
        size_t datalen);

    // Triggers key update to begin. This will fail and return false
    // if either a previous key update is in progress and has not been
    // confirmed or if the initial handshake has not yet been confirmed.
    bool InitiateKeyUpdate();

    int VerifyPeerIdentity();
    void EnableTrace();

    inline Session* session() const { return session_; }
    inline ngtcp2_crypto_side side() const { return side_; }

    bool early_data() const;

    inline bool enable_tls_trace() const {
      return options_->enable_tls_trace;
    }

    inline bool reject_unauthorized() const {
      return options_->reject_unauthorized;
    }

    inline bool request_ocsp() const {
      return options_->ocsp;
    }

    inline bool request_peer_certificate() const {
      return options_->request_peer_certificate;
    }

    inline bool verify_hostname_identity() const {
      return options_->verify_hostname_identity;
    }

    void MaybeSetEarlySession(
        const crypto::ArrayBufferOrViewContents<uint8_t>& session_ticket,
        const crypto::ArrayBufferOrViewContents<uint8_t>&
            remote_transport_params);

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(CryptoContext)
    SET_SELF_SIZE(CryptoContext)

   private:
    bool SetSecrets(
        ngtcp2_crypto_level level,
        const uint8_t* rx_secret,
        const uint8_t* tx_secret,
        size_t secretlen);

    Session* session_;
    BaseObjectPtr<crypto::SecureContext> secure_context_;
    ngtcp2_crypto_side side_;
    std::shared_ptr<Options> options_;
    crypto::SSLPointer ssl_;
    crypto::BIOPointer bio_trace_;

    // There are three distinct levels of crypto data
    // involved in the TLS handshake. We use the handshake_
    // buffer to temporarily store the outbound crypto
    // data until it is acknowledged.
    Buffer handshake_[3];

    bool in_tls_callback_ = false;
    bool in_ocsp_request_ = false;
    bool in_client_hello_ = false;
    bool in_key_update_ = false;
    bool early_data_ = false;

    std::shared_ptr<v8::BackingStore> ocsp_response_;

    struct CallbackScope final {
      CryptoContext* context;

      inline explicit CallbackScope(CryptoContext* context_)
          : context(context_) {
        context_->in_tls_callback_ = true;
      }

      inline ~CallbackScope() {
        context->in_tls_callback_ = false;
      }

      inline static bool is_in_callback(CryptoContext* context) {
        return context->in_tls_callback_;
      }
    };

    struct HandshakeScope final {
      using DoneCB = std::function<void()>;
      CryptoContext* context;
      DoneCB done;

      inline HandshakeScope(CryptoContext* context_, DoneCB done_)
          : context(context_),
            done(done_) {}

      inline ~HandshakeScope() {
        if (!is_handshake_suspended())
          return;

        done();

        if (!CallbackScope::is_in_callback(context))
          context->ResumeHandshake();
      }

      inline bool is_handshake_suspended() const {
        return context->in_ocsp_request_ || context->in_client_hello_;
      }
    };

    friend class Session;
  };

  static bool HasInstance(Environment* env, const v8::Local<v8::Value>& value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  static void DoAttachToEndpoint(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoDestroy(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetRemoteAddress(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCertificate(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetEphemeralKeyInfo(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPeerCertificate(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GracefulClose(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SilentClose(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void UpdateKey(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoDetachFromEndpoint(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void OnClientHelloDone(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void OnOCSPDone(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoOpenStream(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoSendDatagram(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static BaseObjectPtr<Session> CreateServer(
      EndpointWrap* endpoint,
      const std::shared_ptr<SocketAddress>& local_addr,
      const std::shared_ptr<SocketAddress>& remote_addr,
      const CID& dcid,
      const CID& scid,
      const CID& ocid,
      const Config& config,
      const std::shared_ptr<Options>& options,
      const BaseObjectPtr<crypto::SecureContext>& context);

  static BaseObjectPtr<Session> CreateClient(
      EndpointWrap* endpoint,
      const std::shared_ptr<SocketAddress>& local_addr,
      const std::shared_ptr<SocketAddress>& remote_addr,
      const Config& config,
      const std::shared_ptr<Options>& options,
      const BaseObjectPtr<crypto::SecureContext>& context,
      const crypto::ArrayBufferOrViewContents<uint8_t>& session_ticket =
          crypto::ArrayBufferOrViewContents<uint8_t>(),
      const crypto::ArrayBufferOrViewContents<uint8_t>&
          remote_transport_params =
              crypto::ArrayBufferOrViewContents<uint8_t>());

  Session(
      EndpointWrap* endpoint,
      v8::Local<v8::Object> object,
      const std::shared_ptr<SocketAddress>& local_addr,
      const std::shared_ptr<SocketAddress>& remote_addr,
      const Config& config,
      const std::shared_ptr<Options>& options,
      const BaseObjectPtr<crypto::SecureContext>& context,
      const CID& dcid,
      const CID& scid,
      const CID& ocid,
      quic_version version = NGTCP2_PROTO_VER_MAX);

  Session(
      EndpointWrap* endpoint,
      v8::Local<v8::Object> object,
      const std::shared_ptr<SocketAddress>& local_addr,
      const std::shared_ptr<SocketAddress>& remote_addr,
      const Config& config,
      const std::shared_ptr<Options>& options,
      const BaseObjectPtr<crypto::SecureContext>& context,
      quic_version version = NGTCP2_PROTO_VER_MAX,
      const crypto::ArrayBufferOrViewContents<uint8_t>& session_ticket =
          crypto::ArrayBufferOrViewContents<uint8_t>(),
      const crypto::ArrayBufferOrViewContents<uint8_t>&
          remote_transport_params =
              crypto::ArrayBufferOrViewContents<uint8_t>());

  ~Session() override;

  inline ngtcp2_conn* connection() const { return connection_.get(); }
  inline CryptoContext* crypto_context() const {
    return crypto_context_.get();
  }
  inline const CID& dcid() const { return dcid_; }
  inline Application* application() const { return application_.get(); }
  inline EndpointWrap* endpoint() const;
  inline const std::shared_ptr<SocketAddress>& remote_address() const {
    return remote_address_;
  }
  inline const std::shared_ptr<SocketAddress>& local_address() const {
    return local_address_;
  }
  inline const std::string& alpn() const { return options_->alpn; }
  inline const std::string& hostname() const { return options_->hostname; }
  inline bool emit_keylog() const { return options_->keylog; }

  BaseObjectPtr<LogStream> qlogstream();
  BaseObjectPtr<LogStream> keylogstream();

  inline bool is_closing() const { return state_->closing; }
  inline bool is_destroyed() const { return state_->destroyed; }
  inline bool is_graceful_closing() const { return state_->graceful_closing; }
  inline bool is_server() const {
    return crypto_context_->side() == NGTCP2_CRYPTO_SIDE_SERVER;
  }

  v8::Maybe<stream_id> OpenStream(
      Stream::Direction direction = Stream::Direction::BIDIRECTIONAL);
  BaseObjectPtr<Stream> CreateStream(stream_id id);
  BaseObjectPtr<Stream> FindStream(stream_id id) const;
  void AddStream(const BaseObjectPtr<Stream>& stream);

  // Removes the given stream from the Session. All streams must
  // be removed before the Session is destroyed.
  void RemoveStream(stream_id id);
  void ResumeStream(stream_id id);
  bool HasStream(stream_id id) const;
  void StreamDataBlocked(stream_id id);
  void ShutdownStream(stream_id id, error_code code = NGTCP2_NO_ERROR);
  void ShutdownStreamWrite(stream_id id, error_code code = NGTCP2_NO_ERROR);
  const StreamsMap& streams() const { return streams_; }

  // Receive and process a QUIC packet received from the peer
  bool Receive(
      size_t nread,
      std::shared_ptr<v8::BackingStore> store,
      const std::shared_ptr<SocketAddress>& local_address,
      const std::shared_ptr<SocketAddress>& remote_address);

  // Called by ngtcp2 when a chunk of stream data has been received. If
  // the stream does not yet exist, it is created, then the data is
  // forwarded on.
  bool ReceiveStreamData(
      uint32_t flags,
      stream_id id,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset);

  // Causes pending ngtcp2 frames to be serialized and sent
  void SendPendingData();

  bool SendPacket(
      std::unique_ptr<Packet> packet,
      const ngtcp2_path_storage& path);

  uint64_t max_data_left() const;

  uint64_t max_local_streams_uni() const;

  inline bool allow_early_data() const {
    // TODO(@jasnell): For now, we always allow early data.
    // Later there will be reasons we do not want to allow
    // it, such as lack of available system resources.
    return true;
  }

  // Returns true if the Session has entered the closing period after sending
  // a CONNECTION_CLOSE. While true, the Session is only permitted to transmit
  // CONNECTION_CLOSE frames until either the idle timeout period elapses or
  // until the Session is explicitly destroyed.
  bool is_in_closing_period() const;

  // Returns true if the Session has received a CONNECTION_CLOSE frame from the
  // peer. Once in the draining period, the Session is not permitted to send
  // any frames to the peer. The Session will be silently closed after either
  // the idle timeout period elapses or until the Session is explicitly
  // destroyed.
  bool is_in_draining_period() const;

  // Starting a GracefulClose disables the ability to open or accept
  // new streams for this session. Existing streams are allowed to
  // close naturally on their own. Once called, the Session will
  // be immediately closed once there are no remaining streams. Note
  // that no notification is given to the connecting peer that we're
  // in a graceful closing state. A CONNECTION_CLOSE will be sent only
  // once Close() is called.
  void StartGracefulClose();

  bool AttachToNewEndpoint(EndpointWrap* endpoint, bool nat_rebinding = false);

  // Error handling for the Session. client and server instances will do
  // different things here, but ultimately an error means that the Session
  // should be torn down.
  void HandleError();

  // Transmits either a protocol or application connection close to the peer.
  // The choice of which is send is based on the current value of last_error_.
  bool SendConnectionClose();

  enum class SessionCloseFlags {
    NONE,
    SILENT,
    STATELESS_RESET
  };

  // Initiate closing of the Session. This will round trip through JavaScript,
  // causing all currently opened streams to be closed. If the SILENT flag is
  // set, the connected peer will not be notified, otherwise an attempt will be
  // made to send a CONNECTION_CLOSE frame to the peer. If Close is called
  // while within the ngtcp2 callback scope, sending the CONNECTION_CLOSE will
  // be deferred until the ngtcp2 callback scope exits.
  void Close(SessionCloseFlags close_flags = SessionCloseFlags::NONE);

  bool IsResetToken(const CID& cid, const uint8_t* data, size_t datalen);

  // Mark the Session instance destroyed. This will either be invoked
  // synchronously within the callstack of the Session::Close() method or not.
  // If it is invoked within Session::Close(), the Session::Close() will handle
  // sending the CONNECTION_CLOSE frame.
  void Destroy();

  // Extends the QUIC stream flow control window. This is called after received
  // data has been consumed and we want to allow the peer to send more data.
  void ExtendStreamOffset(stream_id id, size_t amount);

  // Extends the QUIC session flow control window
  void ExtendOffset(size_t amount);

  // Retrieve the local transport parameters established for this ngtcp2_conn
  void GetLocalTransportParams(ngtcp2_transport_params* params);

  quic_version version() const;

  inline QuicError last_error() const { return last_error_; }

  inline size_t max_packet_length() const { return max_pkt_len_; }

  // When completing the TLS handshake, the TLS session information is provided
  // to the Session so that the session ticket and the remote transport
  // parameters can be captured to support 0RTT session resumption.
  int set_session(SSL_SESSION* session);

  // True only if ngtcp2 considers the TLS handshake to be completed
  bool is_handshake_completed() const;

  bool is_unable_to_send_packets();

  inline void set_wrapped() { state_->wrapped = 1; }

  void SetSessionTicketAppData(const SessionTicketAppData& app_data);

  SessionTicketAppData::Status GetSessionTicketAppData(
      const SessionTicketAppData& app_data,
      SessionTicketAppData::Flag flag);

  // When a server advertises a preferred address in its initial transport
  // parameters, ngtcp2 on the client side will trigger the
  // OnSelectPreferredAdddress callback which will call this. The paddr
  // argument contains the advertised preferred address. If the new address is
  // going to be used, it needs to be copied over to dest, otherwise dest is
  // left alone. There are two possible strategies that we currently support
  // via user configuration: use the preferred address or ignore it.
  void SelectPreferredAddress(const PreferredAddress& preferred_address);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Session)
  SET_SELF_SIZE(Session)

  struct CallbackScope final {
    BaseObjectPtr<Session> session;
    std::unique_ptr<InternalCallbackScope> internal;
    v8::TryCatch try_catch;

    inline explicit CallbackScope(Session* session_)
        : session(session_),
          internal(new InternalCallbackScope(
              session->env(),
              session->object(),
              {
                session->get_async_id(),
                session->get_trigger_async_id()
              })),
          try_catch(session->env()->isolate()) {
      try_catch.SetVerbose(true);
    }

    inline ~CallbackScope() {
      Environment* env = session->env();
      if (UNLIKELY(try_catch.HasCaught())) {
        session->crypto_context()->in_client_hello_ = false;
        session->crypto_context()->in_ocsp_request_ = false;
        if (!try_catch.HasTerminated() && env->can_call_into_js()) {
          session->set_last_error(kQuicInternalError);
          session->Close();
          CHECK(session->is_destroyed());
        }
        internal->MarkAsFailed();
      }
    }
  };

  // ConnectionCloseScope triggers sending a CONNECTION_CLOSE when not
  // executing within the context of an ngtcp2 callback and the session is in
  // the correct state.
  struct ConnectionCloseScope final {
    BaseObjectPtr<Session> session;
    bool silent = false;

    inline ConnectionCloseScope(Session* session_, bool silent_ = false)
        : session(session_),
          silent(silent_) {
      CHECK(session);
      // If we are already in a ConnectionCloseScope, ignore.
      if (session->in_connection_close_)
        silent = true;
      else
        session->in_connection_close_ = true;
    }

    inline ~ConnectionCloseScope() {
      if (silent ||
          NgCallbackScope::InNgCallbackScope(session.get()) ||
          session->is_in_closing_period() ||
          session->is_in_draining_period()) {
        return;
      }
      session->in_connection_close_ = false;
      session->SendConnectionClose();
    }
  };

  // Used as a guard in the static callback functions (e.g.
  // Session::OnStreamClose) to prevent re-entry into the ngtcp2 callbacks
  struct NgCallbackScope final {
    BaseObjectPtr<Session> session;
    inline explicit NgCallbackScope(Session* session_)
        : session(session_) {
      CHECK(session);
      CHECK(!InNgCallbackScope(session_));
      session->in_ng_callback_ = true;
    }

    inline ~NgCallbackScope() {
      session->in_ng_callback_ = false;
    }

    static inline bool InNgCallbackScope(Session* session) {
      return session->in_ng_callback_;
    }
  };

  // SendSessionScope triggers SendPendingData() when not executing within the
  // context of an ngtcp2 callback. When within an ngtcp2 callback,
  // SendPendingData will always be called when the callbacks complete.
  struct SendSessionScope final {
    BaseObjectPtr<Session> session;

    inline explicit SendSessionScope(Session* session_) : session(session_) {
      CHECK(session);
      session->send_scope_depth_++;
    }

    inline ~SendSessionScope() {
      CHECK(session);
      if (--session->send_scope_depth_ ||
          NgCallbackScope::InNgCallbackScope(session.get()) ||
          session->is_in_closing_period() ||
          session->is_in_draining_period()) {
        return;
      }

      CHECK_EQ(session->send_scope_depth_, 0);
      session->SendPendingData();
    }
  };

  inline void set_last_error(QuicError error = kQuicNoError) {
    last_error_ = error;
  }

 private:
  Session(
      EndpointWrap* endpoint,
      v8::Local<v8::Object> object,
      const std::shared_ptr<SocketAddress>& local_addr,
      const std::shared_ptr<SocketAddress>& remote_addr,
      const std::shared_ptr<Options>& options,
      const BaseObjectPtr<crypto::SecureContext>& context,
      const CID& dcid = CID(),
      ngtcp2_crypto_side side = NGTCP2_CRYPTO_SIDE_CLIENT);

  void set_remote_transport_params();

  bool InitApplication();
  void AttachToEndpoint();

  // Removes the Session from the current socket. This is done with when the
  // session is being destroyed or being migrated to another Endpoint. It is
  // important to keep in mind that the Endpoint uses a BaseObjectPtr for the
  // Session. If the session is removed and there are no other references held,
  // the session object will be destroyed automatically.
  void DetachFromEndpoint();
  void OnIdleTimeout();

  // The the retransmit libuv timer fires, it will call OnRetransmitTimeout,
  // which determines whether or not we need to retransmit data to to packet
  // loss or ack delay.
  void OnRetransmitTimeout();
  void UpdateDataStats();
  void AckedStreamDataOffset(
      stream_id id,
      uint64_t offset,
      uint64_t datalen);
  void ExtendMaxStreamData(stream_id id, uint64_t max_data);
  void ExtendMaxStreams(bool bidi, uint64_t max_streams);
  void ExtendMaxStreamsUni(uint64_t max_streams);
  void ExtendMaxStreamsBidi(uint64_t max_streams);
  void ExtendMaxStreamsRemoteUni(uint64_t max_streams);
  void ExtendMaxStreamsRemoteBidi(uint64_t max_streams);

  // Generates and associates a new connection ID for this Session. ngtcp2 will
  // call this multiple times at the start of a new connection in order to
  // build a pool of available CIDs.
  void GetNewConnectionID(ngtcp2_cid* cid, uint8_t* token, size_t cidlen);

  // Captures the error code and family information from a received connection
  // close frame.
  void GetConnectionCloseInfo();

  // The HandshakeCompleted function is called by ngtcp2 once it determines
  // that the TLS Handshake is done. The only thing we need to do at this point
  // is let the javascript side know.
  bool HandshakeCompleted(const std::shared_ptr<SocketAddress>& remote_address);
  void HandshakeConfirmed();

  // When ngtcp2 receives a successful response to a PATH_CHALLENGE, it will
  // trigger the OnPathValidation callback which will, in turn invoke this.
  // There's really nothing to do here but update stats and and optionally
  // notify the javascript side if there is a handler registered. Notifying the
  // JavaScript side is purely informational.
  void PathValidation(
      const ngtcp2_path* path,
      ngtcp2_path_validation_result res);

  // Performs intake processing on a received QUIC packet. The received data is
  // passed on to ngtcp2 for parsing and processing. ngtcp2 will, in turn,
  // invoke a series of callbacks to handle the received packet.
  bool ReceivePacket(ngtcp2_path* path, const uint8_t* data, ssize_t nread);

  // The retransmit timer allows us to trigger retransmission of packets in
  // case they are considered lost. The exact amount of time is determined
  // internally by ngtcp2 according to the guidelines established by the QUIC
  // spec but we use a libuv timer to actually monitor.
  void ScheduleRetransmit();
  bool SendPacket(std::unique_ptr<Packet> packet);
  void StreamClose(stream_id id, error_code app_error_code);

  // Called when the Session has received a RESET_STREAM frame from the
  // peer, indicating that it will no longer send additional frames for the
  // stream. If the stream is not yet known, reset is ignored. If the stream
  // has already received a STREAM frame with fin set, the stream reset is
  // ignored (the QUIC spec permits implementations to handle this situation
  // however they want.) If the stream has not yet received a STREAM frame
  // with the fin set, then the RESET_STREAM causes the readable side of the
  // stream to be abruptly closed and any additional stream frames that may
  // be received will be discarded if their offset is greater than final_size.
  // On the JavaScript side, receiving a C is undistinguishable from
  // a normal end-of-stream. No additional data events will be emitted, the
  // end event will be emitted, and the readable side of the duplex will be
  // closed.
  //
  // If the stream is still writable, no additional action is taken. If,
  // however, the writable side of the stream has been closed (or was never
  // open in the first place as in the case of peer-initiated unidirectional
  // streams), the reset will cause the stream to be immediately destroyed.
  void StreamReset(
      stream_id id,
      uint64_t final_size,
      error_code app_error_code);

  bool WritePackets(const char* diagnostic_label = nullptr);

  void UpdateConnectionID(
      int type,
      const CID& cid,
      const StatelessResetToken& token);

  // Every QUIC session has a remote address and local address. Those endpoints
  // can change through the lifetime of a connection, so whenever a packet is
  // successfully processed, or when a response is to be sent, we have to keep
  // track of the path and update as we go.
  void UpdateEndpoint(const ngtcp2_path& path);

  // Called by the OnVersionNegotiation callback when a version negotiation
  // frame has been received by the client. The sv parameter is an array of
  // versions supported by the remote peer.
  void VersionNegotiation(const quic_version* sv, size_t nsv);
  void UpdateClosingTimer();

  // The retransmit timer allows us to trigger retransmission of packets in
  // case they are considered lost. The exact amount of time is determined
  // internally by ngtcp2 according to the guidelines established by the QUIC
  // spec but we use a libuv timer to actually monitor. Here we take the
  // calculated timeout and extend out the libuv timer.
  void UpdateRetransmitTimer(uint64_t timeout);

  // Begin connection close by serializing the CONNECTION_CLOSE packet. There
  // are two variants: one to serialize an application close, the other to
  // serialize a protocol close.  The frames are generally identical with the
  // exception of a bit in the header. On server Sessions, we serialize the
  // frame once and may retransmit it multiple times. On client Sessions, we
  // only ever serialize the connection close once.
  bool StartClosingPeriod();

  void IncrementConnectionCloseAttempts();
  bool ShouldAttemptConnectionClose();

  bool SendDatagram(
      const std::shared_ptr<v8::BackingStore>& store,
      size_t offset,
      size_t length);

  // Called when a datagram is received
  void Datagram(
    uint32_t flags,
    const uint8_t* data,
    size_t datalen);

  // Updates the idle timer deadline. If the idle timer fires, the connection
  // will be silently closed. It is important to update this as activity occurs
  // to keep the idle timer from firing.
  void UpdateIdleTimer();

  Application* SelectApplication(
    const std::string& alpn,
    const std::shared_ptr<Application::Options>& options);

  ngtcp2_mem allocator_;
  std::shared_ptr<Options> options_;
  QuicConnectionPointer connection_;
  BaseObjectPtr<EndpointWrap> endpoint_;
  AliasedStruct<State> state_;
  StreamsMap streams_;

  std::shared_ptr<SocketAddress> local_address_;
  std::shared_ptr<SocketAddress> remote_address_;

  std::unique_ptr<Application> application_;
  std::unique_ptr<CryptoContext> crypto_context_;

  TimerWrapHandle idle_;
  TimerWrapHandle retransmit_;

  CID dcid_;
  CID scid_;
  CID pscid_;
  ngtcp2_transport_params transport_params_;
  bool transport_params_set_ = false;
  bool in_ng_callback_ = false;
  bool in_connection_close_ = false;
  bool stateless_reset_ = false;
  size_t send_scope_depth_ = 0;

  size_t max_pkt_len_;
  QuicError last_error_ = kQuicNoError;
  std::unique_ptr<Packet> conn_closebuf_;
  size_t connection_close_attempts_ = 0;
  size_t connection_close_limit_ = 1;

  std::unique_ptr<RoutableConnectionIDStrategy> cid_strategy_;
  BaseObjectPtr<LogStream> qlogstream_;
  BaseObjectPtr<LogStream> keylogstream_;

  struct RemoteTransportParamsDebug final {
    Session* session;
    inline explicit RemoteTransportParamsDebug(Session* session_)
        : session(session_) {}
    std::string ToString() const;
  };

  static const ngtcp2_callbacks callbacks[2];

  // Called by ngtcp2 for both client and server connections when TLS handshake
  // data has been received and needs to be processed. This will be called
  // multiple times during the TLS handshake process and may be called during
  // key updates.
  static int OnReceiveCryptoData(
      ngtcp2_conn* conn,
      ngtcp2_crypto_level crypto_level,
      uint64_t offset,
      const uint8_t* data,
      size_t datalen,
      void* user_data);

  // Called by ngtcp2 for both client and server connections when ngtcp2 has
  // determined that the TLS handshake has been completed. It is important to
  // understand that this is only an indication of the local peer's handshake
  // state. The remote peer might not yet have completed its part of the
  // handshake.
  static int OnHandshakeCompleted(ngtcp2_conn* conn, void* user_data);

  // Called by ngtcp2 for clients when the handshake has been confirmed.
  // Confirmation occurs *after* handshake completion.
  static int OnHandshakeConfirmed(ngtcp2_conn* conn, void* user_data);

  // Called by ngtcp2 when a chunk of stream data has been received.
  // Currently, ngtcp2 ensures that this callback is always called
  // with an offset parameter strictly larger than the previous call's
  // offset + datalen (that is, data will never be delivered out of
  // order). That behavior may change in the future but only via a
  // configuration option.
  static int OnReceiveStreamData(
      ngtcp2_conn* conn,
      uint32_t flags,
      stream_id id,
      uint64_t offset,
      const uint8_t* data,
      size_t datalen,
      void* user_data,
      void* stream_user_data);

  // Called by ngtcp2 when an acknowledgement for a chunk of
  // TLS handshake data has been received by the remote peer.
  // This is only an indication that data was received, not that
  // it was successfully processed. Acknowledgements are a key
  // part of the QUIC reliability mechanism.
  static int OnAckedCryptoOffset(
      ngtcp2_conn* conn,
      ngtcp2_crypto_level crypto_level,
      uint64_t offset,
      uint64_t datalen,
      void* user_data);

  // Called by ngtcp2 when an acknowledgement for a chunk of
  // stream data has been received successfully by the remote peer.
  // This is only an indication that data was received, not that
  // it was successfully processed. Acknowledgements are a key
  // part of the QUIC reliability mechanism.
  static int OnAckedStreamDataOffset(
      ngtcp2_conn* conn,
      stream_id id,
      uint64_t offset,
      uint64_t datalen,
      void* user_data,
      void* stream_user_data);

  // Called by ngtcp2 for a client connection when the server
  // has indicated a preferred address in the transport
  // params.
  // For now, there are two modes: we can accept the preferred address
  // or we can reject it. Later, we may want to implement a callback
  // to ask the user if they want to accept the preferred address or
  // not.
  static int OnSelectPreferredAddress(
      ngtcp2_conn* conn,
      ngtcp2_addr* dest,
      const ngtcp2_preferred_addr* paddr,
      void* user_data);

  static int OnStreamClose(
      ngtcp2_conn* conn,
      stream_id id,
      uint64_t app_error_code,
      void* user_data,
      void* stream_user_data);

  static int OnStreamOpen(
      ngtcp2_conn* conn,
      stream_id id,
      void* user_data);

  // Stream reset means the remote peer will no longer send data
  // on the identified stream. It is essentially a premature close.
  // The final_size parameter is important here in that it identifies
  // exactly how much data the *remote peer* is aware that it sent.
  // If there are lost packets, then the local peer's idea of the final
  // size might not match.
  static int OnStreamReset(
      ngtcp2_conn* conn,
      stream_id id,
      uint64_t final_size,
      uint64_t app_error_code,
      void* user_data,
      void* stream_user_data);

  // Called by ngtcp2 when it needs to generate some random data.
  // We currently do not use it, but the ngtcp2_rand_ctx identifies
  // why the random data is necessary. When ctx is equal to
  // NGTCP2_RAND_CTX_NONE, it typically means the random data
  // is being used during the TLS handshake. When ctx is equal to
  // NGTCP2_RAND_CTX_PATH_CHALLENGE, the random data is being
  // used to construct a PATH_CHALLENGE. These *might* need more
  // secure and robust random number generation given the
  // sensitivity of PATH_CHALLENGE operations (an attacker
  // could use a compromised PATH_CHALLENGE to trick an endpoint
  // into redirecting traffic).
  //
  // The ngtcp2_rand_ctx tells us what the random data is used for.
  // Currently, there is only one use. In the future, we'll want to
  // explore whether we want to handle the different cases uses.
  static int OnRand(
      uint8_t* dest,
      size_t destlen,
      const ngtcp2_rand_ctx* rand_ctx,
      ngtcp2_rand_usage usage);

  // When a new client connection is established, ngtcp2 will call
  // this multiple times to generate a pool of connection IDs to use.
  static int OnGetNewConnectionID(
      ngtcp2_conn* conn,
      ngtcp2_cid* cid,
      uint8_t* token,
      size_t cidlen,
      void* user_data);

  // When a connection is closed, ngtcp2 will call this multiple
  // times to retire connection IDs. It's also possible for this
  // to be called at times throughout the lifecycle of the connection
  // to remove a CID from the availability pool.
  static int OnRemoveConnectionID(
      ngtcp2_conn* conn,
      const ngtcp2_cid* cid,
      void* user_data);

  // Called by ngtcp2 to perform path validation. Path validation
  // is necessary to ensure that a packet is originating from the
  // expected source. If the res parameter indicates success, it
  // means that the path specified has been verified as being
  // valid.
  //
  // Validity here means only that there has been a successful
  // exchange of PATH_CHALLENGE information between the peers.
  // It's critical to understand that the validity of a path
  // can change at any timee so this is only an indication of
  // validity at a specific point in time.
  static int OnPathValidation(
      ngtcp2_conn* conn,
      const ngtcp2_path* path,
      ngtcp2_path_validation_result res,
      void* user_data);

  // Called by ngtcp2 for both client and server connections
  // when a request to extend the maximum number of unidirectional
  // streams has been received
  static int OnExtendMaxStreamsUni(
      ngtcp2_conn* conn,
      uint64_t max_streams,
      void* user_data);

  // Called by ngtcp2 for both client and server connections
  // when a request to extend the maximum number of bidirectional
  // streams has been received.
  static int OnExtendMaxStreamsBidi(
      ngtcp2_conn* conn,
      uint64_t max_streams,
      void* user_data);

  // Triggered by ngtcp2 when the local peer has received a flow
  // control signal from the remote peer indicating that additional
  // data can be sent. The max_data parameter identifies the maximum
  // data offset that may be sent. That is, a value of 99 means that
  // out of a stream of 1000 bytes, only the first 100 may be sent.
  // (offsets 0 through 99).
  static int OnExtendMaxStreamData(
      ngtcp2_conn* conn,
      stream_id id,
      uint64_t max_data,
      void* user_data,
      void* stream_user_data);

  // Triggered by ngtcp2 when a version negotiation is received.
  // What this means is that the remote peer does not support the
  // QUIC version requested. The only thing we can do here (per
  // the QUIC specification) is silently discard the connection
  // and notify the JavaScript side that a different version of
  // QUIC should be used. The sv parameter does list the QUIC
  // versions advertised as supported by the remote peer.
  static int OnVersionNegotiation(
      ngtcp2_conn* conn,
      const ngtcp2_pkt_hd* hd,
      const uint32_t* sv,
      size_t nsv,
      void* user_data);

  // Triggered by ngtcp2 when a stateless reset is received. What this
  // means is that the remote peer might recognize the CID but has lost
  // all state necessary to successfully process it. The only thing we
  // can do is silently close the connection. For server sessions, this
  // means all session state is shut down and discarded, even on the
  // JavaScript side. For client sessions, we discard session state at
  // the C++ layer but -- at least in the future -- we can retain some
  // state at the JavaScript level to allow for automatic session
  // resumption.
  static int OnStatelessReset(
      ngtcp2_conn* conn,
      const ngtcp2_pkt_stateless_reset* sr,
      void* user_data);

  // Triggered by ngtcp2 when the local peer has received an
  // indication from the remote peer indicating that additional
  // unidirectional streams may be sent. The max_streams parameter
  // identifies the highest unidirectional stream ID that may be
  // opened.
  static int OnExtendMaxStreamsRemoteUni(
      ngtcp2_conn* conn,
      uint64_t max_streams,
      void* user_data);

  // Triggered by ngtcp2 when the local peer has received an
  // indication from the remote peer indicating that additional
  // bidirectional streams may be sent. The max_streams parameter
  // identifies the highest bidirectional stream ID that may be
  // opened.
  static int OnExtendMaxStreamsRemoteBidi(
      ngtcp2_conn* conn,
      uint64_t max_streams,
      void* user_data);

  static int OnConnectionIDStatus(
      ngtcp2_conn* conn,
      int type,
      uint64_t seq,
      const ngtcp2_cid* cid,
      const uint8_t* token,
      void* user_data);

  // A QUIC datagram is an independent data packet that is
  // unaffiliated with a stream.
  static int OnDatagram(
      ngtcp2_conn* conn,
      uint32_t flags,
      const uint8_t* data,
      size_t datalen,
      void* user_data);

  friend struct Session::CallbackScope;
  friend struct Session::NgCallbackScope;
  friend struct Session::SendSessionScope;
  friend class Session::CryptoContext;
};

class DefaultApplication final : public Session::Application {
 public:
  DefaultApplication(
      Session* session,
      const std::shared_ptr<Application::Options>& options);

  bool ReceiveStreamData(
      uint32_t flags,
      stream_id id,
      const uint8_t* data,
      size_t datalen,
      uint64_t offset) override;

  int GetStreamData(StreamData* stream_data) override;

  void ResumeStream(stream_id id) override;
  bool ShouldSetFin(const StreamData& stream_data) override;
  bool StreamCommit(StreamData* stream_data, size_t datalen) override;

  SET_SELF_SIZE(DefaultApplication)
  SET_MEMORY_INFO_NAME(DefaultApplication)
  SET_NO_MEMORY_INFO()

 private:
  void ScheduleStream(stream_id id);
  void UnscheduleStream(stream_id id);

  Stream::Queue stream_queue_;
};

class OptionsObject final : public BaseObject {
 public:
  static bool HasInstance(
      Environment* env,
      const v8::Local<v8::Value>& value);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPreferredAddress(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTransportParams(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTLSOptions(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSessionResume(const v8::FunctionCallbackInfo<v8::Value>& args);

  OptionsObject(
      Environment* env,
      v8::Local<v8::Object> object,
      std::shared_ptr<Session::Options> options =
          std::make_shared<Session::Options>());

  inline std::shared_ptr<Session::Options> options() const { return options_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(OptionsObject)
  SET_SELF_SIZE(OptionsObject)

 private:
  v8::Maybe<bool> SetOption(
      const v8::Local<v8::Object>& object,
      const v8::Local<v8::String>& name,
      uint64_t Session::Options::*member);

  v8::Maybe<bool> SetOption(
      const v8::Local<v8::Object>& object,
      const v8::Local<v8::String>& name,
      uint32_t Session::Options::*member);

  v8::Maybe<bool> SetOption(
      const v8::Local<v8::Object>& object,
      const v8::Local<v8::String>& name,
      bool Session::Options::*member);

  std::shared_ptr<Session::Options> options_;
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_QUIC_SESSION_H_
