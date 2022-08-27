#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <aliased_struct.h>
#include <async_wrap.h>
#include <crypto/crypto_keys.h>
#include <env.h>
#include <memory_tracker.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <node_external_reference.h>
#include <node_http_common.h>
#include <timer_wrap.h>
#include <util.h>
#include <v8.h>
#include "crypto.h"
#include "defs.h"
#include "quic.h"

namespace node {
namespace quic {

using QuicConnectionPointer = DeleteFnPtr<ngtcp2_conn, ngtcp2_conn_del>;
class Http3Application;

#define V(_, name, __) uint64_t name;
struct SessionStats final {
  SESSION_STATS(V)
};
#undef V

struct SessionStatsTraits final {
  using Stats = SessionStats;
  using Base = StatsBase<SessionStatsTraits>;

  template <typename Fn>
  static void ToString(const Stats& stats, Fn&& add_field) {
#define V(_, id, desc) add_field(desc, stats.id);
    SESSION_STATS(V)
#undef V
  }
};

using SessionStatsBase = StatsBase<SessionStatsTraits>;

class LogStream;
class DefaultApplication;

// =============================================================================
// PreferredAddress is a helper class used only when a client Session receives
// an advertised preferred address from a server. The helper provides
// information about the servers advertised preferred address. Call Use() to let
// ngtcp2 know which preferred address to use (if any).
class PreferredAddress final {
 public:
  enum class Policy { IGNORE_PREFERED, USE };

  struct AddressInfo final {
    int family;
    uint16_t port;
    std::string address;
  };

  inline PreferredAddress(Environment* env,
                          ngtcp2_path* dest,
                          const ngtcp2_preferred_addr* paddr)
      : env_(env), dest_(dest), paddr_(paddr) {}

  QUIC_NO_COPY_OR_MOVE(PreferredAddress)

  // When a preferred address is advertised by a server, the advertisement also
  // includes a new CID and (optionally) a stateless reset token. If the
  // preferred address is selected, then the client Session will make use of
  // these new values. Access to the cid and reset token are provided via the
  // PreferredAddress class only as a convenience.
  inline CID cid() const { return CID(paddr_->cid); }

  // The stateless reset token associated with the preferred address CID
  inline const uint8_t* stateless_reset_token() const {
    return paddr_->stateless_reset_token;
  }

  // A preferred address advertisement may include both an IPv4 and IPv6
  // address. Only one of which will be used.

  v8::Maybe<AddressInfo> ipv4() const;

  v8::Maybe<AddressInfo> ipv6() const;

  // Instructs the Session to use the advertised preferred address matching the
  // given family. If the advertisement does not include a matching address, the
  // preferred address is ignored. If the given address cannot be successfully
  // resolved using uv_getaddrinfo it is ignored.
  bool Use(const AddressInfo& address) const;

  void CopyToTransportParams(ngtcp2_transport_params* params,
                             const sockaddr* addr);

 private:
  bool Resolve(const AddressInfo& address, uv_getaddrinfo_t* req) const;

  Environment* env_;
  mutable ngtcp2_path* dest_;
  const ngtcp2_preferred_addr* paddr_;
};

struct Path final : public ngtcp2_path {
  Path(const SocketAddress& local, const SocketAddress& remote);
};

struct PathStorage final : public ngtcp2_path_storage {
  inline PathStorage() { ngtcp2_path_storage_zero(this); }
};

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
class Session : public SessionStatsBase, public AsyncWrap {
 public:
  // An Application encapsulates the ALPN-identified application specific
  // semantics associated with the Session.
  class Application : public MemoryRetainer {
   public:
    // A base class for configuring the Application. Specific Application
    // subclasses may extend this with additional configuration properties.
    struct Options {
      // The maximum number of header pairs permitted for a Stream.
      uint64_t max_header_pairs = DEFAULT_MAX_HEADER_LIST_PAIRS;

      // The maximum total number of header bytes (including header
      // name and value) permitted for a Stream.
      uint64_t max_header_length = DEFAULT_MAX_HEADER_LENGTH;

      // HTTP/3 specific options. We keep these here instead of in the
      // HTTP3Application to keep things easy.

      uint64_t max_field_section_size;
      size_t qpack_max_dtable_capacity;
      size_t qpack_encoder_max_dtable_capacity;
      size_t qpack_blocked_streams;
    };

    Application(Session* session, const Options& options)
        : session_(session), options_(options) {}
    QUIC_NO_COPY_OR_MOVE(Application)

    // The Session will call Start as soon as the TLS secrets have been
    // negotiated.
    virtual bool Start();

    struct ReceiveStreamDataFlags final {
      // Identifies the final chunk of data that the peer will send for the
      // stream.
      bool fin = false;
      // Indicates that this chunk of data was received in a 0RTT packet before
      // the TLS handshake completed, suggesting that is is not as secure and
      // could be replayed by an attacker.
      bool early = false;
    };

    // Session will forward all received stream data immediately on to the
    // Application. The only additional processing the Session does is to
    // automatically adjust the session-level flow control window. It is up to
    // the Application to do the same for the Stream-level flow control.
    virtual bool ReceiveStreamData(Stream* stream,
                                   ReceiveStreamDataFlags flags,
                                   const uint8_t* data,
                                   size_t datalen,
                                   uint64_t offset) = 0;

    // Session will forward all data acknowledgements for a stream to the
    // Application.
    virtual void AcknowledgeStreamData(Stream* stream,
                                       uint64_t offset,
                                       size_t datalen);

    // Called to determine if a Header can be added to this application.
    // Applications that do not support headers will always return false.
    virtual bool CanAddHeader(size_t current_count,
                              size_t current_headers_length,
                              size_t this_header_length);

    // Called to mark the identified stream as being blocked. Not all
    // Application types will support blocked streams, and those that do will do
    // so differently. The default implementation here is to simply acknowledge
    // the notification.
    virtual bool BlockStream(stream_id id);

    // Called when the Session determines that the maximum number of
    // remotely-initiated unidirectional streams has been extended. Not all
    // Application types will require this notification so the default is to do
    // nothing.
    virtual void ExtendMaxStreams(EndpointLabel label,
                                  Direction direction,
                                  uint64_t max_streams) {}

    // Called when the Session determines that the flow control window for the
    // given stream has been expanded. Not all Application types will require
    // this notification so the default is to do nothing.
    virtual void ExtendMaxStreamData(Stream* stream, uint64_t max_data) {}

    // Called when the session determines that there is outbound data available
    // to send for the given stream.
    virtual void ResumeStream(stream_id id) {}

    // Different Applications may wish to set some application data in the
    // session ticket (e.g. http/3 would set server settings in the application
    // data). By default, there's nothing to set.
    virtual void SetSessionTicketAppData(const SessionTicketAppData& app_data) {
      DEBUG(session_, "Set session ticket app data does nothing");
    }

    // Different Applications may set some application data in the session
    // ticket (e.g. http/3 would set server settings in the application data).
    // By default, there's nothing to get.
    virtual SessionTicketAppData::Status GetSessionTicketAppData(
        const SessionTicketAppData& app_data, SessionTicketAppData::Flag flag);

    // Notifies the Application that the identified stream has been closed.
    virtual void StreamClose(Stream* stream, v8::Maybe<QuicError> error);

    // Notifies the Application that the identified stream has been reset.
    virtual void StreamReset(Stream* stream,
                             uint64_t final_size,
                             QuicError error);

    // Notifies the Application that the identified stream should stop sending.
    virtual void StreamStopSending(Stream* stream, QuicError error);

    enum class HeadersKind {
      INFO,
      INITIAL,
      TRAILING,
    };

    enum class HeadersFlags {
      NONE,
      TERMINAL,
    };

    // Submits an outbound block of headers for the given stream. Not all
    // Application types will support headers, in which case this function
    // should return false.
    virtual bool SendHeaders(stream_id id,
                             HeadersKind kind,
                             const v8::Local<v8::Array>& headers,
                             HeadersFlags flags = HeadersFlags::NONE) {
      return false;
    }

    // Signals to the Application that it should serialize and transmit any
    // pending session and stream packets it has accumulated.
    void SendPendingData();

    enum class StreamPriority {
      DEFAULT = NGHTTP3_DEFAULT_URGENCY,
      LOW = NGHTTP3_URGENCY_LOW,
      HIGH = NGHTTP3_URGENCY_HIGH,
    };

    enum class StreamPriorityFlags {
      NONE,
      NON_INCREMENTAL,
    };

    // Set the priority level of the stream if supported by the application. Not
    // all applications support priorities, in which case this function is a
    // non-op.
    virtual void SetStreamPriority(
        Stream* stream,
        StreamPriority priority = StreamPriority::DEFAULT,
        StreamPriorityFlags flags = StreamPriorityFlags::NONE) {
      // By default do nothing.
    }

    virtual StreamPriority GetStreamPriority(Stream* stream) {
      return StreamPriority::DEFAULT;
    }

    inline Environment* env() const { return session_->env(); }
    inline Session& session() { return *session_; }
    inline const Options& options() const { return options_; }

   protected:
    BaseObjectPtr<Packet> CreateStreamDataPacket();

    struct StreamData final {
      size_t count = 0;
      size_t remaining = 0;
      stream_id id = -1;
      int fin = 0;
      ngtcp2_vec data[kMaxVectorCount]{};
      ngtcp2_vec* buf = data;
      BaseObjectPtr<Stream> stream;

      std::string ToString() const;
    };

    virtual int GetStreamData(StreamData* data) = 0;
    virtual bool StreamCommit(StreamData* data, size_t datalen) = 0;
    virtual bool ShouldSetFin(const StreamData& data) = 0;

    ssize_t WriteVStream(PathStorage* path,
                         uint8_t* buf,
                         ssize_t* ndatalen,
                         const StreamData& stream_data);

   private:
    Session* session_;
    Options options_;
    bool started_ = false;
  };

  // A utility that wraps the configuration settings for the Session and the
  // underlying ngtcp2_conn. This struct is created when a new Client or Server
  // session is created.
  struct Config final : public ngtcp2_settings {
    ngtcp2_crypto_side side;

    // The QUIC protocol version requested for the Session.
    quic_version version = NGTCP2_PROTO_VER_MAX;

    CIDFactory& cid_factory;

    SocketAddress local_addr;
    SocketAddress remote_addr;

    // The initial destination CID. For server sessions, the dcid value is
    // provided to us by the remote peer in their initial packet, and may be
    // updated as we go using NEW_CONNECTION_ID frames. For client sessions, the
    // dcid value is initially selected at random and will be replaced one or
    // more times when either a RETRY or Initial packet is received from the
    // server.
    CID dcid;

    // The locally selected source CID. This value is always generated locally.
    CID scid;

    // The ocid configuration field is only used by client sessions to identify
    // the original DCID used.
    v8::Maybe<CID> ocid = v8::Nothing<CID>();

    v8::Maybe<CID> retry_scid = v8::Nothing<CID>();

    // For client sessions, the SessionTicket stores the crypto session and
    // stored remote transport parameters that are used to support 0RTT session
    // resumption. This field is unused by server sessions.
    BaseObjectPtr<SessionTicket> session_ticket;

    Config(const Endpoint& endpoint,
           // For server session, The dcid here is the identifier the remote
           // peer sent us to identify itself. It is the CID we will use as the
           // outbound dcid for all packets. For client sessions, the dcid here
           // is initially a randomly generated value that MUST be between 8 and
           // 20 bytes in length.
           const CID& dcid,
           const SocketAddress& local_address,
           const SocketAddress& remote_address,
           quic_version version,
           ngtcp2_crypto_side side);

    Config(const Endpoint& endpoint,
           quic_version version,
           ngtcp2_crypto_side side);

    void EnableQLog(const CID& ocid = CID());
  };

  // The Options struct contains all of the usercode specified options for the
  // session. Most of the options correlate to the transport parameters that are
  // communicated to the remote peer once the session is created.
  struct Options final : public MemoryRetainer {
    // The protocol identifier to be used by this Session.
    std::string alpn = NGHTTP3_ALPN_H3;

    // The SNI hostname to be used. This is used only by client Sessions to
    // identify the SNI host in the TLS client hello message.
    std::string hostname = "";

    PreferredAddress::Policy preferred_address_strategy =
        PreferredAddress::Policy::USE;

    bool qlog = false;

    // Set only on server Sessions, the preferred address communicates the IP
    // address and port that the server would prefer the client to use when
    // communicating with it. See the QUIC specification for more detail on how
    // the preferred address mechanism works.
    v8::Maybe<SocketAddress> preferred_address_ipv4 =
        v8::Nothing<SocketAddress>();
    v8::Maybe<SocketAddress> preferred_address_ipv6 =
        v8::Nothing<SocketAddress>();

    // The initial size of the flow control window of locally initiated streams.
    // This is the maximum number of bytes that the *remote* endpoint can send
    // when the connection is started.
    uint64_t initial_max_stream_data_bidi_local =
        DEFAULT_MAX_STREAM_DATA_BIDI_LOCAL;

    // The initial size of the flow control window of remotely initiated
    // streams. This is the maximum number of bytes that the remote endpoint can
    // send when the connection is started.
    uint64_t initial_max_stream_data_bidi_remote =
        DEFAULT_MAX_STREAM_DATA_BIDI_REMOTE;

    // The initial size of the flow control window of remotely initiated
    // unidirectional streams. This is the maximum number of bytes that the
    // remote endpoint can send when the connection is started.
    uint64_t initial_max_stream_data_uni = DEFAULT_MAX_STREAM_DATA_UNI;

    // The initial size of the session-level flow control window.
    uint64_t initial_max_data = DEFAULT_MAX_DATA;

    // The initial maximum number of concurrent bidirectional streams the remote
    // endpoint is permitted to open.
    uint64_t initial_max_streams_bidi = DEFAULT_MAX_STREAMS_BIDI;

    // The initial maximum number of concurrent unidirectional streams the
    // remote endpoint is permitted to open.
    uint64_t initial_max_streams_uni = DEFAULT_MAX_STREAMS_UNI;

    // The maximum amount of time that a Session is permitted to remain idle
    // before it is silently closed and state is discarded.
    uint64_t max_idle_timeout = DEFAULT_MAX_IDLE_TIMEOUT;

    // The maximum number of Connection IDs that the peer can store. A single
    // Session may have several connection IDs over it's lifetime.
    uint64_t active_connection_id_limit = DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;

    // Establishes the exponent used in ACK Delay field in the ACK frame. See
    // the QUIC specification for details. This is an advanced option that
    // should rarely be modified and only if there is really good reason.
    uint64_t ack_delay_exponent = NGTCP2_DEFAULT_ACK_DELAY_EXPONENT;

    // The maximum amount of time by which the endpoint will delay sending
    // acknowledgements. This is an advanced option that should rarely be
    // modified and only if there is a really good reason. It is used to
    // determine how long a Session will wait to determine that a packet has
    // been lost.
    uint64_t max_ack_delay = NGTCP2_DEFAULT_MAX_ACK_DELAY;

    // The maximum size of DATAGRAM frames that the endpoint will accept.
    // Setting the value to 0 will disable DATAGRAM support.
    uint64_t max_datagram_frame_size = kDefaultMaxPacketLength;

    // When true, communicates that the Session does not support active
    // connection migration. See the QUIC specification for more details on
    // connection migration.
    bool disable_active_migration = false;

    // ==================================================================================
    // TLS Options

    CryptoContext::Options crypto_options;

    // ==================================================================================
    // Application Options

    Application::Options application;

    Options() = default;
    // QUIC_COPY_NO_MOVE(Options)

    void MemoryInfo(MemoryTracker* tracker) const override;
    SET_MEMORY_INFO_NAME(Session::Options)
    SET_SELF_SIZE(Options)
  };

  class OptionsObject : public BaseObject {
   public:
    HAS_INSTANCE()
    static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
        Environment* env);
    static void Initialize(Environment* env, v8::Local<v8::Object> target);
    static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

    OptionsObject(Environment* env, v8::Local<v8::Object> object);
    QUIC_NO_COPY_OR_MOVE(OptionsObject)

    inline const Options& options() const { return options_; }
    inline operator const Options&() const { return options_; }

    void MemoryInfo(MemoryTracker*) const override;
    SET_MEMORY_INFO_NAME(Session::OptionsObject)
    SET_SELF_SIZE(OptionsObject);

   private:
    template <typename Opt>
    v8::Maybe<bool> SetOption(Opt* options,
                              const v8::Local<v8::Object>& object,
                              const v8::Local<v8::String>& name,
                              bool Opt::*member);

    template <typename Opt>
    v8::Maybe<bool> SetOption(Opt* options,
                              const v8::Local<v8::Object>& object,
                              const v8::Local<v8::String>& name,
                              uint64_t Opt::*member);

    template <typename Opt>
    v8::Maybe<bool> SetOption(Opt* options,
                              const v8::Local<v8::Object>& object,
                              const v8::Local<v8::String>& name,
                              uint32_t Opt::*member);

    template <typename Opt>
    v8::Maybe<bool> SetOption(Opt* options,
                              const v8::Local<v8::Object>& object,
                              const v8::Local<v8::String>& name,
                              std::string Opt::*member);

    template <typename Opt>
    v8::Maybe<bool> SetOption(
        Opt* options,
        const v8::Local<v8::Object>& object,
        const v8::Local<v8::String>& name,
        std::vector<std::shared_ptr<crypto::KeyObjectData>> Opt::*member);

    template <typename Opt>
    v8::Maybe<bool> SetOption(Opt* options,
                              const v8::Local<v8::Object>& object,
                              const v8::Local<v8::String>& name,
                              std::vector<Store> Opt::*member);

    Options options_;
  };

  // The Transport Params are the set of configuration options that are sent to
  // the remote peer. They communicate the protocol options the other peer
  // should use when communicating with this session.
  class TransportParams final {
   public:
    enum class Type {
      CLIENT_HELLO = NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO,
      ENCRYPTED_EXTENSIONS = NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS,
    };

    Type type() const { return type_; }

    inline const ngtcp2_transport_params& operator*() const {
      CHECK_NOT_NULL(ptr_);
      return *ptr_;
    }

    inline const ngtcp2_transport_params* operator->() const {
      CHECK_NOT_NULL(ptr_);
      return ptr_;
    }

    inline operator bool() const { return ptr_ != nullptr; }

    inline operator const ngtcp2_transport_params*() const {
      CHECK_NOT_NULL(ptr_);
      return ptr_;
    }

    QuicError error() const { return error_; }

    // Returns an ArrayBuffer containing the encoded transport parameters.
    // If an error occurs during encoding, an empty shared_ptr will be returned
    // and the error() property will be set to an appropriate QuicError.
    Store Encode(Environment* env);

   private:
    // Creates an instance of TransportParams from an existing const
    // ngtcp2_transport_params pointer.
    inline TransportParams(Type type, const ngtcp2_transport_params* ptr)
        : type_(type), ptr_(ptr) {}

    // Creates an instance of TransportParams by decoding the given backing
    // store. If the parameters cannot be successfully decoded, the error()
    // property will be set with an appropriate QuicError and the bool()
    // operator will return false.
    TransportParams(Type type, const ngtcp2_vec& buf);

    TransportParams(const Config& config, const Options& options);

    TransportParams(Type type) : type_(type), ptr_(&params_) {}

    void GenerateStatelessResetToken(const Endpoint& endpoint, const CID& cid);
    void GeneratePreferredAddressToken(Session*, CID* pscid);
    void SetPreferredAddress(const SocketAddress& address);

    Type type_;
    ngtcp2_transport_params params_{};
    const ngtcp2_transport_params* ptr_;
    QuicError error_;

    friend class Session;
    friend class CryptoContext;
  };

  // SendPendingDataScope triggers SendPendingData() on scope exit when not
  // executing within the context of an ngtcp2 callback. When within an ngtcp2
  // callback, SendPendingData will always be called when the callbacks
  // complete.
  struct SendPendingDataScope final {
    Session* session;
    explicit SendPendingDataScope(Session* session_);
    explicit SendPendingDataScope(const BaseObjectPtr<Session>& session_)
        : SendPendingDataScope(session_.get()) {}
    QUIC_NO_COPY_OR_MOVE(SendPendingDataScope)
    ~SendPendingDataScope();
  };

  // JavaScript API
  HAS_INSTANCE()
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  // Internal API
  static BaseObjectPtr<Session> Create(BaseObjectPtr<Endpoint> endpoint,
                                       const Config& config,
                                       const Options& options);

  ~Session() override;

  quic_version version() const;
  const Endpoint& endpoint() const;
  inline CryptoContext& crypto_context() { return crypto_context_; }
  inline Application& application() { return *application_; }
  inline const SocketAddress& remote_address() const { return remote_address_; }
  inline const SocketAddress& local_address() const { return local_address_; }
  inline bool is_destroyed() const { return state_->destroyed; }
  inline bool is_server() const {
    return crypto_context_.side() == NGTCP2_CRYPTO_SIDE_SERVER;
  }

  inline BaseObjectPtr<LogStream>& qlogstream() { return qlogstream_; }
  inline BaseObjectPtr<LogStream>& keylogstream() { return keylogstream_; }

  inline void set_wrapped() { state_->wrapped = true; }

  inline std::string alpn() const { return options_.alpn; }
  inline std::string hostname() const { return options_.hostname; }
  // TODO(@jasnell): For now, we always allow early data. We'll make this
  // configurable.
  bool allow_early_data() const { return true; }

  inline ngtcp2_conn* connection() const {
    CHECK(!is_destroyed());
    return connection_.get();
  }

  // Returns the Stream associated
  BaseObjectPtr<Stream> FindStream(stream_id) const;

  inline void SetLastError(QuicError error) { last_error_ = error; }

  // Initiate closing of the Session. This will round trip through JavaScript,
  // causing all currently opened streams to be closed. An attempt will be made
  // to send a CONNECTION_CLOSE frame to the peer. If Close is called while
  // within the ngtcp2 callback scope, sending the CONNECTION_CLOSE will be
  // deferred until the ngtcp2 callback scope exits.
  void Close();

  // Like Close(), except that the connected peer will not be notified.
  void CloseSilently();

  // Closing gracefully disables the ability to open or accept new streams for
  // this Session. Existing streams are allowed to close naturally on their own.
  // Once called, the Session will be immediately closed once there are no
  // remaining streams. No notification is given to the connected peer that we
  // are in a graceful closing state. A CONNECTION_CLOSE will be sent only once
  // Close() is called.
  void CloseGracefully();

  // Immediately destroy the Session, after which it cannot be used. The only
  // remaining state will be the collected statistics. There must not be any
  // Streams remaining on the Session.
  void Destroy();

  // Get the local transport parameters established for this Session.
  TransportParams GetLocalTransportParams() const;

  // Get the remote transport parameters established for this Session. If the
  // remote transport parameters are not yet known, the returned TransportParams
  // bool() operator will return false.
  TransportParams GetRemoteTransportParams() const;

  inline bool is_graceful_closing() const { return state_->graceful_closing; }

  BaseObjectPtr<Stream> CreateStream(stream_id id);
  BaseObjectPtr<Stream> OpenStream(Direction direction);
  void AddStream(const BaseObjectPtr<Stream>& stream);
  void RemoveStream(stream_id id);
  void ResumeStream(stream_id id);
  void ShutdownStream(stream_id id, QuicError error);
  void StreamDataBlocked(stream_id id);
  void ShutdownStreamWrite(stream_id id, QuicError code);
  void UpdatePath(const PathStorage& path);
  void Send(BaseObjectPtr<Packet> packet);
  void Send(BaseObjectPtr<Packet> packet, const PathStorage& path);
  datagram_id SendDatagram(Store&& data);

  void SetSessionTicketAppData(const SessionTicketAppData& app_data);

  SessionTicketAppData::Status GetSessionTicketAppData(
      const SessionTicketAppData& app_data, SessionTicketAppData::Flag flag);

  bool Receive(Store&& store,
               const SocketAddress& local_address,
               const SocketAddress& remote_address);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Session)
  SET_SELF_SIZE(Session)

  // Really should be private, but can't be because MakeDetachedBaseObject needs
  // visibility.
  Session(v8::Local<v8::Object> object,
          BaseObjectPtr<Endpoint> endpoint,
          const Config& config,
          const Options& options);

  std::string diagnostic_name() const override;

  const Options& options() const { return options_; }

 private:
  // JavaScript API

  static void DoDestroy(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetRemoteAddress(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCertificate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetEphemeralKeyInfo(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPeerCertificate(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GracefulClose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SilentClose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void UpdateKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void OnClientHelloDone(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void OnOCSPDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoOpenStream(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoSendDatagram(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Internal API

  enum class PathValidationResult : uint8_t {
    SUCCESS = NGTCP2_PATH_VALIDATION_RESULT_SUCCESS,
    FAILURE = NGTCP2_PATH_VALIDATION_RESULT_FAILURE,
    ABORTED = NGTCP2_PATH_VALIDATION_RESULT_ABORTED,
  };
  struct PathValidationFlags {
    bool preferredAddress = false;
  };

  struct DatagramReceivedFlag {
    bool early = false;
  };

  std::unique_ptr<Application> SelectApplication(const Config& config,
                                                 const Options& options);

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

  // Returns true if the Session is currently in a state where it is unable to
  // transmit any packets.
  bool is_unable_to_send_packets() const;

  bool can_create_streams() const;

  uint64_t max_data_left() const;
  uint64_t max_local_streams_uni() const;
  uint64_t max_local_streams_bidi() const;
  void DetachFromEndpoint();
  void DoClose(bool silent = false);
  void ExtendStreamOffset(stream_id id, size_t amount);
  void ExtendOffset(size_t amount);
  void UpdateDataStats();
  void SendConnectionClose();
  void OnTimeout();
  void UpdateTimer();
  void SendPendingData();
  bool StartClosingPeriod();

  void EmitClose();
  void EmitError(QuicError error);
  void EmitSessionTicket(Store&& ticket);
  void EmitPathValidation(PathValidationResult result,
                          PathValidationFlags flags,
                          const SocketAddress& local_address,
                          const SocketAddress& remote_address);
  bool EmitClientHello();
  bool EmitOCSP();
  void EmitOCSPResponse();
  void EmitDatagram(Store&& datagram, DatagramReceivedFlag flag);
  void EmitKeylog(const char* line);
  void EmitNewStream(const BaseObjectPtr<Stream>& stream);
  void EmitVersionNegotiation(const ngtcp2_pkt_hd& hd,
                              const quic_version* sv,
                              size_t nsv);
  void EmitHandshakeComplete();
  void EmitDatagramAcknowledged(datagram_id id);
  void EmitDatagramLost(datagram_id id);

  void AcknowledgeStreamDataOffset(Stream* stream,
                                   uint64_t offset,
                                   uint64_t datalen);
  void ActivateConnectionId(uint64_t seq,
                            const CID& cid,
                            v8::Maybe<StatelessResetToken> maybe_reset_token);
  void DatagramAcknowledged(datagram_id datagramId);
  void DatagramLost(datagram_id id);
  void DatagramReceived(const uint8_t* data,
                        size_t datalen,
                        DatagramReceivedFlag flag);
  void DeactivateConnectionId(uint64_t seq,
                              const CID& cid,
                              v8::Maybe<StatelessResetToken> maybe_reset_token);
  void ExtendMaxStreamData(Stream* stream, uint64_t max);
  void ExtendMaxStreams(EndpointLabel label, Direction direction, uint64_t max);
  bool GenerateNewConnectionId(ngtcp2_cid* cid, size_t len, uint8_t* token);
  bool HandshakeCompleted();
  void HandshakeConfirmed();
  int ReceiveCryptoData(ngtcp2_crypto_level level,
                        uint64_t offset,
                        const uint8_t* data,
                        size_t datalen);
  bool ReceiveRxKey(ngtcp2_crypto_level level);
  bool ReceiveTxKey(ngtcp2_crypto_level level);
  void ReceiveNewToken(const ngtcp2_vec* token);
  void ReceiveStatelessReset(const ngtcp2_pkt_stateless_reset* sr);
  void ReceiveStreamData(Stream* stream,
                         Application::ReceiveStreamDataFlags flags,
                         uint64_t offset,
                         const uint8_t* data,
                         size_t datalen);
  void RemoveConnectionId(const CID& cid);
  void SelectPreferredAddress(const PreferredAddress& preferredAddress);
  void StreamClose(Stream* stream, v8::Maybe<QuicError> app_error_code);
  void StreamOpen(stream_id id);
  void StreamReset(Stream* stream,
                   uint64_t final_size,
                   QuicError app_error_code);
  void StreamStopSending(Stream* stream, QuicError app_error_code);
  bool InitApplication();

  void ReportPathValidationStatus(PathValidationResult result,
                                  PathValidationFlags flags,
                                  const SocketAddress& local_address,
                                  const SocketAddress& remote_address);

  void SessionTicketCallback(Store&& ticket, Store&& transport_params);

  using StreamsMap = std::unordered_map<stream_id, BaseObjectPtr<Stream>>;

  // Internal Fields

#define V(_, name, type) type name;
  struct State final {
    SESSION_STATE(V)
  };
#undef V

  ngtcp2_mem allocator_;
  Options options_;
  QuicConnectionPointer connection_;
  BaseObjectPtr<Endpoint> endpoint_;
  AliasedStruct<State> state_;
  StreamsMap streams_;

  datagram_id last_datagram_id_ = 0;

  CIDFactory& cid_factory_;
  BaseObjectPtr<BaseObject> maybe_cid_factory_ref_;

  SocketAddress local_address_;
  SocketAddress remote_address_;

  std::unique_ptr<Application> application_;
  CryptoContext crypto_context_;

  TimerWrapHandle timer_;

  // The CID of the remote peer. This value might change throughout the lifetime
  // of the session.
  CID dcid_;

  // The CID of *this* session. This value should not change, but other CIDs
  // might be generated to also identify this session.
  CID scid_;

  v8::Maybe<CID> ocid_ = v8::Nothing<CID>();

  // If this is a server session, and a preferred address is advertised, this is
  // the CID associated with the preferred address advertisement.
  CID preferred_address_cid_;

  ngtcp2_transport_params transport_params_;
  bool in_ng_callback_ = false;
  size_t send_scope_depth_ = 0;
  size_t connection_close_depth_ = 0;

  QuicError last_error_;
  BaseObjectPtr<Packet> conn_closebuf_;

  BaseObjectPtr<LogStream> qlogstream_;
  BaseObjectPtr<LogStream> keylogstream_;

  friend class CryptoContext;
  friend class Stream;
  friend class Http3Application;

  // ======================================================================================
  // MaybeCloseConnectionScope triggers sending a CONNECTION_CLOSE when not
  // executing within the context of an ngtcp2 callback and the session is in
  // the correct state.
  struct MaybeCloseConnectionScope;

  using CallbackScope = CallbackScopeBase<Session>;

  // The ngtcp2 callbacks are not re-entrant. The NgCallbackScope is used as a
  // guard in the static callback functions to prevent re-entry.
  struct NgCallbackScope;

  // ======================================================================================
  // ngtcp2 static callback functions
  static const ngtcp2_callbacks callbacks[2];

  static Session* From(ngtcp2_conn*, void* user_data);
  static int OnAcknowledgeStreamDataOffset(ngtcp2_conn* conn,
                                           int64_t stream_id,
                                           uint64_t offset,
                                           uint64_t datalen,
                                           void* user_data,
                                           void* stream_user_data);
  static int OnAcknowledgeDatagram(ngtcp2_conn* conn,
                                   uint64_t dgram_id,
                                   void* user_data);
  static int OnConnectionIdStatus(ngtcp2_conn* conn,
                                  int type,
                                  uint64_t seq,
                                  const ngtcp2_cid* cid,
                                  const uint8_t* token,
                                  void* user_data);
  static int OnExtendMaxRemoteStreamsBidi(ngtcp2_conn* conn,
                                          uint64_t max_streams,
                                          void* user_data);
  static int OnExtendMaxRemoteStreamsUni(ngtcp2_conn* conn,
                                         uint64_t max_streams,
                                         void* user_data);
  static int OnExtendMaxStreamsBidi(ngtcp2_conn* conn,
                                    uint64_t max_streams,
                                    void* user_data);
  static int OnExtendMaxStreamData(ngtcp2_conn* conn,
                                   int64_t stream_id,
                                   uint64_t max_data,
                                   void* user_data,
                                   void* stream_user_data);
  static int OnExtendMaxStreamsUni(ngtcp2_conn* conn,
                                   uint64_t max_streams,
                                   void* user_data);
  static int OnGetNewConnectionId(ngtcp2_conn* conn,
                                  ngtcp2_cid* cid,
                                  uint8_t* token,
                                  size_t cidlen,
                                  void* user_data);
  static int OnGetPathChallengeData(ngtcp2_conn* conn,
                                    uint8_t* data,
                                    void* user_data);
  static int OnHandshakeCompleted(ngtcp2_conn* conn, void* user_data);
  static int OnHandshakeConfirmed(ngtcp2_conn* conn, void* user_data);
  static int OnLostDatagram(ngtcp2_conn* conn,
                            uint64_t dgram_id,
                            void* user_data);
  static int OnPathValidation(ngtcp2_conn* conn,
                              uint32_t flags,
                              const ngtcp2_path* path,
                              ngtcp2_path_validation_result res,
                              void* user_data);
  static int OnReceiveCryptoData(ngtcp2_conn* conn,
                                 ngtcp2_crypto_level crypto_level,
                                 uint64_t offset,
                                 const uint8_t* data,
                                 size_t datalen,
                                 void* user_data);
  static int OnReceiveDatagram(ngtcp2_conn* conn,
                               uint32_t flags,
                               const uint8_t* data,
                               size_t datalen,
                               void* user_data);
  static int OnReceiveNewToken(ngtcp2_conn* conn,
                               const ngtcp2_vec* token,
                               void* user_data);
  static int OnReceiveRxKey(ngtcp2_conn* conn,
                            ngtcp2_crypto_level level,
                            void* user_data);
  static int OnReceiveStatelessReset(ngtcp2_conn* conn,
                                     const ngtcp2_pkt_stateless_reset* sr,
                                     void* user_data);
  static int OnReceiveStreamData(ngtcp2_conn* conn,
                                 uint32_t flags,
                                 int64_t stream_id,
                                 uint64_t offset,
                                 const uint8_t* data,
                                 size_t datalen,
                                 void* user_data,
                                 void* stream_user_data);
  static int OnReceiveTxKey(ngtcp2_conn* conn,
                            ngtcp2_crypto_level level,
                            void* user_data);
  static int OnReceiveVersionNegotiation(ngtcp2_conn* conn,
                                         const ngtcp2_pkt_hd* hd,
                                         const uint32_t* sv,
                                         size_t nsv,
                                         void* user_data);
  static int OnRemoveConnectionId(ngtcp2_conn* conn,
                                  const ngtcp2_cid* cid,
                                  void* user_data);
  static int OnSelectPreferredAddress(ngtcp2_conn* conn,
                                      ngtcp2_path* dest,
                                      const ngtcp2_preferred_addr* paddr,
                                      void* user_data);
  static int OnStreamClose(ngtcp2_conn* conn,
                           uint32_t flags,
                           int64_t stream_id,
                           uint64_t app_error_code,
                           void* user_data,
                           void* stream_user_data);
  static int OnStreamOpen(ngtcp2_conn* conn,
                          int64_t stream_id,
                          void* user_data);
  static int OnStreamReset(ngtcp2_conn* conn,
                           int64_t stream_id,
                           uint64_t final_size,
                           uint64_t app_error_code,
                           void* user_data,
                           void* stream_user_data);
  static int OnStreamStopSending(ngtcp2_conn* conn,
                                 int64_t stream_id,
                                 uint64_t app_error_code,
                                 void* user_data,
                                 void* stream_user_data);

  static void OnRand(uint8_t* dest,
                     size_t destlen,
                     const ngtcp2_rand_ctx* rand_ctx);
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
