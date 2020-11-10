#include "node.h"
#include "debug_utils-inl.h"
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

namespace node {

using v8::Array;
using v8::Local;

namespace quic {

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

template <typename M, typename T>
void Http3Application::SetConfig(
    int idx,
    M T::*member) {
  AliasedFloat64Array& buffer = session()->quic_state()->http3config_buffer;
  uint64_t flags = static_cast<uint64_t>(buffer[IDX_HTTP3_CONFIG_COUNT]);
  if (flags & (1ULL << idx))
    config_.*member = static_cast<uint64_t>(buffer[idx]);
}

Http3Application::Http3Application(
    QuicSession* session)
  : QuicApplication(session),
    alloc_info_(MakeAllocator()) {
  // Collect Configuration Details.
  SetConfig<size_t>(IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY,
            &Http3ApplicationConfig::qpack_max_table_capacity);
  SetConfig<size_t>(IDX_HTTP3_QPACK_BLOCKED_STREAMS,
            &Http3ApplicationConfig::qpack_blocked_streams);
  SetConfig(IDX_HTTP3_MAX_HEADER_LIST_SIZE,
            &Http3ApplicationConfig::max_field_section_size);
  SetConfig(IDX_HTTP3_MAX_PUSHES,
            &Http3ApplicationConfig::max_pushes);
  SetConfig(IDX_HTTP3_MAX_HEADER_PAIRS,
            &Http3ApplicationConfig::max_header_pairs);
  SetConfig(IDX_HTTP3_MAX_HEADER_LENGTH,
            &Http3ApplicationConfig::max_header_length);
  set_max_header_pairs(
      session->is_server()
          ? GetServerMaxHeaderPairs(config_.max_header_pairs)
          : GetClientMaxHeaderPairs(config_.max_header_pairs));
  set_max_header_length(config_.max_header_length);

  session->quic_state()->http3config_buffer[IDX_HTTP3_CONFIG_COUNT] = 0;
}

// Push streams in HTTP/3 are a bit complicated.
// First, it's important to know that only an HTTP/3 server can
// create a push stream.
// Second, it's important to recognize that a push stream is
// essentially an *assumed* request. For instance, if a client
// requests a webpage that has links to css and js files, and
// the server expects the client to send subsequent requests
// for those css and js files, the server can shortcut the
// process by opening a push stream for each additional resource
// it assumes the client to make.
// Third, a push stream can only be opened within the context
// of an HTTP/3 request/response. Essentially, a server receives
// a request and while processing the response, the server can
// open one or more push streams.
//
// Now... a push stream consists of two components: a push promise
// and a push fulfillment. The push promise is sent *as part of
// the response on the original stream* and is assigned a push id
// and a block of headers containing the *assumed request headers*.
// The push promise is sent on the request/response bidirectional
// stream.
// The push fulfillment is a unidirectional stream opened by the
// server that contains the push id, the response header block, and
// the response payload.
// Here's where it can get a bit complicated: the server sends the
// push promise and the push fulfillment on two different, and
// independent QUIC streams. The push id is used to correlate
// those on the client side, but, it's entirely possible for the
// client to receive the push fulfillment before it actually receives
// the push promise. It's *unlikely*, but it's possible. Fortunately,
// nghttp3 handles the complexity of that for us internally but
// makes for some weird timing and could lead to some amount of
// buffering to occur.
//
// The *logical* order of events from the client side *should*
// be: (a) receive the push promise containing assumed request
// headers, (b) receive the push fulfillment containing the
// response headers followed immediately by the response payload.
//
// On the server side, the steps are: (a) first create the push
// promise creating the push_id then (b) open the unidirectional
// stream that will be used to fullfil the push promise. Once that
// unidirectional stream is created, the push id and unidirectional
// stream ID must be bound. The CreateAndBindPushStream handles (b)
int64_t Http3Application::CreateAndBindPushStream(int64_t push_id) {
  CHECK(session()->is_server());
  int64_t stream_id;
  if (!session()->OpenUnidirectionalStream(&stream_id))
    return 0;
  return nghttp3_conn_bind_push_stream(
      connection(),
      push_id,
      stream_id) == 0 ? stream_id : 0;
}

bool Http3Application::SubmitPushPromise(
    int64_t id,
    int64_t* push_id,
    int64_t* stream_id,
    const Http3Headers& headers) {
  // Successfully creating the push promise and opening the
  // fulfillment stream will queue nghttp3 up to send data.
  // Creating the SendSessionScope here ensures that when
  // SubmitPush exits, SendPendingData will be called if
  // we are not within the context of an ngtcp2 callback.
  QuicSession::SendSessionScope send_scope(session());

  Debug(
    session(),
    "Submitting %d push promise headers",
    headers.length());
  if (nghttp3_conn_submit_push_promise(
          connection(),
          push_id,
          id,
          headers.data(),
          headers.length()) != 0) {
    return false;
  }
  // Once we've successfully submitting the push promise and have
  // a push id assigned, we create the push fulfillment stream.
  *stream_id = CreateAndBindPushStream(*push_id);
  return *stream_id != 0;  // push stream can never use stream id 0
}

bool Http3Application::SubmitInformation(
    int64_t id,
    const Http3Headers& headers) {
  QuicSession::SendSessionScope send_scope(session());
  Debug(
      session(),
      "Submitting %d informational headers for stream %" PRId64,
      headers.length(),
      id);
  return nghttp3_conn_submit_info(
      connection(),
      id,
      headers.data(),
      headers.length()) == 0;
}

bool Http3Application::SubmitTrailers(
    int64_t id,
    const Http3Headers& headers) {
  QuicSession::SendSessionScope send_scope(session());
  Debug(
      session(),
      "Submitting %d trailing headers for stream %" PRId64,
      headers.length(),
      id);
  return nghttp3_conn_submit_trailers(
      connection(),
      id,
      headers.data(),
      headers.length()) == 0;
}

bool Http3Application::SubmitHeaders(
    int64_t id,
    const Http3Headers& headers,
    int32_t flags) {
  QuicSession::SendSessionScope send_scope(session());
  static constexpr nghttp3_data_reader reader = {
      Http3Application::OnReadData };
  const nghttp3_data_reader* reader_ptr = nullptr;
  if (!(flags & QUICSTREAM_HEADER_FLAGS_TERMINAL))
    reader_ptr = &reader;

  switch (session()->crypto_context()->side()) {
    case NGTCP2_CRYPTO_SIDE_CLIENT:
      return nghttp3_conn_submit_request(
          connection(),
          id,
          headers.data(),
          headers.length(),
          reader_ptr,
          nullptr) == 0;
    case NGTCP2_CRYPTO_SIDE_SERVER:
      return nghttp3_conn_submit_response(
          connection(),
          id,
          headers.data(),
          headers.length(),
          reader_ptr) == 0;
    default:
      UNREACHABLE();
  }
}

// SubmitPush initiates a push stream by first creating a push promise
// with an associated push id, then opening the unidirectional stream
// that is used to fullfill it. Assuming both operations are successful,
// the QuicStream instance is created and added to the server QuicSession.
//
// The headers block passed to the submit push contains the assumed
// *request* headers. The response headers are provided using the
// SubmitHeaders() function on the created QuicStream.
BaseObjectPtr<QuicStream> Http3Application::SubmitPush(
    int64_t id,
    Local<Array> headers) {
  // If the QuicSession is not a server session, return false
  // immediately. Push streams cannot be sent by an HTTP/3 client.
  if (!session()->is_server())
    return {};

  Http3Headers nva(env(), headers);
  int64_t push_id;
  int64_t stream_id;

  // There are several reasons why push may fail. We currently handle
  // them all the same. Later we might want to differentiate when the
  // return value is NGHTTP3_ERR_PUSH_ID_BLOCKED.
  return SubmitPushPromise(id, &push_id, &stream_id, nva) ?
      QuicStream::New(session(), stream_id, push_id) :
      BaseObjectPtr<QuicStream>();
}

// Submit informational headers (response headers that use a 1xx
// status code). If the QuicSession is not a server session, return
// false immediately because info headers cannot be sent by a
// client
bool Http3Application::SubmitInformation(
    int64_t stream_id,
    Local<Array> headers) {
  if (!session()->is_server())
    return false;
  Http3Headers nva(session()->env(), headers);
  return SubmitInformation(stream_id, nva);
}

// For client sessions, submits request headers. For server sessions,
// submits response headers.
bool Http3Application::SubmitHeaders(
    int64_t stream_id,
    Local<Array> headers,
    uint32_t flags) {
  Http3Headers nva(session()->env(), headers);
  return SubmitHeaders(stream_id, nva, flags);
}

// Submits trailing headers for the HTTP/3 request or response.
bool Http3Application::SubmitTrailers(
    int64_t stream_id,
    Local<Array> headers) {
  Http3Headers nva(session()->env(), headers);
  return SubmitTrailers(stream_id, nva);
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

  CHECK_EQ(fns[side](
      &conn,
      &callbacks_[side],
      &config_,
      &alloc_info_,
      this), 0);
  CHECK_NOT_NULL(conn);
  connection_.reset(conn);
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

  Debug(session(), "QPack Max Table Capacity: %" PRIu64,
        config_.qpack_max_table_capacity);
  Debug(session(), "QPack Blocked Streams: %" PRIu64,
        config_.qpack_blocked_streams);
  Debug(session(), "Max Header List Size: %" PRIu64,
        config_.max_field_section_size);
  Debug(session(), "Max Pushes: %" PRIu64,
        config_.max_pushes);

  CreateConnection();
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
    uint32_t flags,
    int64_t stream_id,
    const uint8_t* data,
    size_t datalen,
    uint64_t offset) {
  Debug(session(), "Receiving %" PRIu64 " bytes for stream %" PRIu64 "%s",
        datalen,
        stream_id,
        flags & NGTCP2_STREAM_DATA_FLAG_FIN ? " (fin)" : "");
  ssize_t nread =
      nghttp3_conn_read_stream(
          connection(),
          stream_id,
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
// to nghttp3 for processing. The Http3Application::AckedStreamData might
// be called as a result to acknowledge (and free) QuicStream data.
void Http3Application::AcknowledgeStreamData(
    int64_t stream_id,
    uint64_t offset,
    size_t datalen) {
  if (nghttp3_conn_add_ack_offset(connection(), stream_id, datalen) != 0)
    Debug(session(), "Failure to acknowledge HTTP/3 Stream Data");
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
    uint64_t app_error_code) {
  nghttp3_conn_reset_stream(connection(), stream_id);
  QuicApplication::StreamReset(stream_id, app_error_code);
}

// When SendPendingData tries to send data for a given stream and there
// is no data to send but the QuicStream is still writable, it will
// be paused. When there's data available, the stream is resumed.
void Http3Application::ResumeStream(int64_t stream_id) {
  nghttp3_conn_resume_stream(connection(), stream_id);
}

// When stream data cannot be sent because of flow control, it is marked
// as being blocked. When the flow control windows expands, nghttp3 has
// to be told to unblock the stream so it knows to try sending data again.
void Http3Application::ExtendMaxStreamData(
    int64_t stream_id,
    uint64_t max_data) {
  nghttp3_conn_unblock_stream(connection(), stream_id);
}

// When stream data cannot be sent because of flow control, it is marked
// as being blocked.
bool Http3Application::BlockStream(int64_t stream_id) {
  int err = nghttp3_conn_block_stream(connection(), stream_id);
  if (err != 0) {
    session()->set_last_error(QUIC_ERROR_APPLICATION, err);
    return false;
  }
  return true;
}

// nghttp3 keeps track of how much QuicStream data it has available and
// has sent. StreamCommit is called when a QuicPacket is serialized
// and updates nghttp3's internal state.
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

// GetStreamData is called by SendPendingData to collect the QuicStream data
// that is to be packaged into a serialized QuicPacket. There may or may not
// be any stream data to send. The call to nghttp3_conn_writev_stream will
// provide any available stream data (if any). If nghttp3 is not sure if
// there is data to send, it will subsequently call Http3Application::ReadData
// to collect available data from the QuicStream.
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

// Determines whether SendPendingData should set fin on the QuicStream
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
        break;
    }

    ret = count;
    size_t numbytes =
        nghttp3_vec_len(
            reinterpret_cast<const nghttp3_vec*>(data),
            count);
    std::move(done)(numbytes);

    Debug(session(), "Sending %" PRIu64 " bytes for stream %" PRId64,
          numbytes, stream_id);
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

// Outgoing data is retained in memory until it is acknowledged.
void Http3Application::AckedStreamData(int64_t stream_id, size_t datalen) {
  Acknowledge(stream_id, 0, datalen);
}

void Http3Application::StreamClosed(
    int64_t stream_id,
    uint64_t app_error_code) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  if (stream)
    stream->ReceiveData(1, nullptr, 0, 0);
}

BaseObjectPtr<QuicStream> Http3Application::FindOrCreateStream(
    int64_t stream_id) {
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  if (!stream) {
    if (session()->is_graceful_closing()) {
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
  // Do nothing here for now. nghttp3 uses the on_deferred_consume
  // callback to notify when stream data that had previously been
  // deferred has been delivered to the application so that the
  // stream data offset can be extended. However, we extend the
  // data offset from within QuicStream when the data is delivered
  // so we don't have to do it here.
}

// Called when a nghttp3 detects that a new block of headers
// has been received. Http3Application::ReceiveHeader will
// be called for each name+value pair received, then
// Http3Application::EndHeaders will be called to finalize
// the header block.
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
  if (!Http3Header::IsZeroLength(name, value)) {
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
    auto header = std::make_unique<Http3Header>(
        session()->env(),
        token,
        name,
        value,
        flags);
    return stream->AddHeader(std::move(header));
  }
  return true;
}

// Marks the completion of a headers block.
void Http3Application::EndHeaders(int64_t stream_id, int64_t push_id) {
  Debug(session(), "Ending header block for stream %" PRId64, stream_id);
  BaseObjectPtr<QuicStream> stream = session()->FindStream(stream_id);
  CHECK(stream);
  stream->EndHeaders();
}

void Http3Application::CancelPush(
    int64_t push_id,
    int64_t stream_id) {
  Debug(session(), "push stream canceled");
}

void Http3Application::PushStream(
    int64_t push_id,
    int64_t stream_id) {
  Debug(session(), "Received push stream %" PRIu64 " (%" PRIu64 ")",
        stream_id, push_id);
}

void Http3Application::SendStopSending(
    int64_t stream_id,
    uint64_t app_error_code) {
  ngtcp2_conn_shutdown_stream_read(
      session()->connection(),
      stream_id,
      app_error_code);
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
  app->BeginHeaders(stream_id, QUICSTREAM_HEADERS_KIND_PUSH);
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
  if (!app->ReceiveHeader(stream_id, token, name, value, flags))
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  return 0;
}

int Http3Application::OnEndPushPromise(
    nghttp3_conn* conn,
    int64_t stream_id,
    int64_t push_id,
    void* conn_user_data,
    void* stream_user_data) {
  Http3Application* app = static_cast<Http3Application*>(conn_user_data);
  app->EndHeaders(stream_id, push_id);
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
