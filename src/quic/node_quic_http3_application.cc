#include "node.h"
#include "debug_utils.h"
#include "node_mem-inl.h"
#include "node_quic_buffer-inl.h"
#include "node_quic_http3_application.h"
#include "node_quic_session-inl.h"
#include "node_quic_socket-inl.h"
#include "node_quic_stream-inl.h"
#include "node_quic_util-inl.h"
#include "node_sockaddr-inl.h"
#include "node_http_common-inl.h"

#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>
#include <algorithm>
#include <string>

namespace node {

using v8::Eternal;
using v8::MaybeLocal;
using v8::String;
using v8::Value;

namespace quic {

namespace {
bool IsZeroLengthHeader(nghttp3_rcbuf* name, nghttp3_rcbuf* value) {
  return Http3RcBufferPointer::IsZeroLength(name) ||
         Http3RcBufferPointer::IsZeroLength(value);
}

// nghttp3 uses a numeric identifier for a large number
// of known HTTP header names. These allow us to use
// static strings for those rather than allocating new
// strings all of the time. The list of strings supported
// is included in node_http_common.h
const char* to_http_header_name(int32_t token) {
  switch (token) {
    default:
      // Fall through
    case -1: return nullptr;
#define V(name, value) case NGHTTP3_QPACK_TOKEN__##name: return value;
    HTTP_SPECIAL_HEADERS(V)
#undef V
#define V(name, value) case NGHTTP3_QPACK_TOKEN_##name: return value;
    HTTP_REGULAR_HEADERS(V)
#undef V
  }
}
}  // namespace

Http3Header::Http3Header(
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value) :
    token_(token) {
  // Only retain the name buffer if it's not a known token
  if (token == -1)
    name_.reset(name, true);  // Internalizable
  value_.reset(value);
}

Http3Header::Http3Header(Http3Header&& other) noexcept :
  token_(other.token_),
  name_(std::move(other.name_)),
  value_(std::move(other.value_)) {
  other.token_ = -1;
}

MaybeLocal<String> Http3Header::GetName(QuicApplication* app) const {
  const char* header_name = to_http_header_name(token_);
  Environment* env = app->env();

  // If header_name is not nullptr, then it is a known header with
  // a statically defined name. We can safely internalize it here.
  if (header_name != nullptr) {
    auto& static_str_map = env->isolate_data()->http_static_strs;
    Eternal<String> eternal =
        static_str_map[
            const_cast<void*>(static_cast<const void*>(header_name))];
    if (eternal.IsEmpty()) {
      Local<String> str = OneByteString(env->isolate(), header_name);
      eternal.Set(env->isolate(), str);
      return str;
    }
    return eternal.Get(env->isolate());
  }

  // This is exceedingly unlikely but we need to be prepared just in case.
  return UNLIKELY(!name_) ?
      String::Empty(env->isolate()) :
      Http3RcBufferPointer::External::New(
          static_cast<Http3Application*>(app),
          name_);
}

MaybeLocal<String> Http3Header::GetValue(QuicApplication* app) const {
  Environment* env = app->env();
  return UNLIKELY(!value_) ?
      String::Empty(env->isolate()) :
      Http3RcBufferPointer::External::New(
          static_cast<Http3Application*>(app),
          value_);
}

std::string Http3Header::name() const {
  const char* header_name = to_http_header_name(token_);
  return header_name != nullptr ?
        std::string(header_name) :
        UNLIKELY(!name_) ?
            std::string() :
            std::string(
                reinterpret_cast<const char*>(name_.data()),
                name_.len());
}

std::string Http3Header::value() const {
  return UNLIKELY(!value_) ?
      std::string() :
      std::string(
          reinterpret_cast<const char*>(value_.data()),
          value_.len());
}

size_t Http3Header::length() const {
  return name().length() + value().length();
}

void Http3Header::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("name", name_);
  tracker->TrackField("value", value_);
}

namespace {
template <typename t>
inline void SetConfig(Environment* env, int idx, t* val) {
  AliasedFloat64Array& buffer = env->quic_state()->http3config_buffer;
  uint64_t flags = static_cast<uint64_t>(buffer[IDX_HTTP3_CONFIG_COUNT]);
  if (flags & (1ULL << idx))
    *val = static_cast<t>(buffer[idx]);
}
}  // namespace

Http3Application::Http3Application(
    QuicSession* session)
  : QuicApplication(session),
    alloc_info_(MakeAllocator()) {
  // Collect Configuration Details. An aliased buffer is used here.
  Environment* env = session->env();
  SetConfig(env, IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY,
            &qpack_max_table_capacity_);
  SetConfig(env, IDX_HTTP3_QPACK_BLOCKED_STREAMS,
            &qpack_blocked_streams_);
  SetConfig(env, IDX_HTTP3_MAX_HEADER_LIST_SIZE, &max_header_list_size_);
  SetConfig(env, IDX_HTTP3_MAX_PUSHES, &max_pushes_);

  size_t max_header_pairs = DEFAULT_MAX_HEADER_LIST_PAIRS;
  SetConfig(env, IDX_HTTP3_MAX_HEADER_PAIRS, &max_header_pairs);
  set_max_header_pairs(
      session->is_server()
          ? GetServerMaxHeaderPairs(max_header_pairs)
          : GetClientMaxHeaderPairs(max_header_pairs));

  size_t max_header_length = DEFAULT_MAX_HEADER_LENGTH;
  SetConfig(env, IDX_HTTP3_MAX_HEADER_LENGTH, &max_header_length);
  set_max_header_length(max_header_length);

  env->quic_state()->http3config_buffer[IDX_HTTP3_CONFIG_COUNT] = 0;  // Reset
}

// Submit informational headers (response headers that use a 1xx
// status code).
bool Http3Application::SubmitInformation(
    int64_t stream_id,
    v8::Local<v8::Array> headers) {
  // If the QuicSession is not a server session, return false
  // immediately. Informational headers cannot be sent by an
  // HTTP/3 client
  if (!session()->is_server())
    return false;

  Http3Headers nva(session()->env(), headers);
  Debug(
      session(),
      "Submitting %d informational headers for stream %" PRId64,
      nva.length(),
      stream_id);
  return nghttp3_conn_submit_info(
      connection(),
      stream_id,
      *nva,
      nva.length()) == 0;
}

// For client sessions, submits request headers. For server sessions,
// submits response headers.
bool Http3Application::SubmitHeaders(
    int64_t stream_id,
    v8::Local<v8::Array> headers,
    uint32_t flags) {
  Http3Headers nva(session()->env(), headers);

  // If the TERMINAL flag is set, reader_ptr should be nullptr
  // so the stream will be terminated immediately after submitting
  // the headers.
  nghttp3_data_reader reader = { Http3Application::OnReadData };
  nghttp3_data_reader* reader_ptr = nullptr;
  if (!(flags & QUICSTREAM_HEADER_FLAGS_TERMINAL))
    reader_ptr = &reader;

  switch (session()->crypto_context()->side()) {
    case NGTCP2_CRYPTO_SIDE_CLIENT:
      Debug(
          session(),
          "Submitting %d request headers for stream %" PRId64,
          nva.length(),
          stream_id);
      return nghttp3_conn_submit_request(
          connection(),
          stream_id,
          *nva,
          nva.length(),
          reader_ptr,
          nullptr) == 0;
    case NGTCP2_CRYPTO_SIDE_SERVER:
      Debug(
          session(),
          "Submitting %d response headers for stream %" PRId64,
          nva.length(),
          stream_id);
      return nghttp3_conn_submit_response(
          connection(),
          stream_id,
          *nva,
          nva.length(),
          reader_ptr) == 0;
    default:
      UNREACHABLE();
  }
  return false;
}

// Submits trailing headers for the HTTP/3 request or response.
bool Http3Application::SubmitTrailers(
    int64_t stream_id,
    v8::Local<v8::Array> headers) {
  Http3Headers nva(session()->env(), headers);
  Debug(
      session(),
      "Submitting %d trailing headers for stream %" PRId64,
      nva.length(),
      stream_id);
  return nghttp3_conn_submit_trailers(
      connection(),
      stream_id,
      *nva,
      nva.length()) == 0;
}

void Http3Application::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_nghttp3_memory_, previous_size);
}

void Http3Application::IncreaseAllocatedSize(size_t size) {
  current_nghttp3_memory_ += size;
}

void Http3Application::DecreaseAllocatedSize(size_t size) {
  current_nghttp3_memory_ -= size;
}

void Http3Application::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("current_nghttp3_memory",
                              current_nghttp3_memory_);
}

// Creates the underlying nghttp3 connection state for the session.
nghttp3_conn* Http3Application::CreateConnection(
    nghttp3_conn_settings* settings) {

  // nghttp3_conn_server_new and nghttp3_conn_client_new share
  // identical definitions, so new_fn will work for both.
  using new_fn = decltype(&nghttp3_conn_server_new);
  static new_fn new_fns[] = {
    nghttp3_conn_client_new,  // NGTCP2_CRYPTO_SIDE_CLIENT
    nghttp3_conn_server_new,  // NGTCP2_CRYPTO_SIDE_SERVER
  };

  ngtcp2_crypto_side side = session()->crypto_context()->side();
  nghttp3_conn* conn;

  return new_fns[side](
      &conn,
      &callbacks_[side],
      settings,
      &alloc_info_,
      this) != 0 ? nullptr : conn;
}

// The HTTP/3 QUIC binding uses a single unidirectional control
// stream in each direction to exchange frames impacting the entire
// connection.
bool Http3Application::CreateAndBindControlStream() {
  if (!session()->OpenUnidirectionalStream(&control_stream_id_))
    return false;
  Debug(
      session(),
      "Open stream %" PRId64 " and bind as control stream",
      control_stream_id_);
  return nghttp3_conn_bind_control_stream(
      connection(),
      control_stream_id_) == 0;
}

// The HTTP/3 QUIC binding creates two unidirectional streams in
// each direction to exchange header compression details.
bool Http3Application::CreateAndBindQPackStreams() {
  if (!session()->OpenUnidirectionalStream(&qpack_enc_stream_id_) ||
      !session()->OpenUnidirectionalStream(&qpack_dec_stream_id_)) {
    return false;
  }
  Debug(
      session(),
      "Open streams %" PRId64 " and %" PRId64 " and bind as qpack streams",
      qpack_enc_stream_id_,
      qpack_dec_stream_id_);
  return nghttp3_conn_bind_qpack_streams(
      connection(),
      qpack_enc_stream_id_,
      qpack_dec_stream_id_) == 0;
}

bool Http3Application::Initialize() {
  if (!needs_init())
    return false;

  // The QuicSession must allow for at least three local unidirectional streams.
  // This number is fixed by the http3 specification and represent the
  // control stream and two qpack management streams.
  if (session()->max_local_streams_uni() < 3)
    return false;

  nghttp3_conn_settings settings;
  nghttp3_conn_settings_default(&settings);
  settings.qpack_max_table_capacity = qpack_max_table_capacity_;
  settings.qpack_blocked_streams = qpack_blocked_streams_;
  settings.max_header_list_size = max_header_list_size_;
  settings.max_pushes = max_pushes_;
  Debug(session(), "QPack Max Table Capacity: %" PRIu64,
        qpack_max_table_capacity_);
  Debug(session(), "QPack Blocked Streams: %" PRIu64,
        qpack_blocked_streams_);
  Debug(session(), "Max Header List Size: %" PRIu64,
        max_header_list_size_);
  Debug(session(), "Max Pushes: %" PRIu64, max_pushes_);

  connection_.reset(CreateConnection(&settings));
  CHECK(connection_);
  Debug(session(), "HTTP/3 connection created");

  ngtcp2_transport_params params;
  session()->GetLocalTransportParams(&params);

  if (session()->is_server()) {
    nghttp3_conn_set_max_client_streams_bidi(
        connection(),
        params.initial_max_streams_bidi);
  }

  if (!CreateAndBindControlStream() ||
      !CreateAndBindQPackStreams()) {
    return false;
  }

  set_init_done();
  return true;
}

// All HTTP/3 control, header, and stream data arrives as QUIC stream data.
// Here we pass the received data off to nghttp3 for processing. This will
// trigger the invocation of the various nghttp3 callbacks.
bool Http3Application::ReceiveStreamData(
    int64_t stream_id,
    int fin,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  ssize_t nread =
      nghttp3_conn_read_stream(
          connection(), stream_id, data, datalen, fin);
  if (nread < 0) {
    Debug(session(), "Failure to read HTTP/3 Stream Data [%" PRId64 "]", nread);
    return false;
  }

  return true;
}

void Http3Application::AcknowledgeStreamData(
    int64_t stream_id,
    uint64_t offset,
    size_t datalen) {
  if (nghttp3_conn_add_ack_offset(connection(), stream_id, datalen) != 0)
    Debug(session(), "Failure to acknowledge HTTP/3 Stream Data");
}

void Http3Application::StreamOpen(int64_t stream_id) {
  Debug(session(), "HTTP/3 Stream %" PRId64 " is open.");
}

void Http3Application::StreamClose(
    int64_t stream_id,
    uint64_t app_error_code) {
  if (app_error_code == 0)
    app_error_code = NGTCP2_APP_NOERROR;
  nghttp3_conn_close_stream(connection(), stream_id, app_error_code);
  QuicApplication::StreamClose(stream_id, app_error_code);
}

void Http3Application::StreamReset(
    int64_t stream_id,
    uint64_t final_size,
    uint64_t app_error_code) {
  nghttp3_conn_reset_stream(connection(), stream_id);
  QuicApplication::StreamReset(stream_id, final_size, app_error_code);
}

void Http3Application::ResumeStream(int64_t stream_id) {
  nghttp3_conn_resume_stream(connection(), stream_id);
}

void Http3Application::ExtendMaxStreamsRemoteUni(uint64_t max_streams) {
  nghttp3_conn_set_max_client_streams_bidi(connection(), max_streams);
}

void Http3Application::ExtendMaxStreamData(
    int64_t stream_id,
    uint64_t max_data) {
  nghttp3_conn_unblock_stream(connection(), stream_id);
}

bool Http3Application::StreamCommit(StreamData* stream_data, size_t datalen) {
  int err = nghttp3_conn_add_write_offset(
      connection(),
      stream_data->id,
      datalen);
  if (err != 0) {
    session()->set_last_error(QUIC_ERROR_APPLICATION, err);
    return false;
  }
  return true;
}

int Http3Application::GetStreamData(StreamData* stream_data) {
  ssize_t ret = 0;
  if (connection() && session()->max_data_left()) {
    ret = nghttp3_conn_writev_stream(
        connection(),
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

bool Http3Application::BlockStream(int64_t stream_id) {
  int err = nghttp3_conn_block_stream(connection(), stream_id);
  if (err != 0) {
    session()->set_last_error(QUIC_ERROR_APPLICATION, err);
    return false;
  }
  return true;
}

bool Http3Application::ShouldSetFin(const StreamData& stream_data) {
  return stream_data.id > -1 &&
         !is_control_stream(stream_data.id) &&
         stream_data.fin == 1;
}

// This is where nghttp3 pulls the data from the outgoing
// buffer to prepare it to be sent on the QUIC stream.
ssize_t Http3Application::ReadData(
    int64_t stream_id,
    nghttp3_vec* vec,
    size_t veccnt,
    uint32_t* pflags) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  CHECK(stream);

  size_t count = 0;
  stream->DrainInto(&vec, &count, std::min(veccnt, kMaxVectorCount));
  CHECK_LE(count, kMaxVectorCount);
  size_t numbytes = nghttp3_vec_len(vec, count);
  stream->Commit(numbytes);

  Debug(
      session(),
      "Sending %" PRIu64 " bytes in %d vectors for stream %" PRId64,
      numbytes, count, stream_id);

  if (!stream->is_writable()) {
    Debug(session(), "Ending stream %" PRId64, stream_id);
    *pflags |= NGHTTP3_DATA_FLAG_EOF;
    return count;
  }

  // If count is zero here, it means that there is no data currently
  // available to send but there might be later, so return WOULDBLOCK
  // to tell nghttp3 to hold off attempting to serialize any more
  // data for this stream until it is resumed.
  return count == 0 ? NGHTTP3_ERR_WOULDBLOCK : count;
}

// Outgoing data is retained in memory until it is acknowledged.
void Http3Application::AckedStreamData(
    int64_t stream_id,
    size_t datalen) {
  Acknowledge(stream_id, 0, datalen);
}

void Http3Application::StreamClosed(
    int64_t stream_id,
    uint64_t app_error_code) {
  session()->listener()->OnStreamClose(stream_id, app_error_code);
}

BaseObjectPtr<QuicStream> Http3Application::FindOrCreateStream(
    int64_t stream_id) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  if (!stream) {
    if (session()->is_gracefully_closing()) {
      nghttp3_conn_close_stream(connection(), stream_id, NGTCP2_ERR_CLOSING);
      return {};
    }
    stream = session()->CreateStream(stream_id);
    nghttp3_conn_set_stream_user_data(connection(), stream_id, stream.get());
  }
  CHECK(stream);
  return stream;
}

void Http3Application::ReceiveData(
    int64_t stream_id,
    const uint8_t* data,
    size_t datalen) {
  FindOrCreateStream(stream_id)->ReceiveData(0, data, datalen, 0);
}

void Http3Application::DeferredConsume(
    int64_t stream_id,
    size_t consumed) {
  ReceiveData(stream_id, nullptr, consumed);
}

void Http3Application::BeginHeaders(
  int64_t stream_id,
  QuicStreamHeadersKind kind) {
  Debug(session(), "Starting header block for stream %" PRId64, stream_id);
  FindOrCreateStream(stream_id)->BeginHeaders(kind);
}

// As each header name+value pair is received, it is stored internally
// by the QuicStream until stream->EndHeaders() is called, during which
// the collected headers are converted to an array and passed off to
// the javascript side.
bool Http3Application::ReceiveHeader(
    int64_t stream_id,
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value,
    uint8_t flags) {
  // Protect against zero-length headers (zero-length if either the
  // name or value are zero-length). Such headers are simply ignored.
  if (!IsZeroLengthHeader(name, value)) {
    Debug(session(), "Receiving header for stream %" PRId64, stream_id);
    BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
    CHECK(stream);
    if (token == NGHTTP3_QPACK_TOKEN__STATUS) {
      nghttp3_vec vec = nghttp3_rcbuf_get_buf(value);
      if (vec.base[0] == '1')
        stream->set_headers_kind(QUICSTREAM_HEADERS_KIND_INFORMATIONAL);
      else
        stream->set_headers_kind(QUICSTREAM_HEADERS_KIND_INITIAL);
    }
    auto header = std::make_unique<Http3Header>(token, name, value);
    return stream->AddHeader(std::move(header));
  }
  return true;
}

void Http3Application::EndHeaders(int64_t stream_id) {
  Debug(session(), "Ending header block for stream %" PRId64, stream_id);
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  CHECK(stream);
  stream->EndHeaders();
}

// TODO(@jasnell): Implement Push Promise Support
int Http3Application::BeginPushPromise(
    int64_t stream_id,
    int64_t push_id) {
  return 0;
}

// TODO(@jasnell): Implement Push Promise Support
bool Http3Application::ReceivePushPromise(
    int64_t stream_id,
    int64_t push_id,
    int32_t token,
    nghttp3_rcbuf* name,
    nghttp3_rcbuf* value,
    uint8_t flags) {
  return true;
}

// TODO(@jasnell): Implement Push Promise Support
int Http3Application::EndPushPromise(
    int64_t stream_id,
    int64_t push_id) {
  return 0;
}

// TODO(@jasnell): Implement Push Promise Support
void Http3Application::CancelPush(
    int64_t push_id,
    int64_t stream_id) {
}

// TODO(@jasnell): Implement Push Promise Support
int Http3Application::PushStream(
    int64_t push_id,
    int64_t stream_id) {
  return 0;
}

void Http3Application::SendStopSending(
    int64_t stream_id,
    uint64_t app_error_code) {
  session()->ResetStream(stream_id, app_error_code);
}

void Http3Application::EndStream(int64_t stream_id) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  CHECK(stream);
  stream->ReceiveData(1, nullptr, 0, 0);
}

const nghttp3_conn_callbacks Http3Application::callbacks_[2] = {
  // NGTCP2_CRYPTO_SIDE_CLIENT
  {
    OnAckedStreamData,
    OnStreamClose,
    OnReceiveData,
    OnDeferredConsume,
    OnBeginHeaders,
    OnReceiveHeader,
    OnEndHeaders,
    OnBeginTrailers,  // Begin Trailers
    OnReceiveHeader,  // Receive Trailer
    OnEndHeaders,     // End Trailers
    OnBeginPushPromise,
    OnReceivePushPromise,
    OnEndPushPromise,
    OnCancelPush,
    OnSendStopSending,
    OnPushStream,
    OnEndStream
  },
  // NGTCP2_CRYPTO_SIDE_SERVER
  {
    OnAckedStreamData,
    OnStreamClose,
    OnReceiveData,
    OnDeferredConsume,
    OnBeginHeaders,
    OnReceiveHeader,
    OnEndHeaders,
    OnBeginTrailers,  // Begin Trailers
    OnReceiveHeader,  // Receive Trailer
    OnEndHeaders,     // End Trailers
    OnBeginPushPromise,
    OnReceivePushPromise,
    OnEndPushPromise,
    OnCancelPush,
    OnSendStopSending,
    OnPushStream,
    OnEndStream
  }
};

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

int Http3Application::OnBeginTrailers(
    nghttp3_conn* conn,
    int64_t stream_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->BeginHeaders(stream_id, QUICSTREAM_HEADERS_KIND_TRAILING);
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
  // TODO(@jasnell): Need to determine the appropriate response code here
  // for when the header is not going to be accepted.
  return app->ReceiveHeader(stream_id, token, name, value, flags) ?
      0 : NGHTTP3_ERR_CALLBACK_FAILURE;
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

int Http3Application::OnBeginPushPromise(
    nghttp3_conn* conn,
    int64_t stream_id,
    int64_t push_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  return app->BeginPushPromise(stream_id, push_id);
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
  return app->ReceivePushPromise(
      stream_id,
      push_id,
      token,
      name,
      value,
      flags) ? 0 : NGHTTP3_ERR_CALLBACK_FAILURE;
}

int Http3Application::OnEndPushPromise(
    nghttp3_conn* conn,
    int64_t stream_id,
    int64_t push_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  return app->EndPushPromise(stream_id, push_id);
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
  return app->PushStream(push_id, stream_id);
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
}  // namespace quic
}  // namespace node
