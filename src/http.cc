#include "node.h"
#include "http.h"
#include <http_parser.h>

#include <assert.h>
#include <stdio.h>

#define ON_MESSAGE_SYMBOL String::NewSymbol("onMessage")
#define MESSAGE_HANDLER_SYMBOL String::NewSymbol("messageHandler")
#define ON_HEADERS_COMPLETE_SYMBOL String::NewSymbol("onHeadersComplete")
#define ON_BODY_SYMBOL String::NewSymbol("onBody")
#define ON_MESSAGE_COMPLETE_SYMBOL String::NewSymbol("onMessageComplete")

#define PATH_SYMBOL String::NewSymbol("path")
#define QUERY_STRING_SYMBOL String::NewSymbol("query_string")
#define URI_SYMBOL String::NewSymbol("uri")
#define FRAGMENT_SYMBOL String::NewSymbol("fragment")

#define STATUS_CODE_SYMBOL String::NewSymbol("status_code")
#define HTTP_VERSION_SYMBOL String::NewSymbol("http_version")

using namespace v8;
using namespace node;
using namespace std;


// Native Helper Functions

static Persistent<Function> _fill_field; 
static Persistent<Function> _append_header_field; 
static Persistent<Function> _append_header_value; 

#define CATCH_NATIVE_HTTP_FUNCTION(variable, jsname)                         \
do {                                                                         \
  if (variable.IsEmpty()) {                                                  \
    Local<Object> __g = Context::GetCurrent()->Global();                     \
    Local<Value> __node_v = __g->Get(String::NewSymbol("node"));             \
    Local<Object> __node = __node_v->ToObject();                             \
    Local<Value> __http_v = __node->Get(String::NewSymbol("http"));          \
    Local<Object> __http = __http_v->ToObject();                             \
    Local<Value> __value = __http->Get(String::NewSymbol(jsname));           \
    Handle<Function> __function_handle = Handle<Function>::Cast(__value);    \
    variable = Persistent<Function>::New(__function_handle);                 \
  }                                                                          \
} while(0)

void
fillField (Handle<Value> message, Handle<Value> field, Handle<Value> value)
{
  HandleScope scope;
  CATCH_NATIVE_HTTP_FUNCTION(_fill_field, "fillField");
  Handle<Value> argv[] = { message, field, value };
  _fill_field->Call(message->ToObject(), 3, argv);
}

void
appendHeaderField (Handle<Value> message, Handle<Value> d)
{
  HandleScope scope;
  CATCH_NATIVE_HTTP_FUNCTION(_append_header_field, "appendHeaderField");
  Handle<Value> argv[] = { message, d };
  _append_header_field->Call(message->ToObject(), 2, argv);
}

void
appendHeaderValue (Handle<Value> message, Handle<Value> d)
{
  HandleScope scope;
  CATCH_NATIVE_HTTP_FUNCTION(_append_header_value, "appendHeaderValue");
  Handle<Value> argv[] = { message, d };
  _append_header_value->Call(message->ToObject(), 2, argv);
}


void
HTTPConnection::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(HTTPConnection::v8New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("HTTPClient"), t->GetFunction());

  NODE_SET_METHOD(t->InstanceTemplate(), "connect", Connection::v8Connect);
  NODE_SET_METHOD(t->InstanceTemplate(), "close", Connection::v8Close);
  NODE_SET_METHOD(t->InstanceTemplate(), "send", Connection::v8Send);
  NODE_SET_METHOD(t->InstanceTemplate(), "sendEOF", Connection::v8SendEOF);
}

Handle<Value>
HTTPConnection::v8New (const Arguments& args)
{
  HandleScope scope;

  if (args[0]->IsFunction() == false)
    return ThrowException(String::New("Must pass a class as the first argument."));

  Local<Function> protocol_class = Local<Function>::Cast(args[0]);

  // changeme the type should come from javascript
  new HTTPConnection(args.This(), protocol_class, HTTP_RESPONSE);

  return args.This();
}

void
HTTPConnection::OnReceive (const void *buf, size_t len)
{
  http_parser_execute(&parser_, static_cast<const char*>(buf), len);

  if (http_parser_has_error(&parser_)) {
    // do something.
    Close();
    return;
  }
}

int
HTTPConnection::on_message_begin (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  HandleScope scope;

  Local<Object> protocol = connection->GetProtocol();
  Local<Value> on_message_v = protocol->Get(ON_MESSAGE_SYMBOL);
  if (!on_message_v->IsFunction()) return -1;
  Handle<Function> on_message = Handle<Function>::Cast(on_message_v);
  Local<Object> message_handler = on_message->NewInstance();
  connection->handle_->SetHiddenValue(MESSAGE_HANDLER_SYMBOL, message_handler);
  return 0;
}

#define DEFINE_FILL_VALUE_CALLBACK(callback_name, symbol)                         \
int                                                                               \
HTTPConnection::callback_name (http_parser *parser, const char *buf, size_t len)  \
{                                                                                 \
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);       \
  HandleScope scope;                                                              \
  Local<Value> message_handler_v =                                                \
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);                  \
  fillField(message_handler_v, symbol, String::New(buf, len));                    \
  return 0;                                                                       \
}
DEFINE_FILL_VALUE_CALLBACK(on_path, PATH_SYMBOL)
DEFINE_FILL_VALUE_CALLBACK(on_query_string, QUERY_STRING_SYMBOL)
DEFINE_FILL_VALUE_CALLBACK(on_uri, URI_SYMBOL)
DEFINE_FILL_VALUE_CALLBACK(on_fragment, FRAGMENT_SYMBOL)

int
HTTPConnection::on_header_field (http_parser *parser, const char *buf, size_t len)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  HandleScope scope;
  Local<Value> message_handler_v = 
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);
  appendHeaderField(message_handler_v, String::New(buf, len));
  return 0;
}

int
HTTPConnection::on_header_value (http_parser *parser, const char *buf, size_t len)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  HandleScope scope;
  Local<Value> message_handler_v = 
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);
  appendHeaderValue(message_handler_v, String::New(buf, len));
  return 0;
}

int
HTTPConnection::on_headers_complete (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  HandleScope scope;

  Local<Value> message_handler_v = 
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);
  Local<Object> message_handler = message_handler_v->ToObject();

  // STATUS 
  message_handler->Set(STATUS_CODE_SYMBOL, 
      Integer::New(connection->parser_.status_code));

  // VERSION
  char version[10];
  snprintf( version
          , 10
          , "%d.%d"
          , connection->parser_.version_major
          , connection->parser_.version_minor
          ); 
  message_handler->Set(HTTP_VERSION_SYMBOL, String::New(version));


  Local<Value> on_headers_complete_v = message_handler->Get(ON_HEADERS_COMPLETE_SYMBOL);
  if (on_headers_complete_v->IsFunction() == false) return 0;

  Handle<Function> on_headers_complete = Handle<Function>::Cast(on_headers_complete_v);


  on_headers_complete->Call(message_handler, 0, NULL);

  return 0;
}

int
HTTPConnection::on_body (http_parser *parser, const char *buf, size_t len)
{
  if(len == 0) return 0;

  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  HandleScope scope;

  Local<Value> message_handler_v = 
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);
  Local<Object> message_handler = message_handler_v->ToObject();

  Local<Value> on_body_v = message_handler->Get(ON_BODY_SYMBOL);
  if (on_body_v->IsFunction() == false) return 0;
  Handle<Function> on_body = Handle<Function>::Cast(on_body_v);

  Handle<Value> argv[1];

  // XXX whose encoding should we check? each message should have their own, 
  // probably. it ought to default to raw.
  if(connection->encoding_ == UTF8) {
    // utf8 encoding
    Handle<String> chunk = String::New((const char*)buf, len);
    argv[0] = chunk;

  } else {
    // raw encoding
    Local<Array> array = Array::New(len);
    for (size_t i = 0; i < len; i++) {
      char val = static_cast<const char*>(buf)[i];
      array->Set(Integer::New(i), Integer::New(val));
    }
    argv[0] = array;
  }
  on_body->Call(message_handler, 1, argv);
  return 0;
}

int
HTTPConnection::on_message_complete (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  HandleScope scope;

  Local<Value> message_handler_v = 
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);
  Local<Object> message_handler = message_handler_v->ToObject();

  Local<Value> on_msg_complete_v = message_handler->Get(ON_MESSAGE_COMPLETE_SYMBOL);
  if (on_msg_complete_v->IsFunction()) {
    Handle<Function> on_msg_complete = Handle<Function>::Cast(on_msg_complete_v);
    on_msg_complete->Call(message_handler, 0, NULL);
  }
  connection->handle_->DeleteHiddenValue(MESSAGE_HANDLER_SYMBOL);
  return 0;
}

HTTPConnection::HTTPConnection (Handle<Object> handle, Handle<Function> protocol_class, enum http_parser_type type)
  : Connection(handle, protocol_class) 
{
  http_parser_init (&parser_, type);
  parser_.on_message_begin    = on_message_begin;
  parser_.on_path             = on_path;
  parser_.on_query_string     = on_query_string;
  parser_.on_uri              = on_uri;
  parser_.on_fragment         = on_fragment;
  parser_.on_header_field     = on_header_field;
  parser_.on_header_value     = on_header_value;
  parser_.on_headers_complete = on_headers_complete;
  parser_.on_body             = on_body;
  parser_.on_message_complete = on_message_complete;
  parser_.data = this;
}

