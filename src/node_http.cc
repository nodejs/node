#include <node_http.h>

#include <assert.h>
#include <stdio.h>
#include <strings.h>

#define METHOD_SYMBOL               String::NewSymbol("method")
#define STATUS_CODE_SYMBOL          String::NewSymbol("statusCode")
#define HTTP_VERSION_SYMBOL         String::NewSymbol("httpVersion")
#define SHOULD_KEEP_ALIVE_SYMBOL    String::NewSymbol("should_keep_alive")

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> HTTPConnection::client_constructor_template;
Persistent<FunctionTemplate> HTTPConnection::server_constructor_template;

void
HTTPConnection::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(NewClient);
  client_constructor_template = Persistent<FunctionTemplate>::New(t);
  client_constructor_template->Inherit(Connection::constructor_template);
  client_constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  client_constructor_template->SetClassName(String::NewSymbol("Client"));
  NODE_SET_PROTOTYPE_METHOD(client_constructor_template, "resetParser", ResetParser);
  target->Set(String::NewSymbol("Client"), client_constructor_template->GetFunction());

  t = FunctionTemplate::New(NewServer);
  server_constructor_template = Persistent<FunctionTemplate>::New(t);
  server_constructor_template->Inherit(Connection::constructor_template);
  server_constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  NODE_SET_PROTOTYPE_METHOD(server_constructor_template, "resetParser", ResetParser);
  server_constructor_template->SetClassName(String::NewSymbol("ServerSideConnection"));
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


Handle<Value> HTTPConnection::ResetParser(const Arguments& args) {
  HandleScope scope;
  HTTPConnection *connection = ObjectWrap::Unwrap<HTTPConnection>(args.Holder());
  connection->ResetParser();
  return Undefined();
}


void
HTTPConnection::OnReceive (const void *buf, size_t len)
{
  HandleScope scope;

  assert(attached_);
  size_t nparsed; 

  if (type_ == HTTP_REQUEST) {
    nparsed = http_parse_requests(&parser_, static_cast<const char*>(buf), len);
  } else {
    nparsed = http_parse_responses(&parser_, static_cast<const char*>(buf), len);
  }

  if (nparsed != len) {
    ForceClose();
  }
}

void
HTTPConnection::OnEOF ()
{
  HandleScope scope;
  assert(attached_);
  if (type_ == HTTP_REQUEST) {
    http_parse_requests(&parser_, NULL, 0);
  } else {
    http_parse_responses(&parser_, NULL, 0);
  }
  Emit("eof", 0, NULL);
}

int
HTTPConnection::on_message_begin (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->attached_);
  connection->Emit("messageBegin", 0, NULL);
  return 0;
}

int
HTTPConnection::on_message_complete (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->attached_);
  connection->Emit("messageComplete", 0, NULL);
  return 0;
}

int
HTTPConnection::on_url (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("url", 1, argv);
  return 0;
}

int
HTTPConnection::on_query_string (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("queryString", 1, argv);
  return 0;
}

int
HTTPConnection::on_path (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("path", 1, argv);
  return 0;
}

int
HTTPConnection::on_fragment (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("fragment", 1, argv);
  return 0;
}

const static char normalizer[] =
  "\0------------------------------"
  "-----------------0123456789-----"
  "--abcdefghijklmnopqrstuvwxyz----"
  "--abcdefghijklmnopqrstuvwxyz----"
  "--------------------------------"
  "--------------------------------"
  "--------------------------------"
  "--------------------------------";

int
HTTPConnection::on_header_field (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);

  // NORMALIZE
  size_t i;
  char *nonconstbuf = (char*)buf; // FIXME
  for (i = 0; i < len; i++) { nonconstbuf[i] = normalizer[buf[i]]; }

  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("headerField", 1, argv);
  return 0;
}

int
HTTPConnection::on_header_value (http_parser *parser, const char *buf, size_t len)
{
  HandleScope scope;
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->attached_);

  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit("headerValue", 1, argv);
  return 0;
}

static inline Local<String>
GetMethod (int method)
{
  const char *s;
  switch (method) {
    case HTTP_DELETE:     s = "DELETE"; break;
    case HTTP_GET:        s = "GET"; break;
    case HTTP_HEAD:       s = "HEAD"; break;
    case HTTP_POST:       s = "POST"; break;
    case HTTP_PUT:        s = "PUT"; break;
  }
  HandleScope scope;
  Local<String> method = String::NewSymbol(s);
  return scope.Close(method);
}

int
HTTPConnection::on_headers_complete (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->attached_);
  HandleScope scope;

  Local<Object> message_info = Object::New();

  // METHOD
  if (connection->type_ == HTTP_REQUEST) {
    message_info->Set(METHOD_SYMBOL, GetMethod(connection->parser_.method));
  }

  // STATUS
  if (connection->type_ == HTTP_RESPONSE) {
    message_info->Set(STATUS_CODE_SYMBOL,
        Integer::New(connection->parser_.status_code));
  }

  // VERSION
  char version[10];
  snprintf( version
          , 10
          , "%d.%d"
          , connection->parser_.http_major
          , connection->parser_.http_minor
          );
  message_info->Set(HTTP_VERSION_SYMBOL, String::New(version));

  message_info->Set(SHOULD_KEEP_ALIVE_SYMBOL,
      http_should_keep_alive(&connection->parser_) ? True() : False());

  Local<Value> argv[1] = { message_info };

  connection->Emit("headerComplete", 1, argv);

  return 0;
}

int
HTTPConnection::on_body (http_parser *parser, const char *buf, size_t len)
{
  assert(len != 0);

  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->attached_);
  HandleScope scope;

  // TODO each message should have their encoding.
  // don't look at the conneciton for encoding
  Local<Value> data = Encode(buf, len, connection->encoding_);
  connection->Emit("body", 1, &data);

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
  constructor_template->SetClassName(String::NewSymbol("Server"));
  target->Set(String::NewSymbol("Server"), constructor_template->GetFunction());
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

