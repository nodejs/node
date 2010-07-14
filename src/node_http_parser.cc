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
static Persistent<String> unknown_method_sym;

static Persistent<String> method_sym;
static Persistent<String> status_code_sym;
static Persistent<String> http_version_sym;
static Persistent<String> version_major_sym;
static Persistent<String> version_minor_sym;
static Persistent<String> should_keep_alive_sym;
static Persistent<String> upgrade_sym;

static struct http_parser_settings settings;

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

    // Assign 'buffer_' while we parse. The callbacks will access that varible.
    parser->buffer_ = buffer;
    parser->got_exception_ = false;

    size_t nparsed =
      http_parser_execute(&parser->parser_, &settings, buffer->data()+off, len);

    // Unassign the 'buffer_' variable
    assert(parser->buffer_);
    parser->buffer_ = NULL;

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

    assert(!parser->buffer_);
    parser->got_exception_ = false;

    http_parser_execute(&(parser->parser_), &settings, NULL, 0);

    if (parser->got_exception_) return Local<Value>();

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

    parser_.data = this;
  }

  Buffer * buffer_;  // The buffer currently being parsed.
  bool got_exception_;
  http_parser parser_;
};


static Handle<Value> UrlDecode (const Arguments& args) {
  HandleScope scope;

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(
          String::New("First arg must be a string")));
  }

  bool decode_spaces = args[1]->IsTrue();

  String::Utf8Value in_v(args[0]->ToString());
  size_t l = in_v.length();
  char* out = strdup(*in_v);

  enum { CHAR, HEX0, HEX1 } state = CHAR;

  int n, m, hexchar;
  size_t in_index = 0, out_index = 0;
  char c;
  for (; in_index <= l; in_index++) {
    c = out[in_index];
    switch (state) {
      case CHAR:
        switch (c) {
          case '%':
            n = 0;
            m = 0;
            state = HEX0;
            break;
          case '+':
            if (decode_spaces) c = ' ';
            // pass thru
          default:
            out[out_index++] = c;
            break;
        }
        break;

      case HEX0:
        state = HEX1;
        hexchar = c;
        if ('0' <= c && c <= '9') {
          n = c - '0';
        } else if ('a' <= c && c <= 'f') {
          n = c - 'a' + 10;
        } else if ('A' <= c && c <= 'F') {
          n = c - 'A' + 10;
        } else {
          out[out_index++] = '%';
          out[out_index++] = c;
          state = CHAR;
          break;
        }
        break;

      case HEX1:
        state = CHAR;
        if ('0' <= c && c <= '9') {
          m = c - '0';
        } else if ('a' <= c && c <= 'f') {
          m = c - 'a' + 10;
        } else if ('A' <= c && c <= 'F') {
          m = c - 'A' + 10;
        } else {
          out[out_index++] = '%';
          out[out_index++] = hexchar;
          out[out_index++] = c;
          break;
        }
        out[out_index++] = 16*n + m;
        break;
    }
  }

  Local<String> out_v = String::New(out, out_index-1);
  free(out);
  return scope.Close(out_v);
}


void InitHttpParser(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Parser::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("HTTPParser"));

  NODE_SET_PROTOTYPE_METHOD(t, "execute", Parser::Execute);
  NODE_SET_PROTOTYPE_METHOD(t, "finish", Parser::Finish);
  NODE_SET_PROTOTYPE_METHOD(t, "reinitialize", Parser::Reinitialize);

  target->Set(String::NewSymbol("HTTPParser"), t->GetFunction());
  NODE_SET_METHOD(target, "urlDecode", UrlDecode);

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
