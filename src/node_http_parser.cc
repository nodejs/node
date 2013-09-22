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
#include "node_http_parser.h"

#include "base-object.h"
#include "base-object-inl.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

#include <stdlib.h>  // free()
#include <string.h>  // strdup()

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#else
#include <strings.h>  // strcasecmp()
#endif

// This is a binding to http_parser (https://github.com/joyent/http-parser)
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

using v8::Array;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

const uint32_t kOnHeaders = 0;
const uint32_t kOnHeadersComplete = 1;
const uint32_t kOnBody = 2;
const uint32_t kOnMessageComplete = 3;


#define HTTP_CB(name)                                                         \
  static int name(http_parser* p_) {                                          \
    Parser* self = CONTAINER_OF(p_, Parser, parser_);                         \
    return self->name##_();                                                   \
  }                                                                           \
  int name##_()


#define HTTP_DATA_CB(name)                                                    \
  static int name(http_parser* p_, const char* at, size_t length) {           \
    Parser* self = CONTAINER_OF(p_, Parser, parser_);                         \
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

    str_ = NULL;
    size_ = 0;
  }


  void Update(const char* str, size_t size) {
    if (str_ == NULL)
      str_ = str;
    else if (on_heap_ || str_ + size_ != str) {
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


  Local<String> ToString() const {
    if (str_)
      return OneByteString(node_isolate, str_, size_);
    else
      return String::Empty(node_isolate);
  }


  const char* str_;
  bool on_heap_;
  size_t size_;
};


class Parser : public BaseObject {
 public:
  Parser(Environment* env, Local<Object> wrap, enum http_parser_type type)
      : BaseObject(env, wrap),
        current_buffer_len_(0),
        current_buffer_data_(NULL) {
    MakeWeak<Parser>(this);
    Init(type);
  }


  ~Parser() {
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
      if (num_fields_ == ARRAY_SIZE(fields_)) {
        // ran out of space - flush to javascript land
        Flush();
        num_fields_ = 1;
        num_values_ = 0;
      }
      fields_[num_fields_ - 1].Reset();
    }

    assert(num_fields_ < static_cast<int>(ARRAY_SIZE(fields_)));
    assert(num_fields_ == num_values_ + 1);

    fields_[num_fields_ - 1].Update(at, length);

    return 0;
  }


  HTTP_DATA_CB(on_header_value) {
    if (num_values_ != num_fields_) {
      // start of new header value
      num_values_++;
      values_[num_values_ - 1].Reset();
    }

    assert(num_values_ < static_cast<int>(ARRAY_SIZE(values_)));
    assert(num_values_ == num_fields_);

    values_[num_values_ - 1].Update(at, length);

    return 0;
  }


  HTTP_CB(on_headers_complete) {
    Local<Object> obj = object();
    Local<Value> cb = obj->Get(kOnHeadersComplete);

    if (!cb->IsFunction())
      return 0;

    Local<Object> message_info = Object::New();

    if (have_flushed_) {
      // Slow case, flush remaining headers.
      Flush();
    } else {
      // Fast case, pass headers and URL to JS land.
      message_info->Set(env()->headers_string(), CreateHeaders());
      if (parser_.type == HTTP_REQUEST)
        message_info->Set(env()->url_string(), url_.ToString());
    }
    num_fields_ = num_values_ = 0;

    // METHOD
    if (parser_.type == HTTP_REQUEST) {
      message_info->Set(env()->method_string(),
                        Uint32::NewFromUnsigned(parser_.method));
    }

    // STATUS
    if (parser_.type == HTTP_RESPONSE) {
      message_info->Set(env()->status_code_string(),
                        Integer::New(parser_.status_code, node_isolate));
      message_info->Set(env()->status_message_string(),
                        status_message_.ToString());
    }

    // VERSION
    message_info->Set(env()->version_major_string(),
                      Integer::New(parser_.http_major, node_isolate));
    message_info->Set(env()->version_minor_string(),
                      Integer::New(parser_.http_minor, node_isolate));

    message_info->Set(env()->should_keep_alive_string(),
                      http_should_keep_alive(&parser_) ? True(node_isolate)
                                                       : False(node_isolate));

    message_info->Set(env()->upgrade_string(),
                      parser_.upgrade ? True(node_isolate)
                                      : False(node_isolate));

    Local<Value> argv[1] = { message_info };
    Local<Value> head_response =
        cb.As<Function>()->Call(obj, ARRAY_SIZE(argv), argv);

    if (head_response.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return head_response->IsTrue() ? 1 : 0;
  }


  HTTP_DATA_CB(on_body) {
    HandleScope scope(node_isolate);

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(kOnBody);

    if (!cb->IsFunction())
      return 0;

    Local<Value> argv[3] = {
      current_buffer_,
      Integer::NewFromUnsigned(at - current_buffer_data_, node_isolate),
      Integer::NewFromUnsigned(length, node_isolate)
    };

    Local<Value> r = cb.As<Function>()->Call(obj, ARRAY_SIZE(argv), argv);

    if (r.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return 0;
  }


  HTTP_CB(on_message_complete) {
    HandleScope scope(node_isolate);

    if (num_fields_)
      Flush();  // Flush trailing HTTP headers.

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(kOnMessageComplete);

    if (!cb->IsFunction())
      return 0;

    Local<Value> r = cb.As<Function>()->Call(obj, 0, NULL);

    if (r.IsEmpty()) {
      got_exception_ = true;
      return -1;
    }

    return 0;
  }


  static void New(const FunctionCallbackInfo<Value>& args) {
    HandleScope handle_scope(args.GetIsolate());
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    http_parser_type type =
        static_cast<http_parser_type>(args[0]->Int32Value());
    assert(type == HTTP_REQUEST || type == HTTP_RESPONSE);
    new Parser(env, args.This(), type);
  }


  void Save() {
    url_.Save();
    status_message_.Save();

    for (int i = 0; i < num_fields_; i++) {
      fields_[i].Save();
    }

    for (int i = 0; i < num_values_; i++) {
      values_[i].Save();
    }
  }


  // var bytesParsed = parser->execute(buffer);
  static void Execute(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    Parser* parser = Unwrap<Parser>(args.This());
    assert(parser->current_buffer_.IsEmpty());
    assert(parser->current_buffer_len_ == 0);
    assert(parser->current_buffer_data_ == NULL);
    assert(Buffer::HasInstance(args[0]) == true);

    Local<Object> buffer_obj = args[0].As<Object>();
    char* buffer_data = Buffer::Data(buffer_obj);
    size_t buffer_len = Buffer::Length(buffer_obj);

    // This is a hack to get the current_buffer to the callbacks with the least
    // amount of overhead. Nothing else will run while http_parser_execute()
    // runs, therefore this pointer can be set and used for the execution.
    parser->current_buffer_ = buffer_obj;
    parser->current_buffer_len_ = buffer_len;
    parser->current_buffer_data_ = buffer_data;
    parser->got_exception_ = false;

    size_t nparsed =
      http_parser_execute(&parser->parser_, &settings, buffer_data, buffer_len);

    parser->Save();

    // Unassign the 'buffer_' variable
    parser->current_buffer_.Clear();
    parser->current_buffer_len_ = 0;
    parser->current_buffer_data_ = NULL;

    // If there was an exception in one of the callbacks
    if (parser->got_exception_)
      return;

    Local<Integer> nparsed_obj = Integer::New(nparsed, node_isolate);
    // If there was a parse error in one of the callbacks
    // TODO(bnoordhuis) What if there is an error on EOF?
    if (!parser->parser_.upgrade && nparsed != buffer_len) {
      enum http_errno err = HTTP_PARSER_ERRNO(&parser->parser_);

      Local<Value> e = Exception::Error(
          FIXED_ONE_BYTE_STRING(node_isolate, "Parse Error"));
      Local<Object> obj = e->ToObject();
      obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "bytesParsed"), nparsed_obj);
      obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "code"),
               OneByteString(node_isolate, http_errno_name(err)));

      args.GetReturnValue().Set(e);
    } else {
      args.GetReturnValue().Set(nparsed_obj);
    }
  }


  static void Finish(const FunctionCallbackInfo<Value>& args) {
    HandleScope scope(node_isolate);

    Parser* parser = Unwrap<Parser>(args.This());

    assert(parser->current_buffer_.IsEmpty());
    parser->got_exception_ = false;

    int rv = http_parser_execute(&(parser->parser_), &settings, NULL, 0);

    if (parser->got_exception_)
      return;

    if (rv != 0) {
      enum http_errno err = HTTP_PARSER_ERRNO(&parser->parser_);

      Local<Value> e = Exception::Error(
          FIXED_ONE_BYTE_STRING(node_isolate, "Parse Error"));
      Local<Object> obj = e->ToObject();
      obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "bytesParsed"),
               Integer::New(0, node_isolate));
      obj->Set(FIXED_ONE_BYTE_STRING(node_isolate, "code"),
               OneByteString(node_isolate, http_errno_name(err)));

      args.GetReturnValue().Set(e);
    }
  }


  static void Reinitialize(const FunctionCallbackInfo<Value>& args) {
    HandleScope handle_scope(args.GetIsolate());
    Environment* env = Environment::GetCurrent(args.GetIsolate());

    http_parser_type type =
        static_cast<http_parser_type>(args[0]->Int32Value());

    assert(type == HTTP_REQUEST || type == HTTP_RESPONSE);
    Parser* parser = Unwrap<Parser>(args.This());
    // Should always be called from the same context.
    assert(env == parser->env());
    parser->Init(type);
  }


  template <bool should_pause>
  static void Pause(const FunctionCallbackInfo<Value>& args) {
    HandleScope handle_scope(args.GetIsolate());
    Environment* env = Environment::GetCurrent(args.GetIsolate());
    Parser* parser = Unwrap<Parser>(args.This());
    // Should always be called from the same context.
    assert(env == parser->env());
    http_parser_pause(&parser->parser_, should_pause);
  }


 private:

  Local<Array> CreateHeaders() {
    // num_values_ is either -1 or the entry # of the last header
    // so num_values_ == 0 means there's a single header
    Local<Array> headers = Array::New(2 * num_values_);

    for (int i = 0; i < num_values_; ++i) {
      headers->Set(2 * i, fields_[i].ToString());
      headers->Set(2 * i + 1, values_[i].ToString());
    }

    return headers;
  }


  // spill headers and request path to JS land
  void Flush() {
    HandleScope scope(node_isolate);

    Local<Object> obj = object();
    Local<Value> cb = obj->Get(kOnHeaders);

    if (!cb->IsFunction())
      return;

    Local<Value> argv[2] = {
      CreateHeaders(),
      url_.ToString()
    };

    Local<Value> r = cb.As<Function>()->Call(obj, ARRAY_SIZE(argv), argv);

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
  int num_fields_;
  int num_values_;
  bool have_flushed_;
  bool got_exception_;
  Local<Object> current_buffer_;
  size_t current_buffer_len_;
  char* current_buffer_data_;
  static const struct http_parser_settings settings;
};


const struct http_parser_settings Parser::settings = {
  Parser::on_message_begin,
  Parser::on_url,
  Parser::on_status,
  Parser::on_header_field,
  Parser::on_header_value,
  Parser::on_headers_complete,
  Parser::on_body,
  Parser::on_message_complete
};


void InitHttpParser(Handle<Object> target,
                    Handle<Value> unused,
                    Handle<Context> context) {
  Local<FunctionTemplate> t = FunctionTemplate::New(Parser::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(node_isolate, "HTTPParser"));

  t->Set(FIXED_ONE_BYTE_STRING(node_isolate, "REQUEST"),
         Integer::New(HTTP_REQUEST, node_isolate));
  t->Set(FIXED_ONE_BYTE_STRING(node_isolate, "RESPONSE"),
         Integer::New(HTTP_RESPONSE, node_isolate));
  t->Set(FIXED_ONE_BYTE_STRING(node_isolate, "kOnHeaders"),
         Integer::NewFromUnsigned(kOnHeaders, node_isolate));
  t->Set(FIXED_ONE_BYTE_STRING(node_isolate, "kOnHeadersComplete"),
         Integer::NewFromUnsigned(kOnHeadersComplete, node_isolate));
  t->Set(FIXED_ONE_BYTE_STRING(node_isolate, "kOnBody"),
         Integer::NewFromUnsigned(kOnBody, node_isolate));
  t->Set(FIXED_ONE_BYTE_STRING(node_isolate, "kOnMessageComplete"),
         Integer::NewFromUnsigned(kOnMessageComplete, node_isolate));

  Local<Array> methods = Array::New();
#define V(num, name, string)                                                  \
    methods->Set(num, FIXED_ONE_BYTE_STRING(node_isolate, #string));
  HTTP_METHOD_MAP(V)
#undef V
  t->Set(FIXED_ONE_BYTE_STRING(node_isolate, "methods"), methods);

  NODE_SET_PROTOTYPE_METHOD(t, "execute", Parser::Execute);
  NODE_SET_PROTOTYPE_METHOD(t, "finish", Parser::Finish);
  NODE_SET_PROTOTYPE_METHOD(t, "reinitialize", Parser::Reinitialize);
  NODE_SET_PROTOTYPE_METHOD(t, "pause", Parser::Pause<true>);
  NODE_SET_PROTOTYPE_METHOD(t, "resume", Parser::Pause<false>);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "HTTPParser"),
              t->GetFunction());
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_http_parser, node::InitHttpParser)
