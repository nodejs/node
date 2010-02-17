#include <node_http.h>

#include <assert.h>
#include <stdio.h>
#include <strings.h>

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> HTTPConnection::client_constructor_template;
Persistent<FunctionTemplate> HTTPConnection::server_constructor_template;

static Persistent<String> method_symbol;
static Persistent<String> status_code_symbol;
static Persistent<String> http_version_symbol;
static Persistent<String> version_major_symbol;
static Persistent<String> version_minor_symbol;
static Persistent<String> should_keep_alive_symbol;

static Persistent<String> message_begin_symbol;
static Persistent<String> message_complete_symbol;
static Persistent<String> url_symbol;
static Persistent<String> query_string_symbol;
static Persistent<String> path_symbol;
static Persistent<String> fragment_symbol;
static Persistent<String> header_field_symbol;
static Persistent<String> header_value_symbol;
static Persistent<String> header_complete_symbol;
static Persistent<String> body_symbol;
static Persistent<String> end_symbol;

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

  end_symbol = NODE_PSYMBOL("end");

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

  assert(refs_);
  size_t nparsed;

  nparsed = http_parser_execute(&parser_, static_cast<const char*>(buf), len);

  if (nparsed != len) {
    ForceClose();
  }
}

void
HTTPConnection::OnEOF ()
{
  HandleScope scope;
  assert(refs_);
  size_t nparsed;
  nparsed = http_parser_execute(&parser_, NULL, 0);
  Emit(end_symbol, 0, NULL);
}

int
HTTPConnection::on_message_begin (http_parser *parser)
{
  if (message_begin_symbol.IsEmpty()) {
    method_symbol = NODE_PSYMBOL("method");
    status_code_symbol = NODE_PSYMBOL("statusCode");
    http_version_symbol = NODE_PSYMBOL("httpVersion");
    version_major_symbol = NODE_PSYMBOL("versionMajor");
    version_minor_symbol = NODE_PSYMBOL("versionMinor");
    should_keep_alive_symbol = NODE_PSYMBOL("should_keep_alive");

    message_begin_symbol = NODE_PSYMBOL("messageBegin");
    message_complete_symbol = NODE_PSYMBOL("messageComplete");
    url_symbol = NODE_PSYMBOL("url");
    query_string_symbol = NODE_PSYMBOL("queryString");
    path_symbol = NODE_PSYMBOL("path");
    fragment_symbol = NODE_PSYMBOL("fragment");
    header_field_symbol = NODE_PSYMBOL("headerField");
    header_value_symbol = NODE_PSYMBOL("headerValue");
    header_complete_symbol = NODE_PSYMBOL("headerComplete");
    body_symbol = NODE_PSYMBOL("body");

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
  }

  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->refs_);
  connection->Emit(message_begin_symbol, 0, NULL);
  return 0;
}

int
HTTPConnection::on_message_complete (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->refs_);
  connection->Emit(message_complete_symbol, 0, NULL);
  return 0;
}

int
HTTPConnection::on_url (http_parser *parser, const char *buf, size_t len)
{
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->refs_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit(url_symbol, 1, argv);
  return 0;
}

int
HTTPConnection::on_query_string (http_parser *parser, const char *buf, size_t len)
{
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->refs_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit(query_string_symbol, 1, argv);
  return 0;
}

int
HTTPConnection::on_path (http_parser *parser, const char *buf, size_t len)
{
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->refs_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit(path_symbol, 1, argv);
  return 0;
}

int
HTTPConnection::on_fragment (http_parser *parser, const char *buf, size_t len)
{
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->refs_);
  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit(fragment_symbol, 1, argv);
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
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->refs_);

  // NORMALIZE
  size_t i;
  char *nonconstbuf = (char*)buf; // FIXME
  for (i = 0; i < len; i++) { nonconstbuf[i] = normalizer[buf[i]]; }

  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit(header_field_symbol, 1, argv);
  return 0;
}

int
HTTPConnection::on_header_value (http_parser *parser, const char *buf, size_t len)
{
  HTTPConnection *connection = static_cast<HTTPConnection*>(parser->data);
  assert(connection->refs_);

  Local<Value> argv[1] = { String::New(buf, len) };
  connection->Emit(header_value_symbol, 1, argv);
  return 0;
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

int
HTTPConnection::on_headers_complete (http_parser *parser)
{
  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->refs_);

  Local<Object> message_info = Object::New();

  // METHOD
  if (connection->type_ == HTTP_REQUEST) {
    message_info->Set(method_symbol, method_to_str(connection->parser_.method));
  }

  // STATUS
  if (connection->type_ == HTTP_RESPONSE) {
    message_info->Set(status_code_symbol,
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
  message_info->Set(http_version_symbol, String::New(version));
  message_info->Set(version_major_symbol,
                    Integer::New(connection->parser_.http_major));
  message_info->Set(version_minor_symbol,
                    Integer::New(connection->parser_.http_minor));

  message_info->Set(should_keep_alive_symbol,
      http_should_keep_alive(&connection->parser_) ? True() : False());

  Local<Value> argv[1] = { message_info };

  connection->Emit(header_complete_symbol, 1, argv);

  return 0;
}

int
HTTPConnection::on_body (http_parser *parser, const char *buf, size_t len)
{
  assert(len != 0);

  HTTPConnection *connection = static_cast<HTTPConnection*> (parser->data);
  assert(connection->refs_);

  // TODO each message should have their encoding.
  // don't look at the conneciton for encoding
  Local<Value> data = Encode(buf, len, connection->encoding_);
  connection->Emit(body_symbol, 1, &data);

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

