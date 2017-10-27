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

#include "node.h"
#include "node_buffer.h"

#include "async-wrap-inl.h"
#include "env-inl.h"
#include "http_parser.h"
#include "stream_base-inl.h"
#include "util-inl.h"
#include "v8.h"

#include <stdlib.h>  // free()
#include <string.h>  // strdup()

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
namespace {

using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
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


#define HTTP_CB(name)                                                         \
  static int name(http_parser* p_) {                                          \
    Parser* self = ContainerOf(&Parser::parser_, p_);                         \
    return self->name##_();                                                   \
  }                                                                           \
  int name##_()


#define HTTP_DATA_CB(name)                                                    \
  static int name(http_parser* p_, const char* at, size_t length) {           \
    Parser* self = ContainerOf(&Parser::parser_, p_);                         \
    return self->name##_(at, length);                                         \
  }                                                                           \
  int name##_(const char* at, size_t length)


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
    if (str_)
      return OneByteString(env->isolate(), str_, size_);
    else
      return String::Empty(env->isolate());
  }


  const char* str_;
  bool on_heap_;
  size_t size_;
};


class Parser : public AsyncWrap {
 public:
  Parser(Environment* env, Local<Object> wrap, enum http_parser_type type)
      : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_HTTPPARSER),
        current_buffer_len_(0),
        current_buffer_data_(nullptr) {
    Wrap(object(), this);
    Init(type);
  }


  ~Parser() override {
    ClearWrap(object());
    persistent().Reset();
  }


  size_t self_size() const override {
    return sizeof(*this);
  }


  HTTP_CB(on_message_begin) {
    num_fields_ = num_values_ = 0;
    url_.Reset();
    status_message_.Reset();
    return 0;
  }


  HTTP_DATA_CB(on_url) {
    url_.Update(at, length);
    return 0;
  }


  HTTP_DATA_CB(on_status) {
    status_message_.Update(at, length);
    return 0;
  }


  HTTP_DATA_CB(on_header_field) {
    if (num_fields_ == num_values_) {
      // start of new field name
      num_fields_++;
      if (num_fields_ == arraysize(fields_)) {
        // ran out of space - flush to javascript land
        Flush();
        num_fields_ = 1;
        num_values_ = 0;
      }
      fields_[num_fields_ - 1].Reset();
    }

    CHECK_LT(num_fields_, arraysize(fields_));
    CHECK_EQ(num_fields_, num_values_ + 1);

    fields_[num_fields_ - 1].Update(at, length);

    return 0;
  }


  HTTP_DATA_CB(on_header_value) {
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


  HTTP_CB(on_headers_complete) {
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
    Local<Value> cb = obj->Get(kOnHeadersComplete);

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

    argv[A_SHOULD_KEEP_ALIVE] =
        Boolean::New(env()->isolate(), http_should_keep_alive(&parser_));

    argv[A_UPGRADE] = Boolean::New(env()->isolate(), parser_.upgrade);

    Environment::AsyncCallbackScope callback_scope(env());

    MaybeLocal<Value> head_response =
        MakeCallback(cb.As<Function>(), arraysize(argv), argv);

    if (head_response.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return head_response.ToLocalChecked()->IntegerValue();
  }


  HTTP_DATA_CB(on_body) {
    EscapableHandleScope scope(env()->isolate());

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(kOnBody);

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
      return -1;
    }

    return 0;
  }


  HTTP_CB(on_message_complete) {
    HandleScope scope(env()->isolate());

    if (num_fields_)
      Flush();  // Flush trailing HTTP headers.

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(kOnMessageComplete);

    if (!cb->IsFunction())
      return 0;

    Environment::AsyncCallbackScope callback_scope(env());

    MaybeLocal<Value> r = MakeCallback(cb.As<Function>(), 0, nullptr);

    if (r.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return 0;
  }


  static void New(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    http_parser_type type =
        static_cast<http_parser_type>(args[0]->Int32Value());
    CHECK(type == HTTP_REQUEST || type == HTTP_RESPONSE);
    new Parser(env, args.This(), type);
  }


  static void Close(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());

    if (--parser->refcount_ == 0)
      delete parser;
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
    CHECK_EQ(parser->current_buffer_data_, nullptr);
    CHECK_EQ(Buffer::HasInstance(args[0]), true);

    Local<Object> buffer_obj = args[0].As<Object>();
    char* buffer_data = Buffer::Data(buffer_obj);
    size_t buffer_len = Buffer::Length(buffer_obj);

    // This is a hack to get the current_buffer to the callbacks with the least
    // amount of overhead. Nothing else will run while http_parser_execute()
    // runs, therefore this pointer can be set and used for the execution.
    parser->current_buffer_ = buffer_obj;

    Local<Value> ret = parser->Execute(buffer_data, buffer_len);

    if (!ret.IsEmpty())
      args.GetReturnValue().Set(ret);
  }


  static void Finish(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());

    CHECK(parser->current_buffer_.IsEmpty());
    parser->got_exception_ = false;

    int rv = http_parser_execute(&(parser->parser_), &settings, nullptr, 0);

    if (parser->got_exception_)
      return;

    if (rv != 0) {
      enum http_errno err = HTTP_PARSER_ERRNO(&parser->parser_);

      Local<Value> e = Exception::Error(env->parse_error_string());
      Local<Object> obj = e->ToObject(env->isolate());
      obj->Set(env->bytes_parsed_string(), Integer::New(env->isolate(), 0));
      obj->Set(env->code_string(),
               OneByteString(env->isolate(), http_errno_name(err)));

      args.GetReturnValue().Set(e);
    }
  }


  static void Reinitialize(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    http_parser_type type =
        static_cast<http_parser_type>(args[0]->Int32Value());

    CHECK(type == HTTP_REQUEST || type == HTTP_RESPONSE);
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());
    // Should always be called from the same context.
    CHECK_EQ(env, parser->env());
    // The parser is being reused. Reset the async id and call init() callbacks.
    parser->AsyncReset();
    parser->Init(type);
  }


  template <bool should_pause>
  static void Pause(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());
    // Should always be called from the same context.
    CHECK_EQ(env, parser->env());
    http_parser_pause(&parser->parser_, should_pause);
  }


  static void Consume(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());
    CHECK(args[0]->IsExternal());
    Local<External> stream_obj = args[0].As<External>();
    StreamBase* stream = static_cast<StreamBase*>(stream_obj->Value());
    CHECK_NE(stream, nullptr);

    stream->Consume();

    parser->prev_alloc_cb_ = stream->alloc_cb();
    parser->prev_read_cb_ = stream->read_cb();

    stream->set_alloc_cb({ OnAllocImpl, parser });
    stream->set_read_cb({ OnReadImpl, parser });
  }


  static void Unconsume(const FunctionCallbackInfo<Value>& args) {
    Parser* parser;
    ASSIGN_OR_RETURN_UNWRAP(&parser, args.Holder());

    // Already unconsumed
    if (parser->prev_alloc_cb_.is_empty())
      return;

    // Restore stream's callbacks
    if (args.Length() == 1 && args[0]->IsExternal()) {
      Local<External> stream_obj = args[0].As<External>();
      StreamBase* stream = static_cast<StreamBase*>(stream_obj->Value());
      CHECK_NE(stream, nullptr);

      stream->set_alloc_cb(parser->prev_alloc_cb_);
      stream->set_read_cb(parser->prev_read_cb_);
      stream->Unconsume();
    }

    parser->prev_alloc_cb_.clear();
    parser->prev_read_cb_.clear();
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
  class ScopedRetainParser {
   public:
    explicit ScopedRetainParser(Parser* p) : p_(p) {
      CHECK_GT(p_->refcount_, 0);
      p_->refcount_++;
    }

    ~ScopedRetainParser() {
      if (0 == --p_->refcount_)
        delete p_;
    }

   private:
    Parser* const p_;
  };

  static const size_t kAllocBufferSize = 64 * 1024;

  static void OnAllocImpl(size_t suggested_size, uv_buf_t* buf, void* ctx) {
    Parser* parser = static_cast<Parser*>(ctx);
    Environment* env = parser->env();

    if (env->http_parser_buffer() == nullptr)
      env->set_http_parser_buffer(new char[kAllocBufferSize]);

    buf->base = env->http_parser_buffer();
    buf->len = kAllocBufferSize;
  }


  static void OnReadImpl(ssize_t nread,
                         const uv_buf_t* buf,
                         uv_handle_type pending,
                         void* ctx) {
    Parser* parser = static_cast<Parser*>(ctx);
    HandleScope scope(parser->env()->isolate());

    if (nread < 0) {
      uv_buf_t tmp_buf;
      tmp_buf.base = nullptr;
      tmp_buf.len = 0;
      parser->prev_read_cb_.fn(nread,
                               &tmp_buf,
                               pending,
                               parser->prev_read_cb_.ctx);
      return;
    }

    // Ignore, empty reads have special meaning in http parser
    if (nread == 0)
      return;

    ScopedRetainParser retain(parser);

    parser->current_buffer_.Clear();
    Local<Value> ret = parser->Execute(buf->base, nread);

    // Exception
    if (ret.IsEmpty())
      return;

    Local<Object> obj = parser->object();
    Local<Value> cb = obj->Get(kOnExecute);

    if (!cb->IsFunction())
      return;

    // Hooks for GetCurrentBuffer
    parser->current_buffer_len_ = nread;
    parser->current_buffer_data_ = buf->base;

    parser->MakeCallback(cb.As<Function>(), 1, &ret);

    parser->current_buffer_len_ = 0;
    parser->current_buffer_data_ = nullptr;
  }


  Local<Value> Execute(char* data, size_t len) {
    EscapableHandleScope scope(env()->isolate());

    current_buffer_len_ = len;
    current_buffer_data_ = data;
    got_exception_ = false;

    size_t nparsed =
      http_parser_execute(&parser_, &settings, data, len);

    Save();

    // Unassign the 'buffer_' variable
    current_buffer_.Clear();
    current_buffer_len_ = 0;
    current_buffer_data_ = nullptr;

    // If there was an exception in one of the callbacks
    if (got_exception_)
      return scope.Escape(Local<Value>());

    Local<Integer> nparsed_obj = Integer::New(env()->isolate(), nparsed);
    // If there was a parse error in one of the callbacks
    // TODO(bnoordhuis) What if there is an error on EOF?
    if (!parser_.upgrade && nparsed != len) {
      enum http_errno err = HTTP_PARSER_ERRNO(&parser_);

      Local<Value> e = Exception::Error(env()->parse_error_string());
      Local<Object> obj = e->ToObject(env()->isolate());
      obj->Set(env()->bytes_parsed_string(), nparsed_obj);
      obj->Set(env()->code_string(),
               OneByteString(env()->isolate(), http_errno_name(err)));

      return scope.Escape(e);
    }
    return scope.Escape(nparsed_obj);
  }

  Local<Array> CreateHeaders() {
    Local<Array> headers = Array::New(env()->isolate());
    Local<Function> fn = env()->push_values_to_array_function();
    Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX * 2];
    size_t i = 0;

    do {
      size_t j = 0;
      while (i < num_values_ && j < arraysize(argv) / 2) {
        argv[j * 2] = fields_[i].ToString(env());
        argv[j * 2 + 1] = values_[i].ToString(env());
        i++;
        j++;
      }
      if (j > 0) {
        fn->Call(env()->context(), headers, j * 2, argv).ToLocalChecked();
      }
    } while (i < num_values_);

    return headers;
  }


  // spill headers and request path to JS land
  void Flush() {
    HandleScope scope(env()->isolate());

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(kOnHeaders);

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


  void Init(enum http_parser_type type) {
    http_parser_init(&parser_, type);
    url_.Reset();
    status_message_.Reset();
    num_fields_ = 0;
    num_values_ = 0;
    have_flushed_ = false;
    got_exception_ = false;
  }


  http_parser parser_;
  StringPtr fields_[32];  // header fields
  StringPtr values_[32];  // header values
  StringPtr url_;
  StringPtr status_message_;
  size_t num_fields_;
  size_t num_values_;
  bool have_flushed_;
  bool got_exception_;
  Local<Object> current_buffer_;
  size_t current_buffer_len_;
  char* current_buffer_data_;
  StreamResource::Callback<StreamResource::AllocCb> prev_alloc_cb_;
  StreamResource::Callback<StreamResource::ReadCb> prev_read_cb_;
  int refcount_ = 1;
  static const struct http_parser_settings settings;

  friend class ScopedRetainParser;
};


const struct http_parser_settings Parser::settings = {
  Parser::on_message_begin,
  Parser::on_url,
  Parser::on_status,
  Parser::on_header_field,
  Parser::on_header_value,
  Parser::on_headers_complete,
  Parser::on_body,
  Parser::on_message_complete,
  nullptr,  // on_chunk_header
  nullptr   // on_chunk_complete
};


void InitHttpParser(Local<Object> target,
                    Local<Value> unused,
                    Local<Context> context,
                    void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Local<FunctionTemplate> t = env->NewFunctionTemplate(Parser::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
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
    methods->Set(num, FIXED_ONE_BYTE_STRING(env->isolate(), #string));
  HTTP_METHOD_MAP(V)
#undef V
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "methods"), methods);

  AsyncWrap::AddWrapMethods(env, t);
  env->SetProtoMethod(t, "close", Parser::Close);
  env->SetProtoMethod(t, "execute", Parser::Execute);
  env->SetProtoMethod(t, "finish", Parser::Finish);
  env->SetProtoMethod(t, "reinitialize", Parser::Reinitialize);
  env->SetProtoMethod(t, "pause", Parser::Pause<true>);
  env->SetProtoMethod(t, "resume", Parser::Pause<false>);
  env->SetProtoMethod(t, "consume", Parser::Consume);
  env->SetProtoMethod(t, "unconsume", Parser::Unconsume);
  env->SetProtoMethod(t, "getCurrentBuffer", Parser::GetCurrentBuffer);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "HTTPParser"),
              t->GetFunction());
}

}  // anonymous namespace
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(http_parser, node::InitHttpParser)
