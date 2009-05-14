#include "node.h"
#include "http.h"
#include <http_parser.h>

#include <assert.h>
#include <stdio.h>
#include <strings.h>

#define ENCODING_SYMBOL             String::NewSymbol("encoding")

#define MESSAGE_HANDLER_SYMBOL      String::NewSymbol("messageHandler")

#define ON_MESSAGE_SYMBOL           String::NewSymbol("onMessage")
#define ON_PATH_SYMBOL              String::NewSymbol("onPath")
#define ON_QUERY_STRING_SYMBOL      String::NewSymbol("onQueryString")
#define ON_URI_SYMBOL               String::NewSymbol("onURI")
#define ON_FRAGMENT_SYMBOL          String::NewSymbol("onFragment")
#define ON_HEADER_FIELD_SYMBOL      String::NewSymbol("onHeaderField")
#define ON_HEADER_VALUE_SYMBOL      String::NewSymbol("onHeaderValue")
#define ON_HEADERS_COMPLETE_SYMBOL  String::NewSymbol("onHeadersComplete")
#define ON_BODY_SYMBOL              String::NewSymbol("onBody")
#define ON_MESSAGE_COMPLETE_SYMBOL  String::NewSymbol("onMessageComplete")

#define STATUS_CODE_SYMBOL          String::NewSymbol("status_code")
#define HTTP_VERSION_SYMBOL         String::NewSymbol("http_version")
#define SHOULD_KEEP_ALIVE_SYMBOL    String::NewSymbol("should_keep_alive")

using namespace v8;
using namespace node;
using namespace std;

Persistent<FunctionTemplate> HTTPConnection::client_constructor_template;
Persistent<FunctionTemplate> HTTPConnection::server_constructor_template;

static Persistent<Object> http_module;

void
HTTPConnection::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(v8NewClient);
  client_constructor_template = Persistent<FunctionTemplate>::New(t);
  client_constructor_template->Inherit(Connection::constructor_template);
  client_constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("Client"), client_constructor_template->GetFunction());

  t = FunctionTemplate::New(v8NewServer);
  server_constructor_template = Persistent<FunctionTemplate>::New(t);
  server_constructor_template->Inherit(Connection::constructor_template);
  server_constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("ServerSideSocket"),
              server_constructor_template->GetFunction());
}

Handle<Value>
HTTPConnection::v8NewClient (const Arguments& args)
{
  HandleScope scope;
  if (args[0]->IsFunction() == false)
    return ThrowException(String::New("Must pass a class as the first argument."));
  Local<Function> protocol_class = Local<Function>::Cast(args[0]);
  new HTTPConnection(args.This(), HTTP_RESPONSE);
  return args.This();
}

Handle<Value>
HTTPConnection::v8NewServer (const Arguments& args)
{
  HandleScope scope;
  if (args[0]->IsFunction() == false)
    return ThrowException(String::New("Must pass a class as the first argument."));
  Local<Function> protocol_class = Local<Function>::Cast(args[0]);
  new HTTPConnection(args.This(), HTTP_REQUEST);
  return args.This();
}

void
HTTPConnection::OnReceive (const void *buf, size_t len)
{
  http_parser_execute(&parser_, static_cast<const char*>(buf), len);

  if (http_parser_has_error(&parser_))
    ForceClose();
}

int
HTTPConnection::on_message_begin (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  HandleScope scope;

  Local<Value> on_message_v = connection->handle_->Get(ON_MESSAGE_SYMBOL);
  if (!on_message_v->IsFunction()) return -1;
  Handle<Function> on_message = Handle<Function>::Cast(on_message_v);

  TryCatch try_catch;
  Local<Object> message_handler = on_message->NewInstance();
  if (try_catch.HasCaught()) {
    fatal_exception(try_catch);
    return -1;
  }

  connection->handle_->SetHiddenValue(MESSAGE_HANDLER_SYMBOL, message_handler);
  return 0;
}

#define DEFINE_PARSER_CALLBACK(name, symbol)                                  \
int                                                                           \
HTTPConnection::name (http_parser *parser, const char *buf, size_t len)       \
{                                                                             \
  HandleScope scope;                                                          \
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);   \
  Local<Value> message_handler_v =                                            \
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);              \
  if (message_handler_v->IsObject() == false)                                 \
    return -1;                                                                \
  Local<Object> message_handler = message_handler_v->ToObject();              \
  Local<Value> callback_v = message_handler->Get(symbol);                     \
  if (callback_v->IsFunction() == false)                                      \
    return 0;                                                                 \
  Local<Function> callback = Local<Function>::Cast(callback_v);               \
  TryCatch try_catch;                                                         \
  Local<Value> argv[1] = { String::New(buf, len) };                           \
  Local<Value> ret = callback->Call(message_handler, 1, argv);                \
  if (ret.IsEmpty()) {                                                        \
    fatal_exception(try_catch);                                               \
    return -2;                                                                \
  }                                                                           \
  if (ret->IsFalse()) return -3;                                              \
  return 0;                                                                   \
}

DEFINE_PARSER_CALLBACK(on_path,         ON_PATH_SYMBOL)
DEFINE_PARSER_CALLBACK(on_query_string, ON_QUERY_STRING_SYMBOL)
DEFINE_PARSER_CALLBACK(on_uri,          ON_URI_SYMBOL)
DEFINE_PARSER_CALLBACK(on_fragment,     ON_FRAGMENT_SYMBOL)
DEFINE_PARSER_CALLBACK(on_header_field, ON_HEADER_FIELD_SYMBOL)
DEFINE_PARSER_CALLBACK(on_header_value, ON_HEADER_VALUE_SYMBOL)

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

  // SHOULD KEEP ALIVE
  message_handler->Set( SHOULD_KEEP_ALIVE_SYMBOL
                      , http_parser_should_keep_alive(&connection->parser_) ? True() : False()
                      );


  Local<Value> on_headers_complete_v = message_handler->Get(ON_HEADERS_COMPLETE_SYMBOL);
  if (on_headers_complete_v->IsFunction() == false) return 0;

  Handle<Function> on_headers_complete = Handle<Function>::Cast(on_headers_complete_v);

  TryCatch try_catch;
  Local<Value> ret = on_headers_complete->Call(message_handler, 0, NULL);
  if (ret.IsEmpty()) {
    fatal_exception(try_catch);
    return -2;
  }
  if (ret->IsFalse()) return -3;

  return 0;
}

int
HTTPConnection::on_body (http_parser *parser, const char *buf, size_t len)
{
  assert(len != 0);

  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  HandleScope scope;

  Local<Value> message_handler_v = 
    connection->handle_->GetHiddenValue(MESSAGE_HANDLER_SYMBOL);
  Local<Object> message_handler = message_handler_v->ToObject();

  Local<Value> on_body_v = message_handler->Get(ON_BODY_SYMBOL);
  if (on_body_v->IsFunction() == false) return 0;
  Handle<Function> on_body = Handle<Function>::Cast(on_body_v);

  /* Look at the value of message_handler.encoding to decide how to
   * send the body chunk. This is rather sloppy and unnecesary. FIXME
   */
  enum encoding encoding = RAW;
  Local<Value> encoding_v = message_handler->Get(ENCODING_SYMBOL);
  if (encoding_v->IsString()) {
    Local<String> encoding_string = encoding_v->ToString();
    char buf[5]; // need enough room for "utf8" or "raw"
    encoding_string->WriteAscii(buf, 0, 4);
    buf[4] = '\0';
    if(strcasecmp(buf, "utf8") == 0)
      encoding = UTF8;
  }

  Handle<Value> argv[1];
  // TODO each message should have their encoding. 
  // don't look at the conneciton for encoding
  if(encoding == UTF8) {
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

  TryCatch try_catch;
  Local<Value> ret = on_body->Call(message_handler, 1, argv);
  if (ret.IsEmpty()) {
    fatal_exception(try_catch);
    return -2;
  }
  if (ret->IsFalse()) return -3;

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
  connection->handle_->DeleteHiddenValue(MESSAGE_HANDLER_SYMBOL);

  Local<Value> on_msg_complete_v = message_handler->Get(ON_MESSAGE_COMPLETE_SYMBOL);
  if (on_msg_complete_v->IsFunction() == false) 
    return 0;
  Handle<Function> on_msg_complete = Handle<Function>::Cast(on_msg_complete_v);

  TryCatch try_catch;
  Local<Value> ret = on_msg_complete->Call(message_handler, 0, NULL);
  if (ret.IsEmpty()) {
    fatal_exception(try_catch);
    return -2;
  }
  if (ret->IsFalse()) return -3;

  return 0;
}

HTTPConnection::HTTPConnection (Handle<Object> handle, enum http_parser_type type)
  : Connection(handle) 
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

Persistent<FunctionTemplate> HTTPServer::constructor_template;

void
HTTPServer::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(v8New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(Acceptor::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("LowLevelServer"), constructor_template->GetFunction());
}

Handle<Value>
HTTPServer::v8New (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1 || args[0]->IsFunction() == false)
    return ThrowException(String::New("Must at give connection handler as the first argument"));

  Local<Function> protocol_class = Local<Function>::Cast(args[0]);
  Local<Object> options;

  if (args.Length() > 1 && args[1]->IsObject()) {
    options = args[1]->ToObject();
  } else {
    options = Object::New();
  }

  new HTTPServer(args.This(), protocol_class, options);

  return args.This();
}

Connection*
HTTPServer::OnConnection (struct sockaddr *addr, socklen_t len)
{
  HandleScope scope;
  
  Local<Function> connection_handler = GetConnectionHandler ();
  if (connection_handler.IsEmpty()) {
    Close();
    return NULL;
  }

  TryCatch try_catch;

  Local<Object> connection_handle =
    HTTPConnection::server_constructor_template->GetFunction()->NewInstance(0, NULL);

  if (connection_handle.IsEmpty()) {
    fatal_exception(try_catch);
    return NULL;
  }

  HTTPConnection *connection = NODE_UNWRAP(HTTPConnection, connection_handle);
  if (!connection) return NULL;

  connection->SetAcceptor(handle_);

  return connection;
}

