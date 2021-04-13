#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_bob-inl.h"
#include "node_http_common-inl.h"
#include "node_mem-inl.h"
#include "node_errors.h"
#include "quic/http3.h"
#include "quic/stream.h"

namespace node {

using v8::BigInt;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

namespace quic {

Http3Application::Options::Options(const Http3Application::Options& other)
    : Application::Options(other),
      max_field_section_size(other.max_field_section_size),
      max_pushes(other.max_pushes),
      qpack_max_table_capacity(other.qpack_max_table_capacity),
      qpack_blocked_streams(other.qpack_blocked_streams) {}

bool Http3OptionsObject::HasInstance(
    Environment* env,
    const Local<Value>& value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> Http3OptionsObject::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = env->GetBindingData<BindingState>(env->context());
  Local<FunctionTemplate> tmpl = state->http3_options_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Http3OptionsObject::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "Http3OptionsObject"));
    state->set_http3_options_constructor_template(tmpl);
  }
  return tmpl;
}

void Http3OptionsObject::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "Http3OptionsObject",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);
}

void Http3OptionsObject::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  BindingState* state = BindingState::Get(env);
  Http3OptionsObject* options = new Http3OptionsObject(env, args.This());

  if (args[0]->IsObject()) {
    Local<Object> obj = args[0].As<Object>();

    if (UNLIKELY(options->SetOption(
            obj,
            state->max_field_section_size_string(),
            &Http3Application::Options::max_field_section_size)
                .IsNothing()) ||
        UNLIKELY(options->SetOption(
            obj,
            state->max_pushes_string(),
            &Http3Application::Options::max_pushes)
                .IsNothing()) ||
        UNLIKELY(options->SetOption(
            obj,
            state->qpack_max_table_capacity_string(),
            &Http3Application::Options::qpack_max_table_capacity)
                .IsNothing()) ||
        UNLIKELY(options->SetOption(
            obj,
            state->qpack_blocked_streams_string(),
            &Http3Application::Options::qpack_blocked_streams)
                .IsNothing()) ||
        UNLIKELY(options->SetOption(
            obj,
            state->max_header_pairs_string(),
            &Http3Application::Options::max_header_pairs)
                .IsNothing()) ||
        UNLIKELY(options->SetOption(
            obj,
            state->max_header_length_string(),
            &Http3Application::Options::max_header_length)
                .IsNothing())) {
      return;
    }
  }
}

Maybe<bool> Http3OptionsObject::SetOption(
    const Local<Object>& object,
    const Local<String>& name,
    uint64_t Http3Application::Options::*member) {
  Local<Value> value;
  if (!object->Get(env()->context(), name).ToLocal(&value))
    return Nothing<bool>();

  if (value->IsUndefined())
    return Just(false);

  CHECK_IMPLIES(!value->IsBigInt(), value->IsNumber());

  uint64_t val = 0;
  if (value->IsBigInt()) {
    bool lossless = true;
    val = value.As<BigInt>()->Uint64Value(&lossless);
    if (!lossless) {
      Utf8Value label(env()->isolate(), name);
      THROW_ERR_OUT_OF_RANGE(
          env(),
          (std::string("options.") + (*label) + " is out of range").c_str());
      return Nothing<bool>();
    }
  } else {
    val = static_cast<int64_t>(value.As<Number>()->Value());
  }
  options_.get()->*member = val;
  return Just(true);
}

Http3OptionsObject::Http3OptionsObject(
    Environment* env,
    Local<Object> object,
    std::shared_ptr<Http3Application::Options> options)
    : BaseObject(env, object),
      options_(std::move(options)) {
  MakeWeak();
}

void Http3OptionsObject::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("options", options_);
}

// nghttp3 uses a numeric identifier for a large number
// of known HTTP header names. These allow us to use
// static strings for those rather than allocating new
// strings all of the time. The list of strings supported
// is included in node_http_common.h
#define V1(name, value) case NGHTTP3_QPACK_TOKEN__##name: return value;
#define V2(name, value) case NGHTTP3_QPACK_TOKEN_##name: return value;
const char* Http3HeaderTraits::ToHttpHeaderName(int32_t token) {
  switch (token) {
    default:
      // Fall through
    case -1: return nullptr;
    HTTP_SPECIAL_HEADERS(V1)
    HTTP_REGULAR_HEADERS(V2)
  }
}
#undef V1
#undef V2


Http3Application::Http3Application(
    Session* session,
    const std::shared_ptr<Application::Options>& options)
    : Session::Application(session, options),
      alloc_info_(BindingState::GetHttp3Allocator(session->env())) {
  Debug(session, "Using http3 application");
}

void Http3Application::CreateConnection() {
  // nghttp3_conn_server_new and nghttp3_conn_client_new share
  // identical definitions, so new_fn will work for both.
  using new_fn = decltype(&nghttp3_conn_server_new);
  static new_fn fns[] = {
    nghttp3_conn_client_new,  // NGTCP2_CRYPTO_SIDE_CLIENT
    nghttp3_conn_server_new,  // NGTCP2_CRYPTO_SIDE_SERVER
  };

  ngtcp2_crypto_side side = session()->crypto_context()->side();
  nghttp3_conn* conn;

  nghttp3_settings settings;
  settings.max_field_section_size = options().max_field_section_size;
  settings.max_pushes = options().max_pushes;
  settings.qpack_max_table_capacity = options().qpack_max_table_capacity;
  settings.qpack_blocked_streams = options().qpack_blocked_streams;

  CHECK_EQ(fns[side](
      &conn,
      &callbacks_,
      &settings,
      &alloc_info_,
      this), 0);
  CHECK_NOT_NULL(conn);
  connection_.reset(conn);
}

// The HTTP/3 QUIC binding uses a single unidirectional control
// stream in each direction to exchange frames impacting the entire
// connection.
bool Http3Application::CreateAndBindControlStream() {
  if (!session()->OpenStream(Stream::Direction::UNIDIRECTIONAL)
          .To(&control_stream_id_)) {
    return false;
  }
  Debug(
      session(),
      "Open stream %" PRId64 " and bind as HTTP/3 control stream",
      control_stream_id_);
  return nghttp3_conn_bind_control_stream(
      connection_.get(),
      control_stream_id_) == 0;
}

// The HTTP/3 QUIC binding creates two unidirectional streams in
// each direction to exchange header compression details.
bool Http3Application::CreateAndBindQPackStreams() {
  if (!session()->OpenStream(Stream::Direction::UNIDIRECTIONAL)
          .To(&qpack_enc_stream_id_) ||
      !session()->OpenStream(Stream::Direction::UNIDIRECTIONAL)
          .To(&qpack_dec_stream_id_)) {
    return false;
  }
  Debug(
      session(),
      "Open streams %" PRId64 " and %" PRId64 " as HTTP/3 qpack streams",
      qpack_enc_stream_id_,
      qpack_dec_stream_id_);
  return nghttp3_conn_bind_qpack_streams(
      connection_.get(),
      qpack_enc_stream_id_,
      qpack_dec_stream_id_) == 0;
}

bool Http3Application::Initialize() {
  // The Session must allow for at least three local unidirectional streams.
  // This number is fixed by the http3 specification and represent the
  // control stream and two qpack management streams.
  if (session()->max_local_streams_uni() < 3)
    return false;

  Debug(session(), "QPack Max Table Capacity: %" PRIu64,
        options().qpack_max_table_capacity);
  Debug(session(), "QPack Blocked Streams: %" PRIu64,
        options().qpack_blocked_streams);
  Debug(session(), "Max Header List Size: %" PRIu64,
        options().max_field_section_size);

  CreateConnection();
  Debug(session(), "HTTP/3 connection created");

  ngtcp2_transport_params params;
  session()->GetLocalTransportParams(&params);

  if (session()->is_server()) {
    nghttp3_conn_set_max_client_streams_bidi(
        connection_.get(),
        params.initial_max_streams_bidi);
  }

  if (!CreateAndBindControlStream() || !CreateAndBindQPackStreams())
    return false;

  return true;
}

// All HTTP/3 control, header, and stream data arrives as QUIC stream data.
// Here we pass the received data off to nghttp3 for processing. This will
// trigger the invocation of the various nghttp3 callbacks.
bool Http3Application::ReceiveStreamData(
    uint32_t flags,
    stream_id id,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  Debug(session(),
        "Receiving %" PRIu64 " bytes for HTTP/3 stream %" PRIu64 "%s",
        datalen,
        id,
        flags & NGTCP2_STREAM_DATA_FLAG_FIN ? " (fin)" : "");
  ssize_t nread =
      nghttp3_conn_read_stream(
          connection_.get(),
          id,
          data,
          datalen,
          flags & NGTCP2_STREAM_DATA_FLAG_FIN);
  if (nread < 0) {
    Debug(session(), "Failure to read HTTP/3 Stream Data [%" PRId64 "]", nread);
    return false;
  }

  return true;
}

// This is the QUIC-level stream data acknowledgement. It is called for
// all streams, including unidirectional streams. This has to forward on
// to nghttp3 for processing.
void Http3Application::AcknowledgeStreamData(
    stream_id stream_id,
    uint64_t offset,
    size_t datalen) {
  if (nghttp3_conn_add_ack_offset(connection_.get(), stream_id, datalen) != 0)
    Debug(session(), "Failure to acknowledge HTTP/3 Stream Data");
}

void Http3Application::StreamClose(stream_id id, error_code app_error_code) {
  if (app_error_code == 0)
    app_error_code = NGHTTP3_H3_NO_ERROR;
  int rv = nghttp3_conn_close_stream(connection_.get(), id, app_error_code);
  switch (rv) {
    case NGHTTP3_ERR_STREAM_NOT_FOUND:
      if (ngtcp2_is_bidi_stream(id))
        ExtendMaxStreamsRemoteBidi(1);
      break;
  }
}

void Http3Application::StreamReset(stream_id id, uint64_t app_error_code) {
  nghttp3_conn_reset_stream(connection_.get(), id);
  Application::StreamReset(id, app_error_code);
}

// When SendPendingData tries to send data for a given stream and there
// is no data to send but the Stream is still writable, it will
// be paused. When there's data available, the stream is resumed.
void Http3Application::ResumeStream(stream_id id) {
  nghttp3_conn_resume_stream(connection_.get(), id);
}

// When stream data cannot be sent because of flow control, it is marked
// as being blocked. When the flow control windows expands, nghttp3 has
// to be told to unblock the stream so it knows to try sending data again.
void Http3Application::ExtendMaxStreamData(stream_id id, uint64_t max_data) {
  nghttp3_conn_unblock_stream(connection_.get(), id);
}

bool Http3Application::CanAddHeader(
    size_t current_count,
    size_t current_headers_length,
    size_t this_header_length) {
  // We cannot add the header if we've either reached
  // * the max number of header pairs or
  // * the max number of header bytes
  return current_count < options().max_header_pairs &&
      current_headers_length + this_header_length <=
          options().max_header_length;
}

// When stream data cannot be sent because of flow control, it is marked
// as being blocked.
bool Http3Application::BlockStream(stream_id id) {
  error_code err = nghttp3_conn_block_stream(connection_.get(), id);
  if (err != 0) {
    session()->set_last_error(QuicError{QuicError::Type::APPLICATION, err});
    return false;
  }
  return true;
}

bool Http3Application::StreamCommit(StreamData* stream_data, size_t datalen) {
  error_code err = nghttp3_conn_add_write_offset(
      connection_.get(),
      stream_data->id,
      datalen);
  if (err != 0) {
    session()->set_last_error(QuicError{QuicError::Type::APPLICATION, err});
    return false;
  }
  return true;
}

// GetStreamData is called by SendPendingData to collect the Stream data
// that is to be packaged into a serialized Packet. There may or may not
// be any stream data to send. The call to nghttp3_conn_writev_stream will
// provide any available stream data (if any). If nghttp3 is not sure if
// there is data to send, it will subsequently call Http3Application::ReadData
// to collect available data from the Stream.
int Http3Application::GetStreamData(StreamData* stream_data) {
  ssize_t ret = 0;
  if (connection_ && session()->max_data_left()) {
    ret = nghttp3_conn_writev_stream(
        connection_.get(),
        &stream_data->id,
        &stream_data->fin,
        reinterpret_cast<nghttp3_vec*>(stream_data->data),
        sizeof(stream_data->data));
    if (ret < 0)
      return static_cast<int>(ret);
    else
      stream_data->remaining = stream_data->count = static_cast<size_t>(ret);
  }
  if (stream_data->id > -1) {
    Debug(session(), "Selected %" PRId64 " buffers for stream %" PRId64 "%s",
          stream_data->count,
          stream_data->id,
          stream_data->fin == 1 ? " (fin)" : "");
  }
  return 0;
}

// Determines whether SendPendingData should set fin on the Stream
bool Http3Application::ShouldSetFin(const StreamData& stream_data) {
  return stream_data.id > -1 &&
         !is_control_stream(stream_data.id) &&
         stream_data.fin == 1;
}

void Http3Application::ScheduleStream(stream_id id) {}

void Http3Application::UnscheduleStream(stream_id id) {}

void Http3Application::ExtendMaxStreamsRemoteUni(uint64_t max_streams) {
  ngtcp2_conn_extend_max_streams_uni(session()->connection(), max_streams);
}

void Http3Application::ExtendMaxStreamsRemoteBidi(uint64_t max_streams) {
  ngtcp2_conn_extend_max_streams_bidi(session()->connection(), max_streams);
}

ssize_t Http3Application::ReadData(
    stream_id id,
    nghttp3_vec* vec,
    size_t veccnt,
    uint32_t* pflags) {
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  CHECK(stream);

  ssize_t ret = NGHTTP3_ERR_WOULDBLOCK;

  auto next = [&](
      int status,
      const ngtcp2_vec* data,
      size_t count,
      bob::Done done) {
    CHECK_LE(count, veccnt);

    switch (status) {
      case bob::Status::STATUS_BLOCK:
        // Fall through
      case bob::Status::STATUS_WAIT:
        // Fall through
      case bob::Status::STATUS_EOS:
        return;
      case bob::Status::STATUS_END:
        *pflags |= NGHTTP3_DATA_FLAG_EOF;
        if (UNLIKELY(stream->trailers())) {
          *pflags |= NGHTTP3_DATA_FLAG_NO_END_STREAM;
          stream->ReadyForTrailers();
        }
        break;
    }

    ret = count;
    size_t numbytes =
        nghttp3_vec_len(
            reinterpret_cast<const nghttp3_vec*>(data),
            count);
    std::move(done)(numbytes);

    Debug(session(),
          "Sending %" PRIu64 " bytes for stream %" PRId64,
          numbytes, id);
  };

  CHECK_GE(stream->Pull(
      std::move(next),
      // Set OPTIONS_END here because nghttp3 takes over responsibility
      // for ensuring the data all gets written out.
      bob::Options::OPTIONS_END | bob::Options::OPTIONS_SYNC,
      reinterpret_cast<ngtcp2_vec*>(vec),
      veccnt,
      kMaxVectorCount), 0);

  return ret;
}

void Http3Application::SetSessionTicketAppData(
    const SessionTicketAppData& app_data) {
  // TODO(@jasnell): Implement!!
}

SessionTicketAppData::Status Http3Application::GetSessionTicketAppData(
    const SessionTicketAppData& app_data,
    SessionTicketAppData::Flag flag) {
  // TODO(@jasnell): Implement!!
  return flag == SessionTicketAppData::Flag::STATUS_RENEW ?
    SessionTicketAppData::Status::TICKET_USE_RENEW :
    SessionTicketAppData::Status::TICKET_USE;
}

BaseObjectPtr<Stream> Http3Application::FindOrCreateStream(stream_id id) {
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  if (!stream) {
    if (session()->is_graceful_closing()) {
      nghttp3_conn_close_stream(connection_.get(), id, NGTCP2_ERR_CLOSING);
      return {};
    }
    stream = session()->CreateStream(id);
    if (LIKELY(stream)) {
      session()->AddStream(stream);
      nghttp3_conn_set_stream_user_data(connection_.get(), id, stream.get());
    }
  }
  CHECK(stream);
  return stream;
}

void Http3Application::AckedStreamData(stream_id id, size_t datalen) {
  Acknowledge(id, 0, datalen);
}

void Http3Application::StreamClosed(stream_id id, error_code app_error_code) {
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  if (stream)
    stream->ReceiveData(1, nullptr, 0, 0);
  Application::StreamClose(id, app_error_code);
}

void Http3Application::ReceiveData(
    stream_id id,
    const uint8_t* data,
    size_t datalen) {
  FindOrCreateStream(id)->ReceiveData(0, data, datalen, 0);
}

void Http3Application::DeferredConsume(stream_id id, size_t consumed) {
  // Do nothing here for now. nghttp3 uses the on_deferred_consume
  // callback to notify when stream data that had previously been
  // deferred has been delivered to the application so that the
  // stream data offset can be extended. However, we extend the
  // data offset from within Stream when the data is delivered so
  // we don't have to do it here.
}

void Http3Application::BeginHeaders(stream_id id) {
  Debug(session(), "Starting header block for stream %" PRId64, id);
  FindOrCreateStream(id)->BeginHeaders(Stream::HeadersKind::INITIAL);
}

void Http3Application::ReceiveHeader(
    stream_id id,
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value,
    uint8_t flags) {
  // Protect against zero-length headers (zero-length if either the
  // name or value are zero-length). Such headers are simply ignored.
  if (!Http3Header::IsZeroLength(name, value)) {
    Debug(session(), "Receiving header for stream %" PRId64, id);
    BaseObjectPtr<Stream> stream = session()->FindStream(id);
    CHECK(stream);
    if (token == NGHTTP3_QPACK_TOKEN__STATUS) {
      nghttp3_vec vec = nghttp3_rcbuf_get_buf(value);
      if (vec.base[0] == '1')
        stream->set_headers_kind(Stream::HeadersKind::INFO);
      else
        stream->set_headers_kind(Stream::HeadersKind::INITIAL);
    }
    auto header = std::make_unique<Http3Header>(
        env(),
        token,
        name,
        value,
        flags);
    stream->AddHeader(std::move(header));
  }
}

void Http3Application::EndHeaders(stream_id id) {
  Debug(session(), "Ending header block for stream %" PRId64, id);
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  CHECK(stream);
  stream->EndHeaders();
}

void Http3Application::BeginTrailers(stream_id id) {
  Debug(session(), "Starting trailers block for stream %" PRId64, id);
  FindOrCreateStream(id)->BeginHeaders(Stream::HeadersKind::TRAILING);
}

void Http3Application::ReceiveTrailer(
    stream_id id,
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value,
    uint8_t flags) {
  // Protect against zero-length headers (zero-length if either the
  // name or value are zero-length). Such headers are simply ignored.
  if (!Http3Header::IsZeroLength(name, value)) {
    Debug(session(), "Receiving header for stream %" PRId64, id);
    BaseObjectPtr<Stream> stream = session()->FindStream(id);
    CHECK(stream);
    auto header = std::make_unique<Http3Header>(
        env(),
        token,
        name,
        value,
        flags);
    stream->AddHeader(std::move(header));
  }
}

void Http3Application::EndTrailers(stream_id id) {
  Debug(session(), "Ending trailers block for stream %" PRId64, id);
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  CHECK(stream);
  stream->EndHeaders();
}

void Http3Application::BeginPushPromise(stream_id id, int64_t push_id) {}

void Http3Application::ReceivePushPromise(
    stream_id id,
    int64_t push_id,
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value,
    uint8_t flags) {
  // TODO(@jasnell): Implement support
}

void Http3Application::EndPushPromise(stream_id id, int64_t push_id) {
  // TODO(@jasnell): Implement support
}

void Http3Application::CancelPush(int64_t push_id, stream_id id) {
  // TODO(@jasnell): Implement support
}

void Http3Application::PushStream(int64_t push_id, stream_id id) {
  // TODO(@jasnell): Implement support
}

void Http3Application::SendStopSending(
    stream_id id,
    error_code app_error_code) {
  ngtcp2_conn_shutdown_stream_read(session()->connection(), id, app_error_code);
}

void Http3Application::EndStream(stream_id id) {
  BaseObjectPtr<Stream> stream = session()->FindStream(id);
  CHECK(stream);
  stream->ReceiveData(1, nullptr, 0, 0);
}

void Http3Application::ResetStream(stream_id id, error_code app_error_code) {
  session()->ShutdownStreamWrite(id, app_error_code);
}

bool Http3Application::SendHeaders(
    stream_id id,
    Stream::HeadersKind kind,
    const v8::Local<v8::Array>& headers,
    Stream::SendHeadersFlags flags) {
  Session::SendSessionScope send_scope(session());
  Http3Headers nva(env(), headers);

  switch (kind) {
    case Stream::HeadersKind::INFO: {
      return nghttp3_conn_submit_info(
          connection_.get(),
          id,
          nva.data(),
          nva.length()) == 0;
      break;
    }
    case Stream::HeadersKind::INITIAL: {
      static constexpr nghttp3_data_reader reader = {
          Http3Application::OnReadData };
      const nghttp3_data_reader* reader_ptr = nullptr;

      // If the terminal flag is set, that means that we
      // know we're only sending headers and no body and
      // the stream should writable side should be closed
      // immediately because there is no nghttp3_data_reader
      // provided.
      if (flags != Stream::SendHeadersFlags::TERMINAL)
        reader_ptr = &reader;

      if (session()->is_server()) {
        return nghttp3_conn_submit_response(
            connection_.get(),
            id,
            nva.data(),
            nva.length(),
            reader_ptr);
      } else {
        return nghttp3_conn_submit_request(
            connection_.get(),
            id,
            nva.data(),
            nva.length(),
            reader_ptr,
            nullptr) == 0;
      }
      break;
    }
    case Stream::HeadersKind::TRAILING: {
      return nghttp3_conn_submit_trailers(
          connection_.get(),
          id,
          nva.data(),
          nva.length()) == 0;
      break;
    }
  }

  return false;
}

ssize_t Http3Application::OnReadData(
    nghttp3_conn* conn,
    int64_t stream_id,
    nghttp3_vec* vec,
    size_t veccnt,
    uint32_t* pflags,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  return app->ReadData(stream_id, vec, veccnt, pflags);
}

int Http3Application::OnAckedStreamData(
    nghttp3_conn* conn,
    int64_t stream_id,
    size_t datalen,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->AckedStreamData(stream_id, datalen);
  return 0;
}

int Http3Application::OnStreamClose(
    nghttp3_conn* conn,
    int64_t stream_id,
    uint64_t app_error_code,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->StreamClosed(stream_id, app_error_code);
  return 0;
}

int Http3Application::OnReceiveData(
    nghttp3_conn* conn,
    int64_t stream_id,
    const uint8_t* data,
    size_t datalen,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->ReceiveData(stream_id, data, datalen);
  return 0;
}

int Http3Application::OnDeferredConsume(
    nghttp3_conn* conn,
    int64_t stream_id,
    size_t consumed,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->DeferredConsume(stream_id, consumed);
  return 0;
}

int Http3Application::OnBeginHeaders(
    nghttp3_conn* conn,
    int64_t stream_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->BeginHeaders(stream_id);
  return 0;
}

int Http3Application::OnReceiveHeader(
    nghttp3_conn* conn,
    int64_t stream_id,
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value,
    uint8_t flags,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->ReceiveHeader(stream_id, token, name, value, flags);
  return 0;
}

int Http3Application::OnEndHeaders(
    nghttp3_conn* conn,
    int64_t stream_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->EndHeaders(stream_id);
  return 0;
}

int Http3Application::OnBeginTrailers(
    nghttp3_conn* conn,
    int64_t stream_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->BeginTrailers(stream_id);
  return 0;
}

int Http3Application::OnReceiveTrailer(
    nghttp3_conn* conn,
    int64_t stream_id,
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value,
    uint8_t flags,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->ReceiveTrailer(stream_id, token, name, value, flags);
  return 0;
}

int Http3Application::OnEndTrailers(
    nghttp3_conn* conn,
    int64_t stream_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->EndTrailers(stream_id);
  return 0;
}

int Http3Application::OnBeginPushPromise(
    nghttp3_conn* conn,
    int64_t stream_id,
    int64_t push_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->BeginPushPromise(stream_id, push_id);
  return 0;
}

int Http3Application::OnReceivePushPromise(
    nghttp3_conn* conn,
    int64_t stream_id,
    int64_t push_id,
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value,
    uint8_t flags,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->ReceivePushPromise(stream_id, push_id, token, name, value, flags);
  return 0;
}

int Http3Application::OnEndPushPromise(
    nghttp3_conn* conn,
    int64_t stream_id,
    int64_t push_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->EndPushPromise(stream_id, push_id);
  return 0;
}

int Http3Application::OnCancelPush(
    nghttp3_conn* conn,
    int64_t push_id,
    int64_t stream_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->CancelPush(push_id, stream_id);
  return 0;
}

int Http3Application::OnSendStopSending(
    nghttp3_conn* conn,
    int64_t stream_id,
    uint64_t app_error_code,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->SendStopSending(stream_id, app_error_code);
  return 0;
}

int Http3Application::OnPushStream(
    nghttp3_conn* conn,
    int64_t push_id,
    int64_t stream_id,
    void* conn_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->PushStream(push_id, stream_id);
  return 0;
}

int Http3Application::OnEndStream(
    nghttp3_conn* conn,
    int64_t stream_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->EndStream(stream_id);
  return 0;
}

int Http3Application::OnResetStream(
    nghttp3_conn* conn,
    int64_t stream_id,
    uint64_t app_error_code,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->ResetStream(stream_id, app_error_code);
  return 0;
}

const nghttp3_callbacks Http3Application::callbacks_ = {
  OnAckedStreamData,
  OnStreamClose,
  OnReceiveData,
  OnDeferredConsume,
  OnBeginHeaders,
  OnReceiveHeader,
  OnEndHeaders,
  OnBeginTrailers,
  OnReceiveTrailer,
  OnEndTrailers,
  OnBeginPushPromise,
  OnReceivePushPromise,
  OnEndPushPromise,
  OnCancelPush,
  OnSendStopSending,
  OnPushStream,
  OnEndStream,
  OnResetStream
};

}  // namespace quic
}  // namespace node
