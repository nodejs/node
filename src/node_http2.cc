#include "aliased_buffer.h"
#include "node.h"
#include "node_buffer.h"
#include "node_http2.h"
#include "node_http2_state.h"

#include <queue>

namespace node {

using v8::Boolean;
using v8::Context;
using v8::Float64Array;
using v8::Function;
using v8::Integer;
using v8::String;
using v8::Uint32;
using v8::Uint32Array;
using v8::Undefined;

namespace http2 {

Freelist<Nghttp2Stream, FREELIST_MAX> stream_free_list;

Nghttp2Session::Callbacks Nghttp2Session::callback_struct_saved[2] = {
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
}

void Http2Session::OnFreeSession() {
  ::delete this;
}

ssize_t Http2Session::OnMaxFrameSizePadding(size_t frameLen,
                                            size_t maxPayloadLen) {
  DEBUG_HTTP2("Http2Session: using max frame size padding\n");
  return maxPayloadLen;
}

ssize_t Http2Session::OnCallbackPadding(size_t frameLen,
                                        size_t maxPayloadLen) {
  DEBUG_HTTP2("Http2Session: using callback padding\n");
  Isolate* isolate = env()->isolate();
  Local<Context> context = env()->context();

  HandleScope handle_scope(isolate);
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
  retval = retval <= maxPayloadLen ? retval : maxPayloadLen;
  retval = retval >= frameLen ? retval : frameLen;
#if defined(DEBUG) && DEBUG
  CHECK_GE(retval, frameLen);
  CHECK_LE(retval, maxPayloadLen);
#endif
  return retval;
}

void Http2Session::SetNextStreamID(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  nghttp2_session* s = session->session();
  int32_t id = args[0]->Int32Value(env->context()).ToChecked();
  DEBUG_HTTP2("Http2Session: setting next stream id to %d\n", id);
  nghttp2_session_set_next_stream_id(s, id);
}

void HttpErrorString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uint32_t val = args[0]->Uint32Value(env->context()).ToChecked();
  args.GetReturnValue().Set(
      OneByteString(env->isolate(), nghttp2_strerror(val)));
}

// Serializes the settings object into a Buffer instance that
// would be suitable, for instance, for creating the Base64
// output for an HTTP2-Settings header field.
void PackSettings(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HandleScope scope(env->isolate());

  std::vector<nghttp2_settings_entry> entries;
  entries.reserve(6);

  AliasedBuffer<uint32_t, v8::Uint32Array>& buffer =
      env->http2_state()->settings_buffer;
  uint32_t flags = buffer[IDX_SETTINGS_COUNT];

  if (flags & (1 << IDX_SETTINGS_HEADER_TABLE_SIZE)) {
    DEBUG_HTTP2("Setting header table size: %d\n",
                static_cast<uint32_t>(buffer[IDX_SETTINGS_HEADER_TABLE_SIZE]));
    entries.push_back({NGHTTP2_SETTINGS_HEADER_TABLE_SIZE,
                       buffer[IDX_SETTINGS_HEADER_TABLE_SIZE]});
  }

  if (flags & (1 << IDX_SETTINGS_MAX_CONCURRENT_STREAMS)) {
    DEBUG_HTTP2("Setting max concurrent streams: %d\n",
                static_cast<uint32_t>(
                    buffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS]));
    entries.push_back({NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
                       buffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS]});
  }

  if (flags & (1 << IDX_SETTINGS_MAX_FRAME_SIZE)) {
    DEBUG_HTTP2("Setting max frame size: %d\n",
                static_cast<uint32_t>(buffer[IDX_SETTINGS_MAX_FRAME_SIZE]));
    entries.push_back({NGHTTP2_SETTINGS_MAX_FRAME_SIZE,
                       buffer[IDX_SETTINGS_MAX_FRAME_SIZE]});
  }

  if (flags & (1 << IDX_SETTINGS_INITIAL_WINDOW_SIZE)) {
    DEBUG_HTTP2("Setting initial window size: %d\n",
                static_cast<uint32_t>(
                    buffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE]));
    entries.push_back({NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE,
                       buffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE]});
  }

  if (flags & (1 << IDX_SETTINGS_MAX_HEADER_LIST_SIZE)) {
    DEBUG_HTTP2("Setting max header list size: %d\n",
                static_cast<uint32_t>(
                    buffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE]));
    entries.push_back({NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE,
                       buffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE]});
  }

  if (flags & (1 << IDX_SETTINGS_ENABLE_PUSH)) {
    DEBUG_HTTP2("Setting enable push: %d\n",
                static_cast<uint32_t>(buffer[IDX_SETTINGS_ENABLE_PUSH]));
    entries.push_back({NGHTTP2_SETTINGS_ENABLE_PUSH,
                       buffer[IDX_SETTINGS_ENABLE_PUSH]});
  }

  const size_t len = entries.size() * 6;
  MaybeStackBuffer<char> buf(len);
  ssize_t ret =
      nghttp2_pack_settings_payload(
        reinterpret_cast<uint8_t*>(*buf), len, &entries[0], entries.size());
  if (ret >= 0) {
    args.GetReturnValue().Set(
      Buffer::Copy(env, *buf, len).ToLocalChecked());
  }
}

// Used to fill in the spec defined initial values for each setting.
void RefreshDefaultSettings(const FunctionCallbackInfo<Value>& args) {
  DEBUG_HTTP2("Http2Session: refreshing default settings\n");
  Environment* env = Environment::GetCurrent(args);
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
  buffer[IDX_SETTINGS_COUNT] =
    (1 << IDX_SETTINGS_HEADER_TABLE_SIZE) |
    (1 << IDX_SETTINGS_ENABLE_PUSH) |
    (1 << IDX_SETTINGS_INITIAL_WINDOW_SIZE) |
    (1 << IDX_SETTINGS_MAX_FRAME_SIZE);
}

template <get_setting fn>
void RefreshSettings(const FunctionCallbackInfo<Value>& args) {
  DEBUG_HTTP2("Http2Session: refreshing settings for session\n");
  Environment* env = Environment::GetCurrent(args);
#if defined(DEBUG) && DEBUG
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsObject());
#endif
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args[0].As<Object>());
  nghttp2_session* s = session->session();

  AliasedBuffer<uint32_t, v8::Uint32Array>& buffer =
      env->http2_state()->settings_buffer;
  buffer[IDX_SETTINGS_HEADER_TABLE_SIZE] =
      fn(s, NGHTTP2_SETTINGS_HEADER_TABLE_SIZE);
  buffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS] =
      fn(s, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
  buffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE] =
      fn(s, NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE);
  buffer[IDX_SETTINGS_MAX_FRAME_SIZE] =
      fn(s, NGHTTP2_SETTINGS_MAX_FRAME_SIZE);
  buffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE] =
      fn(s, NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE);
  buffer[IDX_SETTINGS_ENABLE_PUSH] =
      fn(s, NGHTTP2_SETTINGS_ENABLE_PUSH);
}

// Used to fill in the spec defined initial values for each setting.
void RefreshSessionState(const FunctionCallbackInfo<Value>& args) {
  DEBUG_HTTP2("Http2Session: refreshing session state\n");
  Environment* env = Environment::GetCurrent(args);
#if defined(DEBUG) && DEBUG
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsObject());
#endif
  AliasedBuffer<double, v8::Float64Array>& buffer =
      env->http2_state()->session_state_buffer;
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args[0].As<Object>());
  nghttp2_session* s = session->session();

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

void RefreshStreamState(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
#if defined(DEBUG) && DEBUG
  CHECK_EQ(args.Length(), 2);
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsNumber());
#endif
  int32_t id = args[1]->Int32Value(env->context()).ToChecked();
  DEBUG_HTTP2("Http2Session: refreshing stream %d state\n", id);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args[0].As<Object>());
  nghttp2_session* s = session->session();
  Nghttp2Stream* stream;

  AliasedBuffer<double, v8::Float64Array>& buffer =
      env->http2_state()->stream_state_buffer;

  if ((stream = session->FindStream(id)) == nullptr) {
    buffer[IDX_STREAM_STATE] = NGHTTP2_STREAM_STATE_IDLE;
    buffer[IDX_STREAM_STATE_WEIGHT] =
        buffer[IDX_STREAM_STATE_SUM_DEPENDENCY_WEIGHT] =
        buffer[IDX_STREAM_STATE_LOCAL_CLOSE] =
        buffer[IDX_STREAM_STATE_REMOTE_CLOSE] =
        buffer[IDX_STREAM_STATE_LOCAL_WINDOW_SIZE] = 0;
    return;
  }
  nghttp2_stream* str =
      nghttp2_session_find_stream(s, stream->id());

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
        nghttp2_session_get_stream_local_close(s, id);
    buffer[IDX_STREAM_STATE_REMOTE_CLOSE] =
        nghttp2_session_get_stream_remote_close(s, id);
    buffer[IDX_STREAM_STATE_LOCAL_WINDOW_SIZE] =
        nghttp2_session_get_stream_local_window_size(s, id);
  }
}

void Http2Session::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
#if defined(DEBUG) && DEBUG
  CHECK(args.IsConstructCall());
#endif
  int val = args[0]->IntegerValue(env->context()).ToChecked();
  nghttp2_session_type type = static_cast<nghttp2_session_type>(val);
  DEBUG_HTTP2("Http2Session: creating a session of type: %d\n", type);
  new Http2Session(env, args.This(), type);
}


// Capture the stream that this session will use to send and receive data
void Http2Session::Consume(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsExternal());
#endif
  session->Consume(args[0].As<External>());
}

void Http2Session::Destroy(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  DEBUG_HTTP2("Http2Session: destroying session %d\n", session->type());
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();

  bool skipUnconsume = args[0]->BooleanValue(context).ToChecked();

  if (!skipUnconsume)
    session->Unconsume();
  session->Free();
}

void Http2Session::Destroying(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  DEBUG_HTTP2("Http2Session: preparing to destroy session %d\n",
              session->type());
  session->MarkDestroying();
}

void Http2Session::SubmitPriority(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Local<Context> context = env->context();

  nghttp2_priority_spec spec;
  int32_t id = args[0]->Int32Value(context).ToChecked();
  int32_t parent = args[1]->Int32Value(context).ToChecked();
  int32_t weight = args[2]->Int32Value(context).ToChecked();
  bool exclusive = args[3]->BooleanValue(context).ToChecked();
  bool silent = args[4]->BooleanValue(context).ToChecked();
  DEBUG_HTTP2("Http2Session: submitting priority for stream %d: "
              "parent: %d, weight: %d, exclusive: %d, silent: %d\n",
              id, parent, weight, exclusive, silent);

#if defined(DEBUG) && DEBUG
  CHECK_GT(id, 0);
  CHECK_GE(parent, 0);
  CHECK_GE(weight, 0);
#endif

  Nghttp2Stream* stream;
  if (!(stream = session->FindStream(id))) {
    // invalid stream
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }
  nghttp2_priority_spec_init(&spec, parent, weight, exclusive ? 1 : 0);

  args.GetReturnValue().Set(stream->SubmitPriority(&spec, silent));
}

void Http2Session::SubmitSettings(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();

  AliasedBuffer<uint32_t, v8::Uint32Array>& buffer =
      env->http2_state()->settings_buffer;
  uint32_t flags = buffer[IDX_SETTINGS_COUNT];

  std::vector<nghttp2_settings_entry> entries;
  entries.reserve(6);

  if (flags & (1 << IDX_SETTINGS_HEADER_TABLE_SIZE)) {
    DEBUG_HTTP2("Setting header table size: %d\n",
                static_cast<uint32_t>(buffer[IDX_SETTINGS_HEADER_TABLE_SIZE]));
    entries.push_back({NGHTTP2_SETTINGS_HEADER_TABLE_SIZE,
                       buffer[IDX_SETTINGS_HEADER_TABLE_SIZE]});
  }

  if (flags & (1 << IDX_SETTINGS_MAX_CONCURRENT_STREAMS)) {
    DEBUG_HTTP2("Setting max concurrent streams: %d\n",
                static_cast<uint32_t>(
                    buffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS]));
    entries.push_back({NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
                       buffer[IDX_SETTINGS_MAX_CONCURRENT_STREAMS]});
  }

  if (flags & (1 << IDX_SETTINGS_MAX_FRAME_SIZE)) {
    DEBUG_HTTP2("Setting max frame size: %d\n",
                static_cast<uint32_t>(buffer[IDX_SETTINGS_MAX_FRAME_SIZE]));
    entries.push_back({NGHTTP2_SETTINGS_MAX_FRAME_SIZE,
                       buffer[IDX_SETTINGS_MAX_FRAME_SIZE]});
  }

  if (flags & (1 << IDX_SETTINGS_INITIAL_WINDOW_SIZE)) {
    DEBUG_HTTP2("Setting initial window size: %d\n",
                static_cast<uint32_t>(
                    buffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE]));
    entries.push_back({NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE,
                       buffer[IDX_SETTINGS_INITIAL_WINDOW_SIZE]});
  }

  if (flags & (1 << IDX_SETTINGS_MAX_HEADER_LIST_SIZE)) {
    DEBUG_HTTP2("Setting max header list size: %d\n",
                static_cast<uint32_t>(
                    buffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE]));
    entries.push_back({NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE,
                       buffer[IDX_SETTINGS_MAX_HEADER_LIST_SIZE]});
  }

  if (flags & (1 << IDX_SETTINGS_ENABLE_PUSH)) {
    DEBUG_HTTP2("Setting enable push: %d\n",
                static_cast<uint32_t>(buffer[IDX_SETTINGS_ENABLE_PUSH]));
    entries.push_back({NGHTTP2_SETTINGS_ENABLE_PUSH,
                       buffer[IDX_SETTINGS_ENABLE_PUSH]});
  }

  if (entries.size() > 0) {
    args.GetReturnValue().Set(
        session->Nghttp2Session::SubmitSettings(&entries[0], entries.size()));
  } else {
    args.GetReturnValue().Set(
        session->Nghttp2Session::SubmitSettings(nullptr, 0));
  }
}

void Http2Session::SubmitRstStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();

#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsNumber());
  CHECK(args[1]->IsNumber());
#endif

  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());

  int32_t id = args[0]->Int32Value(context).ToChecked();
  uint32_t code = args[1]->Uint32Value(context).ToChecked();

  Nghttp2Stream* stream;
  if (!(stream = session->FindStream(id))) {
    // invalid stream
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }
  DEBUG_HTTP2("Http2Session: sending rst_stream for stream %d, code: %d\n",
              id, code);
  args.GetReturnValue().Set(stream->SubmitRstStream(code));
}

void Http2Session::SubmitRequest(const FunctionCallbackInfo<Value>& args) {
  // args[0] Array of headers
  // args[1] options int
  // args[2] parentStream ID (for priority spec)
  // args[3] weight (for priority spec)
  // args[4] exclusive boolean (for priority spec)
#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsArray());
#endif

  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();

  Local<Array> headers = args[0].As<Array>();
  int options = args[1]->IntegerValue(context).ToChecked();
  int32_t parent = args[2]->Int32Value(context).ToChecked();
  int32_t weight = args[3]->Int32Value(context).ToChecked();
  bool exclusive = args[4]->BooleanValue(context).ToChecked();

  DEBUG_HTTP2("Http2Session: submitting request: headers: %d, options: %d, "
              "parent: %d, weight: %d, exclusive: %d\n", headers->Length(),
              options, parent, weight, exclusive);

  nghttp2_priority_spec prispec;
  nghttp2_priority_spec_init(&prispec, parent, weight, exclusive ? 1 : 0);

  Headers list(isolate, context, headers);

  int32_t ret = session->Nghttp2Session::SubmitRequest(&prispec,
                                                       *list, list.length(),
                                                       nullptr, options);
  DEBUG_HTTP2("Http2Session: request submitted, response: %d\n", ret);
  args.GetReturnValue().Set(ret);
}

void Http2Session::SubmitResponse(const FunctionCallbackInfo<Value>& args) {
#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsNumber());
  CHECK(args[1]->IsArray());
#endif

  Http2Session* session;
  Nghttp2Stream* stream;

  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();

  int32_t id = args[0]->Int32Value(context).ToChecked();
  Local<Array> headers = args[1].As<Array>();
  int options = args[2]->IntegerValue(context).ToChecked();

  DEBUG_HTTP2("Http2Session: submitting response for stream %d: headers: %d, "
              "options: %d\n", id, headers->Length(), options);

  if (!(stream = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }

  Headers list(isolate, context, headers);

  args.GetReturnValue().Set(
      stream->SubmitResponse(*list, list.length(), options));
}

void Http2Session::SubmitFile(const FunctionCallbackInfo<Value>& args) {
#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsNumber());  // Stream ID
  CHECK(args[1]->IsNumber());  // File Descriptor
  CHECK(args[2]->IsArray());   // Headers
  CHECK(args[3]->IsNumber());  // Offset
  CHECK(args[4]->IsNumber());  // Length
#endif

  Http2Session* session;
  Nghttp2Stream* stream;

  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();

  int32_t id = args[0]->Int32Value(context).ToChecked();
  int fd = args[1]->Int32Value(context).ToChecked();
  Local<Array> headers = args[2].As<Array>();

  int64_t offset = args[3]->IntegerValue(context).ToChecked();
  int64_t length = args[4]->IntegerValue(context).ToChecked();
  int options = args[5]->IntegerValue(context).ToChecked();

#if defined(DEBUG) && DEBUG
  CHECK_GE(offset, 0);
#endif

  DEBUG_HTTP2("Http2Session: submitting file %d for stream %d: headers: %d, "
              "end-stream: %d\n", fd, id, headers->Length());

  if (!(stream = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }

  session->chunks_sent_since_last_write_ = 0;

  Headers list(isolate, context, headers);

  args.GetReturnValue().Set(stream->SubmitFile(fd, *list, list.length(),
                                               offset, length, options));
}

void Http2Session::SendHeaders(const FunctionCallbackInfo<Value>& args) {
#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsNumber());
  CHECK(args[1]->IsArray());
#endif

  Http2Session* session;
  Nghttp2Stream* stream;

  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Environment* env = session->env();
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();

  int32_t id = args[0]->Int32Value(env->context()).ToChecked();
  Local<Array> headers = args[1].As<Array>();

  DEBUG_HTTP2("Http2Session: sending informational headers for stream %d, "
              "count: %d\n", id, headers->Length());

  if (!(stream = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }

  Headers list(isolate, context, headers);

  args.GetReturnValue().Set(stream->SubmitInfo(*list, list.length()));
}

void Http2Session::ShutdownStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsNumber());
#endif
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Nghttp2Stream* stream;
  int32_t id = args[0]->Int32Value(env->context()).ToChecked();
  DEBUG_HTTP2("Http2Session: shutting down stream %d\n", id);
  if (!(stream = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }
  stream->Shutdown();
}

void Http2Session::StreamReadStart(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsNumber());
#endif
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Nghttp2Stream* stream;
  int32_t id = args[0]->Int32Value(env->context()).ToChecked();
  if (!(stream = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }
  stream->ReadStart();
}


void Http2Session::StreamReadStop(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsNumber());
#endif
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  Nghttp2Stream* stream;
  int32_t id = args[0]->Int32Value(env->context()).ToChecked();
  if (!(stream = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }
  stream->ReadStop();
}

void Http2Session::SendShutdownNotice(
    const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
  session->SubmitShutdownNotice();
}

void Http2Session::SubmitGoaway(const FunctionCallbackInfo<Value>& args) {
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

  DEBUG_HTTP2("Http2Session: initiating immediate shutdown. "
              "last-stream-id: %d, code: %d, opaque-data: %d\n",
              lastStreamID, errorCode, length);
  int status = nghttp2_submit_goaway(session->session(),
                                     NGHTTP2_FLAG_NONE,
                                     lastStreamID,
                                     errorCode,
                                     data, length);
  args.GetReturnValue().Set(status);
}

void Http2Session::DestroyStream(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
#if defined(DEBUG) && DEBUG
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsNumber());
#endif
  int32_t id = args[0]->Int32Value(env->context()).ToChecked();
  DEBUG_HTTP2("Http2Session: destroy stream %d\n", id);
  Nghttp2Stream* stream;
  if (!(stream = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }
  stream->Destroy();
}

void Http2Session::FlushData(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Http2Session* session;
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());
#if defined(DEBUG) && DEBUG
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsNumber());
#endif
  int32_t id = args[0]->Int32Value(env->context()).ToChecked();
  DEBUG_HTTP2("Http2Session: flushing data to js for stream %d\n", id);
  Nghttp2Stream* stream;
  if (!(stream = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }
  stream->ReadResume();
}

void Http2Session::UpdateChunksSent(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());

  HandleScope scope(isolate);

  uint32_t length = session->chunks_sent_since_last_write_;

  session->object()->Set(env->context(),
                         env->chunks_sent_since_last_write_string(),
                         Integer::NewFromUnsigned(isolate, length)).FromJust();

  args.GetReturnValue().Set(length);
}

void Http2Session::SubmitPushPromise(const FunctionCallbackInfo<Value>& args) {
  Http2Session* session;
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Isolate* isolate = env->isolate();
  ASSIGN_OR_RETURN_UNWRAP(&session, args.Holder());

#if defined(DEBUG) && DEBUG
  CHECK(args[0]->IsNumber());  // parent stream ID
  CHECK(args[1]->IsArray());  // headers array
#endif

  Nghttp2Stream* parent;
  int32_t id = args[0]->Int32Value(context).ToChecked();
  Local<Array> headers = args[1].As<Array>();
  int options = args[2]->IntegerValue(context).ToChecked();

  DEBUG_HTTP2("Http2Session: submitting push promise for stream %d: "
              "options: %d, headers: %d\n", id, options,
              headers->Length());

  if (!(parent = session->FindStream(id))) {
    return args.GetReturnValue().Set(NGHTTP2_ERR_INVALID_STREAM_ID);
  }

  Headers list(isolate, context, headers);

  int32_t ret = parent->SubmitPushPromise(*list, list.length(),
                                          nullptr, options);
  DEBUG_HTTP2("Http2Session: push promise submitted, ret: %d\n", ret);
  args.GetReturnValue().Set(ret);
}

int Http2Session::DoWrite(WriteWrap* req_wrap,
                          uv_buf_t* bufs,
                          size_t count,
                          uv_stream_t* send_handle) {
  Environment* env = req_wrap->env();
  Local<Object> req_wrap_obj = req_wrap->object();
  Local<Context> context = env->context();

  Nghttp2Stream* stream;
  {
    Local<Value> val =
        req_wrap_obj->Get(context, env->stream_string()).ToLocalChecked();
    int32_t id = val->Int32Value(context).ToChecked();
    if (!val->IsNumber() || !(stream = FindStream(id))) {
      // invalid stream
      req_wrap->Dispatched();
      req_wrap->Done(0);
      return NGHTTP2_ERR_INVALID_STREAM_ID;
    }
  }

  chunks_sent_since_last_write_ = 0;

  nghttp2_stream_write_t* req = new nghttp2_stream_write_t;
  req->data = req_wrap;

  auto AfterWrite = [](nghttp2_stream_write_t* req, int status) {
    WriteWrap* wrap = static_cast<WriteWrap*>(req->data);
    wrap->Done(status);
    delete req;
  };
  req_wrap->Dispatched();
  stream->Write(req, bufs, count, AfterWrite);
  return 0;
}

void Http2Session::AllocateSend(uv_buf_t* buf) {
  buf->base = stream_alloc();
  buf->len = kAllocBufferSize;
}

void Http2Session::Send(uv_buf_t* buf, size_t length) {
  DEBUG_HTTP2("Http2Session: Attempting to send data\n");
  if (stream_ == nullptr || !stream_->IsAlive() || stream_->IsClosing()) {
    return;
  }
  HandleScope scope(env()->isolate());
  auto AfterWrite = [](WriteWrap* req_wrap, int status) {
    req_wrap->Dispose();
  };
  Local<Object> req_wrap_obj =
      env()->write_wrap_constructor_function()
          ->NewInstance(env()->context()).ToLocalChecked();
  WriteWrap* write_req = WriteWrap::New(env(),
                                        req_wrap_obj,
                                        this,
                                        AfterWrite);

  chunks_sent_since_last_write_++;
  uv_buf_t actual = uv_buf_init(buf->base, length);
  if (stream_->DoWrite(write_req, &actual, 1, nullptr)) {
    write_req->Dispose();
  }
}

void Http2Session::OnTrailers(Nghttp2Stream* stream,
                              const SubmitTrailers& submit_trailers) {
  DEBUG_HTTP2("Http2Session: prompting for trailers on stream %d\n",
              stream->id());
  Local<Context> context = env()->context();
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Context::Scope context_scope(context);

  Local<Value> argv[1] = {
    Integer::New(isolate, stream->id())
  };

  Local<Value> ret = MakeCallback(env()->ontrailers_string(),
                                  arraysize(argv), argv).ToLocalChecked();
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

void Http2Session::OnHeaders(
    Nghttp2Stream* stream,
    std::queue<nghttp2_header>* headers,
    nghttp2_headers_category cat,
    uint8_t flags) {
  Local<Context> context = env()->context();
  Isolate* isolate = env()->isolate();
  Context::Scope context_scope(context);
  HandleScope scope(isolate);
  Local<String> name_str;
  Local<String> value_str;

  Local<Array> holder = Array::New(isolate);
  Local<Function> fn = env()->push_values_to_array_function();
  Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX * 2];

#if defined(DEBUG) && DEBUG
  CHECK_LE(cat, NGHTTP2_HCAT_HEADERS);
#endif

  // The headers are passed in above as a queue of nghttp2_header structs.
  // The following converts that into a JS array with the structure:
  // [name1, value1, name2, value2, name3, value3, name3, value4] and so on.
  // That array is passed up to the JS layer and converted into an Object form
  // like {name1: value1, name2: value2, name3: [value3, value4]}. We do it
  // this way for performance reasons (it's faster to generate and pass an
  // array than it is to generate and pass the object).
  do {
    size_t j = 0;
    while (!headers->empty() && j < arraysize(argv) / 2) {
      nghttp2_header item = headers->front();
      // The header name and value are passed as external one-byte strings
      name_str =
          ExternalHeader::New<true>(env(), item.name).ToLocalChecked();
      value_str =
          ExternalHeader::New<false>(env(), item.value).ToLocalChecked();
      argv[j * 2] = name_str;
      argv[j * 2 + 1] = value_str;
      headers->pop();
      j++;
    }
    // For performance, we pass name and value pairs to array.protototype.push
    // in batches of size NODE_PUSH_VAL_TO_ARRAY_MAX * 2 until there are no
    // more items to push.
    if (j > 0) {
      fn->Call(env()->context(), holder, j * 2, argv).ToLocalChecked();
    }
  } while (!headers->empty());

  Local<Value> args[4] = {
    Integer::New(isolate, stream->id()),
    Integer::New(isolate, cat),
    Integer::New(isolate, flags),
    holder
  };
  MakeCallback(env()->onheaders_string(), arraysize(args), args);
}


void Http2Session::OnStreamClose(int32_t id, uint32_t code) {
  Isolate* isolate = env()->isolate();
  Local<Context> context = env()->context();
  HandleScope scope(isolate);
  Context::Scope context_scope(context);

  Local<Value> argv[2] = {
    Integer::New(isolate, id),
    Integer::NewFromUnsigned(isolate, code)
  };
  MakeCallback(env()->onstreamclose_string(), arraysize(argv), argv);
}

void Http2Session::OnDataChunk(
    Nghttp2Stream* stream,
    uv_buf_t* chunk) {
  Isolate* isolate = env()->isolate();
  Local<Context> context = env()->context();
  HandleScope scope(isolate);
  Local<Object> obj = Object::New(isolate);
  obj->Set(context,
           env()->id_string(),
           Integer::New(isolate, stream->id())).FromJust();
  ssize_t len = -1;
  Local<Object> buf;
  if (chunk != nullptr) {
    len = chunk->len;
    buf = Buffer::New(isolate, chunk->base, len).ToLocalChecked();
  }
  EmitData(len, buf, obj);
}

void Http2Session::OnSettings(bool ack) {
  Local<Context> context = env()->context();
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Context::Scope context_scope(context);

  Local<Value> argv[1] = { Boolean::New(isolate, ack) };
  MakeCallback(env()->onsettings_string(), arraysize(argv), argv);
}

void Http2Session::OnFrameError(int32_t id, uint8_t type, int error_code) {
  Local<Context> context = env()->context();
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Context::Scope context_scope(context);

  Local<Value> argv[3] = {
    Integer::New(isolate, id),
    Integer::New(isolate, type),
    Integer::New(isolate, error_code)
  };
  MakeCallback(env()->onframeerror_string(), arraysize(argv), argv);
}

void Http2Session::OnPriority(int32_t stream,
                              int32_t parent,
                              int32_t weight,
                              int8_t exclusive) {
  Local<Context> context = env()->context();
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Context::Scope context_scope(context);

  Local<Value> argv[4] = {
    Integer::New(isolate, stream),
    Integer::New(isolate, parent),
    Integer::New(isolate, weight),
    Boolean::New(isolate, exclusive)
  };
  MakeCallback(env()->onpriority_string(), arraysize(argv), argv);
}

void Http2Session::OnGoAway(int32_t lastStreamID,
                            uint32_t errorCode,
                            uint8_t* data,
                            size_t length) {
  Local<Context> context = env()->context();
  Isolate* isolate = env()->isolate();
  HandleScope scope(isolate);
  Context::Scope context_scope(context);

  Local<Value> argv[3] = {
    Integer::NewFromUnsigned(isolate, errorCode),
    Integer::New(isolate, lastStreamID),
    Undefined(isolate)
  };

  if (length > 0) {
    argv[2] = Buffer::Copy(isolate,
                           reinterpret_cast<char*>(data),
                           length).ToLocalChecked();
  }

  MakeCallback(env()->ongoawaydata_string(), arraysize(argv), argv);
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
      DEBUG_HTTP2("Http2Session: fatal error receiving data: %d\n", ret);
      nghttp2_session_terminate_session(session->session(),
                                        NGHTTP2_PROTOCOL_ERROR);
    }
  }
}


void Http2Session::Consume(Local<External> external) {
  DEBUG_HTTP2("Http2Session: consuming socket\n");
#if defined(DEBUG) && DEBUG
  CHECK(prev_alloc_cb_.is_empty());
#endif
  StreamBase* stream = static_cast<StreamBase*>(external->Value());
#if defined(DEBUG) && DEBUG
  CHECK_NE(stream, nullptr);
#endif
  stream->Consume();
  stream_ = stream;
  prev_alloc_cb_ = stream->alloc_cb();
  prev_read_cb_ = stream->read_cb();
  stream->set_alloc_cb({ Http2Session::OnStreamAllocImpl, this });
  stream->set_read_cb({ Http2Session::OnStreamReadImpl, this });
}


void Http2Session::Unconsume() {
  DEBUG_HTTP2("Http2Session: unconsuming socket\n");
  if (prev_alloc_cb_.is_empty())
    return;
  stream_->set_alloc_cb(prev_alloc_cb_);
  stream_->set_read_cb(prev_read_cb_);
  prev_alloc_cb_.clear();
  prev_read_cb_.clear();
  stream_ = nullptr;
}


Headers::Headers(Isolate* isolate,
                 Local<Context> context,
                 Local<Array> headers) {
#if defined(DEBUG) && DEBUG
  CHECK_EQ(headers->Length(), 2);
#endif
  Local<Value> header_string = headers->Get(context, 0).ToLocalChecked();
  Local<Value> header_count = headers->Get(context, 1).ToLocalChecked();
#if defined(DEBUG) && DEBUG
  CHECK(header_string->IsString());
  CHECK(header_count->IsUint32());
#endif
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

#if defined(DEBUG) && DEBUG
  CHECK_EQ(p, header_contents + header_string_len);
  CHECK_EQ(n, count_);
#endif
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);

  http2_state* state = new http2_state(isolate);
  env->set_http2_state(state);

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

  NODE_DEFINE_CONSTANT(target, PADDING_BUF_FRAME_LENGTH);
  NODE_DEFINE_CONSTANT(target, PADDING_BUF_MAX_PAYLOAD_LENGTH);
  NODE_DEFINE_CONSTANT(target, PADDING_BUF_RETURN_VALUE);

  // Method to fetch the nghttp2 string description of an nghttp2 error code
  env->SetMethod(target, "nghttp2ErrorString", HttpErrorString);

  Local<String> http2SessionClassName =
    FIXED_ONE_BYTE_STRING(isolate, "Http2Session");

  Local<FunctionTemplate> session =
      env->NewFunctionTemplate(Http2Session::New);
  session->SetClassName(http2SessionClassName);
  session->InstanceTemplate()->SetInternalFieldCount(1);
  AsyncWrap::AddWrapMethods(env, session);
  env->SetProtoMethod(session, "consume",
                      Http2Session::Consume);
  env->SetProtoMethod(session, "destroy",
                      Http2Session::Destroy);
  env->SetProtoMethod(session, "destroying",
                      Http2Session::Destroying);
  env->SetProtoMethod(session, "sendHeaders",
                      Http2Session::SendHeaders);
  env->SetProtoMethod(session, "submitShutdownNotice",
                      Http2Session::SendShutdownNotice);
  env->SetProtoMethod(session, "submitGoaway",
                      Http2Session::SubmitGoaway);
  env->SetProtoMethod(session, "submitSettings",
                      Http2Session::SubmitSettings);
  env->SetProtoMethod(session, "submitPushPromise",
                      Http2Session::SubmitPushPromise);
  env->SetProtoMethod(session, "submitRstStream",
                      Http2Session::SubmitRstStream);
  env->SetProtoMethod(session, "submitResponse",
                      Http2Session::SubmitResponse);
  env->SetProtoMethod(session, "submitFile",
                      Http2Session::SubmitFile);
  env->SetProtoMethod(session, "submitRequest",
                      Http2Session::SubmitRequest);
  env->SetProtoMethod(session, "submitPriority",
                      Http2Session::SubmitPriority);
  env->SetProtoMethod(session, "shutdownStream",
                      Http2Session::ShutdownStream);
  env->SetProtoMethod(session, "streamReadStart",
                      Http2Session::StreamReadStart);
  env->SetProtoMethod(session, "streamReadStop",
                      Http2Session::StreamReadStop);
  env->SetProtoMethod(session, "setNextStreamID",
                      Http2Session::SetNextStreamID);
  env->SetProtoMethod(session, "destroyStream",
                      Http2Session::DestroyStream);
  env->SetProtoMethod(session, "flushData",
                      Http2Session::FlushData);
  env->SetProtoMethod(session, "updateChunksSent",
                      Http2Session::UpdateChunksSent);
  StreamBase::AddMethods<Http2Session>(env, session,
                                        StreamBase::kFlagHasWritev |
                                        StreamBase::kFlagNoShutdown);
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
  NODE_DEFINE_HIDDEN_CONSTANT(constants, NGHTTP2_ERR_NOMEM);
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

  env->SetMethod(target, "refreshLocalSettings",
                 RefreshSettings<nghttp2_session_get_local_settings>);
  env->SetMethod(target, "refreshRemoteSettings",
                 RefreshSettings<nghttp2_session_get_remote_settings>);
  env->SetMethod(target, "refreshDefaultSettings", RefreshDefaultSettings);
  env->SetMethod(target, "refreshSessionState", RefreshSessionState);
  env->SetMethod(target, "refreshStreamState", RefreshStreamState);
  env->SetMethod(target, "packSettings", PackSettings);

  target->Set(context,
              FIXED_ONE_BYTE_STRING(isolate, "constants"),
              constants).FromJust();
}
}  // namespace http2
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(http2, node::http2::Initialize)
