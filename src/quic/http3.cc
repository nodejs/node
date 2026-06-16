#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <async_wrap-inl.h>
#include <base_object-inl.h>
#include <debug_utils-inl.h>
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node_errors.h>
#include <node_external_reference.h>
#include <node_http_common-inl.h>
#include <node_sockaddr-inl.h>
#include <util-inl.h>
#include <zlib.h>
#include <memory>
#include <unordered_map>
#include <utility>
#include "application.h"
#include "bindingdata.h"
#include "defs.h"
#include "http3.h"
#include "session.h"
#include "sessionticket.h"

namespace node {

using v8::Array;
using v8::BigInt;
using v8::Boolean;
using v8::DictionaryTemplate;
using v8::FunctionCallbackInfo;
using v8::Global;
using v8::HandleScope;
using v8::Integer;
using v8::Just;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Uint32;
using v8::Value;

namespace quic {

enum class HeadersKind : uint8_t {
  HINTS,
  INITIAL,
  TRAILING,
};

enum class HeadersFlags : uint8_t {
  NONE,
  TERMINAL,
};

enum class StreamPriority : uint8_t {
  DEFAULT = NGHTTP3_DEFAULT_URGENCY,
  LOW = NGHTTP3_URGENCY_LOW,
  HIGH = NGHTTP3_URGENCY_HIGH,
};

enum class StreamPriorityFlags : uint8_t {
  NON_INCREMENTAL,
  INCREMENTAL,
};

namespace {
constexpr uint8_t kSessionTicketAppDataVersion = 1;
// Layout: [version(1)][crc(4)][payload(34)] = 39 bytes.
constexpr size_t kSessionTicketAppDataSize = 39;
constexpr size_t kSessionTicketAppDataHeaderSize = 5;  // version + crc
constexpr size_t kSessionTicketAppDataPayloadSize =
    kSessionTicketAppDataSize - kSessionTicketAppDataHeaderSize;

inline void WriteBE32(uint8_t* buf, uint32_t val) {
  buf[0] = static_cast<uint8_t>((val >> 24) & 0xff);
  buf[1] = static_cast<uint8_t>((val >> 16) & 0xff);
  buf[2] = static_cast<uint8_t>((val >> 8) & 0xff);
  buf[3] = static_cast<uint8_t>(val & 0xff);
}

inline uint32_t ReadBE32(const uint8_t* buf) {
  return (static_cast<uint32_t>(buf[0]) << 24) |
         (static_cast<uint32_t>(buf[1]) << 16) |
         (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
}

inline void WriteBE64(uint8_t* buf, uint64_t val) {
  buf[0] = static_cast<uint8_t>((val >> 56) & 0xff);
  buf[1] = static_cast<uint8_t>((val >> 48) & 0xff);
  buf[2] = static_cast<uint8_t>((val >> 40) & 0xff);
  buf[3] = static_cast<uint8_t>((val >> 32) & 0xff);
  buf[4] = static_cast<uint8_t>((val >> 24) & 0xff);
  buf[5] = static_cast<uint8_t>((val >> 16) & 0xff);
  buf[6] = static_cast<uint8_t>((val >> 8) & 0xff);
  buf[7] = static_cast<uint8_t>(val & 0xff);
}

inline uint64_t ReadBE64(const uint8_t* buf) {
  return (static_cast<uint64_t>(buf[0]) << 56) |
         (static_cast<uint64_t>(buf[1]) << 48) |
         (static_cast<uint64_t>(buf[2]) << 40) |
         (static_cast<uint64_t>(buf[3]) << 32) |
         (static_cast<uint64_t>(buf[4]) << 24) |
         (static_cast<uint64_t>(buf[5]) << 16) |
         (static_cast<uint64_t>(buf[6]) << 8) | static_cast<uint64_t>(buf[7]);
}

// Serialize an nghttp3_pri into an RFC 9218 priority field value
// (e.g., "u=3" or "u=0, i"). Returns the number of bytes written.
// This is used only for setting the priority field of HTTP/3 streams on
// the client side.
inline size_t FormatPriority(char* buf, size_t buflen, const nghttp3_pri& pri) {
  int len;
  if (pri.inc) {
    len = snprintf(buf, buflen, "u=%d, i", pri.urgency);
  } else {
    len = snprintf(buf, buflen, "u=%d", pri.urgency);
  }
  return static_cast<size_t>(len);
}
}  // namespace

// ============================================================================
// The HTTP/3 application settings: the RFC 9114/9204 SETTINGS values
// advertised to the peer plus the local header-processing limits. The
// values are supplied by node:http3 (the `settings` option of
// http3.connect()/listen()), carried opaquely through Session::Options,
// and parsed here by the registered parse hook. The QUIC core never
// knows these field names.
struct Http3Settings final {
  // The maximum number of header pairs permitted for a Stream
  // (local enforcement limit, not a wire setting).
  uint64_t max_header_pairs = DEFAULT_MAX_HEADER_LIST_PAIRS;

  // The maximum total number of header bytes (including header
  // name and value) permitted for a Stream (local enforcement limit,
  // not a wire setting).
  uint64_t max_header_length = DEFAULT_MAX_HEADER_LENGTH;

  // The maximum header section size advertised to the peer in SETTINGS.
  // Defaults to match max_header_length so the SETTINGS frame accurately
  // reflects the enforcement limit. A value of 0 would incorrectly tell
  // the peer not to send any headers at all.
  uint64_t max_field_section_size = DEFAULT_MAX_HEADER_LENGTH;
  uint64_t qpack_max_dtable_capacity = 4096;
  uint64_t qpack_encoder_max_dtable_capacity = 4096;
  uint64_t qpack_blocked_streams = 100;

  bool enable_connect_protocol = true;

  // SETTINGS_H3_DATAGRAM (RFC 9297). HTTP/3 datagrams are not yet
  // supported, so this is always false and the setting is never
  // advertised. We reserve the setting logic to enable later.
  bool enable_datagrams = false;

  operator const nghttp3_settings() const;

  static v8::Maybe<Http3Settings> From(Environment* env,
                                       v8::Local<v8::Value> value);

  std::string ToString() const;

  v8::MaybeLocal<v8::Object> ToObject(Environment* env) const;
};

Http3Settings::operator const nghttp3_settings() const {
  return nghttp3_settings{
      .max_field_section_size = max_field_section_size,
      .qpack_max_dtable_capacity =
          static_cast<size_t>(qpack_max_dtable_capacity),
      .qpack_encoder_max_dtable_capacity =
          static_cast<size_t>(qpack_encoder_max_dtable_capacity),
      .qpack_blocked_streams = static_cast<size_t>(qpack_blocked_streams),
      .enable_connect_protocol = enable_connect_protocol,
      .h3_datagram = enable_datagrams,
      // origin_list is nullptr here because it is set directly on the
      // nghttp3_settings in Http3Application::InitializeConnection()
      // from the SNI configuration.
      .origin_list = nullptr,
      .glitch_ratelim_burst = 1000,
      .glitch_ratelim_rate = 33,
      .qpack_indexing_strat = NGHTTP3_QPACK_INDEXING_STRAT_EAGER,
  };
}

std::string Http3Settings::ToString() const {
  DebugIndentScope indent;
  auto prefix = indent.Prefix();
  std::string res("{");
  res += prefix + "max header pairs: " + std::to_string(max_header_pairs);
  res += prefix + "max header length: " + std::to_string(max_header_length);
  res += prefix +
         "max field section size: " + std::to_string(max_field_section_size);
  res += prefix + "qpack max dtable capacity: " +
         std::to_string(qpack_max_dtable_capacity);
  res += prefix + "qpack encoder max dtable capacity: " +
         std::to_string(qpack_encoder_max_dtable_capacity);
  res += prefix +
         "qpack blocked streams: " + std::to_string(qpack_blocked_streams);
  res += prefix + "enable connect protocol: " +
         (enable_connect_protocol ? std::string("yes") : std::string("no"));
  res += prefix + "enable datagrams: " +
         (enable_datagrams ? std::string("yes") : std::string("no"));
  res += indent.Close();
  return res;
}

Maybe<Http3Settings> Http3Settings::From(Environment* env,
                                         Local<Value> value) {
  if (value.IsEmpty() || !value->IsObject()) [[unlikely]] {
    THROW_ERR_INVALID_ARG_TYPE(env, "settings must be an object");
    return Nothing<Http3Settings>();
  }

  Http3Settings settings;
  auto& state = BindingData::Get(env);
  auto params = value.As<Object>();

#define SET(name)                                                              \
  SetOption<Http3Settings, &Http3Settings::name>(                              \
      env, &settings, params, state.name##_string())

  if (!SET(max_header_pairs) || !SET(max_header_length) ||
      !SET(max_field_section_size) || !SET(qpack_max_dtable_capacity) ||
      !SET(qpack_encoder_max_dtable_capacity) || !SET(qpack_blocked_streams) ||
      !SET(enable_connect_protocol)) {
    // The call to SetOption should have scheduled an exception to be thrown.
    return Nothing<Http3Settings>();
  }

#undef SET

  // Ensure the advertised max_field_section_size in SETTINGS is at least
  // as large as max_header_length. Otherwise the peer would be told to
  // restrict headers to a smaller size than the inbound header limits
  // (enforced in AddHeader) accept.
  if (settings.max_field_section_size < settings.max_header_length) {
    settings.max_field_section_size = settings.max_header_length;
  }

  return Just<Http3Settings>(settings);
}

MaybeLocal<Object> Http3Settings::ToObject(Environment* env) const {
  auto& binding_data = BindingData::Get(env);
  auto tmpl = binding_data.http3_settings_template();
  static constexpr std::string_view names[] = {
      "maxHeaderPairs",
      "maxHeaderLength",
      "maxFieldSectionSize",
      "qpackMaxDtableCapacity",
      "qpackEncoderMaxDtableCapacity",
      "qpackBlockedStreams",
      "enableConnectProtocol"
  };
  if (tmpl.IsEmpty()) {
    tmpl = DictionaryTemplate::New(env->isolate(), names);
    binding_data.set_http3_settings_template(tmpl);
  }
  MaybeLocal<Value> values[] = {
      BigInt::NewFromUnsigned(env->isolate(), max_header_pairs),
      BigInt::NewFromUnsigned(env->isolate(), max_header_length),
      BigInt::NewFromUnsigned(env->isolate(), max_field_section_size),
      BigInt::NewFromUnsigned(env->isolate(), qpack_max_dtable_capacity),
      BigInt::NewFromUnsigned(env->isolate(),
                              qpack_encoder_max_dtable_capacity),
      BigInt::NewFromUnsigned(env->isolate(), qpack_blocked_streams),
      Boolean::New(env->isolate(), enable_connect_protocol),
  };
  static_assert(std::size(values) == std::size(names));

  auto obj = tmpl->NewInstance(env->context(), values);
  if (obj->SetPrototypeV2(env->context(), Null(env->isolate())).IsNothing()) {
    return {};
  }
  return obj;
}

// The typed HTTP/3 session-ticket app data: the settings in effect when
// the ticket was issued, validated relax-tolerantly against the current
// settings on resume (0-RTT is blocked only when settings were
// tightened, not relaxed).
struct Http3TicketData final {
  uint64_t max_field_section_size;
  uint64_t qpack_max_dtable_capacity;
  uint64_t qpack_encoder_max_dtable_capacity;
  uint64_t qpack_blocked_streams;
  bool enable_connect_protocol;
  bool enable_datagrams;
};

struct Http3HeadersTraits {
  using nv_t = nghttp3_nv;
};

struct Http3RcBufferPointerTraits {
  using rcbuf_t = nghttp3_rcbuf;
  using vector_t = nghttp3_vec;

  static void inc(rcbuf_t* buf) {
    CHECK_NOT_NULL(buf);
    nghttp3_rcbuf_incref(buf);
  }
  static void dec(rcbuf_t* buf) {
    CHECK_NOT_NULL(buf);
    nghttp3_rcbuf_decref(buf);
  }
  static vector_t get_vec(rcbuf_t* buf) {
    CHECK_NOT_NULL(buf);
    return nghttp3_rcbuf_get_buf(buf);
  }
  static bool is_static(const rcbuf_t* buf) {
    CHECK_NOT_NULL(buf);
    return nghttp3_rcbuf_is_static(buf);
  }
};

using Http3ConnectionPointer = DeleteFnPtr<nghttp3_conn, nghttp3_conn_del>;
using Http3Headers = NgHeaders<Http3HeadersTraits>;
using Http3RcBufferPointer = NgRcBufPointer<Http3RcBufferPointerTraits>;

struct Http3HeaderTraits {
  typedef Http3RcBufferPointer rcbufferpointer_t;
  typedef BindingData allocator_t;

  static const char* ToHttpHeaderName(int32_t token) {
    switch (token) {
      case -1:
        return nullptr;
#define V(key, name)                                                           \
  case NGHTTP3_QPACK_TOKEN__##key:                                             \
    return name;
        HTTP_SPECIAL_HEADERS(V)
#undef V
#define V(key, name)                                                           \
  case NGHTTP3_QPACK_TOKEN_##key:                                              \
    return name;
        HTTP_REGULAR_HEADERS(V)
#undef V
    }
    return nullptr;
  }
};

using Http3Header = NgHeader<Http3HeaderTraits>;

// The Session::Application implementation for HTTP/3, which owns the nghttp3
// connection and all HTTP/3 protocol logic and state.
class Http3Application final : public Session::Application {
 public:
  Http3Application(Session* session, const Http3Settings& settings)
      : Application(session),
        allocator_(BindingData::Get(session->env()).nghttp3_allocator()),
        options_(settings),
        conn_(nullptr) {
    // Build the ORIGIN frame payload from the SNI configuration before
    // creating the nghttp3 connection, since InitializeConnection needs
    // the origin_vec_ to be ready for settings.origin_list.
    if (session->is_server()) {
      BuildOriginPayload();
    }
    conn_ = InitializeConnection();
  }

  // ==========================================================================
  // HTTP/3 events reported up to the session/JS layer. Each routes through
  // Session::DeferOrRun, so an event raised before the session is surfaced to
  // JS (in 0-RTT first-flight frames) is held and replayed after attach.

  void EmitGoaway(stream_id last_stream_id) {
    session().DeferOrRun([this, last_stream_id]() {
      Session& s = session();
      if (s.is_destroyed() || !s.env()->can_call_into_js()) return;
      CallbackScope<Session> cb_scope(&s);
      Local<Value> argv[] = {BigInt::New(s.env()->isolate(), last_stream_id)};
      s.MakeCallback(BindingData::Get(s.env()).session_goaway_callback(),
                     arraysize(argv),
                     argv);
    });
  }

  void EmitOrigins(std::vector<std::string>&& origins) {
    auto held = std::make_shared<std::vector<std::string>>(std::move(origins));
    session().DeferOrRun([this, held]() {
      Session& s = session();
      if (s.is_destroyed() || !s.env()->can_call_into_js()) return;
      if (!s.has_origin_listener()) return;
      CallbackScope<Session> cb_scope(&s);
      auto* isolate = s.env()->isolate();
      LocalVector<Value> elements(isolate, held->size());
      for (size_t i = 0; i < held->size(); i++) {
        Local<Value> str;
        if (!ToV8Value(s.env()->context(), (*held)[i]).ToLocal(&str))
            [[unlikely]] {
          return;
        }
        elements[i] = str;
      }
      Local<Value> argv[] = {
          Array::New(isolate, elements.data(), elements.size())};
      s.MakeCallback(BindingData::Get(s.env()).session_origin_callback(),
                     arraysize(argv),
                     argv);
    });
  }

  void EmitApplicationSettings() {
    session().DeferOrRun([this]() {
      Session& s = session();
      if (s.is_destroyed() || !s.env()->can_call_into_js()) return;
      CallbackScope<Session> cb_scope(&s);
      s.MakeCallback(BindingData::Get(s.env()).session_application_callback(),
                     0,
                     nullptr);
    });
  }

  // ==========================================================================
  // Session::Application

  error_code GetNoErrorCode() const override { return NGHTTP3_H3_NO_ERROR; }

  // HTTP/3 defines H3_INTERNAL_ERROR (0x102) for non-specific failures
  // initiated by the implementation; this is the right code to send
  // on RESET_STREAM when a stream is being aborted without an
  // application-supplied code.
  error_code GetInternalErrorCode() const override {
    return NGHTTP3_H3_INTERNAL_ERROR;
  }

  void EarlyDataRejected() override {
    // When 0-RTT is rejected, destroy the nghttp3 connection and all
    // open streams — ngtcp2 has discarded their internal state.
    // Reset started_ so Start() is called again via on_receive_rx_key
    // at 1RTT to recreate the nghttp3 connection. Use the
    // application's internal error code since this is an error
    // condition (code 0 would be treated as a clean close).
    conn_.reset();
    started_ = false;
    header_state_.clear();
    session().DestroyAllStreams(
        QuicError::ForApplication(GetInternalErrorCode()));
    if (!session().is_destroyed()) {
      session().EmitEarlyDataRejected();
    }
  }

  bool ReceiveStreamOpen(stream_id id) override {
    // In HTTP/3, only create Stream objects for bidirectional streams.
    // Unidirectional streams (control, QPACK encoder/decoder) are
    // managed internally by nghttp3 and should not be exposed to JS.
    if (!ngtcp2_is_bidi_stream(id)) return true;
    auto stream = session().CreateStream(id);
    if (!stream || session().is_destroyed()) [[unlikely]] {
      return !session().is_destroyed();
    }
    return true;
  }

  bool is_started() const override { return started_; }

  bool Start() override {
    if (started_) return true;
    Debug(&session(), "Starting HTTP/3 application.");

    const auto params = session().remote_transport_params();
    if (!params) [[unlikely]] {
      // The params are not available yet. Cannot start.
      Debug(&session(),
            "Cannot start HTTP/3 application yet. No remote transport params");
      return false;
    }

    if (params.initial_max_streams_uni() < 3) {
      // HTTP3 requires 3 unidirectional control streams to be opened in each
      // direction in additional to the bidirectional streams that are used to
      // actually carry request and response payload back and forth.
      // See:
      // https://nghttp2.org/nghttp3/programmers-guide.html#binding-control-streams
      Debug(&session(),
            "Cannot start HTTP/3 application. Initial max "
            "unidirectional streams [%" PRIu64
            "] is too low. Must be at least 3",
            params.initial_max_streams_uni());
      return false;
    }

    // If this is a server session, then set the maximum number of
    // bidirectional streams that can be created. This determines the number
    // of requests that the client can actually created.
    if (session().is_server()) {
      nghttp3_conn_set_max_client_streams_bidi(
          *this, params.initial_max_streams_bidi());
    }

    Debug(&session(), "Creating and binding HTTP/3 control streams");
    bool ret =
        session().OpenUni(&control_stream_id_) &&
        session().OpenUni(&qpack_enc_stream_id_) &&
        session().OpenUni(&qpack_dec_stream_id_) &&
        nghttp3_conn_bind_control_stream(*this, control_stream_id_) == 0 &&
        nghttp3_conn_bind_qpack_streams(
            *this, qpack_enc_stream_id_, qpack_dec_stream_id_) == 0;

    if (env()->enabled_debug_list()->enabled(DebugCategory::QUIC) && ret) {
      Debug(&session(),
            "Created and bound control stream %" PRIi64,
            control_stream_id_);
      Debug(&session(),
            "Created and bound qpack enc stream %" PRIi64,
            qpack_enc_stream_id_);
      Debug(&session(),
            "Created and bound qpack dec streams %" PRIi64,
            qpack_dec_stream_id_);
    }

    started_ = ret;
    return ret;
  }

  void BeginShutdown() override {
    // Only submit a shutdown notice if the H3 connection was fully
    // started (control streams bound). If the TLS handshake failed
    // before Start() was called, conn_ exists but its control streams
    // are unbound, and nghttp3_conn_submit_shutdown_notice would crash.
    if (conn_ && started_) nghttp3_conn_submit_shutdown_notice(*this);
  }

  void CompleteShutdown() override {
    // Same guard as BeginShutdown — nghttp3_conn_shutdown asserts
    // that the control stream is bound (conn->tx.ctrl != NULL).
    if (conn_ && started_) nghttp3_conn_shutdown(*this);
  }

  void PostReceive() override {
    if (pending_goaway_id_ < 0) return;
    stream_id goaway_id = pending_goaway_id_;
    pending_goaway_id_ = -1;

    bool is_notice =
        static_cast<uint64_t>(goaway_id) >= NGHTTP3_SHUTDOWN_NOTICE_STREAM_ID;

    // For the shutdown notice, replace the sentinel stream ID with -1
    // so JS sees a clean marker instead of a huge implementation detail.
    stream_id emit_id = is_notice ? -1 : goaway_id;

    if (!is_notice) {
      // Final GOAWAY: destroy client-initiated bidi streams with IDs >
      // goaway_id. These were not processed by the peer and can be retried.
      // Copy the map because Destroy() modifies it.
      auto streams = session().streams();
      for (auto& [id, stream] : streams) {
        if (session().is_destroyed()) return;
        if (ngtcp2_is_bidi_stream(id) && id > goaway_id) {
          stream->Destroy(
              QuicError::ForApplication(NGHTTP3_H3_REQUEST_REJECTED));
        }
      }
      if (session().is_destroyed()) return;
    }

    // Notify JS for both notice and final GOAWAY. The notice uses
    // -1 to signal "server is shutting down, stop new requests" without
    // implying any specific stream boundary. The final GOAWAY (if it
    // arrives separately) provides the exact stream ID for retry decisions.
    //
    // We do NOT call Close(GRACEFUL) here. The JS ongoaway handler sets
    // isPendingClose (preventing new streams). The session closes naturally
    // when the peer sends CONNECTION_CLOSE after all streams finish.
    // Calling Close(GRACEFUL) would send a GOAWAY back and trigger
    // BeginShutdown, which can interfere with in-progress streams.
    EmitGoaway(emit_id);
  }

  bool ReceiveStreamData(stream_id id,
                         const uint8_t* data,
                         size_t datalen,
                         const Stream::ReceiveDataFlags& flags,
                         void* stream_user_data) override {
    Debug(&session(),
          "HTTP/3 application received %zu bytes of data "
          "on stream %" PRIi64 ". Is final? %d. Is early? %d",
          datalen,
          id,
          flags.fin,
          flags.early);

    uint64_t ts = session().rx_packet_ts();
    if (ts == 0) [[unlikely]] {
      ts = uv_hrtime();
    }
    auto nread = nghttp3_conn_read_stream2(
        *this, id, data, datalen, flags.fin ? 1 : 0, ts);

    if (nread < 0) {
      Debug(&session(),
            "HTTP/3 application failed to read stream data: %s",
            nghttp3_strerror(nread));
      return false;
    }

    if (nread > 0) {
      Debug(&session(),
            "Extending stream and connection offset by %zd bytes",
            nread);
      session().Consume(id, nread);
    }

    // If this data arrived as 0-RTT, mark the stream. We set it after
    // nghttp3_conn_read_stream2 because the stream may not exist until
    // nghttp3 processes the headers (via on_begin_headers).
    if (flags.early) {
      if (auto stream = session().FindStream(id)) {
        stream->set_early();
      }
    }

    return true;
  }

  bool AcknowledgeStreamData(stream_id id, size_t datalen) override {
    Debug(&session(),
          "HTTP/3 application received acknowledgement for %zu bytes of data "
          "on stream %" PRIi64,
          datalen,
          id);
    return nghttp3_conn_add_ack_offset(*this, id, datalen) == 0;
  }

  bool stream_fin_managed_by_application() const override { return true; }

  void StreamWriteShut(stream_id id) override {
    nghttp3_conn_shutdown_stream_write(*this, id);
  }

  void BlockStream(stream_id id) override {
    nghttp3_conn_block_stream(*this, id);
  }

  void ResumeStream(stream_id id) override {
    nghttp3_conn_resume_stream(*this, id);
  }

  void ExtendMaxStreams(Direction direction, uint64_t max_streams) {
    Debug(&session(),
          "HTTP/3 application extending max %s streams by %" PRIu64,
          direction == Direction::BIDIRECTIONAL ? "bidi" : "uni",
          max_streams);
    session().ExtendMaxStreams(direction, max_streams);
  }

  void ExtendMaxStreamData(Stream* stream, uint64_t max_data) override {
    Debug(&session(),
          "HTTP/3 application extending max stream data to %" PRIu64,
          max_data);
    nghttp3_conn_unblock_stream(*this, stream->id());
  }

  void CollectSessionTicketAppData(
      SessionTicket::AppData* app_data) const override {
    uint8_t buf[kSessionTicketAppDataSize];
    buf[0] = kSessionTicketAppDataVersion;

    uint8_t* payload = buf + kSessionTicketAppDataHeaderSize;
    WriteBE64(payload, options_.max_field_section_size);
    WriteBE64(payload + 8, options_.qpack_max_dtable_capacity);
    WriteBE64(payload + 16, options_.qpack_encoder_max_dtable_capacity);
    WriteBE64(payload + 24, options_.qpack_blocked_streams);
    payload[32] = options_.enable_connect_protocol ? 1 : 0;
    payload[33] = options_.enable_datagrams ? 1 : 0;

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, payload, kSessionTicketAppDataPayloadSize);
    WriteBE32(buf + 1, static_cast<uint32_t>(crc));

    app_data->Set(
        uv_buf_init(reinterpret_cast<char*>(buf), kSessionTicketAppDataSize));
  }

  bool ApplySessionTicketData(const PendingTicketAppData& data) override {
    // The pending data was produced by this application's own ticket
    // hook (ParseHttp3Ticket below), which already validated it against
    // the configured settings at decrypt time.
    CHECK(data);
    const auto& ticket = *std::static_pointer_cast<const Http3TicketData>(data);
    // Re-validate that current settings are >= stored settings.
    return options_.max_field_section_size >= ticket.max_field_section_size &&
           options_.qpack_max_dtable_capacity >=
               ticket.qpack_max_dtable_capacity &&
           options_.qpack_encoder_max_dtable_capacity >=
               ticket.qpack_encoder_max_dtable_capacity &&
           options_.qpack_blocked_streams >= ticket.qpack_blocked_streams &&
           (!ticket.enable_connect_protocol ||
            options_.enable_connect_protocol) &&
           (!ticket.enable_datagrams || options_.enable_datagrams);
  }

  MaybeLocal<Object> GetSettingsObject(Environment* env) override {
    return options_.ToObject(env);
  }

  void ReceiveStreamClose(Stream* stream, QuicError&& error) override {
    Debug(
        &session(), "HTTP/3 application closing stream %" PRIi64, stream->id());
    error_code code = NGHTTP3_H3_NO_ERROR;
    if (error.type() == QuicError::Type::APPLICATION) {
      code = error.code();
    }

    int rv = nghttp3_conn_close_stream(*this, stream->id(), code);
    // If the call is successful, the Http3Application::OnStreamClose
    // callback will be invoked when the stream is ready to be closed. We'll
    // handle destroying the actual Stream object there.
    if (rv == 0) return;

    if (rv == NGHTTP3_ERR_STREAM_NOT_FOUND) {
      ExtendMaxStreams(stream->direction(), 1);
      return;
    }

    session().SetError(nghttp3_err_infer_quic_app_error_code(rv));
    session().Close();
  }

  void ReceiveStreamReset(Stream* stream,
                          uint64_t final_size,
                          QuicError&& error) override {
    // We are shutting down the readable side of the local stream here.
    Debug(&session(),
          "HTTP/3 application resetting stream %" PRIi64,
          stream->id());
    int rv = nghttp3_conn_shutdown_stream_read(*this, stream->id());
    if (rv == 0) {
      stream->ReceiveStreamReset(final_size, std::move(error));
      return;
    }

    session().SetError(nghttp3_err_infer_quic_app_error_code(rv));
    session().Close();
  }

  void StreamRemoved(stream_id id) override {
    if (conn_) nghttp3_conn_set_stream_user_data(*this, id, nullptr);
    header_state_.erase(id);
  }

  struct StreamPriorityResult {
    StreamPriority priority;
    StreamPriorityFlags flags;
  };

  bool SubmitHeaders(const Stream& stream,
                     const Local<Array>& headers,
                     HeadersFlags flags) {
    Session::SendPendingDataScope send_scope(&session());
    Http3Headers nva(env(), headers);
    static constexpr nghttp3_data_reader reader = {on_read_data_callback};
    const nghttp3_data_reader* reader_ptr =
        flags != HeadersFlags::TERMINAL ? &reader : nullptr;

    if (session().is_server()) {
      Debug(&session(),
            "Submitting %" PRIu64 " response headers for stream %" PRIu64,
            nva.length(),
            stream.id());
      return nghttp3_conn_submit_response(
                 *this, stream.id(), nva.data(), nva.length(), reader_ptr) == 0;
    }
    Debug(&session(),
          "Submitting %" PRIu64 " request headers for stream %" PRIu64,
          nva.length(),
          stream.id());
    return nghttp3_conn_submit_request(*this,
                                       stream.id(),
                                       nva.data(),
                                       nva.length(),
                                       reader_ptr,
                                       const_cast<Stream*>(&stream)) == 0;
  }

  bool SubmitInfo(const Stream& stream, const Local<Array>& headers) {
    if (!session().is_server()) return false;
    Session::SendPendingDataScope send_scope(&session());
    Http3Headers nva(env(), headers);
    Debug(&session(),
          "Submitting %" PRIu64 " early hints for stream %" PRIu64,
          nva.length(),
          stream.id());
    return nghttp3_conn_submit_info(
               *this, stream.id(), nva.data(), nva.length()) == 0;
  }

  bool SubmitTrailers(const Stream& stream, const Local<Array>& headers) {
    Session::SendPendingDataScope send_scope(&session());
    Http3Headers nva(env(), headers);
    Debug(&session(),
          "Submitting %" PRIu64 " trailing headers for stream %" PRIu64,
          nva.length(),
          stream.id());
    return nghttp3_conn_submit_trailers(
               *this, stream.id(), nva.data(), nva.length()) == 0;
  }

  void SetStreamPriority(const Stream& stream,
                         StreamPriority priority,
                         StreamPriorityFlags flags) {
    nghttp3_pri pri;
    pri.inc = (flags == StreamPriorityFlags::INCREMENTAL) ? 1 : 0;
    switch (priority) {
      case StreamPriority::HIGH:
        pri.urgency = NGHTTP3_URGENCY_HIGH;
        break;
      case StreamPriority::LOW:
        pri.urgency = NGHTTP3_URGENCY_LOW;
        break;
      default:
        pri.urgency = NGHTTP3_DEFAULT_URGENCY;
        break;
    }
    if (session().is_server()) {
      nghttp3_conn_set_server_stream_priority(*this, stream.id(), &pri);
    } else {
      // The client API takes a serialized RFC 9218 priority field value
      // (e.g., "u=0, i") rather than an nghttp3_pri struct.
      char buf[8];
      size_t len = FormatPriority(buf, sizeof(buf), pri);
      nghttp3_conn_set_client_stream_priority(
          *this, stream.id(), reinterpret_cast<const uint8_t*>(buf), len);
    }
  }

  StreamPriorityResult GetStreamPriority(const Stream& stream) {
    // nghttp3_conn_get_stream_priority is only available on the server
    // side, where it reflects the peer's requested priority (e.g., from
    // PRIORITY_UPDATE frames). The client tracks its own requested priority
    // in node:http3, and doesn't use this.
    if (!session().is_server()) {
      return {StreamPriority::DEFAULT, StreamPriorityFlags::NON_INCREMENTAL};
    }
    nghttp3_pri pri;
    if (nghttp3_conn_get_stream_priority(*this, &pri, stream.id()) == 0) {
      StreamPriority level;
      switch (pri.urgency) {
        case NGHTTP3_URGENCY_HIGH:
          level = StreamPriority::HIGH;
          break;
        case NGHTTP3_URGENCY_LOW:
          level = StreamPriority::LOW;
          break;
        default:
          level = StreamPriority::DEFAULT;
          break;
      }
      return {level,
              pri.inc ? StreamPriorityFlags::INCREMENTAL
                      : StreamPriorityFlags::NON_INCREMENTAL};
    }
    return {StreamPriority::DEFAULT, StreamPriorityFlags::NON_INCREMENTAL};
  }

  int GetStreamData(StreamData* data) override {
    static_assert(
        sizeof(ngtcp2_vec) == sizeof(nghttp3_vec) &&
            alignof(ngtcp2_vec) == alignof(nghttp3_vec) &&
            offsetof(ngtcp2_vec, base) == offsetof(nghttp3_vec, base) &&
            offsetof(ngtcp2_vec, len) == offsetof(nghttp3_vec, len),
        "ngtcp2_vec and nghttp3_vec must have identical layout");
    data->count = kMaxVectorCount;
    ssize_t ret = 0;
    Debug(&session(), "HTTP/3 application getting stream data");
    if (conn_ && session().max_data_left()) {
      ret = nghttp3_conn_writev_stream(
          *this,
          &data->id,
          &data->fin,
          reinterpret_cast<nghttp3_vec*>(data->data),
          data->count);
      // A negative return value indicates an error.
      if (ret < 0) {
        return static_cast<int>(ret);
      }

      data->count = static_cast<size_t>(ret);
      if (data->id >= 0 && data->id != control_stream_id_ &&
          data->id != qpack_dec_stream_id_ &&
          data->id != qpack_enc_stream_id_) {
        data->stream = session().FindStream(data->id);
      }
    } else {
      // Nothing can be pulled right now (connection flow control is
      // exhausted). Present an empty result: the caller reuses a single
      // StreamData across send-loop iterations, so the fields must not
      // be left holding the previous iteration's stream.
      data->id = -1;
      data->count = 0;
      data->fin = 0;
      data->stream.reset();
    }

    return 0;
  }

  bool StreamCommit(StreamData* data, size_t datalen) override {
    Debug(&session(),
          "HTTP/3 application committing stream %" PRIi64 " data %zu",
          data->id,
          datalen);
    // datalen is the total framed bytes consumed by ngtcp2, which includes
    // H3 frame overhead (HEADERS frame bytes, DATA frame type/length).
    // nghttp3 tracks its own offset via add_write_offset.
    int err = nghttp3_conn_add_write_offset(*this, data->id, datalen);
    if (err != 0) {
      session().SetError(nghttp3_err_infer_quic_app_error_code(err));
      return false;
    }
    // Raw application bytes are committed to the stream's outbound
    // immediately in on_read_data_callback (so that re-entrant
    // fill_outq calls see the advanced position). We only need to
    // propagate the fin flag here.
    if (data->stream && data->fin) {
      data->stream->Commit(0, true);
    }
    // After body data is committed, if on_read_data_callback signaled
    // EOF+NO_END_STREAM (trailers pending), emit the want-trailers
    // event to JS. This runs outside the NgHttp3CallbackScope so it's
    // safe to call into JS. The JS handler calls sendTrailers() which
    // calls nghttp3_conn_submit_trailers, queuing the TRAILERS frame
    // for the next writev_stream in the send loop.
    if (pending_trailers_stream_ == data->id) {
      pending_trailers_stream_ = -1;
      if (data->stream) EmitWantTrailers(data->stream.get());
    }
    return true;
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Http3Application)
  SET_SELF_SIZE(Http3Application)

 private:
  inline operator nghttp3_conn*() const {
    DCHECK_NOT_NULL(conn_.get());
    return conn_.get();
  }

  inline bool is_control_stream(stream_id id) const {
    return id == control_stream_id_ || id == qpack_dec_stream_id_ ||
           id == qpack_enc_stream_id_;
  }

  void BuildOriginPayload() {
    // Build the serialized ORIGIN frame payload from the SNI configuration.
    // Each origin entry is: 2-byte BE length + origin string.
    // Wildcard ('*') entries and entries with authoritative=false are skipped.
    auto& sni = session().config().options.sni;
    for (auto& [hostname, opts] : sni) {
      if (hostname == "*" || !opts.authoritative) continue;
      std::string origin = "https://";
      origin += hostname;
      if (opts.port != 443) {
        origin += ":";
        origin += std::to_string(opts.port);
      }
      // 2-byte BE length prefix
      uint16_t len = static_cast<uint16_t>(origin.size());
      origin_payload_.push_back(static_cast<uint8_t>((len >> 8) & 0xff));
      origin_payload_.push_back(static_cast<uint8_t>(len & 0xff));
      // Origin string bytes
      origin_payload_.insert(
          origin_payload_.end(), origin.begin(), origin.end());
    }
    if (!origin_payload_.empty()) {
      origin_vec_ = {origin_payload_.data(), origin_payload_.size()};
    }
  }

  Http3ConnectionPointer InitializeConnection() {
    nghttp3_conn* conn = nullptr;
    nghttp3_settings settings = options_;
    if (!origin_payload_.empty()) {
      settings.origin_list = &origin_vec_;
    }
    if (session().is_server()) {
      CHECK_EQ(nghttp3_conn_server_new(
                   &conn, &kCallbacks, &settings, allocator_, this),
               0);
    } else {
      CHECK_EQ(nghttp3_conn_client_new(
                   &conn, &kCallbacks, &settings, allocator_, this),
               0);
    }
    return Http3ConnectionPointer(conn);
  }

  void OnStreamClose(Stream* stream, error_code app_error_code) {
    header_state_.erase(stream->id());
    if (app_error_code != NGHTTP3_H3_NO_ERROR) {
      Debug(&session(),
            "HTTP/3 application received stream close for stream %" PRIi64
            " with code %" PRIu64,
            stream->id(),
            app_error_code);
    }
    auto direction = stream->direction();
    stream->Destroy(QuicError::ForApplication(app_error_code));
    ExtendMaxStreams(direction, 1);
  }

  void OnBeginHeaders(stream_id id) {
    auto stream = FindOrCreateStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application beginning initial block of headers for stream "
          "%" PRIi64,
          id);
    auto& hs = header_state_[id];
    hs.headers.clear();
    hs.headers_length = 0;
    hs.kind = HeadersKind::INITIAL;
  }

  void OnReceiveHeader(stream_id id, std::unique_ptr<Http3Header> header) {
    auto stream = session().FindStream(id);

    if (!stream) [[unlikely]]
      return;
    if (header->name() == ":status" && header->value()[0] == '1') {
      Debug(&session(),
            "HTTP/3 application switching to hints headers for stream %" PRIi64,
            stream->id());
      header_state_[id].kind = HeadersKind::HINTS;
    }
    IF_QUIC_DEBUG(env()) {
      Debug(&session(),
            "Received header \"%s: %s\"",
            header->name(),
            header->value());
    }
    AddHeader(id, std::move(header));
  }

  void OnEndHeaders(stream_id id, int fin) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application received end of headers for stream %" PRIi64,
          id);
    EmitHeaders(stream.get());
    if (fin) {
      // The stream is done. There's no more data to receive!
      Debug(&session(), "Headers are final for stream %" PRIi64, id);
      Stream::ReceiveDataFlags flags{
          .fin = true,
          .early = false,
      };
      stream->ReceiveData(nullptr, 0, flags);
    }
  }

  void OnBeginTrailers(stream_id id) {
    auto stream = FindOrCreateStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application beginning block of trailers for stream %" PRIi64,
          id);
    auto& hs = header_state_[id];
    hs.headers.clear();
    hs.headers_length = 0;
    hs.kind = HeadersKind::TRAILING;
  }

  void OnReceiveTrailer(stream_id id, std::unique_ptr<Http3Header> header) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    IF_QUIC_DEBUG(env()) {
      Debug(&session(),
            "Received header \"%s: %s\"",
            header->name(),
            header->value());
    }
    AddHeader(id, std::move(header));
  }

  void OnEndTrailers(stream_id id, int fin) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application received end of trailers for stream %" PRIi64,
          id);
    EmitHeaders(stream.get());
    if (fin) {
      Debug(&session(), "Trailers are final for stream %" PRIi64, id);
      Stream::ReceiveDataFlags flags{
          .fin = true,
          .early = false,
      };
      stream->ReceiveData(nullptr, 0, flags);
    }
  }

  void AddHeader(stream_id id, std::unique_ptr<Http3Header> header) {
    auto& hs = header_state_[id];
    size_t len = header->length();
    if (hs.headers.size() >= options_.max_header_pairs ||
        hs.headers_length + len > options_.max_header_length) {
      return;
    }
    hs.headers_length += len;
    hs.headers.push_back(std::move(header));
  }

  void EmitHeaders(Stream* stream) {
    auto it = header_state_.find(stream->id());
    if (it == header_state_.end()) return;
    auto& hs = it->second;
    if (!env()->can_call_into_js() || !stream->wants_headers()) {
      hs.headers.clear();
      hs.headers_length = 0;
      return;
    }
    CallbackScope<Stream> cb_scope(stream);

    auto& binding = BindingData::Get(env());
    size_t count = hs.headers.size() * 2;
    LocalVector<Value> values(env()->isolate(), count);

    for (size_t i = 0; i < hs.headers.size(); i++) {
      Local<Value> name;
      Local<Value> value;
      if (!hs.headers[i]->GetName(&binding).ToLocal(&name) ||
          !hs.headers[i]->GetValue(&binding).ToLocal(&value)) [[unlikely]] {
        hs.headers.clear();
        hs.headers_length = 0;
        return;
      }
      values[i * 2] = name;
      values[i * 2 + 1] = value;
    }

    auto kind = hs.kind;
    hs.headers.clear();
    hs.headers_length = 0;

    Local<Value> argv[] = {
        Array::New(env()->isolate(), values.data(), count),
        Integer::NewFromUnsigned(env()->isolate(),
                                 static_cast<uint32_t>(kind))};

    stream->MakeCallback(
        binding.stream_headers_callback(), arraysize(argv), argv);
  }

  void EmitWantTrailers(Stream* stream) {
    if (!env()->can_call_into_js() || !stream->wants_trailers()) {
      return;
    }
    CallbackScope<Stream> cb_scope(stream);
    stream->MakeCallback(
        BindingData::Get(env()).stream_trailers_callback(), 0, nullptr);
  }

  void OnEndStream(stream_id id) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application received end of stream for stream %" PRIi64,
          id);
    Stream::ReceiveDataFlags flags{
        .fin = true,
        .early = false,
    };
    stream->ReceiveData(nullptr, 0, flags);
  }

  void OnStopSending(stream_id id, error_code app_error_code) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application received stop sending for stream %" PRIi64,
          id);
    stream->ReceiveStopSending(QuicError::ForApplication(app_error_code));
  }

  void OnResetStream(stream_id id, error_code app_error_code) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application received reset stream for stream %" PRIi64,
          id);
    stream->ReceiveStreamReset(0, QuicError::ForApplication(app_error_code));
  }

  void OnShutdown(stream_id id) {
    // The peer has sent a GOAWAY frame. This callback fires inside
    // NgHttp3CallbackScope, so we cannot call into JS, destroy streams,
    // or enter Close(GRACEFUL) here (which could trigger FinishClose and
    // deferred destroy, preventing PostReceive from running).
    //
    // Store the GOAWAY stream ID — PostReceive() handles everything
    // outside all callback scopes. For the shutdown notice (first phase,
    // sentinel ID), we still store it so PostReceive knows to enter
    // graceful close mode. For the final GOAWAY (real stream ID), we
    // overwrite with the lower value.
    Debug(&session(), "HTTP/3 received GOAWAY (id=%" PRIi64 ")", id);
    pending_goaway_id_ = id;
  }

  void OnReceiveSettings(const nghttp3_proto_settings* settings) {
    options_.enable_connect_protocol = settings->enable_connect_protocol;
    options_.max_field_section_size = settings->max_field_section_size;
    options_.qpack_blocked_streams = settings->qpack_blocked_streams;
    options_.qpack_max_dtable_capacity = settings->qpack_max_dtable_capacity;

    Debug(&session(),
          "HTTP/3 application received updated settings: %s",
          options_);
    // Report the negotiated settings up to the session/JS layer.
    EmitApplicationSettings();
  }

  // Inbound header-block accumulation, keyed by stream id. Entries are
  // created by the header callbacks and erased on stream close (or
  // wholesale on 0-RTT rejection).
  struct StreamHeaderState {
    std::vector<std::unique_ptr<Http3Header>> headers;
    size_t headers_length = 0;
    HeadersKind kind = HeadersKind::INITIAL;
  };
  std::unordered_map<stream_id, StreamHeaderState> header_state_;
  bool started_ = false;
  nghttp3_mem* allocator_;
  Http3Settings options_;
  Http3ConnectionPointer conn_;
  stream_id control_stream_id_ = -1;
  stream_id qpack_dec_stream_id_ = -1;
  stream_id qpack_enc_stream_id_ = -1;

  // Set by on_read_data_callback when EOF+NO_END_STREAM (trailers pending).
  // Consumed by StreamCommit to trigger EmitWantTrailers outside the
  // nghttp3 callback scope.
  stream_id pending_trailers_stream_ = -1;

  // Set by OnShutdown when the peer sends a final GOAWAY. Consumed by
  // PostReceive() outside all callback scopes to destroy rejected
  // streams and notify JS.
  stream_id pending_goaway_id_ = -1;

  // ORIGIN frame support (RFC 9412).
  // origin_payload_ holds the serialized ORIGIN frame payload for sending.
  // origin_vec_ points into origin_payload_ for nghttp3_settings.origin_list.
  // received_origins_ accumulates origins from received ORIGIN frames.
  std::vector<uint8_t> origin_payload_;
  nghttp3_vec origin_vec_{nullptr, 0};
  std::vector<std::string> received_origins_;

  // ==========================================================================
  // Static callbacks

  static Http3Application* From(nghttp3_conn* conn, void* user_data) {
    DCHECK_NOT_NULL(user_data);
    auto app = static_cast<Http3Application*>(user_data);
    DCHECK_EQ(conn, app->conn_.get());
    return app;
  }

  // Persist the Stream* in the stream user data, so we can look it
  // up directly without a FindStream map lookup every time.
  void BindStreamUserData(stream_id id, Stream* stream) {
    if (conn_) nghttp3_conn_set_stream_user_data(*this, id, stream);
  }

  BaseObjectWeakPtr<Stream> FindOrCreateStream(stream_id id) {
    if (auto stream = session().FindStream(id)) {
      BindStreamUserData(id, stream.get());
      return stream;
    }
    if (auto stream = session().CreateStream(id)) {
      BindStreamUserData(id, stream.get());
      return stream;
    }
    return {};
  }

#define NGHTTP3_CALLBACK_SCOPE(name)                                           \
  auto ptr = From(conn, conn_user_data);                                       \
  CHECK_NOT_NULL(ptr);                                                         \
  auto& name = *ptr;                                                           \
  NgHttp3CallbackScope scope(&name.session());

  static nghttp3_ssize on_read_data_callback(nghttp3_conn* conn,
                                             stream_id id,
                                             nghttp3_vec* vec,
                                             size_t veccnt,
                                             uint32_t* pflags,
                                             void* conn_user_data,
                                             void* stream_user_data) {
    auto ptr = From(conn, conn_user_data);
    CHECK_NOT_NULL(ptr);
    auto& app = *ptr;
    NgHttp3CallbackScope scope(&app.session());

    BaseObjectPtr<Stream> stream(static_cast<Stream*>(stream_user_data));
    if (!stream) [[unlikely]] {
      stream = app.session().FindStream(id);
      if (!stream) return NGHTTP3_ERR_CALLBACK_FAILURE;
    }

    if (stream->is_eos()) {
      *pflags |= NGHTTP3_DATA_FLAG_EOF;
      if (stream->wants_trailers()) {
        *pflags |= NGHTTP3_DATA_FLAG_NO_END_STREAM;
        app.pending_trailers_stream_ = id;
      }
      return 0;
    }

    size_t max_count = std::min(veccnt, static_cast<size_t>(kMaxVectorCount));
    nghttp3_ssize result = 0;

    auto next =
        [&](int status, const ngtcp2_vec* data, size_t count, bob::Done done) {
          switch (status) {
            case bob::Status::STATUS_BLOCK:
            case bob::Status::STATUS_WAIT:
              result = NGHTTP3_ERR_WOULDBLOCK;
              return;
            case bob::Status::STATUS_EOS:
              *pflags |= NGHTTP3_DATA_FLAG_EOF;
              if (stream->wants_trailers()) {
                *pflags |= NGHTTP3_DATA_FLAG_NO_END_STREAM;
                app.pending_trailers_stream_ = id;
              }
              break;
          }
          count = std::min(count, max_count);
          // nghttp3 requires read_data to return either data (count > 0),
          // EOF, or WOULDBLOCK. A STATUS_CONTINUE with 0 vecs means the
          // outbound has no uncommitted data right now (e.g., all data was
          // already committed on a previous call, or the DataQueue is empty
          // but not yet capped). Map this to WOULDBLOCK so nghttp3 sets
          // READ_DATA_BLOCKED and waits for ResumeStream.
          if (count == 0 && !((*pflags) & NGHTTP3_DATA_FLAG_EOF)) {
            result = NGHTTP3_ERR_WOULDBLOCK;
            return;
          }
          size_t raw_bytes = 0;
          for (size_t n = 0; n < count; n++) {
            vec[n].base = data[n].base;
            vec[n].len = data[n].len;
            raw_bytes += data[n].len;
          }
          // Commit the raw application bytes immediately so that the
          // next Pull (if fill_outq re-enters read_data) sees the
          // advanced position. Commit only moves the offset — the
          // underlying buffers stay valid until Acknowledge.
          if (raw_bytes > 0) {
            stream->Commit(raw_bytes);
          }
          result = static_cast<nghttp3_ssize>(count);
        };

    ngtcp2_vec data[kMaxVectorCount];
    stream->Pull(std::move(next),
                 bob::Options::OPTIONS_SYNC,
                 data,
                 max_count,
                 max_count);

    return result;
  }

  static int on_acked_stream_data(nghttp3_conn* conn,
                                  stream_id id,
                                  uint64_t datalen,
                                  void* conn_user_data,
                                  void* stream_user_data) {
    // This callback is invoked by nghttp3_conn_add_ack_offset() (called
    // from Http3Application::AcknowledgeStreamData). We must NOT call
    // AcknowledgeStreamData here — that would re-enter nghttp3 via
    // nghttp3_conn_add_ack_offset, triggering the NgHttp3CallbackScope
    // re-entrancy assertion. Instead, directly notify the stream that data
    // was acknowledged, which is what the base Application implementation
    // does.
    auto ptr = From(conn, conn_user_data);
    CHECK_NOT_NULL(ptr);
    auto& app = *ptr;
    BaseObjectPtr<Stream> stream(static_cast<Stream*>(stream_user_data));
    if (!stream) [[unlikely]] {
      stream = app.session().FindStream(id);
    }
    if (stream) {
      stream->Acknowledge(static_cast<size_t>(datalen));
    }
    return NGTCP2_SUCCESS;
  }

  static int on_stream_close(nghttp3_conn* conn,
                             stream_id id,
                             error_code app_error_code,
                             void* conn_user_data,
                             void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (auto stream = app.session().FindStream(id)) {
      app.OnStreamClose(stream.get(), app_error_code);
    }
    return NGTCP2_SUCCESS;
  }

  static int on_receive_data(nghttp3_conn* conn,
                             stream_id id,
                             const uint8_t* data,
                             size_t datalen,
                             void* conn_user_data,
                             void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    // The on_receive_data callback will never be called for control streams,
    // so we know that if we get here, the data received is for a stream that
    // we know is for an HTTP payload.
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    BaseObjectPtr<Stream> stream(static_cast<Stream*>(stream_user_data));
    if (!stream) [[unlikely]] {
      if (auto created = app.FindOrCreateStream(id)) {
        created->ReceiveData(data, datalen, Stream::ReceiveDataFlags{});
        return NGTCP2_SUCCESS;
      }
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    stream->ReceiveData(data, datalen, Stream::ReceiveDataFlags{});
    return NGTCP2_SUCCESS;
  }

  static int on_deferred_consume(nghttp3_conn* conn,
                                 stream_id id,
                                 size_t consumed,
                                 void* conn_user_data,
                                 void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    Debug(&app.session(),
          "HTTP/3 application deferred consume %zu bytes",
          consumed);
    app.session().Consume(id, consumed);
    return NGTCP2_SUCCESS;
  }

  static int on_begin_headers(nghttp3_conn* conn,
                              stream_id id,
                              void* conn_user_data,
                              void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    app.OnBeginHeaders(id);
    return NGTCP2_SUCCESS;
  }

  static int on_receive_header(nghttp3_conn* conn,
                               stream_id id,
                               int32_t token,
                               nghttp3_rcbuf* name,
                               nghttp3_rcbuf* value,
                               uint8_t flags,
                               void* conn_user_data,
                               void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    if (Http3Header::IsZeroLength(token, name, value)) return NGTCP2_SUCCESS;
    app.OnReceiveHeader(
        id,
        std::make_unique<Http3Header>(app.env(), token, name, value, flags));
    return NGTCP2_SUCCESS;
  }

  static int on_end_headers(nghttp3_conn* conn,
                            stream_id id,
                            int fin,
                            void* conn_user_data,
                            void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    app.OnEndHeaders(id, fin);
    return NGTCP2_SUCCESS;
  }

  static int on_begin_trailers(nghttp3_conn* conn,
                               stream_id id,
                               void* conn_user_data,
                               void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    app.OnBeginTrailers(id);
    return NGTCP2_SUCCESS;
  }

  static int on_receive_trailer(nghttp3_conn* conn,
                                stream_id id,
                                int32_t token,
                                nghttp3_rcbuf* name,
                                nghttp3_rcbuf* value,
                                uint8_t flags,
                                void* conn_user_data,
                                void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    if (Http3Header::IsZeroLength(token, name, value)) return NGTCP2_SUCCESS;
    app.OnReceiveTrailer(
        id,
        std::make_unique<Http3Header>(app.env(), token, name, value, flags));
    return NGTCP2_SUCCESS;
  }

  static int on_end_trailers(nghttp3_conn* conn,
                             stream_id id,
                             int fin,
                             void* conn_user_data,
                             void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    app.OnEndTrailers(id, fin);
    return NGTCP2_SUCCESS;
  }

  static int on_end_stream(nghttp3_conn* conn,
                           stream_id id,
                           void* conn_user_data,
                           void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    app.OnEndStream(id);
    return NGTCP2_SUCCESS;
  }

  static int on_stop_sending(nghttp3_conn* conn,
                             stream_id id,
                             error_code app_error_code,
                             void* conn_user_data,
                             void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    app.OnStopSending(id, app_error_code);
    return NGTCP2_SUCCESS;
  }

  static int on_reset_stream(nghttp3_conn* conn,
                             stream_id id,
                             error_code app_error_code,
                             void* conn_user_data,
                             void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (app.is_control_stream(id)) [[unlikely]] {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    app.OnResetStream(id, app_error_code);
    return NGTCP2_SUCCESS;
  }

  static int on_shutdown(nghttp3_conn* conn,
                         stream_id id,
                         void* conn_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    app.OnShutdown(id);
    return NGTCP2_SUCCESS;
  }

  static int on_receive_settings(nghttp3_conn* conn,
                                 const nghttp3_proto_settings* settings,
                                 void* conn_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    app.OnReceiveSettings(settings);
    return NGTCP2_SUCCESS;
  }

  static int on_receive_origin(nghttp3_conn* conn,
                               const uint8_t* origin,
                               size_t originlen,
                               void* conn_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    app.received_origins_.emplace_back(reinterpret_cast<const char*>(origin),
                                       originlen);
    return NGTCP2_SUCCESS;
  }

  static int on_end_origin(nghttp3_conn* conn, void* conn_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    if (!app.received_origins_.empty()) {
      app.EmitOrigins(std::move(app.received_origins_));
      app.received_origins_.clear();
    }
    return NGTCP2_SUCCESS;
  }

  static void on_rand(uint8_t* dest, size_t destlen) {
    CHECK(ncrypto::CSPRNG(dest, destlen));
  }

  static constexpr nghttp3_callbacks kCallbacks = {
      on_acked_stream_data,
      on_stream_close,
      on_receive_data,
      on_deferred_consume,
      on_begin_headers,
      on_receive_header,
      on_end_headers,
      on_begin_trailers,
      on_receive_trailer,
      on_end_trailers,
      on_stop_sending,
      on_end_stream,
      on_reset_stream,
      on_shutdown,
      nullptr,  // recv_settings (deprecated)
      on_receive_origin,
      on_end_origin,
      on_rand,
      on_receive_settings};
};

// The per-session JS-facing handle for HTTP/3 stream operations
// (kHttp3Handle). One per session, with a weak reference to the Session,
// operating on the Application reached from the passed stream handle.
class Http3Binding final : public BaseObject {
 public:
  static BaseObjectPtr<Http3Binding> Create(Session* session);

  Http3Binding(Environment* env, Local<Object> object)
      : BaseObject(env, object) {
    MakeWeak();
  }

  JS_CONSTRUCTOR(Http3Binding);

  JS_METHOD(SendHeaders);
  JS_METHOD(SendInformationalHeaders);
  JS_METHOD(SendTrailers);
  JS_METHOD(SetPriority);
  JS_METHOD(GetPriority);

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Http3Binding)
  SET_SELF_SIZE(Http3Binding)
};

JS_CONSTRUCTOR_IMPL(Http3Binding, http3binding_constructor_template, {
  JS_ILLEGAL_CONSTRUCTOR();
  JS_CLASS(http3binding);
  SetProtoMethod(env->isolate(), tmpl, "sendHeaders", SendHeaders);
  SetProtoMethod(env->isolate(),
                 tmpl,
                 "sendInformationalHeaders",
                 SendInformationalHeaders);
  SetProtoMethod(env->isolate(), tmpl, "sendTrailers", SendTrailers);
  SetProtoMethod(env->isolate(), tmpl, "setPriority", SetPriority);
  SetProtoMethodNoSideEffect(env->isolate(), tmpl, "getPriority", GetPriority);
})

BaseObjectPtr<Http3Binding> Http3Binding::Create(Session* session) {
  JS_NEW_INSTANCE_OR_RETURN(session->env(), obj, BaseObjectPtr<Http3Binding>());
  return MakeBaseObject<Http3Binding>(session->env(), obj);
}

void Http3Binding::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(SendHeaders);
  registry->Register(SendInformationalHeaders);
  registry->Register(SendTrailers);
  registry->Register(SetPriority);
  registry->Register(GetPriority);
}

// The binding is only ever attached to an HTTP/3 session, so the session's
// installed application is always an Http3Application.
inline Http3Application& Http3App(Session& session) {
  return static_cast<Http3Application&>(session.application());
}

JS_METHOD_IMPL(Http3Binding::SendHeaders) {
  Http3Binding* binding;
  ASSIGN_OR_RETURN_UNWRAP(&binding, args.This());
  CHECK(args[1]->IsArray());   // headers
  CHECK(args[2]->IsUint32());  // flags

  Stream* stream = BaseObject::Unwrap<Stream>(args[0]);
  if (stream == nullptr) return args.GetReturnValue().Set(false);

  Session& session = stream->session();
  if (!session.has_application()) return args.GetReturnValue().Set(false);

  Local<Array> headers = args[1].As<Array>();
  auto flags = static_cast<HeadersFlags>(args[2].As<Uint32>()->Value());

  // A pending stream has no id yet; defer the submission until the transport
  // opens it (the priority header, if any, rides along in this header block).
  if (stream->is_pending()) {
    auto held = std::make_shared<Global<Array>>(binding->env()->isolate(),
                                                headers);
    stream->RunWhenOpen([stream, flags, held]() {
      Session& session = stream->session();
      if (!session.has_application()) return;
      Http3Application& app = Http3App(session);
      Environment* env = stream->env();
      HandleScope scope(env->isolate());
      if (!app.SubmitHeaders(*stream, held->Get(env->isolate()), flags)) {
        stream->Destroy(QuicError::ForApplication(app.GetInternalErrorCode()));
      }
    });
    return args.GetReturnValue().Set(true);
  }

  args.GetReturnValue().Set(
      Http3App(session).SubmitHeaders(*stream, headers, flags));
}

// Informational/trailing headers are only sent on open streams so they need
// no pending-stream deferral.
JS_METHOD_IMPL(Http3Binding::SendInformationalHeaders) {
  Http3Binding* binding;
  ASSIGN_OR_RETURN_UNWRAP(&binding, args.This());
  CHECK(args[1]->IsArray());  // headers
  Stream* stream = BaseObject::Unwrap<Stream>(args[0]);
  if (stream == nullptr || stream->is_pending())
    return args.GetReturnValue().Set(false);
  Session& session = stream->session();
  if (!session.has_application()) return args.GetReturnValue().Set(false);
  args.GetReturnValue().Set(
      Http3App(session).SubmitInfo(*stream, args[1].As<Array>()));
}

JS_METHOD_IMPL(Http3Binding::SendTrailers) {
  Http3Binding* binding;
  ASSIGN_OR_RETURN_UNWRAP(&binding, args.This());
  CHECK(args[1]->IsArray());  // headers
  Stream* stream = BaseObject::Unwrap<Stream>(args[0]);
  if (stream == nullptr || stream->is_pending())
    return args.GetReturnValue().Set(false);
  Session& session = stream->session();
  if (!session.has_application()) return args.GetReturnValue().Set(false);
  args.GetReturnValue().Set(
      Http3App(session).SubmitTrailers(*stream, args[1].As<Array>()));
}

JS_METHOD_IMPL(Http3Binding::SetPriority) {
  Http3Binding* binding;
  ASSIGN_OR_RETURN_UNWRAP(&binding, args.This());
  CHECK(args[1]->IsUint32());  // packed: (urgency << 1) | incremental

  Stream* stream = BaseObject::Unwrap<Stream>(args[0]);
  if (stream == nullptr) return;
  Session& session = stream->session();
  if (!session.has_application()) return;

  uint32_t packed = args[1].As<Uint32>()->Value();
  auto priority = static_cast<StreamPriority>(packed >> 1);
  StreamPriorityFlags flags = (packed & 1)
                                  ? StreamPriorityFlags::INCREMENTAL
                                  : StreamPriorityFlags::NON_INCREMENTAL;

  // A PRIORITY_UPDATE needs the stream to exist in nghttp3, which only happens
  // once the deferred header submission runs; defer until the stream opens.
  if (stream->is_pending()) {
    stream->RunWhenOpen([stream, priority, flags]() {
      if (stream->session().has_application()) {
        Http3App(stream->session()).SetStreamPriority(*stream, priority, flags);
      }
    });
    return;
  }
  Http3App(session).SetStreamPriority(*stream, priority, flags);
}

JS_METHOD_IMPL(Http3Binding::GetPriority) {
  Http3Binding* binding;
  ASSIGN_OR_RETURN_UNWRAP(&binding, args.This());
  Stream* stream = BaseObject::Unwrap<Stream>(args[0]);
  if (stream == nullptr) return;
  Session& session = stream->session();
  if (!session.has_application() || stream->is_pending()) return;

  auto result = Http3App(session).GetStreamPriority(*stream);
  uint32_t packed =
      (static_cast<uint32_t>(result.priority) << 1) |
      (result.flags == StreamPriorityFlags::INCREMENTAL ? 1 : 0);
  args.GetReturnValue().Set(packed);
}

namespace {
std::unique_ptr<Session::Application> CreateHttp3Application(Session* session);
}  // namespace

void CreateHttp3Handle(const FunctionCallbackInfo<Value>& args) {
  Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args[0]);

  if (!session->has_application()) {
    if (session->is_active()) {
      THROW_ERR_INVALID_STATE(
          session->env(),
          "An application can only be attached to a QUIC session before it "
          "becomes active (begins emitting events)");
      return;
    }
    session->SetApplication(CreateHttp3Application(session));

    if (session->is_server() && !session->application().Start()) {
      // Start() failed (e.g. the peer's initial_max_streams_uni is < 3), so
      // the application cannot run HTTP/3.
      THROW_ERR_INVALID_STATE(
          session->env(),
          "The HTTP/3 application could not be started");
      return;
    }
  }

  BaseObjectPtr<Http3Binding> handle = Http3Binding::Create(session);
  if (handle) args.GetReturnValue().Set(handle->object());
}

void RegisterHttp3ExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(CreateHttp3Handle);
  Http3Binding::RegisterExternalReferences(registry);
}

void InitHttp3PerContext(Local<Object> target) {
  // The HTTP/3 header kind/flags values consumed by node:http3 when calling
  // the kHttp3Handle methods. These are http3-owned constants exposed on the
  // quic binding object.
  constexpr int QUIC_STREAM_HEADERS_KIND_HINTS =
      static_cast<uint8_t>(HeadersKind::HINTS);
  constexpr int QUIC_STREAM_HEADERS_KIND_INITIAL =
      static_cast<uint8_t>(HeadersKind::INITIAL);
  constexpr int QUIC_STREAM_HEADERS_KIND_TRAILING =
      static_cast<uint8_t>(HeadersKind::TRAILING);
  constexpr int QUIC_STREAM_HEADERS_FLAGS_NONE =
      static_cast<uint8_t>(HeadersFlags::NONE);
  constexpr int QUIC_STREAM_HEADERS_FLAGS_TERMINAL =
      static_cast<uint8_t>(HeadersFlags::TERMINAL);

  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_HINTS);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_INITIAL);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_KIND_TRAILING);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_FLAGS_NONE);
  NODE_DEFINE_CONSTANT(target, QUIC_STREAM_HEADERS_FLAGS_TERMINAL);
}

namespace {
// Resolves the effective HTTP/3 settings for a session's options: the
// consumer-supplied settings (or defaults). SETTINGS_H3_DATAGRAM stays
// disabled (HTTP/3 datagrams not yet supported).
Http3Settings ResolveHttp3Settings(const Session::Options& options) {
  return options.application_settings
             ? *std::static_pointer_cast<const Http3Settings>(
                   options.application_settings)
             : Http3Settings();
}

std::unique_ptr<Session::Application> CreateHttp3Application(Session* session) {
  Debug(session, "Installing HTTP/3 application");
  return std::make_unique<Http3Application>(
      session,
      ResolveHttp3Settings(std::as_const(*session).config().options));
}

Maybe<std::shared_ptr<void>> ParseHttp3Settings(Environment* env,
                                                Local<Value> value) {
  Http3Settings settings;
  if (!Http3Settings::From(env, value).To(&settings)) {
    return Nothing<std::shared_ptr<void>>();
  }
  return Just(std::static_pointer_cast<void>(
      std::make_shared<Http3Settings>(settings)));
}

// Parses and validates the typed HTTP/3 session-ticket app data against
// the session's configured settings at decrypt time (relax-tolerant:
// 0-RTT is rejected only when settings were tightened relative to
// ticket issuance). Returns nullptr to reject the ticket.
PendingTicketAppData ParseHttp3Ticket(const uv_buf_t& data,
                                      const Session::Options& options) {
  if (data.len != kSessionTicketAppDataSize) return nullptr;

  const uint8_t* buf = reinterpret_cast<const uint8_t*>(data.base);

  if (buf[0] != kSessionTicketAppDataVersion) {
    return nullptr;
  }

  const uint8_t* payload = buf + kSessionTicketAppDataHeaderSize;
  uint32_t stored_crc = ReadBE32(buf + 1);
  uLong computed_crc = crc32(0L, Z_NULL, 0);
  computed_crc = crc32(computed_crc, payload, kSessionTicketAppDataPayloadSize);
  if (stored_crc != static_cast<uint32_t>(computed_crc)) return nullptr;

  Http3TicketData ticket{
      ReadBE64(payload),
      ReadBE64(payload + 8),
      ReadBE64(payload + 16),
      ReadBE64(payload + 24),
      payload[32] != 0,
      payload[33] != 0,
  };

  Http3Settings current = ResolveHttp3Settings(options);
  if (current.max_field_section_size < ticket.max_field_section_size ||
      current.qpack_max_dtable_capacity < ticket.qpack_max_dtable_capacity ||
      current.qpack_encoder_max_dtable_capacity <
          ticket.qpack_encoder_max_dtable_capacity ||
      current.qpack_blocked_streams < ticket.qpack_blocked_streams ||
      (ticket.enable_connect_protocol && !current.enable_connect_protocol) ||
      (ticket.enable_datagrams && !current.enable_datagrams)) {
    return nullptr;
  }

  return std::static_pointer_cast<void>(
      std::make_shared<Http3TicketData>(ticket));
}
}  // namespace

void RegisterHttp3Application() {
  RegisterApplicationFactory("http3",
                             {
                                 .create = CreateHttp3Application,
                                 .parse_settings = ParseHttp3Settings,
                                 .parse_ticket = ParseHttp3Ticket,
                             });
}

}  // namespace quic
}  // namespace node
#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
