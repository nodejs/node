#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <optional>
#include <variant>

#include "base_object.h"
#include "bindingdata.h"
#include "defs.h"
#include "session.h"
#include "sessionticket.h"
#include "streams.h"

namespace node::quic {

// Parsed session ticket application data, produced by
// Application::ParseTicketData() before ALPN negotiation and consumed
// by Application::ApplySessionTicketData() after.
struct DefaultTicketData {};
struct Http3TicketData {
  uint64_t max_field_section_size;
  uint64_t qpack_max_dtable_capacity;
  uint64_t qpack_encoder_max_dtable_capacity;
  uint64_t qpack_blocked_streams;
  bool enable_connect_protocol;
  bool enable_datagrams;
};
using PendingTicketAppData =
    std::variant<std::monostate, DefaultTicketData, Http3TicketData>;

// An Application implements the ALPN-protocol specific semantics on behalf
// of a QUIC Session.
class Session::Application : public MemoryRetainer {
 public:
  using Options = Session::Application_Options;

  Application(Session* session, const Options& options);
  DISALLOW_COPY_AND_MOVE(Application)

  // The type of Application, exposed via the session state so JS
  // can observe which Application was selected after ALPN negotiation.
  // This is used primarily for testing/debugging.
  enum class Type : uint8_t {
    NONE = 0,     // Not yet selected (server pre-negotiation)
    DEFAULT = 1,  // DefaultApplication (non-h3 ALPN)
    HTTP3 = 2,    // Http3ApplicationImpl (h3 / h3-XX ALPN)
  };
  virtual Type type() const = 0;

  virtual bool Start();

  // Returns true if Start() has been called successfully.
  virtual bool is_started() const { return false; }

  // Called when the server rejects 0-RTT early data. The application
  // must destroy all streams that were opened during the 0-RTT phase
  // since ngtcp2 has already discarded their internal state.
  virtual void EarlyDataRejected() = 0;

  // The "no error code" is the application-level error code that signals
  // "no error". Per the QUIC spec, this can vary by application protocol
  // and is not necessarily 0.
  virtual error_code GetNoErrorCode() const = 0;

  // The "internal error code" is the application-level error code used
  // to signal a non-specific failure when no more specific code has
  // been provided by the caller. For example, `writer.fail(reason)` on
  // the JS side uses this code when `reason` is not a `QuicError`
  // carrying an explicit code. For HTTP/3 this is
  // `NGHTTP3_H3_INTERNAL_ERROR` (0x102); for raw QUIC applications
  // there is no defined application code so we fall back to
  // NGTCP2_INTERNAL_ERROR (0x1).
  virtual error_code GetInternalErrorCode() const = 0;

  // Called after Session::Receive processes a packet, outside all callback
  // scopes. Applications can use this to handle deferred operations that
  // require calling into JS (e.g., HTTP/3 GOAWAY processing).
  virtual void PostReceive() {}

  // Called when ngtcp2 notifies us that a new remote stream has been
  // opened. The Application decides whether to create a Stream object
  // (and fire the JS onstream callback) based on the stream type. For
  // example, HTTP/3 only creates Stream objects for bidi streams since
  // uni streams are managed internally by nghttp3.
  virtual bool ReceiveStreamOpen(stream_id id) = 0;

  // Session will forward all received stream data immediately on to the
  // Application. The only additional processing the Session does is to
  // automatically adjust the session-level flow control window. It is up to
  // the Application to do the same for the Stream-level flow control.
  virtual bool ReceiveStreamData(stream_id id,
                                 const uint8_t* data,
                                 size_t datalen,
                                 const Stream::ReceiveDataFlags& flags,
                                 void* stream_user_data) = 0;

  // Session will forward all data acknowledgements for a stream to the
  // Application.
  virtual bool AcknowledgeStreamData(stream_id id, size_t datalen);

  // Called to determine if a Header can be added to this application.
  // Applications that do not support headers will always return false.
  virtual bool CanAddHeader(size_t current_count,
                            size_t current_headers_length,
                            size_t this_header_length) {
    return false;
  }

  // Called when ngtcp2 reports NGTCP2_ERR_STREAM_SHUT_WR for a stream.
  // Applications that manage their own framing (e.g., HTTP/3) must inform
  // their protocol layer that the stream's write side is shut so it stops
  // queuing data for that stream. The default is a no-op.
  virtual void StreamWriteShut(stream_id id) {}

  // Called to mark the identified stream as being blocked. Not all
  // Application types will support blocked streams, and those that do will do
  // so differently.
  virtual void BlockStream(stream_id id) {}

  // Called when the session determines that there is outbound data available
  // to send for the given stream.
  virtual void ResumeStream(stream_id id) {}

  // Called when the Session determines that the maximum number of
  // remotely-initiated unidirectional streams has been extended. Not all
  // Application types will require this notification so the default is to do
  // nothing.
  virtual void ExtendMaxStreams(EndpointLabel label,
                                Direction direction,
                                uint64_t max_streams) {}

  // Returns true if the application manages stream FIN internally (e.g.,
  // HTTP/3 uses nghttp3 which sends FIN via the fin flag in writev_stream).
  // When true, the stream infrastructure must NOT call
  // ngtcp2_conn_shutdown_stream_write when the JS write side ends —
  // the application protocol layer handles it.
  virtual bool stream_fin_managed_by_application() const { return false; }

  // Called when the Session determines that the flow control window for the
  // given stream has been expanded. Not all Application types will require
  // this notification so the default is to do nothing.
  virtual void ExtendMaxStreamData(Stream* stream, uint64_t max_data) {
    Debug(session_, "Application extending max stream data");
    // By default do nothing.
  }

  // Different Applications may wish to set some application data in the
  // session ticket (e.g. http/3 would set server settings in the application
  // data). The first byte written MUST be the Application::Type enum value.
  // By default, writes just the type byte.
  virtual void CollectSessionTicketAppData(
      SessionTicket::AppData* app_data) const;

  // Different Applications may set some application data in the session
  // ticket (e.g. http/3 would set server settings in the application data).
  // By default, there's nothing to get.
  virtual SessionTicket::AppData::Status ExtractSessionTicketAppData(
      const SessionTicket::AppData& app_data,
      SessionTicket::AppData::Source::Flag flag);

  // Validates parsed ticket data against current application options.
  // Returns false if the stored settings are more permissive than the
  // current config (e.g., a feature was enabled when the ticket was
  // issued but is now disabled).
  static bool ValidateTicketData(const PendingTicketAppData& data,
                                 const Application_Options& options);

  // Parse session ticket app data before ALPN negotiation. Reads the
  // type byte and dispatches to the appropriate application-specific
  // parser. Returns std::nullopt if parsing fails.
  static std::optional<PendingTicketAppData> ParseTicketData(
      const uv_buf_t& data);

  // Called after ALPN negotiation to validate and apply previously
  // parsed session ticket app data. Returns false if the data is
  // incompatible (e.g., type mismatch or settings downgrade), which
  // causes the handshake to fail.
  virtual bool ApplySessionTicketData(const PendingTicketAppData& data) = 0;

  // Notifies the Application that the identified stream has been closed.
  virtual void ReceiveStreamClose(Stream* stream,
                                  QuicError&& error = QuicError());

  // Notifies the Application that the identified stream has been reset.
  virtual void ReceiveStreamReset(Stream* stream,
                                  uint64_t final_size,
                                  QuicError&& error = QuicError());

  // Notifies the Application that the identified stream should stop sending.
  virtual void ReceiveStreamStopSending(Stream* stream,
                                        QuicError&& error = QuicError());

  // Submits an outbound block of headers for the given stream. Not all
  // Application types will support headers, in which case this function
  // should return false.
  virtual bool SendHeaders(const Stream& stream,
                           HeadersKind kind,
                           const v8::Local<v8::Array>& headers,
                           HeadersFlags flags = HeadersFlags::NONE) {
    return false;
  }

  // Signals to the Application that it should serialize and transmit any
  // pending session and stream packets it has accumulated.
  void SendPendingData();

  // Returns true if the application protocol supports sending and
  // receiving headers on streams (e.g. HTTP/3). Applications that
  // do not support headers should return false (the default).
  virtual bool SupportsHeaders() const { return false; }

  // Initiates application-level graceful shutdown signaling (e.g.,
  // HTTP/3 GOAWAY). Called when Session::Close(GRACEFUL) is invoked.
  virtual void BeginShutdown() {}

  // Completes the application-level graceful shutdown. Called from
  // FinishClose() before CONNECTION_CLOSE is sent. For HTTP/3, this
  // sends the final GOAWAY with the actual last accepted stream ID.
  virtual void CompleteShutdown() {}

  // Set the priority level of the stream if supported by the application. Not
  // all applications support priorities, in which case this function is a
  // non-op.
  virtual void SetStreamPriority(
      const Stream& stream,
      StreamPriority priority = StreamPriority::DEFAULT,
      StreamPriorityFlags flags = StreamPriorityFlags::NON_INCREMENTAL) {}

  struct StreamPriorityResult {
    StreamPriority priority;
    StreamPriorityFlags flags;
  };

  // Get the priority level of the stream if supported by the application. Not
  // all applications support priorities, in which case this function returns
  // the default stream priority.
  virtual StreamPriorityResult GetStreamPriority(const Stream& stream) {
    return {StreamPriority::DEFAULT, StreamPriorityFlags::NON_INCREMENTAL};
  }

  // The StreamData struct is used by the application to pass pending stream
  // data to the session for transmission.
  struct StreamData;

  virtual int GetStreamData(StreamData* data) = 0;
  virtual bool StreamCommit(StreamData* data, size_t datalen) = 0;

  inline Environment* env() const { return session().env(); }

  inline Session& session() {
    CHECK_NOT_NULL(session_);
    return *session_;
  }
  inline const Session& session() const {
    CHECK_NOT_NULL(session_);
    return *session_;
  }

 private:
  Packet::Ptr CreateStreamDataPacket();

  // Tries to pack a pending datagram into the current packet buffer.
  // If < 0 is returned, either NGTCP2_ERR_WRITE_MORE or a fatal error is
  // returned; the caller must check. If > 0 is returned, the packet is done
  // and the value is the size of the finalized packet. If 0 is returned,
  // the datagram is either congestion limited or was abandoned
  ssize_t TryWritePendingDatagram(PathStorage* path,
                                  uint8_t* dest,
                                  size_t destlen);

  // Write the given stream_data into the buffer.
  ssize_t WriteVStream(PathStorage* path,
                       uint8_t* buf,
                       ssize_t* ndatalen,
                       size_t max_packet_size,
                       const StreamData& stream_data);

  Session* session_ = nullptr;
};

struct Session::Application::StreamData final {
  // The actual number of vectors in the struct, up to kMaxVectorCount.
  size_t count = 0;
  // The stream identifier. If this is a negative value then no stream is
  // identified.
  stream_id id = -1;
  int fin = 0;
  ngtcp2_vec data[kMaxVectorCount]{};
  BaseObjectPtr<Stream> stream;

  static_assert(sizeof(ngtcp2_vec) == sizeof(nghttp3_vec) &&
                    alignof(ngtcp2_vec) == alignof(nghttp3_vec) &&
                    offsetof(ngtcp2_vec, base) == offsetof(nghttp3_vec, base) &&
                    offsetof(ngtcp2_vec, len) == offsetof(nghttp3_vec, len),
                "ngtcp2_vec and nghttp3_vec must have identical layout");
  inline operator nghttp3_vec*() {
    return reinterpret_cast<nghttp3_vec*>(data);
  }

  inline operator const ngtcp2_vec*() const { return data; }
  inline operator ngtcp2_vec*() { return data; }

  std::string ToString() const;
};

// Create a DefaultApplication for the given session.
std::unique_ptr<Session::Application> CreateDefaultApplication(
    Session* session, const Session::Application_Options& options);

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
