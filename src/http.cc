#include "node.h"
#include "http.h"
#include <http_parser.h>

#include <assert.h>
#include <stdio.h>

#define ON_MESSAGE_SYMBOL String::NewSymbol("onMessage")
#define MESSAGE_HANDLER_SYMBOL String::NewSymbol("messageHandler")
#define ON_HEADERS_COMPLETE_SYMBOL String::NewSymbol("onHeadersComplete")

#define PATH_SYMBOL String::NewSymbol("path")
#define STATUS_CODE_SYMBOL String::NewSymbol("status_code")
#define HTTP_VERSION_SYMBOL String::NewSymbol("http_version")

using namespace v8;
using namespace node;
using namespace std;


// Helper Functions

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

  Handle<Function> protocol = Handle<Function>::Cast(args[0]);

  int argc = args.Length();
  Handle<Value> argv[argc];
  
  argv[0] = args.This();
  for (int i = 1; i < args.Length(); i++) {
    argv[i] = args[i];
  }

  Local<Object> protocol_instance = protocol->NewInstance(argc, argv);

  // changeme the type should come from javascript
  new HTTPConnection(args.This(), protocol_instance, HTTP_RESPONSE);

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

int
HTTPConnection::on_path (http_parser *parser, const char *buf, size_t len)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);

  HandleScope scope;

  Local<Value> message_handler_v = 
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);
  fillField(message_handler_v, PATH_SYMBOL, String::New(buf, len));

  return 0;
}

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

  Local<Value> message_handler_v = 
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);
  Local<Object> message_handler = message_handler_v->ToObject();

  // STATUS 
  message_handler->Set(STATUS_CODE_SYMBOL, 
      Integer::New(connection->parser_.status_code));

  // VERSION
  char version[10];
  snprintf(version, 10, "%d.%d", connection->parser_.version_major, connection->parser_.version_minor); 
  message_handler->Set(HTTP_VERSION_SYMBOL, String::New(version));




  Local<Value> on_headers_complete_v = message_handler->Get(ON_HEADERS_COMPLETE_SYMBOL);
  if (on_headers_complete_v->IsFunction() == false) return 0;

  Handle<Function> on_headers_complete = Handle<Function>::Cast(on_headers_complete_v);


  on_headers_complete->Call(message_handler, 0, NULL);

  return 0;
}

HTTPConnection::HTTPConnection (Handle<Object> handle, Handle<Object> protocol, enum http_parser_type type)
  : Connection(handle, protocol) 
{
  http_parser_init (&parser_, type);
  parser_.on_message_begin    = on_message_begin;
  parser_.on_path             = on_path;
  parser_.on_query_string     = NULL;
  parser_.on_uri              = NULL;
  parser_.on_fragment         = NULL;
  parser_.on_header_field     = on_header_field;
  parser_.on_header_value     = on_header_value;
  parser_.on_headers_complete = on_headers_complete;
  parser_.on_body             = NULL;
  parser_.on_message_complete = NULL;
  parser_.data = this;
}

