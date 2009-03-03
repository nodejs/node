#include "node_http.h"

#include <oi_socket.h>
#include <ebb_request_parser.h>

#include <string>
#include <list>

#include <v8.h>

using namespace v8;
using namespace std;

static Persistent<ObjectTemplate> request_template;
static struct ev_loop *loop;

// globals
static Persistent<String> path_str; 
static Persistent<String> uri_str; 
static Persistent<String> query_string_str; 
static Persistent<String> fragment_str; 
static Persistent<String> method_str; 
static Persistent<String> http_version_str; 

static Persistent<String> on_request_str; 
static Persistent<String> on_body_str; 
static Persistent<String> respond_str; 

static Persistent<String> copy_str;
static Persistent<String> delete_str;
static Persistent<String> get_str;
static Persistent<String> head_str;
static Persistent<String> lock_str;
static Persistent<String> mkcol_str;
static Persistent<String> move_str;
static Persistent<String> options_str;
static Persistent<String> post_str;
static Persistent<String> propfind_str;
static Persistent<String> proppatch_str;
static Persistent<String> put_str;
static Persistent<String> trace_str;
static Persistent<String> unlock_str;

class Server {
public:
  Server (Handle<Object> _js_server);
  ~Server ();

  int Start(struct addrinfo *servinfo);
  void Stop();

  Handle<Value> Callback()
  {
    HandleScope scope;
    Handle<Value> value = js_server->Get(on_request_str);
    return scope.Close(value);
  }

private:
  oi_server server;
  Persistent<Object> js_server;
};

class Connection {
public:
  Connection ()
  {
    oi_socket_init (&socket, 30.0);
    ebb_request_parser_init (&parser);
  }
  ebb_request_parser parser;
  oi_socket socket;
  Persistent<Function> js_onRequest;

  friend class Server;
};

class HttpRequest {
 public:
  HttpRequest (Connection &c);
  ~HttpRequest();

  void MakeBodyCallback (const char *base, size_t length);
  Local<Object> CreateJSObject ();

  string path;
  string query_string;
  string fragment;
  string uri;

  list<string> header_fields;
  list<string> header_values;

  Connection &connection;
  ebb_request parser_info;
 private:
  Persistent<Object> js_object;
};

static Handle<Value>
GetMethodString (int method)
{
  switch(method) {
    case EBB_COPY:      return copy_str;
    case EBB_DELETE:    return delete_str;
    case EBB_GET:       return get_str;
    case EBB_HEAD:      return head_str;
    case EBB_LOCK:      return lock_str;
    case EBB_MKCOL:     return mkcol_str;
    case EBB_MOVE:      return move_str;
    case EBB_OPTIONS:   return options_str;
    case EBB_POST:      return post_str;
    case EBB_PROPFIND:  return propfind_str;
    case EBB_PROPPATCH: return proppatch_str;
    case EBB_PUT:       return put_str;
    case EBB_TRACE:     return trace_str;
    case EBB_UNLOCK:    return unlock_str;
  }
  return Null();
}

static Handle<Value>
RespondCallback (const Arguments& args) 
{
  HandleScope scope;

  Handle<External> field = Handle<External>::Cast(args.Holder()->GetInternalField(0));

  HttpRequest* request = static_cast<HttpRequest*>(field->Value());

  Handle<Value> arg = args[0];

  // TODO Make sure that we write reponses in the correct order. With
  // keep-alive it's possible that one response can return before the last
  // one has been sent!!!
  
  //printf("response called\n");

  if(arg == Null()) {

    //printf("response got null\n");
    delete request; 

  } else {

    Handle<String> s = arg->ToString();

    //printf("response called len %d\n", s->Length());

    oi_buf *buf = oi_buf_new2(s->Length());
    s->WriteAscii(buf->base, 0, s->Length());

    oi_socket_write(&request->connection.socket, buf);

  }

  return Undefined();
}

static void
on_path (ebb_request *req, const char *buf, size_t len)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);
  request->path.append(buf, len);
}

static void
on_uri (ebb_request *req, const char *buf, size_t len)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);
  request->uri.append(buf, len);
}

static void
on_query_string (ebb_request *req, const char *buf, size_t len)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);
  request->query_string.append(buf, len);
}

static void
on_fragment (ebb_request *req, const char *buf, size_t len)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);
  request->fragment.append(buf, len);
}

static void
on_header_field (ebb_request *req, const char *buf, size_t len, int header_index)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);

  if( request->header_fields.size() == header_index - 1) {
    request->header_fields.back().append(buf, len);
  } else { 
    request->header_fields.push_back( string(buf, len) );
  }
}

static void
on_header_value (ebb_request *req, const char *buf, size_t len, int header_index)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);

  if( request->header_values.size() == header_index - 1) {
    request->header_values.back().append(buf, len);
  } else { 
    request->header_values.push_back( string(buf, len) );
  }
}

static void
on_headers_complete (ebb_request *req)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);

  HandleScope scope;

  Local<Object> js_request = request->CreateJSObject();

  // Set up an exception handler before calling the Process function
  TryCatch try_catch;

  // Invoke the process function, giving the global object as 'this'
  // and one argument, the request.
  const int argc = 1;
  Handle<Value> argv[argc] = { js_request };
  Handle<Value> r = request->connection.js_onRequest->Call(Context::GetCurrent()->Global(), argc, argv);
  if (r.IsEmpty()) {
    String::Utf8Value error(try_catch.Exception());
    printf("error: %s\n", *error);
  }
}

static void
on_request_complete (ebb_request *req)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);
  request->MakeBodyCallback(NULL, 0); // EOF
}

static void
on_body (ebb_request *req, const char *base, size_t length)
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);

  if(length)
    request->MakeBodyCallback(base, length);
}

static ebb_request * on_request
  ( void *data
  ) 
{
  Connection *connection = static_cast<Connection*> (data);

  HttpRequest *request = new HttpRequest(*connection);
  
  return &request->parser_info;
}

static void on_read 
  ( oi_socket *socket
  , const void *buf
  , size_t count
  )
{
  Connection *connection = static_cast<Connection*> (socket->data);
  ebb_request_parser_execute ( &connection->parser
                             // FIXME change ebb to use void*
                             , static_cast<const char*> (buf) 
                             , count
                             );
  if(ebb_request_parser_has_error(&connection->parser)) {
    fprintf(stderr, "parse error closing connection\n");
    oi_socket_close(&connection->socket);
  }
}

static void on_close 
  ( oi_socket *socket
  )
{
  Connection *connection = static_cast<Connection*> (socket->data);
  // TODO free requests
  delete connection;
}

static void on_drain 
  ( oi_socket *socket
  )
{
  Connection *connection = static_cast<Connection*> (socket->data);
  //oi_socket_close(&connection->socket);
}

HttpRequest::~HttpRequest ()
{
  //printf("request is being destructed\n");

  connection.socket.on_drain = oi_socket_close;

  HandleScope scope;
  // delete a reference to the respond method
  js_object->Delete(respond_str);
  js_object.Dispose();
}

HttpRequest::HttpRequest (Connection &c) : connection(c)
{
  ebb_request_init(&parser_info); 
  parser_info.on_path             = on_path;
  parser_info.on_query_string     = on_query_string;
  parser_info.on_uri              = on_uri;
  parser_info.on_fragment         = on_fragment;
  parser_info.on_header_field     = on_header_field;
  parser_info.on_header_value     = on_header_value;
  parser_info.on_headers_complete = on_headers_complete;
  parser_info.on_body             = on_body;
  parser_info.on_complete         = on_request_complete;
  parser_info.data                = this;
}

void
HttpRequest::MakeBodyCallback (const char *base, size_t length)
{
  HandleScope handle_scope;
  // 
  // XXX don't always allocate onBody strings
  //
  Handle<Value> onBody_val = js_object->Get(on_body_str);  
  if (!onBody_val->IsFunction()) return;
  Handle<Function> onBody = Handle<Function>::Cast(onBody_val);

  TryCatch try_catch;
  const int argc = 1;
  Handle<Value> argv[argc];
  
  if(length) {
    // TODO use array for binary data
    Handle<String> chunk = String::New(base, length);
    argv[0] = chunk;
  } else {
    argv[0] = Null();
  }

  Handle<Value> result = onBody->Call(js_object, argc, argv);

  if (result.IsEmpty()) {
    String::Utf8Value error(try_catch.Exception());
    printf("error: %s\n", *error);
  }
}

Local<Object>
HttpRequest::CreateJSObject ()
{
  HandleScope scope;

  if (request_template.IsEmpty()) {
    Handle<ObjectTemplate> raw_template = ObjectTemplate::New();
    raw_template->SetInternalFieldCount(1);
    raw_template->Set(respond_str, FunctionTemplate::New(RespondCallback));

    request_template = Persistent<ObjectTemplate>::New(raw_template);
  }

  // Create an empty http request wrapper.
  Handle<Object> result = request_template->NewInstance();

  // Wrap the raw C++ pointer in an External so it can be referenced
  // from within JavaScript.
  Handle<External> request_ptr = External::New(this);

  // Store the request pointer in the JavaScript wrapper.
  result->SetInternalField(0, request_ptr);

  result->Set ( path_str
              , String::New(path.c_str(), path.length())
              );

  result->Set ( uri_str
              , String::New(uri.c_str(), uri.length())
              );

  result->Set ( query_string_str
              , String::New(query_string.c_str(), query_string.length())
              );

  result->Set ( fragment_str
              , String::New(fragment.c_str(), fragment.length())
              );

  result->Set ( method_str
              , GetMethodString(parser_info.method)
              );

  char version[10];
  snprintf ( version
           , 10 // big enough? :)
           , "%d.%d"
           , parser_info.version_major
           , parser_info.version_minor
           ); 
  result->Set ( http_version_str
              , String::New(version)
              );

  
  Handle<Object> headers = Object::New();
  list<string>::iterator field_iterator = header_fields.begin();
  list<string>::iterator value_iterator = header_values.begin();
  while( value_iterator != header_values.end() ) {
    string &f = *field_iterator;
    string &v = *value_iterator;
    
    headers->Set( String::New(f.c_str(), f.length() )
                , String::New(v.c_str(), v.length() ) 
                );

    field_iterator++;
    value_iterator++;
  }
  result->Set(String::NewSymbol("headers"), headers);


  js_object = Persistent<Object>::New(result);

  return scope.Close(result);
}


static oi_socket*
on_connection (oi_server *_server, struct sockaddr *addr, socklen_t len)
{
  HandleScope scope;

  Server *server = static_cast<Server*> (_server->data);

  Handle<Value> callback_v = server->Callback();

  if(callback_v == Undefined())
    return NULL;

  Connection *connection = new Connection();
    connection->socket.on_read    = on_read;
    connection->socket.on_error   = NULL;
    connection->socket.on_close   = on_close;
    connection->socket.on_timeout = NULL;
    connection->socket.on_drain   = on_drain;
    connection->socket.data       = connection;

    connection->parser.new_request = on_request;
    connection->parser.data        = connection;

  Handle<Function> f = Handle<Function>::Cast(callback_v);
  connection->js_onRequest = Persistent<Function>::New(f);

  return &connection->socket;
}

static void
server_destroy (Persistent<Value> _, void *data)
{
  Server *server = static_cast<Server *> (data);
  delete server;
}

Server::Server (Handle<Object> _js_server)
{
  oi_server_init(&server, 1024);
  server.on_connection = on_connection;
  server.data = this;
  HandleScope scope;
  js_server = Persistent<Object>::New (_js_server);
  // are we ever going to need this external?
  js_server->SetInternalField (0, External::New(this));
  js_server.MakeWeak (this, server_destroy);
}

Server::~Server ()
{
  Stop();
  js_server.Dispose();
  js_server.Clear(); // necessary? 
}

int
Server::Start(struct addrinfo *servinfo) 
{
  int r = oi_server_listen(&server, servinfo);
  if(r == 0)
    oi_server_attach(&server, loop);
  return r;
}

void
Server::Stop() 
{
  oi_server_close (&server);
  oi_server_detach (&server);
}

/* This constructor takes 2 arguments: host, port. */
static Handle<Value>
server_constructor (const Arguments& args) 
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  String::AsciiValue host(args[0]->ToString());
  String::AsciiValue port(args[1]->ToString());

  // get addrinfo for localhost, PORT
  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  int r = getaddrinfo(NULL, *port, &hints, &servinfo);
  if (r != 0)
    return Undefined(); // XXX raise error?

  //
  //
  //
  // TODO host is ignored for now assumed localhost
  //
  //
  //
  //

  Server *server = new Server(args.This());
  if(server == NULL)
    return Undefined(); // XXX raise error?

  r = server->Start(servinfo);
  if (r != 0)
    return Undefined(); // XXX raise error?

  printf("Running at http://localhost:%s/\n", *port);
 
  return args.This();
}

Handle<Object>
node_http_initialize (struct ev_loop *_loop)
{
  HandleScope scope;

  loop = _loop;

  Local<Object> http = Object::New();

  Local<FunctionTemplate> server_t = FunctionTemplate::New(server_constructor);
  server_t->InstanceTemplate()->SetInternalFieldCount(1);

  http->Set(String::New("Server"), server_t->GetFunction());

  path_str         = Persistent<String>::New( String::NewSymbol("path") );
  uri_str          = Persistent<String>::New( String::NewSymbol("uri") );
  query_string_str = Persistent<String>::New( String::NewSymbol("query_string") );
  fragment_str     = Persistent<String>::New( String::NewSymbol("fragment") );
  method_str       = Persistent<String>::New( String::NewSymbol("method") );
  http_version_str = Persistent<String>::New( String::NewSymbol("http_version") );

  on_request_str = Persistent<String>::New( String::NewSymbol("onRequest") );
  on_body_str    = Persistent<String>::New( String::NewSymbol("onBody") );
  respond_str    = Persistent<String>::New( String::NewSymbol("respond") );

  copy_str      = Persistent<String>::New( String::New("COPY") );
  delete_str    = Persistent<String>::New( String::New("DELETE") );
  get_str       = Persistent<String>::New( String::New("GET") );
  head_str      = Persistent<String>::New( String::New("HEAD") );
  lock_str      = Persistent<String>::New( String::New("LOCK") );
  mkcol_str     = Persistent<String>::New( String::New("MKCOL") );
  move_str      = Persistent<String>::New( String::New("MOVE") );
  options_str   = Persistent<String>::New( String::New("OPTIONS") );
  post_str      = Persistent<String>::New( String::New("POST") );
  propfind_str  = Persistent<String>::New( String::New("PROPFIND") );
  proppatch_str = Persistent<String>::New( String::New("PROPPATCH") );
  put_str       = Persistent<String>::New( String::New("PUT") );
  trace_str     = Persistent<String>::New( String::New("TRACE") );
  unlock_str    = Persistent<String>::New( String::New("UNLOCK") );

  return scope.Close(http);
}
