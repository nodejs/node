#include <node_http_parser.h>

#include <v8.h>
#include <node.h>
#include <node_buffer.h>

#include <http_parser.h>

#include <strings.h>  /* strcasecmp() */

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

 #include <execinfo.h>
 #include <stdio.h>
 #include <stdlib.h>
 
namespace node {

using namespace v8;

  static int deep = 0;

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
static Persistent<String> unknown_method_sym;

static Persistent<String> method_sym;
static Persistent<String> status_code_sym;
static Persistent<String> http_version_sym;
static Persistent<String> version_major_sym;
static Persistent<String> version_minor_sym;
static Persistent<String> should_keep_alive_sym;

// Callback prototype for http_cb
#define DEFINE_HTTP_CB(name)                                             \
  static int name(http_parser *p) {                                      \
    Parser *parser = static_cast<Parser*>(p->data);                      \
    Local<Value> cb_value = parser->handle_->Get(name##_sym);            \
    if (!cb_value->IsFunction()) return 0;                               \
    Local<Function> cb = Local<Function>::Cast(cb_value);                \
    Local<Value> ret = cb->Call(parser->handle_, 0, NULL);               \
    return ret.IsEmpty() ? -1 : 0;                                       \
  }

// Callback prototype for http_data_cb
#define DEFINE_HTTP_DATA_CB(name)                                        \
  static int name(http_parser *p, const char *at, size_t length) {       \
    Parser *parser = static_cast<Parser*>(p->data);                      \
    assert(parser->buffer_);                                             \
    Local<Value> cb_value = parser->handle_->Get(name##_sym);            \
    if (!cb_value->IsFunction()) return 0;                               \
    Local<Function> cb = Local<Function>::Cast(cb_value);                \
    Local<Value> argv[3] = { Local<Value>::New(parser->buffer_->handle_) \
                           , Integer::New(at - parser->buffer_->data())  \
                           , Integer::New(length)                        \
                           };                                            \
    Local<Value> ret = cb->Call(parser->handle_, 3, argv);               \
    assert(parser->buffer_);                                             \
    return ret.IsEmpty() ? -1 : 0;                                       \
  }


static inline Persistent<String>
method_to_str(enum http_method m) {
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
    default:              return unknown_method_sym;
  }
}


class Parser : public ObjectWrap {
 public:
  Parser(enum http_parser_type type) : ObjectWrap() {
    buffer_ = NULL;
    Init(type);
  }

  ~Parser() {
    assert(buffer_ == NULL && "Destroying a parser while it's parsing");
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

    Local<Value> argv[1] = { message_info };

    Local<Value> ret = cb->Call(parser->handle_, 1, argv);

    return ret.IsEmpty() ? -1 : 0;
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
    assert(!parser->buffer_);

    return args.This();
  }

  // var bytesParsed = parser->execute(buffer, off, len);
  static Handle<Value> Execute(const Arguments& args) {
    HandleScope scope;

    Parser *parser = ObjectWrap::Unwrap<Parser>(args.This());

    assert(!parser->buffer_);
    if (parser->buffer_) {
      return ThrowException(Exception::TypeError(
            String::New("Already parsing a buffer")));
    }

    if (!Buffer::HasInstance(args[0])) {
      return ThrowException(Exception::TypeError(
            String::New("Argument should be a buffer")));
    }

    Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

    size_t off = args[1]->Int32Value();
    if (off >= buffer->length()) {
      return ThrowException(Exception::Error(
            String::New("Offset is out of bounds")));
    }

    size_t len = args[2]->Int32Value();
    if (off+len > buffer->length()) {
      return ThrowException(Exception::Error(
            String::New("Length is extends beyond buffer")));
    }

    TryCatch try_catch;

    // Assign 'buffer_' while we parse. The callbacks will access that varible.
    parser->buffer_ = buffer;

    size_t nparsed =
      http_parser_execute(&parser->parser_, buffer->data()+off, len);

    // Unassign the 'buffer_' variable
    assert(parser->buffer_);
    parser->buffer_ = NULL;

    // If there was an exception in one of the callbacks
    if (try_catch.HasCaught()) return try_catch.ReThrow();

    Local<Integer> nparsed_obj = Integer::New(nparsed);
    // If there was a parse error in one of the callbacks 
    // TODO What if there is an error on EOF?
    if (nparsed != len) {
      Local<Value> e = Exception::Error(String::New("Parse Error"));
      Local<Object> obj = e->ToObject();
      obj->Set(String::NewSymbol("bytesParsed"), nparsed_obj);
      return ThrowException(e);
    }

    assert(!parser->buffer_);
    return scope.Close(nparsed_obj);
  }

  static Handle<Value> Finish(const Arguments& args) {
    HandleScope scope;

    Parser *parser = ObjectWrap::Unwrap<Parser>(args.This());

    assert(!parser->buffer_);

    http_parser_execute(&(parser->parser_), NULL, 0);

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
    assert(buffer_ == NULL); // don't call this during Execute()
    http_parser_init(&parser_, type);

    parser_.on_message_begin    = on_message_begin;
    parser_.on_path             = on_path;
    parser_.on_query_string     = on_query_string;
    parser_.on_url              = on_url;
    parser_.on_fragment         = on_fragment;
    parser_.on_header_field     = on_header_field;
    parser_.on_header_value     = on_header_value;
    parser_.on_headers_complete = on_headers_complete;
    parser_.on_body             = on_body;
    parser_.on_message_complete = on_message_complete;

    parser_.data = this;
  }

  Buffer * buffer_;  // The buffer currently being parsed.
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
  unknown_method_sym = NODE_PSYMBOL("UNKNOWN_METHOD");

  method_sym = NODE_PSYMBOL("method");
  status_code_sym = NODE_PSYMBOL("statusCode");
  http_version_sym = NODE_PSYMBOL("httpVersion");
  version_major_sym = NODE_PSYMBOL("versionMajor");
  version_minor_sym = NODE_PSYMBOL("versionMinor");
  should_keep_alive_sym = NODE_PSYMBOL("shouldKeepAlive");
}

}  // namespace node

