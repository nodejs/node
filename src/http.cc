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

#define METHOD_SYMBOL               String::NewSymbol("method")
#define STATUS_CODE_SYMBOL          String::NewSymbol("statusCode")
#define HTTP_VERSION_SYMBOL         String::NewSymbol("httpVersion")
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

  Local<FunctionTemplate> t = FunctionTemplate::New(NewClient);
  client_constructor_template = Persistent<FunctionTemplate>::New(t);
  client_constructor_template->Inherit(Connection::constructor_template);
  client_constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("LowLevelClient"), client_constructor_template->GetFunction());

  t = FunctionTemplate::New(NewServer);
  server_constructor_template = Persistent<FunctionTemplate>::New(t);
  server_constructor_template->Inherit(Connection::constructor_template);
  server_constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("ServerSideSocket"),
              server_constructor_template->GetFunction());
}

Handle<Value>
HTTPConnection::NewClient (const Arguments& args)
{
  HandleScope scope;

  HTTPConnection *connection = new HTTPConnection(HTTP_RESPONSE);
  connection->Wrap(args.This());

  return args.This();
}

Handle<Value>
HTTPConnection::NewServer (const Arguments& args)
{
  HandleScope scope;

  HTTPConnection *connection = new HTTPConnection(HTTP_REQUEST);
  connection->Wrap(args.This());

  return args.This();
}

void
HTTPConnection::OnReceive (const void *buf, size_t len)
{
  assert(attached_);
  http_parser_execute(&parser_, static_cast<const char*>(buf), len);
  if (http_parser_has_error(&parser_)) ForceClose();
}

int
HTTPConnection::on_message_begin (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->attached_);
  connection->Emit("message_begin", 0, NULL);
  return 0;
}

int
HTTPConnection::on_message_complete (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->attached_);
  connection->Emit("message_complete", 0, NULL);
  return 0;
}

int
HTTPConnection::on_uri (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("uri", 1, argv);
  return 0;
}

int
HTTPConnection::on_header_field (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("header_field", 1, argv);
  return 0;
}

int
HTTPConnection::on_header_value (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("header_value", 1, argv);
  return 0;
}

static inline Local<String>
GetMethod (int method)
{
  switch (method) {
    case HTTP_COPY:       return String::NewSymbol("COPY");
    case HTTP_DELETE:     return String::NewSymbol("DELETE");
    case HTTP_GET:        return String::NewSymbol("GET");
    case HTTP_HEAD:       return String::NewSymbol("HEAD");
    case HTTP_LOCK:       return String::NewSymbol("LOCK");
    case HTTP_MKCOL:      return String::NewSymbol("MKCOL");
    case HTTP_MOVE:       return String::NewSymbol("MOVE");
    case HTTP_OPTIONS:    return String::NewSymbol("OPTIONS");
    case HTTP_POST:       return String::NewSymbol("POST");
    case HTTP_PROPFIND:   return String::NewSymbol("PROPFIND");
    case HTTP_PROPPATCH:  return String::NewSymbol("PROPPATCH");
    case HTTP_PUT:        return String::NewSymbol("PUT");
    case HTTP_TRACE:      return String::NewSymbol("TRACE");
    case HTTP_UNLOCK:     return String::NewSymbol("UNLOCK");
  }
  return Local<String>();
}

int
HTTPConnection::on_headers_complete (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->attached_);
  HandleScope scope;

  Local<Object> message_info = Object::New();

  // METHOD 
  if (connection->parser_.type == HTTP_REQUEST)
    message_info->Set(METHOD_SYMBOL, GetMethod(connection->parser_.method));

  // STATUS 
  if (connection->parser_.type == HTTP_RESPONSE)
    message_info->Set(STATUS_CODE_SYMBOL, 
        Integer::New(connection->parser_.status_code));

  // VERSION
  char version[10];
  snprintf( version
          , 10
          , "%d.%d"
          , connection->parser_.version_major
          , connection->parser_.version_minor
          ); 
  message_info->Set(HTTP_VERSION_SYMBOL, String::New(version));

  message_info->Set(SHOULD_KEEP_ALIVE_SYMBOL, 
      http_parser_should_keep_alive(&connection->parser_) ? True() : False());

  Local<Value> argv[1] = { message_info };

  connection->Emit("headers_complete", 1, argv);

  return 0;
}

int
HTTPConnection::on_body (http_parser *parser, const char *buf, size_t len)
{
  assert(len != 0);

  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->attached_);
  HandleScope scope;

  Handle<Value> argv[1];

  // TODO each message should have their encoding. 
  // don't look at the conneciton for encoding

  if (connection->encoding_ == UTF8) {
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

  connection->Emit("body", 1, argv);

  return 0;
}

Persistent<FunctionTemplate> HTTPServer::constructor_template;

void
HTTPServer::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(Server::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("LowLevelServer"), constructor_template->GetFunction());
}

Handle<Value>
HTTPServer::New (const Arguments& args)
{
  HandleScope scope;

  HTTPServer *server = new HTTPServer();
  server->Wrap(args.This());

  return args.This();
}

Handle<FunctionTemplate>
HTTPServer::GetConnectionTemplate (void)
{
  return HTTPConnection::server_constructor_template;
}

Connection*
HTTPServer::UnwrapConnection (Local<Object> connection)
{
  HandleScope scope;
  return ObjectWrap::Unwrap<HTTPConnection>(connection);
}

