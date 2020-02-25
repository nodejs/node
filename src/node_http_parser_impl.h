// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// This file is included from 2 files, node_http_parser_traditional.cc
// and node_http_parser_llhttp.cc.

#pragma once

#include "node.h"
#include "node_buffer.h"
#include "util.h"

#include "async_wrap-inl.h"
#include "env-inl.h"
#include "stream_base-inl.h"
#include "v8.h"

#include "http_parser_adaptor.h"

#include <cstdlib>  // free()
#include <cstring>  // strdup(), strchr()


// This is a binding to http_parser (https://github.com/nodejs/http-parser)
// The goal is to decouple sockets from parsing for more javascript-level
// agility. A Buffer is read from a socket and passed to parser.execute().
// The parser then issues callbacks with slices of the data
//     parser.onMessageBegin
//     parser.onPath
//     parser.onBody
//     ...
// No copying is performed when slicing the buffer, only small reference
// allocations.


namespace node {
namespace {  // NOLINT(build/namespaces)

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

const uint32_t kOnHeaders = 0;
const uint32_t kOnHeadersComplete = 1;
const uint32_t kOnBody = 2;
const uint32_t kOnMessageComplete = 3;
const uint32_t kOnExecute = 4;
// Any more fields than this will be flushed into JS
const size_t kMaxHeaderFieldsCount = 32;

inline bool IsOWS(char c) {
  return c == ' ' || c == '\t';
}

// helper class for the Parser
struct StringPtr {
  StringPtr() {
    on_heap_ = false;
    Reset();
  }


  ~StringPtr() {
    Reset();
  }


  // If str_ does not point to a heap string yet, this function makes it do
  // so. This is called at the end of each http_parser_execute() so as not
  // to leak references. See issue #2438 and test-http-parser-bad-ref.js.
  void Save() {
    if (!on_heap_ && size_ > 0) {
      char* s = new char[size_];
      memcpy(s, str_, size_);
      str_ = s;
      on_heap_ = true;
    }
  }


  void Reset() {
    if (on_heap_) {
      delete[] str_;
      on_heap_ = false;
    }

    str_ = nullptr;
    size_ = 0;
  }


  void Update(const char* str, size_t size) {
    if (str_ == nullptr) {
      str_ = str;
    } else if (on_heap_ || str_ + size_ != str) {
      // Non-consecutive input, make a copy on the heap.
      // TODO(bnoordhuis) Use slab allocation, O(n) allocs is bad.
      char* s = new char[size_ + size];
      memcpy(s, str_, size_);
      memcpy(s + size_, str, size);

      if (on_heap_)
        delete[] str_;
      else
        on_heap_ = true;

      str_ = s;
    }
    size_ += size;
  }


  Local<String> ToString(Environment* env) const {
    if (size_ != 0)
      return OneByteString(env->isolate(), str_, size_);
    else
      return String::Empty(env->isolate());
  }


  // Strip trailing OWS (SPC or HTAB) from string.
  Local<String> ToTrimmedString(Environment* env) {
    while (size_ > 0 && IsOWS(str_[size_ - 1])) {
      size_--;
    }
    return ToString(env);
  }


  const char* str_;
  bool on_heap_;
  size_t size_;
};

class Parser : public AsyncWrap, public StreamListener {
 public:
  Parser(Environment* env, Local<Object> wrap)
      : AsyncWrap(env, wrap),
        current_buffer_len_(0),
        current_buffer_data_(nullptr) {
  }


  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("current_buffer", current_buffer_);
  }

  SET_MEMORY_INFO_NAME(Parser)
  SET_SELF_SIZE(Parser)

  int on_message_begin() {
    num_fields_ = num_values_ = 0;
    url_.Reset();
    status_message_.Reset();
    return 0;
  }


  int on_url(const char* at, size_t length) {
    int rv = TrackHeader(length);
    if (rv != 0) {
      return rv;
    }

    url_.Update(at, length);
    return 0;
  }


  int on_status(const char* at, size_t length) {
    int rv = TrackHeader(length);
    if (rv != 0) {
      return rv;
    }

    status_message_.Update(at, length);
    return 0;
  }


  int on_header_field(const char* at, size_t length) {
    int rv = TrackHeader(length);
    if (rv != 0) {
      return rv;
    }

    if (num_fields_ == num_values_) {
      // start of new field name
      num_fields_++;
      if (num_fields_ == kMaxHeaderFieldsCount) {
        // ran out of space - flush to javascript land
        Flush();
        num_fields_ = 1;
        num_values_ = 0;
      }
      fields_[num_fields_ - 1].Reset();
    }

    CHECK_LT(num_fields_, kMaxHeaderFieldsCount);
    CHECK_EQ(num_fields_, num_values_ + 1);

    fields_[num_fields_ - 1].Update(at, length);

    return 0;
  }


  int on_header_value(const char* at, size_t length) {
    int rv = TrackHeader(length);
    if (rv != 0) {
      return rv;
    }

    if (num_values_ != num_fields_) {
      // start of new header value
      num_values_++;
      values_[num_values_ - 1].Reset();
    }

    CHECK_LT(num_values_, arraysize(values_));
    CHECK_EQ(num_values_, num_fields_);

    values_[num_values_ - 1].Update(at, length);

    return 0;
  }


  int on_headers_complete() {
#ifdef NODE_EXPERIMENTAL_HTTP
    header_nread_ = 0;
#endif  /* NODE_EXPERIMENTAL_HTTP */

    // Arguments for the on-headers-complete javascript callback. This
    // list needs to be kept in sync with the actual argument list for
    // `parserOnHeadersComplete` in lib/_http_common.js.
    enum on_headers_complete_arg_index {
      A_VERSION_MAJOR = 0,
      A_VERSION_MINOR,
      A_HEADERS,
      A_METHOD,
      A_URL,
      A_STATUS_CODE,
      A_STATUS_MESSAGE,
      A_UPGRADE,
      A_SHOULD_KEEP_ALIVE,
      A_MAX
    };

    Local<Value> argv[A_MAX];
    Local<Object> obj = object();
    Local<Value> cb = obj->Get(env()->context(),
                               kOnHeadersComplete).ToLocalChecked();

    if (!cb->IsFunction())
      return 0;

    Local<Value> undefined = Undefined(env()->isolate());
    for (size_t i = 0; i < arraysize(argv); i++)
      argv[i] = undefined;

    if (have_flushed_) {
      // Slow case, flush remaining headers.
      Flush();
    } else {
      // Fast case, pass headers and URL to JS land.
      argv[A_HEADERS] = CreateHeaders();
      if (parser_.type == HTTP_REQUEST)
        argv[A_URL] = url_.ToString(env());
    }

    num_fields_ = 0;
    num_values_ = 0;

    // METHOD
    if (parser_.type == HTTP_REQUEST) {
      argv[A_METHOD] =
          Uint32::NewFromUnsigned(env()->isolate(), parser_.method);
    }

    // STATUS
    if (parser_.type == HTTP_RESPONSE) {
      argv[A_STATUS_CODE] =
          Integer::New(env()->isolate(), parser_.status_code);
      argv[A_STATUS_MESSAGE] = status_message_.ToString(env());
    }

    // VERSION
    argv[A_VERSION_MAJOR] = Integer::New(env()->isolate(), parser_.http_major);
    argv[A_VERSION_MINOR] = Integer::New(env()->isolate(), parser_.http_minor);

    bool should_keep_alive;
#ifdef NODE_EXPERIMENTAL_HTTP
    should_keep_alive = llhttp_should_keep_alive(&parser_);
#else  /* !NODE_EXPERIMENTAL_HTTP */
    should_keep_alive = http_should_keep_alive(&parser_);
#endif  /* NODE_EXPERIMENTAL_HTTP */

    argv[A_SHOULD_KEEP_ALIVE] =
        Boolean::New(env()->isolate(), should_keep_alive);

    argv[A_UPGRADE] = Boolean::New(env()->isolate(), parser_.upgrade);

    MaybeLocal<Value> head_response;
    {
      InternalCallbackScope callback_scope(
          this, InternalCallbackScope::kSkipTaskQueues);
      head_response = cb.As<Function>()->Call(
          env()->context(), object(), arraysize(argv), argv);
      if (head_response.IsEmpty()) callback_scope.MarkAsFailed();
    }

    int64_t val;

    if (head_response.IsEmpty() || !head_response.ToLocalChecked()
                                        ->IntegerValue(env()->context())
                                        .To(&val)) {
      got_exception_ = true;
      return -1;
    }

    return val;
  }


  int on_body(const char* at, size_t length) {
    EscapableHandleScope scope(env()->isolate());

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(env()->context(), kOnBody).ToLocalChecked();

    if (!cb->IsFunction())
      return 0;

    // We came from consumed stream
    if (current_buffer_.IsEmpty()) {
      // Make sure Buffer will be in parent HandleScope
      current_buffer_ = scope.Escape(Buffer::Copy(
          env()->isolate(),
          current_buffer_data_,
          current_buffer_len_).ToLocalChecked());
    }

    Local<Value> argv[3] = {
      current_buffer_,
      Integer::NewFromUnsigned(env()->isolate(), at - current_buffer_data_),
      Integer::NewFromUnsigned(env()->isolate(), length)
    };

    MaybeLocal<Value> r = MakeCallback(cb.As<Function>(),
                                       arraysize(argv),
                                       argv);

    if (r.IsEmpty()) {
      got_exception_ = true;
#ifdef NODE_EXPERIMENTAL_HTTP
      llhttp_set_error_reason(&parser_, "HPE_JS_EXCEPTION:JS Exception");
#endif  /* NODE_EXPERIMENTAL_HTTP */
      return HPE_USER;
    }

    return 0;
  }


  int on_message_complete() {
    HandleScope scope(env()->isolate());

    if (num_fields_)
      Flush();  // Flush trailing HTTP headers.

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(env()->context(),
                               kOnMessageComplete).ToLocalChecked();

    if (!cb->IsFunction())
      return 0;

    MaybeLocal<Value> r;
    {
      InternalCallbackScope callback_scope(
          this, InternalCallbackScope::kSkipTaskQueues);
      r = cb.As<Function>()->Call(env()->context(), object(), 0, nullptr);
      if (r.IsEmpty()) callback_scope.MarkAsFailed();
    }

    if (r.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return 0;
  }

#ifdef NODE_EXPERIMENTAL_HTTP
  // Reset nread for the next chunk
  int on_chunk_header() {
    header_nread_ = 0;
    return 0;
  }


  // Reset nread for the next chunk
  int on_chunk_complete() {
    header_nread_ = 0;
    return 0;
  }
#endif  /* NODE_EXPERIMENTAL_HTTP */


  static void New(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    new Parser(env, args.This());
  }


  static void Close(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());

    delete parser;
  }


  static void Free(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());

    // Since the Parser destructor isn't going to run the destroy() callbacks
    // it needs to be triggered manually.
    parser->EmitTraceEventDestroy();
    parser->EmitDestroy();
  }


  void Save() {
    url_.Save();
    status_message_.Save();

    for (size_t i = 0; i < num_fields_; i++) {
      fields_[i].Save();
    }

    for (size_t i = 0; i < num_values_; i++) {
      values_[i].Save();
    }
  }


  // var bytesParsed = parser->execute(buffer);
  static void Execute(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());
    CHECK(parser->current_buffer_.IsEmpty());
    CHECK_EQ(parser->current_buffer_len_, 0);
    CHECK_NULL(parser->current_buffer_data_);

    ArrayBufferViewContents<char> buffer(args[0]);

    // This is a hack to get the current_buffer to the callbacks with the least
    // amount of overhead. Nothing else will run while http_parser_execute()
    // runs, therefore this pointer can be set and used for the execution.
    parser->current_buffer_ = args[0].As<Object>();

    Local<Value> ret = parser->Execute(buffer.data(), buffer.length());

    if (!ret.IsEmpty())
      args.GetReturnValue().Set(ret);
  }


  static void Finish(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());

    CHECK(parser->current_buffer_.IsEmpty());
    Local<Value> ret = parser->Execute(nullptr, 0);

    if (!ret.IsEmpty())
      args.GetReturnValue().Set(ret);
  }


  static void Initialize(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    bool lenient = args[2]->IsTrue();

    CHECK(args[0]->IsInt32());
    CHECK(args[1]->IsObject());

    parser_type_t type =
        static_cast<parser_type_t>(args[0].As<Int32>()->Value());

    CHECK(type == HTTP_REQUEST || type == HTTP_RESPONSE);
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());
    // Should always be called from the same context.
    CHECK_EQ(env, parser->env());

    AsyncWrap::ProviderType provider =
        (type == HTTP_REQUEST ?
            AsyncWrap::PROVIDER_HTTPINCOMINGMESSAGE
            : AsyncWrap::PROVIDER_HTTPCLIENTREQUEST);

    parser->set_provider_type(provider);
    parser->AsyncReset(args[1].As<Object>());
    parser->Init(type, lenient);
  }

  template <bool should_pause>
  static void Pause(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());
    // Should always be called from the same context.
    CHECK_EQ(env, parser->env());

#ifdef NODE_EXPERIMENTAL_HTTP
    if (parser->execute_depth_) {
      parser->pending_pause_ = should_pause;
      return;
    }

    if (should_pause) {
      llhttp_pause(&parser->parser_);
    } else {
      llhttp_resume(&parser->parser_);
    }
#else  /* !NODE_EXPERIMENTAL_HTTP */
    http_parser_pause(&parser->parser_, should_pause);
#endif  /* NODE_EXPERIMENTAL_HTTP */
  }


  static void Consume(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());
    CHECK(args[0]->IsObject());
    StreamBase* stream = StreamBase::FromObject(args[0].As<Object>());
    CHECK_NOT_NULL(stream);
    stream->PushStreamListener(parser);
  }


  static void Unconsume(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());

    // Already unconsumed
    if (parser->stream_ == nullptr)
      return;

    parser->stream_->RemoveStreamListener(parser);
  }


  static void GetCurrentBuffer(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());

    Local<Object> ret = Buffer::Copy(
        parser->env(),
        parser->current_buffer_data_,
        parser->current_buffer_len_).ToLocalChecked();

    args.GetReturnValue().Set(ret);
  }

 protected:
  static const size_t kAllocBufferSize = 64 * 1024;

  uv_buf_t OnStreamAlloc(size_t suggested_size) override {
    // For most types of streams, OnStreamRead will be immediately after
    // OnStreamAlloc, and will consume all data, so using a static buffer for
    // reading is more efficient. For other streams, just use Malloc() directly.
    if (env()->http_parser_buffer_in_use())
      return uv_buf_init(Malloc(suggested_size), suggested_size);
    env()->set_http_parser_buffer_in_use(true);

    if (env()->http_parser_buffer() == nullptr)
      env()->set_http_parser_buffer(new char[kAllocBufferSize]);

    return uv_buf_init(env()->http_parser_buffer(), kAllocBufferSize);
  }


  void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override {
    HandleScope scope(env()->isolate());
    // Once we’re done here, either indicate that the HTTP parser buffer
    // is free for re-use, or free() the data if it didn’t come from there
    // in the first place.
    auto on_scope_leave = OnScopeLeave([&]() {
      if (buf.base == env()->http_parser_buffer())
        env()->set_http_parser_buffer_in_use(false);
      else
        free(buf.base);
    });

    if (nread < 0) {
      PassReadErrorToPreviousListener(nread);
      return;
    }

    // Ignore, empty reads have special meaning in http parser
    if (nread == 0)
      return;

    current_buffer_.Clear();
    Local<Value> ret = Execute(buf.base, nread);

    // Exception
    if (ret.IsEmpty())
      return;

    Local<Value> cb =
        object()->Get(env()->context(), kOnExecute).ToLocalChecked();

    if (!cb->IsFunction())
      return;

    // Hooks for GetCurrentBuffer
    current_buffer_len_ = nread;
    current_buffer_data_ = buf.base;

    MakeCallback(cb.As<Function>(), 1, &ret);

    current_buffer_len_ = 0;
    current_buffer_data_ = nullptr;
  }


  Local<Value> Execute(const char* data, size_t len) {
    EscapableHandleScope scope(env()->isolate());

    current_buffer_len_ = len;
    current_buffer_data_ = data;
    got_exception_ = false;

    parser_errno_t err;

#ifdef NODE_EXPERIMENTAL_HTTP
    // Do not allow re-entering `http_parser_execute()`
    CHECK_EQ(execute_depth_, 0);

    execute_depth_++;
    if (data == nullptr) {
      err = llhttp_finish(&parser_);
    } else {
      err = llhttp_execute(&parser_, data, len);
      Save();
    }
    execute_depth_--;

    // Calculate bytes read and resume after Upgrade/CONNECT pause
    size_t nread = len;
    if (err != HPE_OK) {
      nread = llhttp_get_error_pos(&parser_) - data;

      // This isn't a real pause, just a way to stop parsing early.
      if (err == HPE_PAUSED_UPGRADE) {
        err = HPE_OK;
        llhttp_resume_after_upgrade(&parser_);
      }
    }

    // Apply pending pause
    if (pending_pause_) {
      pending_pause_ = false;
      llhttp_pause(&parser_);
    }
#else  /* !NODE_EXPERIMENTAL_HTTP */
    size_t nread = http_parser_execute(&parser_, &settings, data, len);
    err = HTTP_PARSER_ERRNO(&parser_);

    // Finish()
    if (data == nullptr) {
      // `http_parser_execute()` returns either `0` or `1` when `len` is 0
      // (part of the finishing sequence).
      CHECK_EQ(len, 0);
      switch (nread) {
        case 0:
          err = HPE_OK;
          break;
        case 1:
          nread = 0;
          break;
        default:
          UNREACHABLE();
      }

    // Regular Execute()
    } else {
      Save();
    }
#endif  /* NODE_EXPERIMENTAL_HTTP */

    // Unassign the 'buffer_' variable
    current_buffer_.Clear();
    current_buffer_len_ = 0;
    current_buffer_data_ = nullptr;

    // If there was an exception in one of the callbacks
    if (got_exception_)
      return scope.Escape(Local<Value>());

    Local<Integer> nread_obj = Integer::New(env()->isolate(), nread);

    // If there was a parse error in one of the callbacks
    // TODO(bnoordhuis) What if there is an error on EOF?
    if (!parser_.upgrade && err != HPE_OK) {
      Local<Value> e = Exception::Error(env()->parse_error_string());
      Local<Object> obj = e->ToObject(env()->isolate()->GetCurrentContext())
        .ToLocalChecked();
      obj->Set(env()->context(),
               env()->bytes_parsed_string(),
               nread_obj).Check();
#ifdef NODE_EXPERIMENTAL_HTTP
      const char* errno_reason = llhttp_get_error_reason(&parser_);

      Local<String> code;
      Local<String> reason;
      if (err == HPE_USER) {
        const char* colon = strchr(errno_reason, ':');
        CHECK_NOT_NULL(colon);
        code = OneByteString(env()->isolate(), errno_reason,
                             colon - errno_reason);
        reason = OneByteString(env()->isolate(), colon + 1);
      } else {
        code = OneByteString(env()->isolate(), llhttp_errno_name(err));
        reason = OneByteString(env()->isolate(), errno_reason);
      }

      obj->Set(env()->context(), env()->code_string(), code).Check();
      obj->Set(env()->context(), env()->reason_string(), reason).Check();
#else  /* !NODE_EXPERIMENTAL_HTTP */
      obj->Set(env()->context(),
               env()->code_string(),
               OneByteString(env()->isolate(),
                             http_errno_name(err))).Check();
#endif  /* NODE_EXPERIMENTAL_HTTP */
      return scope.Escape(e);
    }

    // No return value is needed for `Finish()`
    if (data == nullptr) {
      return scope.Escape(Local<Value>());
    }
    return scope.Escape(nread_obj);
  }

  Local<Array> CreateHeaders() {
    // There could be extra entries but the max size should be fixed
    Local<Value> headers_v[kMaxHeaderFieldsCount * 2];

    for (size_t i = 0; i < num_values_; ++i) {
      headers_v[i * 2] = fields_[i].ToString(env());
      headers_v[i * 2 + 1] = values_[i].ToTrimmedString(env());
    }

    return Array::New(env()->isolate(), headers_v, num_values_ * 2);
  }


  // spill headers and request path to JS land
  void Flush() {
    HandleScope scope(env()->isolate());

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(env()->context(), kOnHeaders).ToLocalChecked();

    if (!cb->IsFunction())
      return;

    Local<Value> argv[2] = {
      CreateHeaders(),
      url_.ToString(env())
    };

    MaybeLocal<Value> r = MakeCallback(cb.As<Function>(),
                                       arraysize(argv),
                                       argv);

    if (r.IsEmpty())
      got_exception_ = true;

    url_.Reset();
    have_flushed_ = true;
  }


  void Init(parser_type_t type, bool lenient) {
#ifdef NODE_EXPERIMENTAL_HTTP
    llhttp_init(&parser_, type, &settings);
    llhttp_set_lenient(&parser_, lenient);
    header_nread_ = 0;
#else  /* !NODE_EXPERIMENTAL_HTTP */
    http_parser_init(&parser_, type);
    parser_.lenient_http_headers = lenient;
#endif  /* NODE_EXPERIMENTAL_HTTP */
    url_.Reset();
    status_message_.Reset();
    num_fields_ = 0;
    num_values_ = 0;
    have_flushed_ = false;
    got_exception_ = false;
  }


  int TrackHeader(size_t len) {
#ifdef NODE_EXPERIMENTAL_HTTP
    header_nread_ += len;
    if (header_nread_ >= per_process::cli_options->max_http_header_size) {
      llhttp_set_error_reason(&parser_, "HPE_HEADER_OVERFLOW:Header overflow");
      return HPE_USER;
    }
#endif  /* NODE_EXPERIMENTAL_HTTP */
    return 0;
  }


  int MaybePause() {
#ifdef NODE_EXPERIMENTAL_HTTP
    CHECK_NE(execute_depth_, 0);

    if (!pending_pause_) {
      return 0;
    }

    pending_pause_ = false;
    llhttp_set_error_reason(&parser_, "Paused in callback");
    return HPE_PAUSED;
#else  /* !NODE_EXPERIMENTAL_HTTP */
    return 0;
#endif  /* NODE_EXPERIMENTAL_HTTP */
  }

  parser_t parser_;
  StringPtr fields_[kMaxHeaderFieldsCount];  // header fields
  StringPtr values_[kMaxHeaderFieldsCount];  // header values
  StringPtr url_;
  StringPtr status_message_;
  size_t num_fields_;
  size_t num_values_;
  bool have_flushed_;
  bool got_exception_;
  Local<Object> current_buffer_;
  size_t current_buffer_len_;
  const char* current_buffer_data_;
#ifdef NODE_EXPERIMENTAL_HTTP
  unsigned int execute_depth_ = 0;
  bool pending_pause_ = false;
  uint64_t header_nread_ = 0;
#endif  /* NODE_EXPERIMENTAL_HTTP */

  // These are helper functions for filling `http_parser_settings`, which turn
  // a member function of Parser into a C-style HTTP parser callback.
  template <typename Parser, Parser> struct Proxy;
  template <typename Parser, typename ...Args, int (Parser::*Member)(Args...)>
  struct Proxy<int (Parser::*)(Args...), Member> {
    static int Raw(parser_t* p, Args ... args) {
      Parser* parser = ContainerOf(&Parser::parser_, p);
      int rv = (parser->*Member)(std::forward<Args>(args)...);
      if (rv == 0) {
        rv = parser->MaybePause();
      }
      return rv;
    }
  };

  typedef int (Parser::*Call)();
  typedef int (Parser::*DataCall)(const char* at, size_t length);

  static const parser_settings_t settings;
};

const parser_settings_t Parser::settings = {
  Proxy<Call, &Parser::on_message_begin>::Raw,
  Proxy<DataCall, &Parser::on_url>::Raw,
  Proxy<DataCall, &Parser::on_status>::Raw,
  Proxy<DataCall, &Parser::on_header_field>::Raw,
  Proxy<DataCall, &Parser::on_header_value>::Raw,
  Proxy<Call, &Parser::on_headers_complete>::Raw,
  Proxy<DataCall, &Parser::on_body>::Raw,
  Proxy<Call, &Parser::on_message_complete>::Raw,
#ifdef NODE_EXPERIMENTAL_HTTP
  Proxy<Call, &Parser::on_chunk_header>::Raw,
  Proxy<Call, &Parser::on_chunk_complete>::Raw,
#else  /* !NODE_EXPERIMENTAL_HTTP */
  nullptr,
  nullptr,
#endif  /* NODE_EXPERIMENTAL_HTTP */
};


#ifndef NODE_EXPERIMENTAL_HTTP
void InitMaxHttpHeaderSizeOnce() {
  const uint32_t max_http_header_size =
      per_process::cli_options->max_http_header_size;
  http_parser_set_max_header_size(max_http_header_size);
}
#endif  /* NODE_EXPERIMENTAL_HTTP */


void InitializeHttpParser(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context,
                          void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Local<FunctionTemplate> t = env->NewFunctionTemplate(Parser::New);
  t->InstanceTemplate()->SetInternalFieldCount(Parser::kInternalFieldCount);
  t->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "HTTPParser"));

  t->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "REQUEST"),
         Integer::New(env->isolate(), HTTP_REQUEST));
  t->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "RESPONSE"),
         Integer::New(env->isolate(), HTTP_RESPONSE));
  t->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "kOnHeaders"),
         Integer::NewFromUnsigned(env->isolate(), kOnHeaders));
  t->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "kOnHeadersComplete"),
         Integer::NewFromUnsigned(env->isolate(), kOnHeadersComplete));
  t->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "kOnBody"),
         Integer::NewFromUnsigned(env->isolate(), kOnBody));
  t->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "kOnMessageComplete"),
         Integer::NewFromUnsigned(env->isolate(), kOnMessageComplete));
  t->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "kOnExecute"),
         Integer::NewFromUnsigned(env->isolate(), kOnExecute));

  Local<Array> methods = Array::New(env->isolate());
#define V(num, name, string)                                                  \
    methods->Set(env->context(),                                              \
        num, FIXED_ONE_BYTE_STRING(env->isolate(), #string)).Check();
  HTTP_METHOD_MAP(V)
#undef V
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "methods"),
              methods).Check();

  t->Inherit(AsyncWrap::GetConstructorTemplate(env));
  env->SetProtoMethod(t, "close", Parser::Close);
  env->SetProtoMethod(t, "free", Parser::Free);
  env->SetProtoMethod(t, "execute", Parser::Execute);
  env->SetProtoMethod(t, "finish", Parser::Finish);
  env->SetProtoMethod(t, "initialize", Parser::Initialize);
  env->SetProtoMethod(t, "pause", Parser::Pause<true>);
  env->SetProtoMethod(t, "resume", Parser::Pause<false>);
  env->SetProtoMethod(t, "consume", Parser::Consume);
  env->SetProtoMethod(t, "unconsume", Parser::Unconsume);
  env->SetProtoMethod(t, "getCurrentBuffer", Parser::GetCurrentBuffer);

  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "HTTPParser"),
              t->GetFunction(env->context()).ToLocalChecked()).Check();

#ifndef NODE_EXPERIMENTAL_HTTP
  static uv_once_t init_once = UV_ONCE_INIT;
  uv_once(&init_once, InitMaxHttpHeaderSizeOnce);
#endif  /* NODE_EXPERIMENTAL_HTTP */
}

}  // anonymous namespace
}  // namespace node
