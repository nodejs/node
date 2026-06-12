#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>
#include <string_view>

#include "base_object.h"
#include "bindingdata.h"
#include "defs.h"
#include "session.h"
#include "sessionticket.h"
#include "streams.h"

namespace node::quic {

// Opaque application-typed session-ticket data, produced by a registered
// application factory's ticket hook at decrypt time and consumed by
// Application::ApplySessionTicketData() once the application is
// installed. The QUIC core never inspects the contents; only the owning
// application knows the concrete type.
using PendingTicketAppData = std::shared_ptr<void>;

// An Application implements the protocol-specific semantics on behalf
// of a QUIC Session.
class Session::Application : public MemoryRetainer {
 public:
  explicit Application(Session* session);
  DISALLOW_COPY_AND_MOVE(Application)

  // The session-ticket app-data type byte: the leading byte of the
  // application data embedded in session tickets, identifying how the
  // remainder of the data is interpreted. DEFAULT tags the native opaque
  // byte-match data used when no application is installed (the
  // `appTicketData` option); application-typed values (e.g. HTTP3) tag
  // data owned by the matching application's ticket hooks.
  enum class Type : uint8_t {
    DEFAULT = 1,  // Native opaque byte-match data (no application)
    HTTP3 = 2,    // Http3Conn typed settings data
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

  // Called at install time to apply session ticket app data previously
  // parsed (and validated) by this application's registered ticket hook
  // at decrypt time. Returns false if the data is incompatible, which
  // causes the handshake to fail.
  virtual bool ApplySessionTicketData(const PendingTicketAppData& data) = 0;

  // Returns a JS object describing the application's effective protocol
  // settings (some values may have been updated by negotiation with the
  // peer), or an empty result when the application exposes no settings.
  // The shape is application-defined; the QUIC core never interprets it.
  virtual v8::MaybeLocal<v8::Object> GetSettingsObject(Environment* env) {
    return {};
  }

  // Notifies the Application that the identified stream has been closed.
  virtual void ReceiveStreamClose(Stream* stream,
                                  QuicError&& error = QuicError());

  // Notify the Application that this stream is about to be removed
  virtual void StreamRemoved(stream_id id) {}

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
  Session* session_ = nullptr;
};

// The registration record for a protocol-specific Session::Application
// implementation. Protocols register themselves under a name at binding
// initialization (e.g. "http3"); a session installs one only when its
// options request that name explicitly. The settings produced and
// consumed by these hooks are opaque to the QUIC core: only the
// registering protocol knows their shape or field names.
struct ApplicationFactory {
  // Creates the Application for the given session. Any parsed settings
  // holder produced by parse_settings is carried on the session's
  // options (application_settings); implementations resolve it from
  // there (using their defaults when it is nullptr).
  std::unique_ptr<Session::Application> (*create)(Session* session) = nullptr;

  // Parses the application-specific settings value, supplied through an
  // internal symbol by the application's consumer layer (e.g.
  // node:http3), into an opaque holder carried on Session::Options.
  // Called while session options are processed; invalid user-supplied
  // values should throw and return Nothing.
  v8::Maybe<std::shared_ptr<void>> (*parse_settings)(
      Environment* env, v8::Local<v8::Value> value) = nullptr;

  // Parses and validates application-typed session-ticket data (the
  // full payload, including the leading type byte) against the session
  // options at ticket-decrypt time, before the Application instance
  // exists. Returns the parsed data for the ApplySessionTicketData()
  // call at install time, or nullptr to reject the ticket (0-RTT is
  // abandoned and the handshake falls back to a full 1-RTT exchange).
  PendingTicketAppData (*parse_ticket)(const uv_buf_t& data,
                                       const Session::Options& options) =
      nullptr;
};
void RegisterApplicationFactory(std::string_view name,
                                const ApplicationFactory& factory);
// Returns the factory registered under name, or nullptr.
const ApplicationFactory* FindApplicationFactory(std::string_view name);

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
