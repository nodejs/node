#include "util.h"
#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <async_wrap-inl.h>
#include <debug_utils-inl.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node_bob.h>
#include <node_sockaddr-inl.h>
#include <uv.h>
#include <v8.h>
#include "application.h"
#include "defs.h"
#include "endpoint.h"
#include "http3.h"
#include "packet.h"
#include "session.h"

namespace node {

using v8::BigInt;
using v8::Boolean;
using v8::DictionaryTemplate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Value;

namespace quic {

// ============================================================================
// Session::Application_Options
const Session::Application_Options Session::Application_Options::kDefault = {};

Session::Application_Options::operator const nghttp3_settings() const {
  // In theory, Application::Options might contain options for more than just
  // HTTP/3. Here we extract only the properties that are relevant to HTTP/3.
  // Later if we add more application types we can add more properties or
  // divide this up into multiple option structs.
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
      // nghttp3_settings in Http3ApplicationImpl::InitializeConnection()
      // from the SNI configuration.
      .origin_list = nullptr,
      .glitch_ratelim_burst = 1000,
      .glitch_ratelim_rate = 33,
      .qpack_indexing_strat = NGHTTP3_QPACK_INDEXING_STRAT_EAGER,
  };
}

std::string Session::Application_Options::ToString() const {
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

Maybe<Session::Application_Options> Session::Application_Options::From(
    Environment* env, Local<Value> value) {
  if (value.IsEmpty()) [[unlikely]] {
    THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
    return Nothing<Application_Options>();
  }

  Application_Options options;
  auto& state = BindingData::Get(env);

#define SET(name)                                                              \
  SetOption<Session::Application_Options,                                      \
            &Session::Application_Options::name>(                              \
      env, &options, params, state.name##_string())

  if (!value->IsUndefined()) {
    if (!value->IsObject()) {
      THROW_ERR_INVALID_ARG_TYPE(env, "options must be an object");
      return Nothing<Application_Options>();
    }
    auto params = value.As<Object>();
    if (!SET(max_header_pairs) || !SET(max_header_length) ||
        !SET(max_field_section_size) || !SET(qpack_max_dtable_capacity) ||
        !SET(qpack_encoder_max_dtable_capacity) ||
        !SET(qpack_blocked_streams) || !SET(enable_connect_protocol) ||
        !SET(enable_datagrams)) {
      // The call to SetOption should have scheduled an exception to be thrown.
      return Nothing<Application_Options>();
    }
  }

#undef SET

  // Ensure the advertised max_field_section_size in SETTINGS is at least
  // as large as max_header_length. Otherwise the peer would be told to
  // restrict headers to a smaller size than what CanAddHeader accepts.
  if (options.max_field_section_size < options.max_header_length) {
    options.max_field_section_size = options.max_header_length;
  }

  return Just<Application_Options>(options);
}

MaybeLocal<Object> Session::Application_Options::ToObject(
    Environment* env) const {
  auto& binding_data = BindingData::Get(env);
  auto tmpl = binding_data.application_options_template();
  static constexpr std::string_view names[] = {
      "maxHeaderPairs",
      "maxHeaderLength",
      "maxFieldSectionSize",
      "qpackMaxDtableCapacity",
      "qpackEncoderMaxDtableCapacity",
      "qpackBlockedStreams",
      "enableConnectProtocol",
      "enableDatagrams",
  };
  if (tmpl.IsEmpty()) {
    tmpl = DictionaryTemplate::New(env->isolate(), names);
    binding_data.set_application_options_template(tmpl);
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
      Boolean::New(env->isolate(), enable_datagrams),
  };
  static_assert(std::size(values) == std::size(names));

  auto obj = tmpl->NewInstance(env->context(), values);
  if (obj->SetPrototypeV2(env->context(), Null(env->isolate())).IsNothing()) {
    return {};
  }
  return obj;
}

// ============================================================================

Session::Application::Application(Session* session, const Options& options)
    : session_(session) {}

bool Session::Application::Start() {
  // By default there is nothing to do. Specific implementations may
  // override to perform more actions.
  Debug(session_, "Session application started");
  return true;
}

bool Session::Application::AcknowledgeStreamData(stream_id id, size_t datalen) {
  if (auto stream = session().FindStream(id)) [[likely]] {
    stream->Acknowledge(datalen);
  }
  // Returning true even when the stream is not found is intentional.
  // After a stream is destroyed, the peer can still ACK data that was
  // previously sent. This is benign and should not be treated as an error.
  return true;
}

void Session::Application::CollectSessionTicketAppData(
    SessionTicket::AppData* app_data) const {
  // By default, write just the application type byte.
  uint8_t buf[1] = {static_cast<uint8_t>(type())};
  app_data->Set(uv_buf_init(reinterpret_cast<char*>(buf), 1));
}

SessionTicket::AppData::Status
Session::Application::ExtractSessionTicketAppData(
    const SessionTicket::AppData& app_data, Flag flag) {
  // By default we do not have any application data to retrieve.
  return flag == Flag::STATUS_RENEW
             ? SessionTicket::AppData::Status::TICKET_USE_RENEW
             : SessionTicket::AppData::Status::TICKET_USE;
}

std::optional<PendingTicketAppData> Session::Application::ParseTicketData(
    const uv_buf_t& data) {
  if (data.len == 0 || data.base == nullptr) return std::nullopt;
  auto app_type =
      static_cast<Type>(reinterpret_cast<const uint8_t*>(data.base)[0]);
  switch (app_type) {
    case Type::DEFAULT:
      return DefaultTicketData{};
    case Type::HTTP3:
      return ParseHttp3TicketData(data);
    default:
      return std::nullopt;
  }
}

bool Session::Application::ValidateTicketData(
    const PendingTicketAppData& data, const Application_Options& options) {
  if (std::holds_alternative<Http3TicketData>(data)) {
    // TODO(@jasnell): This validation probably belongs in http3.cc but keeping
    // it here for now.
    const auto& ticket = std::get<Http3TicketData>(data);
    return options.max_field_section_size >= ticket.max_field_section_size &&
           options.qpack_max_dtable_capacity >=
               ticket.qpack_max_dtable_capacity &&
           options.qpack_encoder_max_dtable_capacity >=
               ticket.qpack_encoder_max_dtable_capacity &&
           options.qpack_blocked_streams >= ticket.qpack_blocked_streams &&
           (!ticket.enable_connect_protocol ||
            options.enable_connect_protocol) &&
           (!ticket.enable_datagrams || options.enable_datagrams);
  }
  // DefaultTicketData always validates.
  return true;
}

void Session::Application::ReceiveStreamClose(Stream* stream,
                                              QuicError&& error) {
  DCHECK_NOT_NULL(stream);
  stream->Destroy(std::move(error));
}

void Session::Application::ReceiveStreamStopSending(Stream* stream,
                                                    QuicError&& error) {
  DCHECK_NOT_NULL(stream);
  stream->ReceiveStopSending(std::move(error));
}

void Session::Application::ReceiveStreamReset(Stream* stream,
                                              uint64_t final_size,
                                              QuicError&& error) {
  stream->ReceiveStreamReset(final_size, std::move(error));
}

// ============================================================================
// The DefaultApplication is the default implementation of Session::Application
// that is used for all unrecognized ALPN identifiers.
class DefaultApplication final : public Session::Application {
 public:
  // Marked NOLINT because the cpp linter gets confused about this using
  // statement not being sorted with the using v8 statements at the top
  // of the namespace.
  DefaultApplication(Session* session, const Options& options)
      : Session::Application(session, options), options_(options) {}

  const Options& options() const override { return options_; }

  Session::Application::Type type() const override {
    return Session::Application::Type::DEFAULT;
  }

  error_code GetNoErrorCode() const override { return 0; }

  // Raw QUIC has no application-defined "general failure" code, so
  // fall back to the QUIC transport-level INTERNAL_ERROR (0x1) used
  // by ngtcp2 for unspecified failures.
  error_code GetInternalErrorCode() const override {
    return NGTCP2_INTERNAL_ERROR;
  }

  void EarlyDataRejected() override {
    // Destroy all open streams — ngtcp2 has already discarded their
    // internal state when it rejected the early data. Use the
    // application's internal error code since this is an error
    // condition (code 0 would be treated as a clean close).
    session().DestroyAllStreams(
        QuicError::ForApplication(GetInternalErrorCode()));
    if (!session().is_destroyed()) {
      session().EmitEarlyDataRejected();
    }
  }

  bool ApplySessionTicketData(const PendingTicketAppData& data) override {
    return std::holds_alternative<DefaultTicketData>(data);
  }

  bool ReceiveStreamOpen(stream_id id) override {
    auto stream = session().CreateStream(id);
    if (!stream || session().is_destroyed()) [[unlikely]] {
      return !session().is_destroyed();
    }
    return true;
  }

  bool ReceiveStreamData(stream_id id,
                         const uint8_t* data,
                         size_t datalen,
                         const Stream::ReceiveDataFlags& flags,
                         void* stream_user_data) override {
    BaseObjectPtr<Stream> stream;
    if (stream_user_data == nullptr) {
      // This is the first time we're seeing this stream. Implicitly create it.
      stream = session().CreateStream(id);
      if (!stream || session().is_destroyed()) [[unlikely]] {
        // We couldn't create the stream, or the session was destroyed
        // during the onstream callback (via MakeCallback re-entrancy).
        return false;
      }

      // The stream was created, but was immediately destroyed because there's
      // no onstream handler.
      if (stream->is_destroyed()) [[unlikely]] {
        return true;
      }
    } else {
      stream = BaseObjectPtr<Stream>(Stream::From(stream_user_data));
      if (!stream) {
        Debug(&session(),
              "Default application failed to get existing stream "
              "from user data");
        return false;
      }
    }

    CHECK(stream);

    // Now we can actually receive the data! Woo!
    stream->ReceiveData(data, datalen, flags);
    return true;
  }

  int GetStreamData(Session::StreamData* stream_data) override {
    // Reset the state of stream_data before proceeding...
    stream_data->id = -1;
    stream_data->count = 0;
    stream_data->fin = false;
    stream_data->stream.reset();
    Debug(&session(), "Default application getting stream data");
    DCHECK_NOT_NULL(stream_data);
    // If the queue is empty, there aren't any streams with data yet

    // If the connection-level flow control window is exhausted,
    // there is no point in pulling stream data.
    if (!session().max_data_left()) return 0;
    if (stream_queue_.IsEmpty()) return 0;

    Stream* stream = stream_queue_.PopFront();
    CHECK_NOT_NULL(stream);
    stream_data->stream.reset(stream);
    stream_data->id = stream->id();
    auto next =
        [&](int status, const ngtcp2_vec* data, size_t count, bob::Done done) {
          switch (status) {
            case bob::Status::STATUS_BLOCK:
              // Fall through
            case bob::Status::STATUS_WAIT:
              return;
            case bob::Status::STATUS_EOS:
              stream_data->fin = true;
          }

          // It is possible that the data pointers returned are not actually
          // the data pointers in the stream_data. If that's the case, we need
          // to copy over the pointers.
          count = std::min(count, kMaxVectorCount);
          ngtcp2_vec* dest = *stream_data;
          if (dest != data) {
            for (size_t n = 0; n < count; n++) {
              dest[n] = data[n];
            }
          }

          stream_data->count = count;

          if (count > 0) {
            stream->Schedule(&stream_queue_);
          }

          // Not calling done here because we defer committing
          // the data until after we're sure it's written.
        };

    if (!stream->is_eos()) [[likely]] {
      int ret = stream->Pull(std::move(next),
                             bob::Options::OPTIONS_SYNC,
                             stream_data->data,
                             arraysize(stream_data->data),
                             kMaxVectorCount);
      if (ret == bob::Status::STATUS_EOS) {
        stream_data->fin = true;
      }
    } else {
      stream_data->fin = true;
    }

    return 0;
  }

  void ResumeStream(stream_id id) override { ScheduleStream(id); }

  void StreamWriteShut(stream_id id) override {
    if (auto stream = session().FindStream(id)) [[likely]] {
      stream->Unschedule();
    }
  }

  void BlockStream(stream_id id) override {
    if (auto stream = session().FindStream(id)) [[likely]] {
      // Remove the stream from the send queue. It will be re-scheduled
      // via ExtendMaxStreamData when the peer grants more flow control.
      // Without this, SendPendingData would repeatedly pop and retry
      // the same blocked stream in an infinite loop.
      stream->Unschedule();
      stream->EmitBlocked();
    }
  }

  void ExtendMaxStreamData(Stream* stream, uint64_t max_data) override {
    // The peer granted more flow control for this stream. Re-schedule
    // it so SendPendingData will resume writing.
    DCHECK_NOT_NULL(stream);
    stream->Schedule(&stream_queue_);
  }

  bool StreamCommit(Session::StreamData* stream_data, size_t datalen) override {
    DCHECK_NOT_NULL(stream_data);
    CHECK(stream_data->stream);
    stream_data->stream->Commit(datalen, stream_data->fin);
    return true;
  }

  SET_SELF_SIZE(DefaultApplication)
  SET_MEMORY_INFO_NAME(DefaultApplication)
  SET_NO_MEMORY_INFO()

 private:
  void ScheduleStream(stream_id id) {
    if (auto stream = session().FindStream(id)) [[likely]] {
      stream->Schedule(&stream_queue_);
    }
  }

  Options options_;

  Stream::Queue stream_queue_;
};

std::unique_ptr<Session::Application> CreateDefaultApplication(
    Session* session, const Session::Application_Options& options) {
  return std::make_unique<DefaultApplication>(session, options);
}

}  // namespace quic
}  // namespace node

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
