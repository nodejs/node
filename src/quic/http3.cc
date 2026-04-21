#include "nghttp3/lib/nghttp3_conn.h"
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
#include <node_http_common-inl.h>
#include <node_sockaddr-inl.h>
#include <util-inl.h>
#include <zlib.h>
#include "application.h"
#include "bindingdata.h"
#include "defs.h"
#include "http3.h"
#include "session.h"
#include "sessionticket.h"

namespace node {

using v8::Array;
using v8::Local;

namespace quic {

namespace {
constexpr uint8_t kSessionTicketAppDataVersion = 1;
// Layout: [type(1)][version(1)][crc(4)][payload(34)] = 40 bytes
constexpr size_t kSessionTicketAppDataSize = 40;
constexpr size_t kSessionTicketAppDataHeaderSize = 6;  // type + version + crc
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

// Implements the low-level HTTP/3 Application semantics.
class Http3ApplicationImpl final : public Session::Application {
 public:
  Http3ApplicationImpl(Session* session, const Options& options)
      : Application(session, options),
        allocator_(BindingData::Get(env()).nghttp3_allocator()),
        options_(options),
        conn_(nullptr) {
    // Build the ORIGIN frame payload from the SNI configuration before
    // creating the nghttp3 connection, since InitializeConnection needs
    // the origin_vec_ to be ready for settings.origin_list.
    if (session->is_server()) {
      BuildOriginPayload();
    }
    conn_ = InitializeConnection();
    session->set_priority_supported();
  }

  Session::Application::Type type() const override {
    return Session::Application::Type::HTTP3;
  }

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
    // at 1RTT to recreate the nghttp3 connection.
    conn_.reset();
    started_ = false;
    session().DestroyAllStreams(QuicError::ForApplication(0));
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

  bool SupportsHeaders() const override { return true; }

  bool is_started() const override { return started_; }

  bool Start() override {
    if (started_) return true;
    started_ = true;
    Debug(&session(), "Starting HTTP/3 application.");

    auto params = ngtcp2_conn_get_remote_transport_params(session());
    if (params == nullptr) [[unlikely]] {
      // The params are not available yet. Cannot start.
      Debug(&session(),
            "Cannot start HTTP/3 application yet. No remote transport params");
      return false;
    }

    if (params->initial_max_streams_uni < 3) {
      // HTTP3 requires 3 unidirectional control streams to be opened in each
      // direction in additional to the bidirectional streams that are used to
      // actually carry request and response payload back and forth.
      // See:
      // https://nghttp2.org/nghttp3/programmers-guide.html#binding-control-streams
      Debug(&session(),
            "Cannot start HTTP/3 application. Initial max "
            "unidirectional streams [%zu] is too low. Must be at least 3",
            params->initial_max_streams_uni);
      return false;
    }

    // If this is a server session, then set the maximum number of
    // bidirectional streams that can be created. This determines the number
    // of requests that the client can actually created.
    if (session().is_server()) {
      nghttp3_conn_set_max_client_streams_bidi(
          *this, params->initial_max_streams_bidi);
    }

    Debug(&session(), "Creating and binding HTTP/3 control streams");
    bool ret =
        ngtcp2_conn_open_uni_stream(session(), &control_stream_id_, nullptr) ==
            0 &&
        ngtcp2_conn_open_uni_stream(
            session(), &qpack_enc_stream_id_, nullptr) == 0 &&
        ngtcp2_conn_open_uni_stream(
            session(), &qpack_dec_stream_id_, nullptr) == 0 &&
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

    return ret;
  }

  void BeginShutdown() override {
    if (conn_) nghttp3_conn_submit_shutdown_notice(*this);
  }

  void CompleteShutdown() override {
    if (conn_) nghttp3_conn_shutdown(*this);
  }

  bool ReceiveStreamData(stream_id id,
                         const uint8_t* data,
                         size_t datalen,
                         const Stream::ReceiveDataFlags& flags,
                         void* unused) override {
    Debug(&session(),
          "HTTP/3 application received %zu bytes of data "
          "on stream %" PRIi64 ". Is final? %d. Is early? %d",
          datalen,
          id,
          flags.fin,
          flags.early);

    auto nread = nghttp3_conn_read_stream2(
        *this, id, data, datalen, flags.fin ? 1 : 0, uv_hrtime());

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
      session().ExtendStreamOffset(id, nread);
      session().ExtendOffset(nread);
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

  bool CanAddHeader(size_t current_count,
                    size_t current_headers_length,
                    size_t this_header_length) override {
    // We cannot add the header if we've either reached
    // * the max number of header pairs or
    // * the max number of header bytes (name + value combined)
    // current_count is the number of entries in the headers vector
    // (each pair = name entry + value entry = 2 entries).
    return (current_count / 2 < options_.max_header_pairs) &&
           (current_headers_length + this_header_length) <=
               options_.max_header_length;
  }

  bool stream_fin_managed_by_application() const override { return true; }

  void StreamWriteShut(stream_id id) override {
    nghttp3_conn_shutdown_stream_write(*this, id);
  }

  void BlockStream(stream_id id) override {
    nghttp3_conn_block_stream(*this, id);
    Application::BlockStream(id);
  }

  void ResumeStream(stream_id id) override {
    nghttp3_conn_resume_stream(*this, id);
    Application::ResumeStream(id);
  }

  void ExtendMaxStreams(EndpointLabel label,
                        Direction direction,
                        uint64_t max_streams) override {
    switch (label) {
      case EndpointLabel::LOCAL:
        return;
      case EndpointLabel::REMOTE: {
        switch (direction) {
          case Direction::BIDIRECTIONAL: {
            Debug(&session(),
                  "HTTP/3 application extending max bidi streams by %" PRIu64,
                  max_streams);
            ngtcp2_conn_extend_max_streams_bidi(
                session(), static_cast<size_t>(max_streams));
            break;
          }
          case Direction::UNIDIRECTIONAL: {
            Debug(&session(),
                  "HTTP/3 application extending max uni streams by %" PRIu64,
                  max_streams);
            ngtcp2_conn_extend_max_streams_uni(
                session(), static_cast<size_t>(max_streams));
            break;
          }
        }
      }
    }
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
    buf[0] = static_cast<uint8_t>(Type::HTTP3);
    buf[1] = kSessionTicketAppDataVersion;

    uint8_t* payload = buf + kSessionTicketAppDataHeaderSize;
    WriteBE64(payload, options_.max_field_section_size);
    WriteBE64(payload + 8, options_.qpack_max_dtable_capacity);
    WriteBE64(payload + 16, options_.qpack_encoder_max_dtable_capacity);
    WriteBE64(payload + 24, options_.qpack_blocked_streams);
    payload[32] = options_.enable_connect_protocol ? 1 : 0;
    payload[33] = options_.enable_datagrams ? 1 : 0;

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, payload, kSessionTicketAppDataPayloadSize);
    WriteBE32(buf + 2, static_cast<uint32_t>(crc));

    app_data->Set(
        uv_buf_init(reinterpret_cast<char*>(buf), kSessionTicketAppDataSize));
  }

  SessionTicket::AppData::Status ExtractSessionTicketAppData(
      const SessionTicket::AppData& app_data,
      SessionTicket::AppData::Source::Flag flag) override {
    auto data = app_data.Get();
    if (!data || data->len != kSessionTicketAppDataSize) {
      return SessionTicket::AppData::Status::TICKET_IGNORE_RENEW;
    }

    const uint8_t* buf = reinterpret_cast<const uint8_t*>(data->base);

    // buf[0] is the application type byte, buf[1] is the version.
    if (buf[0] != static_cast<uint8_t>(Type::HTTP3) ||
        buf[1] != kSessionTicketAppDataVersion) {
      Debug(&session(),
            "Ticket app data rejected: type=%d version=%d "
            "(expected type=%d version=%d)",
            buf[0],
            buf[1],
            static_cast<uint8_t>(Type::HTTP3),
            kSessionTicketAppDataVersion);
      return SessionTicket::AppData::Status::TICKET_IGNORE_RENEW;
    }

    const uint8_t* payload = buf + kSessionTicketAppDataHeaderSize;
    uint32_t stored_crc = ReadBE32(buf + 2);
    uLong computed_crc = crc32(0L, Z_NULL, 0);
    computed_crc =
        crc32(computed_crc, payload, kSessionTicketAppDataPayloadSize);
    if (stored_crc != static_cast<uint32_t>(computed_crc)) {
      Debug(&session(),
            "Ticket app data rejected: CRC mismatch "
            "(stored=%u computed=%u)",
            stored_crc,
            static_cast<uint32_t>(computed_crc));
      return SessionTicket::AppData::Status::TICKET_IGNORE_RENEW;
    }

    uint64_t stored_max_field_section_size = ReadBE64(payload);
    uint64_t stored_qpack_max_dtable_capacity = ReadBE64(payload + 8);
    uint64_t stored_qpack_encoder_max_dtable_capacity = ReadBE64(payload + 16);
    uint64_t stored_qpack_blocked_streams = ReadBE64(payload + 24);
    bool stored_enable_connect_protocol = payload[32] != 0;
    bool stored_enable_datagrams = payload[33] != 0;

    Debug(&session(),
          "Ticket app data: stored mfss=%" PRIu64 " qmdc=%" PRIu64
          " qemdc=%" PRIu64 " qbs=%" PRIu64 " ecp=%d ed=%d",
          stored_max_field_section_size,
          stored_qpack_max_dtable_capacity,
          stored_qpack_encoder_max_dtable_capacity,
          stored_qpack_blocked_streams,
          stored_enable_connect_protocol,
          stored_enable_datagrams);
    Debug(&session(),
          "Current opts: mfss=%" PRIu64 " qmdc=%" PRIu64 " qemdc=%" PRIu64
          " qbs=%" PRIu64 " ecp=%d ed=%d",
          options_.max_field_section_size,
          options_.qpack_max_dtable_capacity,
          options_.qpack_encoder_max_dtable_capacity,
          options_.qpack_blocked_streams,
          options_.enable_connect_protocol,
          options_.enable_datagrams);
    if (options_.max_field_section_size < stored_max_field_section_size ||
        options_.qpack_max_dtable_capacity < stored_qpack_max_dtable_capacity ||
        options_.qpack_encoder_max_dtable_capacity <
            stored_qpack_encoder_max_dtable_capacity ||
        options_.qpack_blocked_streams < stored_qpack_blocked_streams ||
        (stored_enable_connect_protocol && !options_.enable_connect_protocol) ||
        (stored_enable_datagrams && !options_.enable_datagrams)) {
      Debug(&session(), "Ticket app data REJECTED");
      return SessionTicket::AppData::Status::TICKET_IGNORE_RENEW;
    }
    Debug(&session(), "Ticket app data ACCEPTED");

    return flag == SessionTicket::AppData::Source::Flag::STATUS_RENEW
               ? SessionTicket::AppData::Status::TICKET_USE_RENEW
               : SessionTicket::AppData::Status::TICKET_USE;
  }

  bool ApplySessionTicketData(const PendingTicketAppData& data) override {
    if (!std::holds_alternative<Http3TicketData>(data)) return false;
    const auto& ticket = std::get<Http3TicketData>(data);
    // Validate that current settings are >= stored settings.
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

  void ReceiveStreamClose(Stream* stream,
                          QuicError&& error = QuicError()) override {
    Debug(
        &session(), "HTTP/3 application closing stream %" PRIi64, stream->id());
    error_code code = NGHTTP3_H3_NO_ERROR;
    if (error.type() == QuicError::Type::APPLICATION) {
      code = error.code();
    }

    int rv = nghttp3_conn_close_stream(*this, stream->id(), code);
    // If the call is successful, Http3Application::OnStreamClose callback will
    // be invoked when the stream is ready to be closed. We'll handle destroying
    // the actual Stream object there.
    if (rv == 0) return;

    if (rv == NGHTTP3_ERR_STREAM_NOT_FOUND) {
      ExtendMaxStreams(EndpointLabel::REMOTE, stream->direction(), 1);
      return;
    }

    session().SetLastError(
        QuicError::ForApplication(nghttp3_err_infer_quic_app_error_code(rv)));
    session().Close();
  }

  void ReceiveStreamReset(Stream* stream,
                          uint64_t final_size,
                          QuicError&& error = QuicError()) override {
    // We are shutting down the readable side of the local stream here.
    Debug(&session(),
          "HTTP/3 application resetting stream %" PRIi64,
          stream->id());
    int rv = nghttp3_conn_shutdown_stream_read(*this, stream->id());
    if (rv == 0) {
      stream->ReceiveStreamReset(final_size, std::move(error));
      return;
    }

    session().SetLastError(
        QuicError::ForApplication(nghttp3_err_infer_quic_app_error_code(rv)));
    session().Close();
  }

  void ReceiveStreamStopSending(Stream* stream,
                                QuicError&& error = QuicError()) override {
    Application::ReceiveStreamStopSending(stream, std::move(error));
  }

  bool SendHeaders(const Stream& stream,
                   HeadersKind kind,
                   const Local<Array>& headers,
                   HeadersFlags flags = HeadersFlags::NONE) override {
    Session::SendPendingDataScope send_scope(&session());
    Http3Headers nva(env(), headers);

    switch (kind) {
      case HeadersKind::HINTS: {
        if (!session().is_server()) {
          // Client side cannot send hints
          return false;
        }
        Debug(&session(),
              "Submitting %" PRIu64 " early hints for stream %" PRIu64,
              stream.id());
        return nghttp3_conn_submit_info(
                   *this, stream.id(), nva.data(), nva.length()) == 0;
        break;
      }
      case HeadersKind::INITIAL: {
        static constexpr nghttp3_data_reader reader = {on_read_data_callback};
        const nghttp3_data_reader* reader_ptr = nullptr;

        // If the terminal flag is set, that means that we know we're only
        // sending headers and no body and the stream writable side should be
        // closed immediately because there is no nghttp3_data_reader provided.
        if (flags != HeadersFlags::TERMINAL) {
          reader_ptr = &reader;
        }

        if (session().is_server()) {
          // If this is a server, we're submitting a response...
          Debug(&session(),
                "Submitting %" PRIu64 " response headers for stream %" PRIu64,
                nva.length(),
                stream.id());
          return nghttp3_conn_submit_response(*this,
                                              stream.id(),
                                              nva.data(),
                                              nva.length(),
                                              reader_ptr) == 0;
        } else {
          // Otherwise we're submitting a request...
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
        break;
      }
      case HeadersKind::TRAILING: {
        Debug(&session(),
              "Submitting %" PRIu64 " trailing headers for stream %" PRIu64,
              nva.length(),
              stream.id());
        return nghttp3_conn_submit_trailers(
                   *this, stream.id(), nva.data(), nva.length()) == 0;
        break;
      }
    }

    return false;
  }

  void SetStreamPriority(const Stream& stream,
                         StreamPriority priority,
                         StreamPriorityFlags flags) override {
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

  StreamPriorityResult GetStreamPriority(const Stream& stream) override {
    // nghttp3_conn_get_stream_priority is only available on the server
    // side, where it reflects the peer's requested priority (e.g., from
    // PRIORITY_UPDATE frames). Client-side priority is tracked by the
    // Stream itself and returned directly from GetPriority in streams.cc.
    if (!session().is_server()) {
      auto& stored = stream.stored_priority();
      return {stored.priority, stored.flags};
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
    data->count = kMaxVectorCount;
    ssize_t ret = 0;
    Debug(&session(), "HTTP/3 application getting stream data");
    if (conn_ && session().max_data_left()) {
      ret = nghttp3_conn_writev_stream(
          *this, &data->id, &data->fin, *data, data->count);
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
      session().SetLastError(QuicError::ForApplication(
          nghttp3_err_infer_quic_app_error_code(err)));
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
      if (data->stream) data->stream->EmitWantTrailers();
    }
    return true;
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Http3ApplicationImpl)
  SET_SELF_SIZE(Http3ApplicationImpl)

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
    if (app_error_code != NGHTTP3_H3_NO_ERROR) {
      Debug(&session(),
            "HTTP/3 application received stream close for stream %" PRIi64
            " with code %" PRIu64,
            stream->id(),
            app_error_code);
    }
    auto direction = stream->direction();
    stream->Destroy(QuicError::ForApplication(app_error_code));
    ExtendMaxStreams(EndpointLabel::REMOTE, direction, 1);
  }

  void OnBeginHeaders(stream_id id) {
    auto stream = FindOrCreateStream(conn_.get(), &session(), id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application beginning initial block of headers for stream "
          "%" PRIi64,
          id);
    stream->BeginHeaders(HeadersKind::INITIAL);
  }

  void OnReceiveHeader(stream_id id, Http3Header&& header) {
    auto stream = session().FindStream(id);

    if (!stream) [[unlikely]]
      return;
    if (header.name() == ":status" && header.value()[0] == '1') {
      Debug(&session(),
            "HTTP/3 application switching to hints headers for stream %" PRIi64,
            stream->id());
      stream->set_headers_kind(HeadersKind::HINTS);
    }
    IF_QUIC_DEBUG(env()) {
      Debug(&session(),
            "Received header \"%s: %s\"",
            header.name(),
            header.value());
    }
    stream->AddHeader(std::move(header));
  }

  void OnEndHeaders(stream_id id, int fin) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application received end of headers for stream %" PRIi64,
          id);
    stream->EmitHeaders();
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
    auto stream = FindOrCreateStream(conn_.get(), &session(), id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application beginning block of trailers for stream %" PRIi64,
          id);
    stream->BeginHeaders(HeadersKind::TRAILING);
  }

  void OnReceiveTrailer(stream_id id, Http3Header&& header) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    IF_QUIC_DEBUG(env()) {
      Debug(&session(),
            "Received header \"%s: %s\"",
            header.name(),
            header.value());
    }
    stream->AddHeader(std::move(header));
  }

  void OnEndTrailers(stream_id id, int fin) {
    auto stream = session().FindStream(id);
    if (!stream) [[unlikely]]
      return;
    Debug(&session(),
          "HTTP/3 application received end of trailers for stream %" PRIi64,
          id);
    stream->EmitHeaders();
    if (fin) {
      Debug(&session(), "Trailers are final for stream %" PRIi64, id);
      Stream::ReceiveDataFlags flags{
          .fin = true,
          .early = false,
      };
      stream->ReceiveData(nullptr, 0, flags);
    }
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
      // Final GOAWAY: destroy client-initiated bidi streams with
      // IDs > goaway_id. These were not processed by the peer and
      // can be retried. Copy the map because Destroy modifies it.
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
    session().EmitGoaway(emit_id);
  }

  void OnReceiveSettings(const nghttp3_proto_settings* settings) {
    options_.enable_connect_protocol = settings->enable_connect_protocol;
    options_.enable_datagrams = settings->h3_datagram;
    options_.max_field_section_size = settings->max_field_section_size;
    options_.qpack_blocked_streams = settings->qpack_blocked_streams;
    options_.qpack_max_dtable_capacity = settings->qpack_max_dtable_capacity;

    // Per RFC 9297 §3, an H3 endpoint MUST NOT send HTTP Datagrams
    // unless the peer indicated support via SETTINGS_H3_DATAGRAM=1.
    // If the peer disabled it, set the session's max datagram size to 0
    // which blocks sends at the existing JS/C++ check.
    if (!settings->h3_datagram) {
      session().set_max_datagram_size(0);
    }

    Debug(&session(),
          "HTTP/3 application received updated settings: %s",
          options_);
  }

  bool started_ = false;
  nghttp3_mem* allocator_;
  Options options_;
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

  static Http3ApplicationImpl* From(nghttp3_conn* conn, void* user_data) {
    DCHECK_NOT_NULL(user_data);
    auto app = static_cast<Http3ApplicationImpl*>(user_data);
    DCHECK_EQ(conn, app->conn_.get());
    return app;
  }

  static BaseObjectWeakPtr<Stream> FindOrCreateStream(nghttp3_conn* conn,
                                                      Session* session,
                                                      stream_id id) {
    if (auto stream = session->FindStream(id)) {
      return stream;
    }
    if (auto stream = session->CreateStream(id)) {
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

    auto stream = app.session().FindStream(id);
    if (!stream) return NGHTTP3_ERR_CALLBACK_FAILURE;

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
    // from Http3ApplicationImpl::AcknowledgeStreamData). We must NOT call
    // AcknowledgeStreamData here — that would re-enter nghttp3 via
    // nghttp3_conn_add_ack_offset, triggering the NgHttp3CallbackScope
    // re-entrancy assertion. Instead, directly notify the stream that data
    // was acknowledged, which is what the base Application implementation
    // does.
    auto ptr = From(conn, conn_user_data);
    CHECK_NOT_NULL(ptr);
    auto& app = *ptr;
    if (auto stream = app.session().FindStream(id)) {
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
    auto& session = app.session();
    if (auto stream = FindOrCreateStream(conn, &session, id)) [[likely]] {
      stream->ReceiveData(data, datalen, Stream::ReceiveDataFlags{});
      return NGTCP2_SUCCESS;
    }
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  static int on_deferred_consume(nghttp3_conn* conn,
                                 stream_id id,
                                 size_t consumed,
                                 void* conn_user_data,
                                 void* stream_user_data) {
    NGHTTP3_CALLBACK_SCOPE(app);
    auto& session = app.session();
    Debug(&session, "HTTP/3 application deferred consume %zu bytes", consumed);
    session.ExtendStreamOffset(id, consumed);
    session.ExtendOffset(consumed);
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
    app.OnReceiveHeader(id, Http3Header(app.env(), token, name, value, flags));
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
    app.OnReceiveTrailer(id, Http3Header(app.env(), token, name, value, flags));
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
      app.session().EmitOrigins(std::move(app.received_origins_));
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

std::optional<PendingTicketAppData> ParseHttp3TicketData(const uv_buf_t& data) {
  if (data.len != kSessionTicketAppDataSize) return std::nullopt;

  const uint8_t* buf = reinterpret_cast<const uint8_t*>(data.base);

  // buf[0] is the type byte (already checked by caller), buf[1] is version.
  if (buf[1] != kSessionTicketAppDataVersion) return std::nullopt;

  const uint8_t* payload = buf + kSessionTicketAppDataHeaderSize;
  uint32_t stored_crc = ReadBE32(buf + 2);
  uLong computed_crc = crc32(0L, Z_NULL, 0);
  computed_crc = crc32(computed_crc, payload, kSessionTicketAppDataPayloadSize);
  if (stored_crc != static_cast<uint32_t>(computed_crc)) return std::nullopt;

  return Http3TicketData{
      ReadBE64(payload),
      ReadBE64(payload + 8),
      ReadBE64(payload + 16),
      ReadBE64(payload + 24),
      payload[32] != 0,
      payload[33] != 0,
  };
}

std::unique_ptr<Session::Application> CreateHttp3Application(
    Session* session, const Session::Application_Options& options) {
  Debug(session, "Selecting HTTP/3 application");
  return std::make_unique<Http3ApplicationImpl>(session, options);
}

}  // namespace quic
}  // namespace node
#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
