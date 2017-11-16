#include "aliased_buffer.h"
#include "node.h"
#include "node_buffer.h"
#include "node_http2.h"
#include "node_http2_state.h"

#include <queue>
#include <algorithm>

namespace node {

using v8::Boolean;
using v8::Context;
using v8::Float64Array;
using v8::Function;
using v8::Integer;
using v8::Number;
using v8::ObjectTemplate;
using v8::String;
using v8::Uint32;
using v8::Uint32Array;
using v8::Undefined;

namespace http2 {

const Http2Session::Callbacks Http2Session::callback_struct_saved[2] = {
    Callbacks(false),
    Callbacks(true)};


Http2Options::Http2Options(Environment* env) {
  nghttp2_option_new(&options_);

  nghttp2_option_set_no_auto_window_update(options_, 1);

  AliasedBuffer<uint32_t, v8::Uint32Array>& buffer =
      env->http2_state()->options_buffer;
  uint32_t flags = buffer[IDX_OPTIONS_FLAGS];

  if (flags & (1 << IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE)) {
    nghttp2_option_set_max_deflate_dynamic_table_size(
        options_,
        buffer[IDX_OPTIONS_MAX_DEFLATE_DYNAMIC_TABLE_SIZE]);
  }

  if (flags & (1 << IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS)) {
    nghttp2_option_set_max_reserved_remote_streams(
        options_,
        buffer[IDX_OPTIONS_MAX_RESERVED_REMOTE_STREAMS]);
  }

  if (flags & (1 << IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH)) {
    nghttp2_option_set_max_send_header_block_length(
        options_,
        buffer[IDX_OPTIONS_MAX_SEND_HEADER_BLOCK_LENGTH]);
  }

  // Recommended default
  nghttp2_option_set_peer_max_concurrent_streams(options_, 100);
  if (flags & (1 << IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS)) {
    nghttp2_option_set_peer_max_concurrent_streams(
        options_,
        buffer[IDX_OPTIONS_PEER_MAX_CONCURRENT_STREAMS]);
  }

  if (flags & (1 << IDX_OPTIONS_PADDING_STRATEGY)) {
    padding_strategy_type strategy =
        static_cast<padding_strategy_type>(
            buffer.GetValue(IDX_OPTIONS_PADDING_STRATEGY));
    SetPaddingStrategy(strategy);
  }

  if (flags & (1 << IDX_OPTIONS_MAX_HEADER_LIST_PAIRS)) {
    SetMaxHeaderPairs(buffer[IDX_OPTIONS_MAX_HEADER_LIST_PAIRS]);
  }

  if (flags & (1 << IDX_OPTIONS_MAX_OUTSTANDING_PINGS)) {
    SetMaxOutstandingPings(buffer[IDX_OPTIONS_MAX_OUTSTANDING_PINGS]);
  }
}


Http2Settings::Http2Settings(Environment* env) : env_(env) {
  entries_.AllocateSufficientStorage(IDX_SETTINGS_COUNT);
  AliasedBuffer<uint32_t, v8::Uint32Array>& buffer =
      env->http2_state()->settings_buffer;
  uint32_t flags = buffer[IDX_SETTINGS_COUNT];

  size_t n = 0;

  if (flags & (1 << IDX_SETTINGS_HEADER_TABLE_SIZE)) {
    uint32_t val = buffer[IDX_SETTINGS_HEADER_TABLE_SIZE];
    DEBUG_HTTP2("Http2Settings: setting header table size: %d\n", val);
    entries_[n].settings_id = NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
    entries_[n].value = val;
    n++;
  }

  if (flags & (1 << IDX_SETTINGS_MAX_CONCURRENT_STREAMS)) {
    uint32_t val = buffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS];
    DEBUG_HTTP2("Http2Settings: setting max concurrent streams: %d\n", val);
    entries_[n].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
    entries_[n].value = val;
    n++;
  }

  if (flags & (1 << IDX_SETTINGS_MAX_FRAME_SIZE)) {
    uint32_t val = buffer[IDX_SETTINGS_MAX_FRAME_SIZE];
    DEBUG_HTTP2("Http2Settings: setting max frame size: %d\n", val);
    entries_[n].settings_id = NGHTTP2_SETTINGS_MAX_FRAME_SIZE;
    entries_[n].value = val;
    n++;
  }

  if (flags & (1 << IDX_SETTINGS_INITIAL_WINDOW_SIZE)) {
    uint32_t val = buffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE];
    DEBUG_HTTP2("Http2Settings: setting initial window size: %d\n", val);
    entries_[n].settings_id = NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE;
    entries_[n].value = val;
    n++;
  }

  if (flags & (1 << IDX_SETTINGS_MAX_HEADER_LIST_SIZE)) {
    uint32_t val = buffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE];
    DEBUG_HTTP2("Http2Settings: setting max header list size: %d\n", val);
    entries_[n].settings_id = NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE;
    entries_[n].value = val;
    n++;
  }

  if (flags & (1 << IDX_SETTINGS_ENABLE_PUSH)) {
    uint32_t val = buffer[IDX_SETTINGS_ENABLE_PUSH];
    DEBUG_HTTP2("Http2Settings: setting enable push: %d\n", val);
    entries_[n].settings_id = NGHTTP2_SETTINGS_ENABLE_PUSH;
    entries_[n].value = val;
    n++;
  }

  count_ = n;
}


inline Local<Value> Http2Settings::Pack() {
  const size_t len = count_ * 6;
  Local<Value> buf = Buffer::New(env_, len).ToLocalChecked();
  ssize_t ret =
      nghttp2_pack_settings_payload(
        reinterpret_cast<uint8_t*>(Buffer::Data(buf)), len,
        *entries_, count_);
  if (ret >= 0)
    return buf;
  else
    return Undefined(env_->isolate());
}


inline void Http2Settings::Update(Environment* env,
                                  Http2Session* session,
                                  get_setting fn) {
  AliasedBuffer<uint32_t, v8::Uint32Array>& buffer =
      env->http2_state()->settings_buffer;
  buffer[IDX_SETTINGS_HEADER_TABLE_SIZE] =
      fn(session->session(), NGHTTP2_SETTINGS_HEADER_TABLE_SIZE);
  buffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS] =
      fn(session->session(), NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
  buffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE] =
      fn(session->session(), NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE);
  buffer[IDX_SETTINGS_MAX_FRAME_SIZE] =
      fn(session->session(), NGHTTP2_SETTINGS_MAX_FRAME_SIZE);
  buffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE] =
      fn(session->session(), NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE);
  buffer[IDX_SETTINGS_ENABLE_PUSH] =
      fn(session->session(), NGHTTP2_SETTINGS_ENABLE_PUSH);
}


inline void Http2Settings::RefreshDefaults(Environment* env) {
  AliasedBuffer<uint32_t, v8::Uint32Array>& buffer =
      env->http2_state()->settings_buffer;

  buffer[IDX_SETTINGS_HEADER_TABLE_SIZE] =
      DEFAULT_SETTINGS_HEADER_TABLE_SIZE;
  buffer[IDX_SETTINGS_ENABLE_PUSH] =
      DEFAULT_SETTINGS_ENABLE_PUSH;
  buffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE] =
      DEFAULT_SETTINGS_INITIAL_WINDOW_SIZE;
  buffer[IDX_SETTINGS_MAX_FRAME_SIZE] =
      DEFAULT_SETTINGS_MAX_FRAME_SIZE;
  buffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE] =
      DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE;
  buffer[IDX_SETTINGS_COUNT] =
    (1 << IDX_SETTINGS_HEADER_TABLE_SIZE) |
    (1 << IDX_SETTINGS_ENABLE_PUSH) |
    (1 << IDX_SETTINGS_INITIAL_WINDOW_SIZE) |
    (1 << IDX_SETTINGS_MAX_FRAME_SIZE) |
    (1 << IDX_SETTINGS_MAX_HEADER_LIST_SIZE);
}


Http2Priority::Http2Priority(Environment* env,
                             Local<Value> parent,
                             Local<Value> weight,
                             Local<Value> exclusive) {
  Local<Context> context = env->context();
  int32_t parent_ = parent->Int32Value(context).ToChecked();
  int32_t weight_ = weight->Int32Value(context).ToChecked();
  bool exclusive_ = exclusive->BooleanValue(context).ToChecked();
  DEBUG_HTTP2("Http2Priority: parent: %d, weight: %d, exclusive: %d\n",
              parent_, weight_, exclusive_);
  nghttp2_priority_spec_init(&spec, parent_, weight_, exclusive_ ? 1 : 0);
}


inline const char* Http2Session::TypeName() {
  switch (session_type_) {
    case NGHTTP2_SESSION_SERVER: return "server";
    case NGHTTP2_SESSION_CLIENT: return "client";
    default:
      // This should never happen
      ABORT();
  }
}


Headers::Headers(Isolate* isolate,
                 Local<Context> context,
                 Local<Array> headers) {
  Local<Value> header_string = headers->Get(context, 0).ToLocalChecked();
  Local<Value> header_count = headers->Get(context, 1).ToLocalChecked();
  count_ = header_count.As<Uint32>()->Value();
  int header_string_len = header_string.As<String>()->Length();

  if (count_ == 0) {
    CHECK_EQ(header_string_len, 0);
    return;
  }

  // Allocate a single buffer with count_ nghttp2_nv structs, followed
  // by the raw header data as passed from JS. This looks like:
  // | possible padding | nghttp2_nv | nghttp2_nv | ... | header contents |
  buf_.AllocateSufficientStorage((alignof(nghttp2_nv) - 1) +
                                 count_ * sizeof(nghttp2_nv) +
                                 header_string_len);
  // Make sure the start address is aligned appropriately for an nghttp2_nv*.
  char* start = reinterpret_cast<char*>(
      ROUND_UP(reinterpret_cast<uintptr_t>(*buf_), alignof(nghttp2_nv)));
  char* header_contents = start + (count_ * sizeof(nghttp2_nv));
  nghttp2_nv* const nva = reinterpret_cast<nghttp2_nv*>(start);

  CHECK_LE(header_contents + header_string_len, *buf_ + buf_.length());
  CHECK_EQ(header_string.As<String>()
              ->WriteOneByte(reinterpret_cast<uint8_t*>(header_contents),
                             0, header_string_len,
                             String::NO_NULL_TERMINATION),
           header_string_len);

  size_t n = 0;
  char* p;
  for (p = header_contents; p < header_contents + header_string_len; n++) {
    if (n >= count_) {
      // This can happen if a passed header contained a null byte. In that
      // case, just provide nghttp2 with an invalid header to make it reject
      // the headers list.
      static uint8_t zero = '\0';
      nva[0].name = nva[0].value = &zero;
      nva[0].namelen = nva[0].valuelen = 1;
      count_ = 1;
      return;
    }

    nva[n].flags = NGHTTP2_NV_FLAG_NONE;
    nva[n].name = reinterpret_cast<uint8_t*>(p);
    nva[n].namelen = strlen(p);
    p += nva[n].namelen + 1;
    nva[n].value = reinterpret_cast<uint8_t*>(p);
    nva[n].valuelen = strlen(p);
    p += nva[n].valuelen + 1;
  }
}


Http2Session::Callbacks::Callbacks(bool kHasGetPaddingCallback) {
  CHECK_EQ(nghttp2_session_callbacks_new(&callbacks), 0);
  nghttp2_session_callbacks_set_on_begin_headers_callback(
    callbacks, OnBeginHeadersCallback);
  nghttp2_session_callbacks_set_on_header_callback2(
    callbacks, OnHeaderCallback);
  nghttp2_session_callbacks_set_on_frame_recv_callback(
    callbacks, OnFrameReceive);
  nghttp2_session_callbacks_set_on_stream_close_callback(
    callbacks, OnStreamClose);
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
    callbacks, OnDataChunkReceived);
  nghttp2_session_callbacks_set_on_frame_not_send_callback(
    callbacks, OnFrameNotSent);
  nghttp2_session_callbacks_set_on_invalid_header_callback2(
    callbacks, OnInvalidHeader);
  nghttp2_session_callbacks_set_error_callback(
    callbacks, OnNghttpError);

  if (kHasGetPaddingCallback) {
    nghttp2_session_callbacks_set_select_padding_callback(
      callbacks, OnSelectPadding);
  }
}


Http2Session::Callbacks::~Callbacks() {
  nghttp2_session_callbacks_del(callbacks);
}


Http2Session::Http2Session(Environment* env,
                           Local<Object> wrap,
                           nghttp2_session_type type)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_HTTP2SESSION),
      session_type_(type) {
  MakeWeak<Http2Session>(this);

  Http2Options opts(env);

  int32_t maxHeaderPairs = opts.GetMaxHeaderPairs();
  max_header_pairs_ =
      type == NGHTTP2_SESSION_SERVER
          ? std::max(maxHeaderPairs, 4)     // minimum # of request headers
          : std::max(maxHeaderPairs, 1);    // minimum # of response headers

  max_outstanding_pings_ = opts.GetMaxOutstandingPings();

  padding_strategy_ = opts.GetPaddingStrategy();

  bool hasGetPaddingCallback =
      padding_strategy_ == PADDING_STRATEGY_MAX ||
      padding_strategy_ == PADDING_STRATEGY_CALLBACK;

  nghttp2_session_callbacks* callbacks
      = callback_struct_saved[hasGetPaddingCallback ? 1 : 0].callbacks;

  auto fn = type == NGHTTP2_SESSION_SERVER ?
      nghttp2_session_server_new2 :
      nghttp2_session_client_new2;

  // This should fail only if the system is out of memory, which
  // is going to cause lots of other problems anyway, or if any
  // of the options are out of acceptable range, which we should
  // be catching before it gets this far. Either way, crash if this
  // fails.
  CHECK_EQ(fn(&session_, callbacks, this, *opts), 0);

  Start();
}


Http2Session::~Http2Session() {
  CHECK(persistent().IsEmpty());
  Close();
}

// For every node::Http2Session instance, there is a uv_prepare_t handle
// whose callback is triggered on every tick of the event loop. When
// run, nghttp2 is prompted to send any queued data it may have stored.
// TODO(jasnell): Currently, this creates one uv_prepare_t per Http2Session,
//                we should investigate to see if it's faster to create a
//                single uv_prepare_t for all Http2Sessions, then iterate
//                over each.
void Http2Session::Start() {
  prep_ = new uv_prepare_t();
  uv_prepare_init(env()->event_loop(), prep_);
  prep_->data = static_cast<void*>(this);
  uv_prepare_start(prep_, [](uv_prepare_t* t) {
    Http2Session* session = static_cast<Http2Session*>(t->data);
    HandleScope scope(session->env()->isolate());
    Context::Scope context_scope(session->env()->context());

    // Sending data may call arbitrary JS code, so keep track of
    // async context.
    InternalCallbackScope callback_scope(session);
    session->SendPendingData();
  });
}

// Stop the uv_prep_t from further activity, destroy the handle
void Http2Session::Stop() {
  DEBUG_HTTP2SESSION(this, "stopping uv_prep_t handle");
  CHECK_EQ(uv_prepare_stop(prep_), 0);
  auto prep_close = [](uv_handle_t* handle) {
    delete reinterpret_cast<uv_prepare_t*>(handle);
  };
  uv_close(reinterpret_cast<uv_handle_t*>(prep_), prep_close);
  prep_ = nullptr;
}


void Http2Session::Close() {
  DEBUG_HTTP2SESSION(this, "closing session");
  if (!object().IsEmpty())
    ClearWrap(object());
  persistent().Reset();

  if (session_ == nullptr)
    return;

  CHECK_EQ(nghttp2_session_terminate_session(session_, NGHTTP2_NO_ERROR), 0);
  nghttp2_session_del(session_);
  session_ = nullptr;

  while (!outstanding_pings_.empty()) {
    Http2Session::Http2Ping* ping = PopPing();
    ping->Done(false);
  }

  Stop();
}


inline Http2Stream* Http2Session::FindStream(int32_t id) {
  auto s = streams_.find(id);
  return s != streams_.end() ? s->second : nullptr;
}


inline void Http2Session::AddStream(Http2Stream* stream) {
  streams_[stream->id()] = stream;
}


inline void Http2Session::RemoveStream(int32_t id) {
  streams_.erase(id);
}


inline ssize_t Http2Session::OnMaxFrameSizePadding(size_t frameLen,
                                                   size_t maxPayloadLen) {
  DEBUG_HTTP2SESSION2(this, "using max frame size padding: %d", maxPayloadLen);
  return maxPayloadLen;
}


inline ssize_t Http2Session::OnCallbackPadding(size_t frameLen,
                                               size_t maxPayloadLen) {
  DEBUG_HTTP2SESSION(this, "using callback to determine padding");
  Isolate* isolate = env()->isolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = env()->context();
  Context::Scope context_scope(context);

#if defined(DEBUG) && DEBUG
  CHECK(object()->Has(context, env()->ongetpadding_string()).FromJust());
#endif

  AliasedBuffer<uint32_t, v8::Uint32Array>& buffer =
      env()->http2_state()->padding_buffer;
  buffer[PADDING_BUF_FRAME_LENGTH] = frameLen;
  buffer[PADDING_BUF_MAX_PAYLOAD_LENGTH] = maxPayloadLen;
  buffer[PADDING_BUF_RETURN_VALUE] = frameLen;
  MakeCallback(env()->ongetpadding_string(), 0, nullptr);
  uint32_t retval = buffer[PADDING_BUF_RETURN_VALUE];
  retval = std::min(retval, static_cast<uint32_t>(maxPayloadLen));
  retval = std::max(retval, static_cast<uint32_t>(frameLen));
  DEBUG_HTTP2SESSION2(this, "using padding size %d", retval);
  return retval;
}


// Submits a graceful shutdown notice to nghttp
// See: https://nghttp2.org/documentation/nghttp2_submit_shutdown_notice.html
inline void Http2Session::SubmitShutdownNotice() {
  // Only an HTTP2 Server is permitted to send a shutdown notice
  if (session_type_ == NGHTTP2_SESSION_CLIENT)
    return;
  DEBUG_HTTP2SESSION(this, "sending shutdown notice");
  // The only situation where this should fail is if the system is
  // out of memory, which will cause other problems. Go ahead and crash
  // in that case.
  CHECK_EQ(nghttp2_submit_shutdown_notice(session_), 0);
}


// Note: This *must* send a SETTINGS frame even if niv == 0
inline void Http2Session::Settings(const nghttp2_settings_entry iv[],
                                   size_t niv) {
  DEBUG_HTTP2SESSION2(this, "submitting %d settings", niv);
  // This will fail either if the system is out of memory, or if the settings
  // values are not within the appropriate range. We should be catching the
  // latter before it gets this far so crash in either case.
  CHECK_EQ(nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv, niv), 0);
}


// Write data received from the i/o stream to the underlying nghttp2_session.
inline ssize_t Http2Session::Write(const uv_buf_t* bufs, size_t nbufs) {
  size_t total = 0;
  // Note that nghttp2_session_mem_recv is a synchronous operation that
  // will trigger a number of other callbacks. Those will, in turn have
  // multiple side effects.
  for (size_t n = 0; n < nbufs; n++) {
    ssize_t ret =
      nghttp2_session_mem_recv(session_,
                               reinterpret_cast<uint8_t*>(bufs[n].base),
                               bufs[n].len);
    CHECK_NE(ret, NGHTTP2_ERR_NOMEM);

    if (ret < 0)
      return ret;

    total += ret;
  }
  // Send any data that was queued up while processing the received data.
  SendPendingData();
  return total;
}


inline int32_t GetFrameID(const nghttp2_frame* frame) {
  // If this is a push promise, we want to grab the id of the promised stream
  return (frame->hd.type == NGHTTP2_PUSH_PROMISE) ?
      frame->push_promise.promised_stream_id :
      frame->hd.stream_id;
}


inline int Http2Session::OnBeginHeadersCallback(nghttp2_session* handle,
                                                const nghttp2_frame* frame,
                                                void* user_data) {
  Http2Session* session = static_cast<Http2Session*>(user_data);
  int32_t id = GetFrameID(frame);
  DEBUG_HTTP2SESSION2(session, "beginning headers for stream %d", id);

  Http2Stream* stream = session->FindStream(id);
  if (stream == nullptr) {
    new Http2Stream(session, id, frame->headers.cat);
  } else {
    stream->StartHeaders(frame->headers.cat);
  }
  return 0;
}


inline int Http2Session::OnHeaderCallback(nghttp2_session* handle,
                                          const nghttp2_frame* frame,
                                          nghttp2_rcbuf* name,
                                          nghttp2_rcbuf* value,
                                          uint8_t flags,
                                          void* user_data) {
  Http2Session* session = static_cast<Http2Session*>(user_data);
  int32_t id = GetFrameID(frame);
  Http2Stream* stream = session->FindStream(id);
  if (!stream->AddHeader(name, value, flags)) {
    // This will only happen if the connected peer sends us more
    // than the allowed number of header items at any given time
    stream->SubmitRstStream(NGHTTP2_ENHANCE_YOUR_CALM);
    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
  }
  return 0;
}


inline int Http2Session::OnFrameReceive(nghttp2_session* handle,
                                        const nghttp2_frame* frame,
                                        void* user_data) {
  Http2Session* session = static_cast<Http2Session*>(user_data);
  DEBUG_HTTP2SESSION2(session, "complete frame received: type: %d",
                      frame->hd.type);
  switch (frame->hd.type) {
    case NGHTTP2_DATA:
      session->HandleDataFrame(frame);
      break;
    case NGHTTP2_PUSH_PROMISE:
      // Intentional fall-through, handled just like headers frames
    case NGHTTP2_HEADERS:
      session->HandleHeadersFrame(frame);
      break;
    case NGHTTP2_SETTINGS:
      session->HandleSettingsFrame(frame);
      break;
    case NGHTTP2_PRIORITY:
      session->HandlePriorityFrame(frame);
      break;
    case NGHTTP2_GOAWAY:
      session->HandleGoawayFrame(frame);
      break;
    case NGHTTP2_PING:
      session->HandlePingFrame(frame);
    default:
      break;
  }
  return 0;
}


inline int Http2Session::OnFrameNotSent(nghttp2_session* handle,
                                        const nghttp2_frame* frame,
                                        int error_code,
                                        void* user_data) {
  Http2Session* session = static_cast<Http2Session*>(user_data);
  Environment* env = session->env();
  DEBUG_HTTP2SESSION2(session, "frame type %d was not sent, code: %d",
                      frame->hd.type, error_code);
  // Do not report if the frame was not sent due to the session closing
  if (error_code != NGHTTP2_ERR_SESSION_CLOSING &&
      error_code != NGHTTP2_ERR_STREAM_CLOSED &&
      error_code != NGHTTP2_ERR_STREAM_CLOSING) {
    Isolate* isolate = env->isolate();
    HandleScope scope(isolate);
    Local<Context> context = env->context();
    Context::Scope context_scope(context);

    Local<Value> argv[3] = {
      Integer::New(isolate, frame->hd.stream_id),
      Integer::New(isolate, frame->hd.type),
      Integer::New(isolate, error_code)
    };
    session->MakeCallback(env->onframeerror_string(), arraysize(argv), argv);
  }
  return 0;
}


inline int Http2Session::OnStreamClose(nghttp2_session* handle,
                                       int32_t id,
                                       uint32_t code,
                                       void* user_data) {
  Http2Session* session = static_cast<Http2Session*>(user_data);
  Environment* env = session->env();
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env->context();
  Context::Scope context_scope(context);
  DEBUG_HTTP2SESSION2(session, "stream %d closed with code: %d", id, code);
  Http2Stream* stream = session->FindStream(id);
  // Intentionally ignore the callback if the stream does not exist
  if (stream != nullptr) {
    stream->Close(code);
    // It is possible for the stream close to occur before the stream is
    // ever passed on to the javascript side. If that happens, ignore this.
    Local<Value> fn =
        stream->object()->Get(context, env->onstreamclose_string())
            .ToLocalChecked();
    if (fn->IsFunction()) {
      Local<Value> argv[1] = { Integer::NewFromUnsigned(isolate, code) };
      stream->MakeCallback(fn.As<Function>(), arraysize(argv), argv);
    }
  }
  return 0;
}


inline int Http2Session::OnInvalidHeader(nghttp2_session* session,
                                         const nghttp2_frame* frame,
                                         nghttp2_rcbuf* name,
                                         nghttp2_rcbuf* value,
                                         uint8_t flags,
                                         void* user_data) {
  // Ignore invalid header fields by default.
  return 0;
}


inline int Http2Session::OnDataChunkReceived(nghttp2_session* handle,
                                             uint8_t flags,
                                             int32_t id,
                                             const uint8_t* data,
                                             size_t len,
                                             void* user_data) {
  Http2Session* session = static_cast<Http2Session*>(user_data);
  DEBUG_HTTP2SESSION2(session, "buffering data chunk for stream %d, size: "
              "%d, flags: %d", id, len, flags);
  // We should never actually get a 0-length chunk so this check is
  // only a precaution at this point.
  if (len > 0) {
    CHECK_EQ(nghttp2_session_consume_connection(handle, len), 0);
    Http2Stream* stream = session->FindStream(id);
    stream->AddChunk(data, len);
  }
  return 0;
}


inline ssize_t Http2Session::OnSelectPadding(nghttp2_session* session,
                                             const nghttp2_frame* frame,
                                             size_t maxPayloadLen,
                                             void* user_data) {
  Http2Session* handle = static_cast<Http2Session*>(user_data);
  ssize_t padding = frame->hd.length;

  return handle->padding_strategy_ == PADDING_STRATEGY_MAX
    ? handle->OnMaxFrameSizePadding(padding, maxPayloadLen)
    : handle->OnCallbackPadding(padding, maxPayloadLen);
}

#define BAD_PEER_MESSAGE "Remote peer returned unexpected data while we "     \
                         "expected SETTINGS frame.  Perhaps, peer does not "  \
                         "support HTTP/2 properly."

inline int Http2Session::OnNghttpError(nghttp2_session* handle,
                                       const char* message,
                                       size_t len,
                                       void* user_data) {
  // Unfortunately, this is currently the only way for us to know if
  // the session errored because the peer is not an http2 peer.
  Http2Session* session = static_cast<Http2Session*>(user_data);
    DEBUG_HTTP2SESSION2(session, "Error '%.*s'", len, message);
  if (strncmp(message, BAD_PEER_MESSAGE, len) == 0) {
    Environment* env = session->env();
    Isolate* isolate = env->isolate();
    HandleScope scope(isolate);
    Local<Context> context = env->context();
    Context::Scope context_scope(context);

    Local<Value> argv[1] = {
      Integer::New(isolate, NGHTTP2_ERR_PROTO),
    };
    session->MakeCallback(env->error_string(), arraysize(argv), argv);
  }
  return 0;
}


inline void Http2Session::GetTrailers(Http2Stream* stream, uint32_t* flags) {
  if (stream->HasTrailers()) {
    Http2Stream::SubmitTrailers submit_trailers{this, stream, flags};
    stream->OnTrailers(submit_trailers);
  }
}


Http2Stream::SubmitTrailers::SubmitTrailers(
    Http2Session* session,
    Http2Stream* stream,
    uint32_t* flags)
  : session_(session), stream_(stream), flags_(flags) { }


inline void Http2Stream::SubmitTrailers::Submit(nghttp2_nv* trailers,
                                                 size_t length) const {
  if (length == 0)
    return;
  DEBUG_HTTP2SESSION2(session_, "sending trailers for stream %d, count: %d",
                      stream_->id(), length);
  *flags_ |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
  CHECK_EQ(
      nghttp2_submit_trailer(**session_, stream_->id(), trailers, length), 0);
}


inline void Http2Session::HandleHeadersFrame(const nghttp2_frame* frame) {
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env()->context();
  Context::Scope context_scope(context);

  int32_t id = GetFrameID(frame);
  DEBUG_HTTP2SESSION2(this, "handle headers frame for stream %d", id);
  Http2Stream* stream = FindStream(id);

  nghttp2_header* headers = stream->headers();
  size_t count = stream->headers_count();

  Local<String> name_str;
  Local<String> value_str;

  Local<Array> holder = Array::New(isolate);
  Local<Function> fn = env()->push_values_to_array_function();
  Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX * 2];

  // The headers are passed in above as a queue of nghttp2_header structs.
  // The following converts that into a JS array with the structure:
  // [name1, value1, name2, value2, name3, value3, name3, value4] and so on.
  // That array is passed up to the JS layer and converted into an Object form
  // like {name1: value1, name2: value2, name3: [value3, value4]}. We do it
  // this way for performance reasons (it's faster to generate and pass an
  // array than it is to generate and pass the object).
  size_t n = 0;
  while (count > 0) {
    size_t j = 0;
    while (count > 0 && j < arraysize(argv) / 2) {
      nghttp2_header item = headers[n++];
      // The header name and value are passed as external one-byte strings
      name_str =
          ExternalHeader::New<true>(env(), item.name).ToLocalChecked();
      value_str =
          ExternalHeader::New<false>(env(), item.value).ToLocalChecked();
      argv[j * 2] = name_str;
      argv[j * 2 + 1] = value_str;
      count--;
      j++;
    }
    // For performance, we pass name and value pairs to array.protototype.push
    // in batches of size NODE_PUSH_VAL_TO_ARRAY_MAX * 2 until there are no
    // more items to push.
    if (j > 0) {
      fn->Call(env()->context(), holder, j * 2, argv).ToLocalChecked();
    }
  }

  Local<Value> args[5] = {
    stream->object(),
    Integer::New(isolate, id),
    Integer::New(isolate, stream->headers_category()),
    Integer::New(isolate, frame->hd.flags),
    holder
  };
  MakeCallback(env()->onheaders_string(), arraysize(args), args);
}


inline void Http2Session::HandlePriorityFrame(const nghttp2_frame* frame) {
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env()->context();
  Context::Scope context_scope(context);

  nghttp2_priority priority_frame = frame->priority;
  int32_t id = GetFrameID(frame);
  DEBUG_HTTP2SESSION2(this, "handle priority frame for stream %d", id);
  // Priority frame stream ID should never be <= 0. nghttp2 handles this for us
  nghttp2_priority_spec spec = priority_frame.pri_spec;

  Local<Value> argv[4] = {
    Integer::New(isolate, id),
    Integer::New(isolate, spec.stream_id),
    Integer::New(isolate, spec.weight),
    Boolean::New(isolate, spec.exclusive)
  };
  MakeCallback(env()->onpriority_string(), arraysize(argv), argv);
}


inline void Http2Session::HandleDataFrame(const nghttp2_frame* frame) {
  int32_t id = GetFrameID(frame);
  DEBUG_HTTP2SESSION2(this, "handling data frame for stream %d", id);
  Http2Stream* stream = FindStream(id);

  if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
    stream->AddChunk(nullptr, 0);
  }

  if (stream->IsReading())
    stream->FlushDataChunks();
}


inline void Http2Session::HandleGoawayFrame(const nghttp2_frame* frame) {
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env()->context();
  Context::Scope context_scope(context);

  nghttp2_goaway goaway_frame = frame->goaway;
  DEBUG_HTTP2SESSION(this, "handling goaway frame");

  Local<Value> argv[3] = {
    Integer::NewFromUnsigned(isolate, goaway_frame.error_code),
    Integer::New(isolate, goaway_frame.last_stream_id),
    Undefined(isolate)
  };

  size_t length = goaway_frame.opaque_data_len;
  if (length > 0) {
    argv[2] = Buffer::Copy(isolate,
                           reinterpret_cast<char*>(goaway_frame.opaque_data),
                           length).ToLocalChecked();
  }

  MakeCallback(env()->ongoawaydata_string(), arraysize(argv), argv);
}

inline void Http2Session::HandlePingFrame(const nghttp2_frame* frame) {
  bool ack = frame->hd.flags & NGHTTP2_FLAG_ACK;
  if (ack) {
    Http2Ping* ping = PopPing();
    if (ping != nullptr)
      ping->Done(true, frame->ping.opaque_data);
  }
}


inline void Http2Session::HandleSettingsFrame(const nghttp2_frame* frame) {
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env()->context();
  Context::Scope context_scope(context);

  bool ack = frame->hd.flags & NGHTTP2_FLAG_ACK;

  Local<Value> argv[1] = { Boolean::New(isolate, ack) };
  MakeCallback(env()->onsettings_string(), arraysize(argv), argv);
}


inline void Http2Session::SendPendingData() {
  DEBUG_HTTP2SESSION(this, "sending pending data");
  // Do not attempt to send data on the socket if the destroying flag has
  // been set. That means everything is shutting down and the socket
  // will not be usable.
  if (IsDestroying())
    return;

  WriteWrap* req = nullptr;
  char* dest = nullptr;
  size_t destRemaining = 0;
  size_t destLength = 0;             // amount of data stored in dest
  size_t destOffset = 0;             // current write offset of dest

  const uint8_t* src;                // pointer to the serialized data
  ssize_t srcLength = 0;             // length of serialized data chunk

  // While srcLength is greater than zero
  while ((srcLength = nghttp2_session_mem_send(session_, &src)) > 0) {
    if (req == nullptr) {
      req = AllocateSend();
      destRemaining = req->ExtraSize();
      dest = req->Extra();
    }
    DEBUG_HTTP2SESSION2(this, "nghttp2 has %d bytes to send", srcLength);
    size_t srcRemaining = srcLength;
    size_t srcOffset = 0;

    // The amount of data we have to copy is greater than the space
    // remaining. Copy what we can into the remaining space, send it,
    // the proceed with the rest.
    while (srcRemaining > destRemaining) {
      DEBUG_HTTP2SESSION2(this, "pushing %d bytes to the socket",
                          destLength + destRemaining);
      memcpy(dest + destOffset, src + srcOffset, destRemaining);
      destLength += destRemaining;
      Send(req, dest, destLength);
      destOffset = 0;
      destLength = 0;
      srcRemaining -= destRemaining;
      srcOffset += destRemaining;
      req = AllocateSend();
      destRemaining = req->ExtraSize();
      dest = req->Extra();
    }

    if (srcRemaining > 0) {
      memcpy(dest + destOffset, src + srcOffset, srcRemaining);
      destLength += srcRemaining;
      destOffset += srcRemaining;
      destRemaining -= srcRemaining;
      srcRemaining = 0;
      srcOffset = 0;
    }
  }
  CHECK_NE(srcLength, NGHTTP2_ERR_NOMEM);

  if (destLength > 0) {
    DEBUG_HTTP2SESSION2(this, "pushing %d bytes to the socket", destLength);
    Send(req, dest, destLength);
  }
}


inline Http2Stream* Http2Session::SubmitRequest(
    nghttp2_priority_spec* prispec,
    nghttp2_nv* nva,
    size_t len,
    int32_t* ret,
    int options) {
  DEBUG_HTTP2SESSION(this, "submitting request");
  Http2Stream* stream = nullptr;
  Http2Stream::Provider::Stream prov(options);
  *ret = nghttp2_submit_request(session_, prispec, nva, len, *prov, nullptr);
  CHECK_NE(*ret, NGHTTP2_ERR_NOMEM);
  if (*ret > 0)
    stream = new Http2Stream(this, *ret, NGHTTP2_HCAT_HEADERS, options);
  return stream;
}

inline void Http2Session::SetChunksSinceLastWrite(size_t n) {
  chunks_sent_since_last_write_ = n;
}


WriteWrap* Http2Session::AllocateSend() {
  HandleScope scope(env()->isolate());
  auto AfterWrite = [](WriteWrap* req, int status) {
    req->Dispose();
  };
  Local<Object> obj =
      env()->write_wrap_constructor_function()
          ->NewInstance(env()->context()).ToLocalChecked();
  // Base the amount allocated on the remote peers max frame size
  uint32_t size =
      nghttp2_session_get_remote_settings(
          session(),
          NGHTTP2_SETTINGS_MAX_FRAME_SIZE);
  // Max frame size + 9 bytes for the header
  return WriteWrap::New(env(), obj, stream_, AfterWrite, size + 9);
}

void Http2Session::Send(WriteWrap* req, char* buf, size_t length) {
  DEBUG_HTTP2SESSION(this, "attempting to send data");
  if (stream_ == nullptr || !stream_->IsAlive() || stream_->IsClosing()) {
    return;
  }

  chunks_sent_since_last_write_++;
  uv_buf_t actual = uv_buf_init(buf, length);
  if (stream_->DoWrite(req, &actual, 1, nullptr)) {
    req->Dispose();
  }
}


void Http2Session::OnStreamAllocImpl(size_t suggested_size,
                                     uv_buf_t* buf,
                                     void* ctx) {
  Http2Session* session = static_cast<Http2Session*>(ctx);
  buf->base = session->stream_alloc();
  buf->len = kAllocBufferSize;
}


void Http2Session::OnStreamReadImpl(ssize_t nread,
                                    const uv_buf_t* bufs,
                                    uv_handle_type pending,
                                    void* ctx) {
  Http2Session* session = static_cast<Http2Session*>(ctx);
  if (nread < 0) {
    uv_buf_t tmp_buf;
    tmp_buf.base = nullptr;
    tmp_buf.len = 0;
    session->prev_read_cb_.fn(nread,
                              &tmp_buf,
                              pending,
                              session->prev_read_cb_.ctx);
    return;
  }
  if (nread > 0) {
    // Only pass data on if nread > 0
    uv_buf_t buf[] { uv_buf_init((*bufs).base, nread) };
    ssize_t ret = session->Write(buf, 1);
    if (ret < 0) {
      DEBUG_HTTP2SESSION2(session, "fatal error receiving data: %d", ret);
      CHECK_EQ(nghttp2_session_terminate_session(session->session(),
                                                 NGHTTP2_PROTOCOL_ERROR), 0);
    }
  }
}


void Http2Session::Consume(Local<External> external) {
  StreamBase* stream = static_cast<StreamBase*>(external->Value());
  stream->Consume();
  stream_ = stream;
  prev_alloc_cb_ = stream->alloc_cb();
  prev_read_cb_ = stream->read_cb();
  stream->set_alloc_cb({ Http2Session::OnStreamAllocImpl, this });
  stream->set_read_cb({ Http2Session::OnStreamReadImpl, this });
  DEBUG_HTTP2SESSION(this, "i/o stream consumed");
}


void Http2Session::Unconsume() {
  if (prev_alloc_cb_.is_empty())
    return;
  stream_->set_alloc_cb(prev_alloc_cb_);
  stream_->set_read_cb(prev_read_cb_);
  prev_alloc_cb_.clear();
  prev_read_cb_.clear();
  stream_ = nullptr;
  DEBUG_HTTP2SESSION(this, "i/o stream unconsumed");
}




Http2Stream::Http2Stream(
    Http2Session* session,
    int32_t id,
    nghttp2_headers_category category,
    int options) : AsyncWrap(session->env(),
                             session->env()->http2stream_constructor_template()
                                 ->NewInstance(session->env()->context())
                                     .ToLocalChecked(),
                             AsyncWrap::PROVIDER_HTTP2STREAM),
                   StreamBase(session->env()),
                   session_(session),
                   id_(id),
                   current_headers_category_(category) {
  MakeWeak<Http2Stream>(this);

  // Limit the number of header pairs
  max_header_pairs_ = session->GetMaxHeaderPairs();
  if (max_header_pairs_ == 0)
  max_header_pairs_ = DEFAULT_MAX_HEADER_LIST_PAIRS;
  current_headers_.reserve(max_header_pairs_);

  // Limit the number of header octets
  max_header_length_ =
      std::min(
        nghttp2_session_get_local_settings(
          session->session(),
          NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE),
      MAX_MAX_HEADER_LIST_SIZE);

  if (options & STREAM_OPTION_GET_TRAILERS)
    flags_ |= NGHTTP2_STREAM_FLAG_TRAILERS;

  if (options & STREAM_OPTION_EMPTY_PAYLOAD)
    Shutdown();
  session->AddStream(this);
}


Http2Stream::~Http2Stream() {
  CHECK(persistent().IsEmpty());
  if (!object().IsEmpty())
    ClearWrap(object());
  persistent().Reset();
}

void Http2Stream::StartHeaders(nghttp2_headers_category category) {
  DEBUG_HTTP2STREAM2(this, "starting headers, category: %d", id_, category);
  current_headers_length_ = 0;
  current_headers_.clear();
  current_headers_category_ = category;
}

nghttp2_stream* Http2Stream::operator*() {
  return nghttp2_session_find_stream(**session_, id_);
}


void Http2Stream::OnTrailers(const SubmitTrailers& submit_trailers) {
  DEBUG_HTTP2STREAM(this, "prompting for trailers");
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Local<Context> context = env()->context();
  Context::Scope context_scope(context);

  Local<Value> ret =
      MakeCallback(env()->ontrailers_string(), 0, nullptr).ToLocalChecked();
  if (!ret.IsEmpty()) {
    if (ret->IsArray()) {
      Local<Array> headers = ret.As<Array>();
      if (headers->Length() > 0) {
        Headers trailers(isolate, context, headers);
        submit_trailers.Submit(*trailers, trailers.length());
      }
    }
  }
}


inline void Http2Stream::AddChunk(const uint8_t* data, size_t len) {
  char* buf = nullptr;
  if (len > 0) {
    buf = Malloc<char>(len);
    memcpy(buf, data, len);
  }
  data_chunks_.emplace(uv_buf_init(buf, len));
}


int Http2Stream::DoWrite(WriteWrap* req_wrap,
                         uv_buf_t* bufs,
                         size_t count,
                         uv_stream_t* send_handle) {
  session_->SetChunksSinceLastWrite();

  nghttp2_stream_write_t* req = new nghttp2_stream_write_t;
  req->data = req_wrap;

  auto AfterWrite = [](nghttp2_stream_write_t* req, int status) {
    WriteWrap* wrap = static_cast<WriteWrap*>(req->data);
    wrap->Done(status);
    delete req;
  };
  req_wrap->Dispatched();
  Write(req, bufs, count, AfterWrite);
  return 0;
}


inline void Http2Stream::Close(int32_t code) {
  flags_ |= NGHTTP2_STREAM_FLAG_CLOSED;
  code_ = code;
  DEBUG_HTTP2STREAM2(this, "closed with code %d", code);
}


inline void Http2Stream::Shutdown() {
  flags_ |= NGHTTP2_STREAM_FLAG_SHUT;
  CHECK_NE(nghttp2_session_resume_data(session_->session(), id_),
           NGHTTP2_ERR_NOMEM);
  DEBUG_HTTP2STREAM(this, "writable side shutdown");
}

int Http2Stream::DoShutdown(ShutdownWrap* req_wrap) {
  req_wrap->Dispatched();
  Shutdown();
  req_wrap->Done(0);
  return 0;
}

inline void Http2Stream::Destroy() {
  DEBUG_HTTP2STREAM(this, "destroying stream");
  // Do nothing if this stream instance is already destroyed
  if (IsDestroyed())
    return;

  flags_ |= NGHTTP2_STREAM_FLAG_DESTROYED;
  Http2Session* session = this->session_;

  if (session != nullptr) {
    session_->RemoveStream(id_);
    session_ = nullptr;
  }

  // Free any remaining incoming data chunks.
  while (!data_chunks_.empty()) {
    uv_buf_t buf = data_chunks_.front();
    free(buf.base);
    data_chunks_.pop();
  }

  // Free any remaining outgoing data chunks.
  while (!queue_.empty()) {
    nghttp2_stream_write* head = queue_.front();
    head->cb(head->req, UV_ECANCELED);
    delete head;
    queue_.pop();
  }

  if (!object().IsEmpty())
    ClearWrap(object());
  persistent().Reset();

  delete this;
}


void Http2Stream::OnDataChunk(
    uv_buf_t* chunk) {
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  ssize_t len = -1;
  Local<Object> buf;
  if (chunk != nullptr) {
    len = chunk->len;
    buf = Buffer::New(isolate, chunk->base, len).ToLocalChecked();
  }
  EmitData(len, buf, this->object());
}


inline void Http2Stream::FlushDataChunks() {
  if (!data_chunks_.empty()) {
    uv_buf_t buf = data_chunks_.front();
    data_chunks_.pop();
    if (buf.len > 0) {
      CHECK_EQ(nghttp2_session_consume_stream(session_->session(),
                                              id_, buf.len), 0);
      OnDataChunk(&buf);
    } else {
      OnDataChunk(nullptr);
    }
  }
}


inline int Http2Stream::SubmitResponse(nghttp2_nv* nva,
                                       size_t len,
                                       int options) {
  DEBUG_HTTP2STREAM(this, "submitting response");
  if (options & STREAM_OPTION_GET_TRAILERS)
    flags_ |= NGHTTP2_STREAM_FLAG_TRAILERS;

  if (!IsWritable())
    options |= STREAM_OPTION_EMPTY_PAYLOAD;

  Http2Stream::Provider::Stream prov(this, options);
  int ret = nghttp2_submit_response(session_->session(), id_, nva, len, *prov);
  CHECK_NE(ret, NGHTTP2_ERR_NOMEM);
  return ret;
}


// Initiate a response that contains data read from a file descriptor.
inline int Http2Stream::SubmitFile(int fd,
                                   nghttp2_nv* nva, size_t len,
                                   int64_t offset,
                                   int64_t length,
                                   int options) {
  DEBUG_HTTP2STREAM(this, "submitting file");
  if (options & STREAM_OPTION_GET_TRAILERS)
    flags_ |= NGHTTP2_STREAM_FLAG_TRAILERS;

  if (offset > 0) fd_offset_ = offset;
  if (length > -1) fd_length_ = length;

  Http2Stream::Provider::FD prov(this, options, fd);
  int ret = nghttp2_submit_response(session_->session(), id_, nva, len, *prov);
  CHECK_NE(ret, NGHTTP2_ERR_NOMEM);
  return ret;
}


// Submit informational headers for a stream.
inline int Http2Stream::SubmitInfo(nghttp2_nv* nva, size_t len) {
  DEBUG_HTTP2STREAM2(this, "sending %d informational headers", len);
  int ret = nghttp2_submit_headers(session_->session(),
                                   NGHTTP2_FLAG_NONE,
                                   id_, nullptr,
                                   nva, len, nullptr);
  CHECK_NE(ret, NGHTTP2_ERR_NOMEM);
  return ret;
}


inline int Http2Stream::SubmitPriority(nghttp2_priority_spec* prispec,
                                       bool silent) {
  DEBUG_HTTP2STREAM(this, "sending priority spec");
  int ret = silent ?
      nghttp2_session_change_stream_priority(session_->session(),
                                             id_, prispec) :
      nghttp2_submit_priority(session_->session(),
                              NGHTTP2_FLAG_NONE,
                              id_, prispec);
  CHECK_NE(ret, NGHTTP2_ERR_NOMEM);
  return ret;
}


inline int Http2Stream::SubmitRstStream(const uint32_t code) {
  DEBUG_HTTP2STREAM2(this, "sending rst-stream with code %d", code);
  session_->SendPendingData();
  CHECK_EQ(nghttp2_submit_rst_stream(session_->session(),
                                     NGHTTP2_FLAG_NONE,
                                     id_,
                                     code), 0);
  return 0;
}


// Submit a push promise.
inline Http2Stream* Http2Stream::SubmitPushPromise(nghttp2_nv* nva,
                                                   size_t len,
                                                   int32_t* ret,
                                                   int options) {
  DEBUG_HTTP2STREAM(this, "sending push promise");
  *ret = nghttp2_submit_push_promise(session_->session(), NGHTTP2_FLAG_NONE,
                                     id_, nva, len, nullptr);
  CHECK_NE(*ret, NGHTTP2_ERR_NOMEM);
  Http2Stream* stream = nullptr;
  if (*ret > 0)
    stream = new Http2Stream(session_, *ret, NGHTTP2_HCAT_HEADERS, options);

  return stream;
}

inline int Http2Stream::ReadStart() {
  flags_ |= NGHTTP2_STREAM_FLAG_READ_START;
  flags_ &= ~NGHTTP2_STREAM_FLAG_READ_PAUSED;

  // Flush any queued data chunks immediately out to the JS layer
  FlushDataChunks();
  DEBUG_HTTP2STREAM(this, "reading starting");
  return 0;
}


inline int Http2Stream::ReadStop() {
  if (!IsReading())
    return 0;
  flags_ |= NGHTTP2_STREAM_FLAG_READ_PAUSED;
  DEBUG_HTTP2STREAM(this, "reading stopped");
  return 0;
}

// Queue the given set of uv_but_t handles for writing to an
// nghttp2_stream. The callback will be invoked once the chunks
// of data have been flushed to the underlying nghttp2_session.
// Note that this does *not* mean that the data has been flushed
// to the socket yet.
inline int Http2Stream::Write(nghttp2_stream_write_t* req,
                              const uv_buf_t bufs[],
                              unsigned int nbufs,
                              nghttp2_stream_write_cb cb) {
  if (!IsWritable()) {
    if (cb != nullptr)
      cb(req, UV_EOF);
    return 0;
  }
  DEBUG_HTTP2STREAM2(this, "queuing %d buffers to send", id_, nbufs);
  nghttp2_stream_write* item = new nghttp2_stream_write;
  item->cb = cb;
  item->req = req;
  item->nbufs = nbufs;
  item->bufs.AllocateSufficientStorage(nbufs);
  memcpy(*(item->bufs), bufs, nbufs * sizeof(*bufs));
  queue_.push(item);
  CHECK_NE(nghttp2_session_resume_data(**session_, id_), NGHTTP2_ERR_NOMEM);
  return 0;
}

inline size_t GetBufferLength(nghttp2_rcbuf* buf) {
  return nghttp2_rcbuf_get_buf(buf).len;
}

inline bool Http2Stream::AddHeader(nghttp2_rcbuf* name,
                                   nghttp2_rcbuf* value,
                                   uint8_t flags) {
  size_t length = GetBufferLength(name) + GetBufferLength(value) + 32;
  if (current_headers_.size() == max_header_pairs_ ||
      current_headers_length_ + length > max_header_length_) {
    return false;
  }
  nghttp2_header header;
  header.name = name;
  header.value = value;
  header.flags = flags;
  current_headers_.push_back(header);
  nghttp2_rcbuf_incref(name);
  nghttp2_rcbuf_incref(value);
  current_headers_length_ += length;
  return true;
}


Http2Stream* GetStream(Http2Session* session,
                       int32_t id,
                       nghttp2_data_source* source) {
  Http2Stream* stream = static_cast<Http2Stream*>(source->ptr);
  if (stream == nullptr)
    stream = session->FindStream(id);
  CHECK_NE(stream, nullptr);
  CHECK_EQ(id, stream->id());
  return stream;
}

Http2Stream::Provider::Provider(Http2Stream* stream, int options) {
  provider_.source.ptr = stream;
  empty_ = options & STREAM_OPTION_EMPTY_PAYLOAD;
}

Http2Stream::Provider::Provider(int options) {
  provider_.source.ptr = nullptr;
  empty_ = options & STREAM_OPTION_EMPTY_PAYLOAD;
}

Http2Stream::Provider::~Provider() {
  provider_.source.ptr = nullptr;
}

Http2Stream::Provider::FD::FD(Http2Stream* stream, int options, int fd)
    : Http2Stream::Provider(stream, options) {
  provider_.source.fd = fd;
  provider_.read_callback = Http2Stream::Provider::FD::OnRead;
}

Http2Stream::Provider::FD::FD(int options, int fd)
    : Http2Stream::Provider(options) {
  provider_.source.fd = fd;
  provider_.read_callback = Http2Stream::Provider::FD::OnRead;
}

ssize_t Http2Stream::Provider::FD::OnRead(nghttp2_session* handle,
                                          int32_t id,
                                          uint8_t* buf,
                                          size_t length,
                                          uint32_t* flags,
                                          nghttp2_data_source* source,
                                          void* user_data) {
  Http2Session* session = static_cast<Http2Session*>(user_data);
  Http2Stream* stream = session->FindStream(id);
  DEBUG_HTTP2SESSION2(session, "reading outbound file data for stream %d", id);
  CHECK_EQ(id, stream->id());

  int fd = source->fd;
  int64_t offset = stream->fd_offset_;
  ssize_t numchars = 0;

  if (stream->fd_length_ >= 0 &&
      stream->fd_length_ < static_cast<int64_t>(length))
    length = stream->fd_length_;

  uv_buf_t data;
  data.base = reinterpret_cast<char*>(buf);
  data.len = length;

  uv_fs_t read_req;

  if (length > 0) {
    // TODO(addaleax): Never use synchronous I/O on the main thread.
    numchars = uv_fs_read(session->event_loop(),
                          &read_req,
                          fd, &data, 1,
                          offset, nullptr);
    uv_fs_req_cleanup(&read_req);
  }

  // Close the stream with an error if reading fails
  if (numchars < 0)
    return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;

  // Update the read offset for the next read
  stream->fd_offset_ += numchars;
  stream->fd_length_ -= numchars;

  // if numchars < length, assume that we are done.
  if (static_cast<size_t>(numchars) < length || length <= 0) {
    DEBUG_HTTP2SESSION2(session, "no more data for stream %d", id);
    *flags |= NGHTTP2_DATA_FLAG_EOF;
    session->GetTrailers(stream, flags);
  }

  return numchars;
}

Http2Stream::Provider::Stream::Stream(int options)
    : Http2Stream::Provider(options) {
  provider_.read_callback = Http2Stream::Provider::Stream::OnRead;
}

Http2Stream::Provider::Stream::Stream(Http2Stream* stream, int options)
    : Http2Stream::Provider(stream, options) {
  provider_.read_callback = Http2Stream::Provider::Stream::OnRead;
}

ssize_t Http2Stream::Provider::Stream::OnRead(nghttp2_session* handle,
                                              int32_t id,
                                              uint8_t* buf,
                                              size_t length,
                                              uint32_t* flags,
                                              nghttp2_data_source* source,
                                              void* user_data) {
  Http2Session* session = static_cast<Http2Session*>(user_data);
  DEBUG_HTTP2SESSION2(session, "reading outbound data for stream %d", id);
  Http2Stream* stream = GetStream(session, id, source);
  CHECK_EQ(id, stream->id());

  size_t amount = 0;          // amount of data being sent in this data frame.

  uv_buf_t current;

  if (!stream->queue_.empty()) {
    DEBUG_HTTP2SESSION2(session, "stream %d has pending outbound data", id);
    nghttp2_stream_write* head = stream->queue_.front();
    current = head->bufs[stream->queue_index_];
    size_t clen = current.len - stream->queue_offset_;
    amount = std::min(clen, length);
    DEBUG_HTTP2SESSION2(session, "sending %d bytes for data frame on stream %d",
                        amount, id);
    if (amount > 0) {
      memcpy(buf, current.base + stream->queue_offset_, amount);
      stream->queue_offset_ += amount;
    }
    if (stream->queue_offset_ == current.len) {
      stream->queue_index_++;
      stream->queue_offset_ = 0;
    }
    if (stream->queue_index_ == head->nbufs) {
      head->cb(head->req, 0);
      delete head;
      stream->queue_.pop();
      stream->queue_offset_ = 0;
      stream->queue_index_ = 0;
    }
  }

  if (amount == 0 && stream->IsWritable() && stream->queue_.empty()) {
    DEBUG_HTTP2SESSION2(session, "deferring stream %d", id);
    return NGHTTP2_ERR_DEFERRED;
  }

  if (stream->queue_.empty() && !stream->IsWritable()) {
    DEBUG_HTTP2SESSION2(session, "no more data for stream %d", id);
    *flags |= NGHTTP2_DATA_FLAG_EOF;

    session->GetTrailers(stream, flags);
  }

  return amount;
}



// Implementation of the JavaScript API

void HttpErrorString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uint32_t val = args[0]->Uint32Value(env->context()).ToChecked();
  args.GetReturnValue().Set(
      String::NewFromOneByte(
          env->isolate(),
          reinterpret_cast<const uint8_t*>(nghttp2_strerror(val)),
          v8::NewStringType::kInternalized).ToLocalChecked());
}


// Serializes the settings object into a Buffer instance that
// would be suitable, for instance, for creating the Base64
// output for an HTTP2-Settings header field.
void PackSettings(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Settings settings(env);
  args.GetReturnValue().Set(settings.Pack());
}


void RefreshDefaultSettings(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Settings::RefreshDefaults(env);
}


void Http2Session::SetNextStreamID(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  int32_t id = args[0]->Int32Value(env->context()).ToChecked();
  if (nghttp2_session_set_next_stream_id(**session, id) < 0) {
    DEBUG_HTTP2SESSION2(session, "failed to set next stream id to %d", id);
    return args.GetReturnValue().Set(false);
  }
  args.GetReturnValue().Set(true);
  DEBUG_HTTP2SESSION2(session, "set next stream id to %d", id);
}


template <get_setting fn>
void Http2Session::RefreshSettings(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Http2Settings::Update(env, session, fn);
  DEBUG_HTTP2SESSION(session, "settings refreshed for session");
}


void Http2Session::RefreshState(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  DEBUG_HTTP2SESSION(session, "refreshing state");

  AliasedBuffer<double, v8::Float64Array>& buffer =
      env->http2_state()->session_state_buffer;

  nghttp2_session* s = **session;

  buffer[IDX_SESSION_STATE_EFFECTIVE_LOCAL_WINDOW_SIZE] =
      nghttp2_session_get_effective_local_window_size(s);
  buffer[IDX_SESSION_STATE_EFFECTIVE_RECV_DATA_LENGTH] =
      nghttp2_session_get_effective_recv_data_length(s);
  buffer[IDX_SESSION_STATE_NEXT_STREAM_ID] =
      nghttp2_session_get_next_stream_id(s);
  buffer[IDX_SESSION_STATE_LOCAL_WINDOW_SIZE] =
      nghttp2_session_get_local_window_size(s);
  buffer[IDX_SESSION_STATE_LAST_PROC_STREAM_ID] =
      nghttp2_session_get_last_proc_stream_id(s);
  buffer[IDX_SESSION_STATE_REMOTE_WINDOW_SIZE] =
      nghttp2_session_get_remote_window_size(s);
  buffer[IDX_SESSION_STATE_OUTBOUND_QUEUE_SIZE] =
      nghttp2_session_get_outbound_queue_size(s);
  buffer[IDX_SESSION_STATE_HD_DEFLATE_DYNAMIC_TABLE_SIZE] =
      nghttp2_session_get_hd_deflate_dynamic_table_size(s);
  buffer[IDX_SESSION_STATE_HD_INFLATE_DYNAMIC_TABLE_SIZE] =
      nghttp2_session_get_hd_inflate_dynamic_table_size(s);
}


void Http2Session::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());
  int val = args[0]->IntegerValue(env->context()).ToChecked();
  nghttp2_session_type type = static_cast<nghttp2_session_type>(val);
  Http2Session* session = new Http2Session(env, args.This(), type);
  session->get_async_id();  // avoid compiler warning
  DEBUG_HTTP2SESSION(session, "session created");
}


void Http2Session::Consume(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  CHECK(args[0]->IsExternal());
  session->Consume(args[0].As<External>());
}


void Http2Session::Destroy(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  DEBUG_HTTP2SESSION(session, "destroying session");

  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();

  bool skipUnconsume = args[0]->BooleanValue(context).ToChecked();

  if (!skipUnconsume)
    session->Unconsume();
  session->Close();
}


void Http2Session::Destroying(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->MarkDestroying();
  DEBUG_HTTP2SESSION(session, "preparing to destroy session");
}


void Http2Session::Settings(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();

  Http2Settings settings(env);
  session->Http2Session::Settings(*settings, settings.length());
  DEBUG_HTTP2SESSION(session, "settings submitted");
}


void Http2Session::Request(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();

  Local<Array> headers = args[0].As<Array>();
  int options = args[1]->IntegerValue(context).ToChecked();
  Http2Priority priority(env, args[2], args[3], args[4]);

  Headers list(isolate, context, headers);

  DEBUG_HTTP2SESSION(session, "request submitted");

  int32_t ret = 0;
  Http2Stream* stream =
      session->Http2Session::SubmitRequest(*priority, *list, list.length(),
                                           &ret, options);

  if (ret <= 0) {
    DEBUG_HTTP2SESSION2(session, "could not submit request: %s",
                        nghttp2_strerror(ret));
    return args.GetReturnValue().Set(ret);
  }

  DEBUG_HTTP2SESSION2(session, "request submitted, new stream id %d",
                      stream->id());
  args.GetReturnValue().Set(stream->object());
}


void Http2Session::ShutdownNotice(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->SubmitShutdownNotice();
  DEBUG_HTTP2SESSION(session, "shutdown notice sent");
}


void Http2Session::Goaway(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());

  uint32_t errorCode = args[0]->Uint32Value(context).ToChecked();
  int32_t lastStreamID = args[1]->Int32Value(context).ToChecked();
  Local<Value> opaqueData = args[2];

  uint8_t* data = NULL;
  size_t length = 0;

  if (opaqueData->BooleanValue(context).ToChecked()) {
    THROW_AND_RETURN_UNLESS_BUFFER(env, opaqueData);
    SPREAD_BUFFER_ARG(opaqueData, buf);
    data = reinterpret_cast<uint8_t*>(buf_data);
    length = buf_length;
  }

  int status = nghttp2_submit_goaway(session->session(),
                                     NGHTTP2_FLAG_NONE,
                                     lastStreamID,
                                     errorCode,
                                     data, length);
  CHECK_NE(status, NGHTTP2_ERR_NOMEM);
  args.GetReturnValue().Set(status);
  DEBUG_HTTP2SESSION2(session, "immediate shutdown initiated with "
                      "last stream id %d, code %d, and opaque-data length %d",
                      lastStreamID, errorCode, length);
}


void Http2Session::UpdateChunksSent(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());

  uint32_t length = session->chunks_sent_since_last_write_;

  session->object()->Set(env->context(),
                         env->chunks_sent_since_last_write_string(),
                         Integer::NewFromUnsigned(isolate, length)).FromJust();

  args.GetReturnValue().Set(length);
}


void Http2Stream::RstStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  uint32_t code = args[0]->Uint32Value(context).ToChecked();
  args.GetReturnValue().Set(stream->SubmitRstStream(code));
  DEBUG_HTTP2STREAM2(stream, "rst_stream code %d sent", code);
}


void Http2Stream::Respond(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  Local<Array> headers = args[0].As<Array>();
  int options = args[1]->IntegerValue(context).ToChecked();

  Headers list(isolate, context, headers);

  args.GetReturnValue().Set(
      stream->SubmitResponse(*list, list.length(), options));
  DEBUG_HTTP2STREAM(stream, "response submitted");
}


void Http2Stream::RespondFD(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  int fd = args[0]->Int32Value(context).ToChecked();
  Local<Array> headers = args[1].As<Array>();

  int64_t offset = args[2]->IntegerValue(context).ToChecked();
  int64_t length = args[3]->IntegerValue(context).ToChecked();
  int options = args[4]->IntegerValue(context).ToChecked();

  stream->session()->SetChunksSinceLastWrite();

  Headers list(isolate, context, headers);
  args.GetReturnValue().Set(stream->SubmitFile(fd, *list, list.length(),
                                               offset, length, options));
  DEBUG_HTTP2STREAM2(stream, "file response submitted for fd %d", fd);
}


void Http2Stream::Info(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  Local<Array> headers = args[0].As<Array>();

  Headers list(isolate, context, headers);
  args.GetReturnValue().Set(stream->SubmitInfo(*list, list.length()));
  DEBUG_HTTP2STREAM2(stream, "%d informational headers sent",
                     headers->Length());
}


void Http2Stream::GetID(const FunctionCallbackInfo<Value>& args) {
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  args.GetReturnValue().Set(stream->id());
}


void Http2Stream::Destroy(const FunctionCallbackInfo<Value>& args) {
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  DEBUG_HTTP2STREAM(stream, "destroying stream");
  stream->Destroy();
}


void Http2Stream::FlushData(const FunctionCallbackInfo<Value>& args) {
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());
  stream->ReadStart();
  DEBUG_HTTP2STREAM(stream, "data flushed to js");
}


void Http2Stream::PushPromise(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  Http2Stream* parent;
  ASSIGN_OR_RETURN_UNWRAP(&parent, args.Holder());

  Local<Array> headers = args[0].As<Array>();
  int options = args[1]->IntegerValue(context).ToChecked();

  Headers list(isolate, context, headers);

  DEBUG_HTTP2STREAM(parent, "creating push promise");

  int32_t ret = 0;
  Http2Stream* stream = parent->SubmitPushPromise(*list, list.length(),
                                                  &ret, options);
  if (ret <= 0) {
    DEBUG_HTTP2STREAM2(parent, "failed to create push stream: %d", ret);
    return args.GetReturnValue().Set(ret);
  }
  DEBUG_HTTP2STREAM2(parent, "push stream %d created", stream->id());
  args.GetReturnValue().Set(stream->object());
}


void Http2Stream::Priority(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  Http2Priority priority(env, args[0], args[1], args[2]);
  bool silent = args[3]->BooleanValue(context).ToChecked();

  CHECK_EQ(stream->SubmitPriority(*priority, silent), 0);
  DEBUG_HTTP2STREAM(stream, "priority submitted");
}


void Http2Stream::RefreshState(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Stream* stream;
  ASSIGN_OR_RETURN_UNWRAP(&stream, args.Holder());

  DEBUG_HTTP2STREAM(stream, "refreshing state");

  AliasedBuffer<double, v8::Float64Array>& buffer =
      env->http2_state()->stream_state_buffer;

  nghttp2_stream* str = **stream;
  nghttp2_session* s = **(stream->session());

  if (str == nullptr) {
    buffer[IDX_STREAM_STATE] = NGHTTP2_STREAM_STATE_IDLE;
    buffer[IDX_STREAM_STATE_WEIGHT] =
        buffer[IDX_STREAM_STATE_SUM_DEPENDENCY_WEIGHT] =
        buffer[IDX_STREAM_STATE_LOCAL_CLOSE] =
        buffer[IDX_STREAM_STATE_REMOTE_CLOSE] =
        buffer[IDX_STREAM_STATE_LOCAL_WINDOW_SIZE] = 0;
  } else {
    buffer[IDX_STREAM_STATE] =
        nghttp2_stream_get_state(str);
    buffer[IDX_STREAM_STATE_WEIGHT] =
        nghttp2_stream_get_weight(str);
    buffer[IDX_STREAM_STATE_SUM_DEPENDENCY_WEIGHT] =
        nghttp2_stream_get_sum_dependency_weight(str);
    buffer[IDX_STREAM_STATE_LOCAL_CLOSE] =
        nghttp2_session_get_stream_local_close(s, stream->id());
    buffer[IDX_STREAM_STATE_REMOTE_CLOSE] =
        nghttp2_session_get_stream_remote_close(s, stream->id());
    buffer[IDX_STREAM_STATE_LOCAL_WINDOW_SIZE] =
        nghttp2_session_get_stream_local_window_size(s, stream->id());
  }
}

void Http2Session::Ping(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());

  uint8_t* payload = nullptr;
  if (Buffer::HasInstance(args[0])) {
    payload = reinterpret_cast<uint8_t*>(Buffer::Data(args[0]));
    CHECK_EQ(Buffer::Length(args[0]), 8);
  }

  Http2Session::Http2Ping* ping = new Http2Ping(session);
  Local<Object> obj = ping->object();
  obj->Set(env->context(), env->ondone_string(), args[1]).FromJust();

  if (!session->AddPing(ping)) {
    ping->Done(false);
    return args.GetReturnValue().Set(false);
  }

  ping->Send(payload);
  args.GetReturnValue().Set(true);
}

Http2Session::Http2Ping* Http2Session::PopPing() {
  Http2Ping* ping = nullptr;
  if (!outstanding_pings_.empty()) {
    ping = outstanding_pings_.front();
    outstanding_pings_.pop();
  }
  return ping;
}

bool Http2Session::AddPing(Http2Session::Http2Ping* ping) {
  if (outstanding_pings_.size() == max_outstanding_pings_)
    return false;
  outstanding_pings_.push(ping);
  return true;
}

Http2Session::Http2Ping::Http2Ping(
    Http2Session* session)
        : AsyncWrap(session->env(),
                    session->env()->http2ping_constructor_template()
                        ->NewInstance(session->env()->context())
                            .ToLocalChecked(),
                    AsyncWrap::PROVIDER_HTTP2PING),
          session_(session),
          startTime_(uv_hrtime()) { }

Http2Session::Http2Ping::~Http2Ping() {
  if (!object().IsEmpty())
    ClearWrap(object());
  persistent().Reset();
  CHECK(persistent().IsEmpty());
}

void Http2Session::Http2Ping::Send(uint8_t* payload) {
  uint8_t data[8];
  if (payload == nullptr) {
    memcpy(&data, &startTime_, arraysize(data));
    payload = data;
  }
  CHECK_EQ(nghttp2_submit_ping(**session_, NGHTTP2_FLAG_NONE, payload), 0);
}

void Http2Session::Http2Ping::Done(bool ack, const uint8_t* payload) {
  uint64_t end = uv_hrtime();
  double duration = (end - startTime_) / 1e6;

  Local<Value> buf = Undefined(env()->isolate());
  if (payload != nullptr) {
    buf = Buffer::Copy(env()->isolate(),
                       reinterpret_cast<const char*>(payload),
                       8).ToLocalChecked();
  }

  Local<Value> argv[3] = {
    Boolean::New(env()->isolate(), ack),
    Number::New(env()->isolate(), duration),
    buf
  };
  MakeCallback(env()->ondone_string(), arraysize(argv), argv);
  delete this;
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);

  std::unique_ptr<http2_state> state(new http2_state(isolate));

#define SET_STATE_TYPEDARRAY(name, field)             \
  target->Set(context,                                \
              FIXED_ONE_BYTE_STRING(isolate, (name)), \
              (field)).FromJust()

  // Initialize the buffer used for padding callbacks
  SET_STATE_TYPEDARRAY(
    "paddingBuffer", state->padding_buffer.GetJSArray());
  // Initialize the buffer used to store the session state
  SET_STATE_TYPEDARRAY(
    "sessionState", state->session_state_buffer.GetJSArray());
  // Initialize the buffer used to store the stream state
  SET_STATE_TYPEDARRAY(
    "streamState", state->stream_state_buffer.GetJSArray());
  SET_STATE_TYPEDARRAY(
    "settingsBuffer", state->settings_buffer.GetJSArray());
  SET_STATE_TYPEDARRAY(
    "optionsBuffer", state->options_buffer.GetJSArray());
#undef SET_STATE_TYPEDARRAY

  env->set_http2_state(std::move(state));

  NODE_DEFINE_CONSTANT(target, PADDING_BUF_FRAME_LENGTH);
  NODE_DEFINE_CONSTANT(target, PADDING_BUF_MAX_PAYLOAD_LENGTH);
  NODE_DEFINE_CONSTANT(target, PADDING_BUF_RETURN_VALUE);

  // Method to fetch the nghttp2 string description of an nghttp2 error code
  env->SetMethod(target, "nghttp2ErrorString", HttpErrorString);

  Local<String> http2SessionClassName =
    FIXED_ONE_BYTE_STRING(isolate, "Http2Session");

  Local<FunctionTemplate> ping = FunctionTemplate::New(env->isolate());
  ping->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Http2Ping"));
  AsyncWrap::AddWrapMethods(env, ping);
  Local<ObjectTemplate> pingt = ping->InstanceTemplate();
  pingt->SetInternalFieldCount(1);
  env->set_http2ping_constructor_template(pingt);

  Local<FunctionTemplate> stream = FunctionTemplate::New(env->isolate());
  stream->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Http2Stream"));
  env->SetProtoMethod(stream, "id", Http2Stream::GetID);
  env->SetProtoMethod(stream, "destroy", Http2Stream::Destroy);
  env->SetProtoMethod(stream, "flushData", Http2Stream::FlushData);
  env->SetProtoMethod(stream, "priority", Http2Stream::Priority);
  env->SetProtoMethod(stream, "pushPromise", Http2Stream::PushPromise);
  env->SetProtoMethod(stream, "info", Http2Stream::Info);
  env->SetProtoMethod(stream, "respondFD", Http2Stream::RespondFD);
  env->SetProtoMethod(stream, "respond", Http2Stream::Respond);
  env->SetProtoMethod(stream, "rstStream", Http2Stream::RstStream);
  env->SetProtoMethod(stream, "refreshState", Http2Stream::RefreshState);
  AsyncWrap::AddWrapMethods(env, stream);
  StreamBase::AddMethods<Http2Stream>(env, stream, StreamBase::kFlagHasWritev);
  Local<ObjectTemplate> streamt = stream->InstanceTemplate();
  streamt->SetInternalFieldCount(1);
  env->set_http2stream_constructor_template(streamt);
  target->Set(context,
              FIXED_ONE_BYTE_STRING(env->isolate(), "Http2Stream"),
              stream->GetFunction()).FromJust();

  Local<FunctionTemplate> session =
      env->NewFunctionTemplate(Http2Session::New);
  session->SetClassName(http2SessionClassName);
  session->InstanceTemplate()->SetInternalFieldCount(1);
  AsyncWrap::AddWrapMethods(env, session);
  env->SetProtoMethod(session, "ping", Http2Session::Ping);
  env->SetProtoMethod(session, "consume", Http2Session::Consume);
  env->SetProtoMethod(session, "destroy", Http2Session::Destroy);
  env->SetProtoMethod(session, "destroying", Http2Session::Destroying);
  env->SetProtoMethod(session, "shutdownNotice", Http2Session::ShutdownNotice);
  env->SetProtoMethod(session, "goaway", Http2Session::Goaway);
  env->SetProtoMethod(session, "settings", Http2Session::Settings);
  env->SetProtoMethod(session, "request", Http2Session::Request);
  env->SetProtoMethod(session, "setNextStreamID",
                      Http2Session::SetNextStreamID);
  env->SetProtoMethod(session, "updateChunksSent",
                      Http2Session::UpdateChunksSent);
  env->SetProtoMethod(session, "refreshState", Http2Session::RefreshState);
  env->SetProtoMethod(
      session, "localSettings",
      Http2Session::RefreshSettings<nghttp2_session_get_local_settings>);
  env->SetProtoMethod(
      session, "remoteSettings",
      Http2Session::RefreshSettings<nghttp2_session_get_remote_settings>);
  target->Set(context,
              http2SessionClassName,
              session->GetFunction()).FromJust();

  Local<Object> constants = Object::New(isolate);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SESSION_SERVER);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SESSION_CLIENT);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_STREAM_STATE_IDLE);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_STREAM_STATE_OPEN);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_STREAM_STATE_RESERVED_LOCAL);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_STREAM_STATE_RESERVED_REMOTE);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_STREAM_STATE_HALF_CLOSED_LOCAL);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_STREAM_STATE_HALF_CLOSED_REMOTE);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_STREAM_STATE_CLOSED);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_NO_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_PROTOCOL_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_INTERNAL_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_FLOW_CONTROL_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SETTINGS_TIMEOUT);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_STREAM_CLOSED);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_FRAME_SIZE_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_REFUSED_STREAM);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_CANCEL);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_COMPRESSION_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_CONNECT_ERROR);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_ENHANCE_YOUR_CALM);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_INADEQUATE_SECURITY);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_HTTP_1_1_REQUIRED);

  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_HCAT_REQUEST);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_HCAT_RESPONSE);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_HCAT_PUSH_RESPONSE);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_HCAT_HEADERS);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_NV_FLAG_NONE);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_NV_FLAG_NO_INDEX);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_ERR_DEFERRED);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_ERR_INVALID_ARGUMENT);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_ERR_STREAM_CLOSED);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_ERR_FRAME_SIZE_ERROR);

  NODE_DEFINE_HIDDEN_CONSTANT(constants, STREAM_OPTION_EMPTY_PAYLOAD);
  NODE_DEFINE_HIDDEN_CONSTANT(constants, STREAM_OPTION_GET_TRAILERS);

  NODE_DEFINE_CONSTANT(constants, NGHTTP2_FLAG_NONE);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_FLAG_END_STREAM);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_FLAG_END_HEADERS);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_FLAG_ACK);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_FLAG_PADDED);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_FLAG_PRIORITY);

  NODE_DEFINE_CONSTANT(constants, DEFAULT_SETTINGS_HEADER_TABLE_SIZE);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_SETTINGS_ENABLE_PUSH);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_SETTINGS_INITIAL_WINDOW_SIZE);
  NODE_DEFINE_CONSTANT(constants, DEFAULT_SETTINGS_MAX_FRAME_SIZE);
  NODE_DEFINE_CONSTANT(constants, MAX_MAX_FRAME_SIZE);
  NODE_DEFINE_CONSTANT(constants, MIN_MAX_FRAME_SIZE);
  NODE_DEFINE_CONSTANT(constants, MAX_INITIAL_WINDOW_SIZE);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_DEFAULT_WEIGHT);

  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SETTINGS_HEADER_TABLE_SIZE);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SETTINGS_ENABLE_PUSH);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SETTINGS_MAX_FRAME_SIZE);
  NODE_DEFINE_CONSTANT(constants, NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE);

  NODE_DEFINE_CONSTANT(constants, PADDING_STRATEGY_NONE);
  NODE_DEFINE_CONSTANT(constants, PADDING_STRATEGY_MAX);
  NODE_DEFINE_CONSTANT(constants, PADDING_STRATEGY_CALLBACK);

#define STRING_CONSTANT(NAME, VALUE)                                          \
  NODE_DEFINE_STRING_CONSTANT(constants, "HTTP2_HEADER_" # NAME, VALUE);
HTTP_KNOWN_HEADERS(STRING_CONSTANT)
#undef STRING_CONSTANT

#define STRING_CONSTANT(NAME, VALUE)                                          \
  NODE_DEFINE_STRING_CONSTANT(constants, "HTTP2_METHOD_" # NAME, VALUE);
HTTP_KNOWN_METHODS(STRING_CONSTANT)
#undef STRING_CONSTANT

#define V(name, _) NODE_DEFINE_CONSTANT(constants, HTTP_STATUS_##name);
HTTP_STATUS_CODES(V)
#undef V

  env->SetMethod(target, "refreshDefaultSettings", RefreshDefaultSettings);
  env->SetMethod(target, "packSettings", PackSettings);

  target->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "constants"),
              constants).FromJust();
}
}  // namespace http2
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(http2, node::http2::Initialize)
