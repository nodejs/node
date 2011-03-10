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

#include <node_http_parser.h>

#include <v8.h>
#include <node.h>
#include <node_buffer.h>

#include <http_parser.h>

#include <strings.h>  /* strcasecmp() */
#include <string.h>  /* strdup() */
#include <stdlib.h>  /* free() */

// This is a binding to http_parser (http://github.com/ry/http-parser)
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

using namespace v8;

static Persistent<String> on_message_begin_sym;
static Persistent<String> on_path_sym;
static Persistent<String> on_query_string_sym;
static Persistent<String> on_url_sym;
static Persistent<String> on_fragment_sym;
static Persistent<String> on_header_field_sym;
static Persistent<String> on_header_value_sym;
static Persistent<String> on_headers_complete_sym;
static Persistent<String> on_body_sym;
static Persistent<String> on_message_complete_sym;

static Persistent<String> delete_sym;
static Persistent<String> get_sym;
static Persistent<String> head_sym;
static Persistent<String> post_sym;
static Persistent<String> put_sym;
static Persistent<String> connect_sym;
static Persistent<String> options_sym;
static Persistent<String> trace_sym;
static Persistent<String> copy_sym;
static Persistent<String> lock_sym;
static Persistent<String> mkcol_sym;
static Persistent<String> move_sym;
static Persistent<String> propfind_sym;
static Persistent<String> proppatch_sym;
static Persistent<String> unlock_sym;
static Persistent<String> report_sym;
static Persistent<String> mkactivity_sym;
static Persistent<String> checkout_sym;
static Persistent<String> merge_sym;
static Persistent<String> msearch_sym;
static Persistent<String> notify_sym;
static Persistent<String> subscribe_sym;
static Persistent<String> unsubscribe_sym;
static Persistent<String> unknown_method_sym;

static Persistent<String> method_sym;
static Persistent<String> status_code_sym;
static Persistent<String> http_version_sym;
static Persistent<String> version_major_sym;
static Persistent<String> version_minor_sym;
static Persistent<String> should_keep_alive_sym;
static Persistent<String> upgrade_sym;

static struct http_parser_settings settings;


// This is a hack to get the current_buffer to the callbacks with the least
// amount of overhead. Nothing else will run while http_parser_execute()
// runs, therefore this pointer can be set and used for the execution.
static Local<Value>* current_buffer;
static char* current_buffer_data;
static size_t current_buffer_len;


// Callback prototype for http_cb
#define DEFINE_HTTP_CB(name)                                             \
  static int name(http_parser *p) {                                      \
    Parser *parser = static_cast<Parser*>(p->data);                      \
    Local<Value> cb_value = parser->handle_->Get(name##_sym);            \
    if (!cb_value->IsFunction()) return 0;                               \
    Local<Function> cb = Local<Function>::Cast(cb_value);                \
    Local<Value> ret = cb->Call(parser->handle_, 0, NULL);               \
    if (ret.IsEmpty()) {                                                 \
      parser->got_exception_ = true;                                     \
      return -1;                                                         \
    } else {                                                             \
      return 0;                                                          \
    }                                                                    \
  }

// Callback prototype for http_data_cb
#define DEFINE_HTTP_DATA_CB(name)                                        \
  static int name(http_parser *p, const char *at, size_t length) {       \
    Parser *parser = static_cast<Parser*>(p->data);                      \
    assert(current_buffer);                                              \
    Local<Value> cb_value = parser->handle_->Get(name##_sym);            \
    if (!cb_value->IsFunction()) return 0;                               \
    Local<Function> cb = Local<Function>::Cast(cb_value);                \
    Local<Value> argv[3] = { *current_buffer                             \
                           , Integer::New(at - current_buffer_data)      \
                           , Integer::New(length)                        \
                           };                                            \
    Local<Value> ret = cb->Call(parser->handle_, 3, argv);               \
    assert(current_buffer);                                              \
    if (ret.IsEmpty()) {                                                 \
      parser->got_exception_ = true;                                     \
      return -1;                                                         \
    } else {                                                             \
      return 0;                                                          \
    }                                                                    \
  }


static inline Persistent<String>
method_to_str(unsigned short m) {
  switch (m) {
    case HTTP_DELETE:     return delete_sym;
    case HTTP_GET:        return get_sym;
    case HTTP_HEAD:       return head_sym;
    case HTTP_POST:       return post_sym;
    case HTTP_PUT:        return put_sym;
    case HTTP_CONNECT:    return connect_sym;
    case HTTP_OPTIONS:    return options_sym;
    case HTTP_TRACE:      return trace_sym;
    case HTTP_COPY:       return copy_sym;
    case HTTP_LOCK:       return lock_sym;
    case HTTP_MKCOL:      return mkcol_sym;
    case HTTP_MOVE:       return move_sym;
    case HTTP_PROPFIND:   return propfind_sym;
    case HTTP_PROPPATCH:  return proppatch_sym;
    case HTTP_UNLOCK:     return unlock_sym;
    case HTTP_REPORT:     return report_sym;
    case HTTP_MKACTIVITY: return mkactivity_sym;
    case HTTP_CHECKOUT:   return checkout_sym;
    case HTTP_MERGE:      return merge_sym;
    case HTTP_MSEARCH:    return msearch_sym;
    case HTTP_NOTIFY:     return notify_sym;
    case HTTP_SUBSCRIBE:  return subscribe_sym;
    case HTTP_UNSUBSCRIBE:return unsubscribe_sym;
    default:              return unknown_method_sym;
  }
}


class Parser : public ObjectWrap {
 public:
  Parser(enum http_parser_type type) : ObjectWrap() {
    Init(type);
  }

  ~Parser() {
  }

  DEFINE_HTTP_CB(on_message_begin)
  DEFINE_HTTP_CB(on_message_complete)

  DEFINE_HTTP_DATA_CB(on_path)
  DEFINE_HTTP_DATA_CB(on_url)
  DEFINE_HTTP_DATA_CB(on_fragment)
  DEFINE_HTTP_DATA_CB(on_query_string)
  DEFINE_HTTP_DATA_CB(on_header_field)
  DEFINE_HTTP_DATA_CB(on_header_value)
  DEFINE_HTTP_DATA_CB(on_body)

  static int on_headers_complete(http_parser *p) {
    Parser *parser = static_cast<Parser*>(p->data);

    Local<Value> cb_value = parser->handle_->Get(on_headers_complete_sym);
    if (!cb_value->IsFunction()) return 0;
    Local<Function> cb = Local<Function>::Cast(cb_value);


    Local<Object> message_info = Object::New();

    // METHOD
    if (p->type == HTTP_REQUEST) {
      message_info->Set(method_sym, method_to_str(p->method));
    }

    // STATUS
    if (p->type == HTTP_RESPONSE) {
      message_info->Set(status_code_sym, Integer::New(p->status_code));
    }

    // VERSION
    message_info->Set(version_major_sym, Integer::New(p->http_major));
    message_info->Set(version_minor_sym, Integer::New(p->http_minor));

    message_info->Set(should_keep_alive_sym,
        http_should_keep_alive(p) ? True() : False());

    message_info->Set(upgrade_sym, p->upgrade ? True() : False());

    Local<Value> argv[1] = { message_info };

    Local<Value> head_response = cb->Call(parser->handle_, 1, argv);

    if (head_response.IsEmpty()) {
      parser->got_exception_ = true;
      return -1;
    } else {
      return head_response->IsTrue() ? 1 : 0;
    }
  }

  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;

    String::Utf8Value type(args[0]->ToString());

    Parser *parser;

    if (0 == strcasecmp(*type, "request")) {
      parser = new Parser(HTTP_REQUEST);
    } else if (0 == strcasecmp(*type, "response")) {
      parser = new Parser(HTTP_RESPONSE);
    } else {
      return ThrowException(Exception::Error(
            String::New("Constructor argument be 'request' or 'response'")));
    }

    parser->Wrap(args.This());

    return args.This();
  }

  // var bytesParsed = parser->execute(buffer, off, len);
  static Handle<Value> Execute(const Arguments& args) {
    HandleScope scope;

    Parser *parser = ObjectWrap::Unwrap<Parser>(args.This());

    assert(!current_buffer);
    assert(!current_buffer_data);

    if (current_buffer) {
      return ThrowException(Exception::TypeError(
            String::New("Already parsing a buffer")));
    }

    Local<Value> buffer_v = args[0];

    if (!Buffer::HasInstance(buffer_v)) {
      return ThrowException(Exception::TypeError(
            String::New("Argument should be a buffer")));
    }

    Local<Object> buffer_obj = buffer_v->ToObject();
    char *buffer_data = Buffer::Data(buffer_obj);
    size_t buffer_len = Buffer::Length(buffer_obj);

    size_t off = args[1]->Int32Value();
    if (off >= buffer_len) {
      return ThrowException(Exception::Error(
            String::New("Offset is out of bounds")));
    }

    size_t len = args[2]->Int32Value();
    if (off+len > buffer_len) {
      return ThrowException(Exception::Error(
            String::New("Length is extends beyond buffer")));
    }

    // Assign 'buffer_' while we parse. The callbacks will access that varible.
    current_buffer = &buffer_v;
    current_buffer_data = buffer_data;
    current_buffer_len = buffer_len;
    parser->got_exception_ = false;

    size_t nparsed =
      http_parser_execute(&parser->parser_, &settings, buffer_data + off, len);

    // Unassign the 'buffer_' variable
    assert(current_buffer);
    current_buffer = NULL;
    current_buffer_data = NULL;

    // If there was an exception in one of the callbacks
    if (parser->got_exception_) return Local<Value>();

    Local<Integer> nparsed_obj = Integer::New(nparsed);
    // If there was a parse error in one of the callbacks
    // TODO What if there is an error on EOF?
    if (!parser->parser_.upgrade && nparsed != len) {
      Local<Value> e = Exception::Error(String::NewSymbol("Parse Error"));
      Local<Object> obj = e->ToObject();
      obj->Set(String::NewSymbol("bytesParsed"), nparsed_obj);
      return scope.Close(e);
    } else {
      return scope.Close(nparsed_obj);
    }
  }

  static Handle<Value> Finish(const Arguments& args) {
    HandleScope scope;

    Parser *parser = ObjectWrap::Unwrap<Parser>(args.This());

    assert(!current_buffer);
    parser->got_exception_ = false;

    int rv = http_parser_execute(&(parser->parser_), &settings, NULL, 0);

    if (parser->got_exception_) return Local<Value>();

    if (rv != 0) {
      Local<Value> e = Exception::Error(String::NewSymbol("Parse Error"));
      Local<Object> obj = e->ToObject();
      obj->Set(String::NewSymbol("bytesParsed"), Integer::New(0));
      return scope.Close(e);
    }

    return Undefined();
  }

  static Handle<Value> Reinitialize(const Arguments& args) {
    HandleScope scope;
    Parser *parser = ObjectWrap::Unwrap<Parser>(args.This());

    String::Utf8Value type(args[0]->ToString());

    if (0 == strcasecmp(*type, "request")) {
      parser->Init(HTTP_REQUEST);
    } else if (0 == strcasecmp(*type, "response")) {
      parser->Init(HTTP_RESPONSE);
    } else {
      return ThrowException(Exception::Error(
            String::New("Argument be 'request' or 'response'")));
    }
    return Undefined();
  }


 private:

  void Init (enum http_parser_type type) {
    http_parser_init(&parser_, type);
    parser_.data = this;
  }

  bool got_exception_;
  http_parser parser_;
};


void InitHttpParser(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Parser::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("HTTPParser"));

  NODE_SET_PROTOTYPE_METHOD(t, "execute", Parser::Execute);
  NODE_SET_PROTOTYPE_METHOD(t, "finish", Parser::Finish);
  NODE_SET_PROTOTYPE_METHOD(t, "reinitialize", Parser::Reinitialize);

  target->Set(String::NewSymbol("HTTPParser"), t->GetFunction());

  on_message_begin_sym    = NODE_PSYMBOL("onMessageBegin");
  on_path_sym             = NODE_PSYMBOL("onPath");
  on_query_string_sym     = NODE_PSYMBOL("onQueryString");
  on_url_sym              = NODE_PSYMBOL("onURL");
  on_fragment_sym         = NODE_PSYMBOL("onFragment");
  on_header_field_sym     = NODE_PSYMBOL("onHeaderField");
  on_header_value_sym     = NODE_PSYMBOL("onHeaderValue");
  on_headers_complete_sym = NODE_PSYMBOL("onHeadersComplete");
  on_body_sym             = NODE_PSYMBOL("onBody");
  on_message_complete_sym = NODE_PSYMBOL("onMessageComplete");

  delete_sym = NODE_PSYMBOL("DELETE");
  get_sym = NODE_PSYMBOL("GET");
  head_sym = NODE_PSYMBOL("HEAD");
  post_sym = NODE_PSYMBOL("POST");
  put_sym = NODE_PSYMBOL("PUT");
  connect_sym = NODE_PSYMBOL("CONNECT");
  options_sym = NODE_PSYMBOL("OPTIONS");
  trace_sym = NODE_PSYMBOL("TRACE");
  copy_sym = NODE_PSYMBOL("COPY");
  lock_sym = NODE_PSYMBOL("LOCK");
  mkcol_sym = NODE_PSYMBOL("MKCOL");
  move_sym = NODE_PSYMBOL("MOVE");
  propfind_sym = NODE_PSYMBOL("PROPFIND");
  proppatch_sym = NODE_PSYMBOL("PROPPATCH");
  unlock_sym = NODE_PSYMBOL("UNLOCK");
  report_sym = NODE_PSYMBOL("REPORT");
  mkactivity_sym = NODE_PSYMBOL("MKACTIVITY");
  checkout_sym = NODE_PSYMBOL("CHECKOUT");
  merge_sym = NODE_PSYMBOL("MERGE");
  msearch_sym = NODE_PSYMBOL("M-SEARCH");
  notify_sym = NODE_PSYMBOL("NOTIFY");
  subscribe_sym = NODE_PSYMBOL("SUBSCRIBE");
  unsubscribe_sym = NODE_PSYMBOL("UNSUBSCRIBE");;
  unknown_method_sym = NODE_PSYMBOL("UNKNOWN_METHOD");

  method_sym = NODE_PSYMBOL("method");
  status_code_sym = NODE_PSYMBOL("statusCode");
  http_version_sym = NODE_PSYMBOL("httpVersion");
  version_major_sym = NODE_PSYMBOL("versionMajor");
  version_minor_sym = NODE_PSYMBOL("versionMinor");
  should_keep_alive_sym = NODE_PSYMBOL("shouldKeepAlive");
  upgrade_sym = NODE_PSYMBOL("upgrade");

  settings.on_message_begin    = Parser::on_message_begin;
  settings.on_path             = Parser::on_path;
  settings.on_query_string     = Parser::on_query_string;
  settings.on_url              = Parser::on_url;
  settings.on_fragment         = Parser::on_fragment;
  settings.on_header_field     = Parser::on_header_field;
  settings.on_header_value     = Parser::on_header_value;
  settings.on_headers_complete = Parser::on_headers_complete;
  settings.on_body             = Parser::on_body;
  settings.on_message_complete = Parser::on_message_complete;
}

}  // namespace node

NODE_MODULE(node_http_parser, node::InitHttpParser);
