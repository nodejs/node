#include "net.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace v8;
using namespace node;

#define ON_RECEIVE_SYMBOL     String::NewSymbol("onReceive")
#define ON_DISCONNECT_SYMBOL  String::NewSymbol("onDisconnect")
#define ON_CONNECT_SYMBOL     String::NewSymbol("onConnect")
#define ON_DRAIN_SYMBOL       String::NewSymbol("onDrain")
#define ON_TIMEOUT_SYMBOL     String::NewSymbol("onTimeout")
#define ON_ERROR_SYMBOL       String::NewSymbol("onError")
#define ON_EOF_SYMBOL         String::NewSymbol("onEOF")

#define SEND_SYMBOL           String::NewSymbol("send")
#define DISCONNECT_SYMBOL     String::NewSymbol("disconnect")
#define CONNECT_SYMBOL        String::NewSymbol("connect")
#define ENCODING_SYMBOL       String::NewSymbol("encoding")
#define TIMEOUT_SYMBOL        String::NewSymbol("timeout")

#define PROTOCOL_SYMBOL String::NewSymbol("protocol")

static const struct addrinfo tcp_hints = 
/* ai_flags      */ { AI_PASSIVE
/* ai_family     */ , AF_UNSPEC
/* ai_socktype   */ , SOCK_STREAM
/* ai_protocol   */ , 0
/* ai_addrlen    */ , 0
/* ai_addr       */ , NULL
/* ai_canonname  */ , NULL
/* ai_next       */ , NULL
                    };

Persistent<Function> tcp_connection_constructor;

void 
Connection::Initialize (v8::Handle<v8::Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Connection::v8New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("TCPConnection"), t->GetFunction());

  tcp_connection_constructor = Persistent<Function>::New(t->GetFunction());

  NODE_SET_METHOD(t->InstanceTemplate(), "connect", Connection::v8Connect);
  NODE_SET_METHOD(t->InstanceTemplate(), "close", Connection::v8Close);
  NODE_SET_METHOD(t->InstanceTemplate(), "send", Connection::v8Send);
  NODE_SET_METHOD(t->InstanceTemplate(), "sendEOF", Connection::v8SendEOF);
}

Connection::Connection (Handle<Object> handle, Handle<Object> protocol)
  : ObjectWrap(handle) 
{
  HandleScope scope;

  handle_->Set(PROTOCOL_SYMBOL, protocol);

  encoding_ = RAW;
  Local<Value> encoding_v = protocol->Get(ENCODING_SYMBOL);
  if (encoding_v->IsString()) {
    Local<String> encoding_string = encoding_v->ToString();
    char buf[5]; // need enough room for "utf8" or "raw"
    encoding_string->WriteAscii(buf, 0, 4);
    buf[4] = '\0';
    if(strcasecmp(buf, "utf8") == 0) encoding_ = UTF8;
  }

  double timeout = 0.0; // default
  Local<Value> timeout_v = protocol->Get(TIMEOUT_SYMBOL);
  if (encoding_v->IsInt32())
    timeout = timeout_v->Int32Value() / 1000.0;

  host_ = NULL;
  port_ = NULL;

  oi_socket_init(&socket_, timeout);
  socket_.on_connect = Connection::on_connect;
  socket_.on_read    = Connection::on_read;
  socket_.on_drain   = Connection::on_drain;
  socket_.on_error   = Connection::on_error;
  socket_.on_close   = Connection::on_close;
  socket_.on_timeout = Connection::on_timeout;
  socket_.data = this;
}

Local<Object>
Connection::GetProtocol (void)
{
  HandleScope scope;

  Local<Value> protocol_v = handle_->Get(PROTOCOL_SYMBOL);
  if (protocol_v->IsObject()) {
    Local<Object> protocol = protocol_v->ToObject();
    return scope.Close(protocol);
  }

  return Local<Object>();
}

Handle<Value>
Connection::v8New (const Arguments& args)
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

  new Connection(args.This(), protocol_instance);

  return args.This();
}

Handle<Value>
Connection::v8Connect (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());

  if (args.Length() == 0 || args[0]->IsInt32() == false)
    return ThrowException(String::New("Must specify a port."));

  String::AsciiValue port_sv(args[0]->ToString());
  assert(connection->port_ == NULL);
  connection->port_ = strdup(*port_sv);

  assert(connection->host_ == NULL);
  if (args.Length() > 1 && args[1]->IsString()) {
    String::Utf8Value host_sv(args[1]->ToString());
    connection->host_ = strdup(*host_sv);
  }
  
  /* For the moment I will do DNS lookups in the eio thread pool. This is
   * sub-optimal and cannot handle massive numbers of requests but it is 
   * quite portable. 
   * In the future I will move to a system using adns or udns: 
   * http://lists.schmorp.de/pipermail/libev/2009q1/000632.html
   */
  eio_warmup();
  eio_custom( Connection::Resolve
            , EIO_PRI_DEFAULT
            , Connection::AfterResolve
            , connection
            );
  return Undefined();
}

int
Connection::Resolve (eio_req *req)
{
  Connection *connection = static_cast<Connection*> (req->data);
  struct addrinfo *address = NULL;
  req->result = getaddrinfo(connection->host_, connection->port_, &tcp_hints, &address);
  req->ptr2 = address;

  free(connection->host_);
  connection->host_ = NULL;

  free(connection->port_);
  connection->port_ = NULL;

  return 0;
}

int
Connection::AfterResolve (eio_req *req)
{
  Connection *connection = static_cast<Connection*> (req->data);
  struct addrinfo *address = static_cast<struct addrinfo *>(req->ptr2);

  req->ptr2 = NULL;

  int r = 0;
  if (req->result == 0) {
    r = connection->Connect(address);
  }

  if (address) freeaddrinfo(address); 

  // no error. return.
  if (r == 0 && req->result == 0) {
    oi_socket_attach (EV_DEFAULT_UC_ &connection->socket_);
    return 0;
  }

  // return error?

  return r | req->result;
}

Handle<Value>
Connection::v8Close (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());
  connection->Close();
  return Undefined();
}

Handle<Value>
Connection::v8Send (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());

  if (args[0] == Null()) {
    oi_socket_write_eof(&connection->socket_);

  } else if (args[0]->IsString()) {
    // utf8 encoding
    Local<String> s = args[0]->ToString();
    size_t length = s->Utf8Length();
    oi_buf *buf = oi_buf_new2(length);
    s->WriteUtf8(buf->base, length);
    connection->Send(buf);

  } else if (args[0]->IsArray()) {
    // raw encoding
    Handle<Array> array = Handle<Array>::Cast(args[0]);
    size_t length = array->Length();
    oi_buf *buf = oi_buf_new2(length);
    for (size_t i = 0; i < length; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf->base[i] = int_value->IntegerValue();
    }
    connection->Send(buf);

  } else return ThrowException(String::New("Bad argument"));

  return Undefined();  
}

Handle<Value>
Connection::v8SendEOF (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());
  connection->SendEOF();
  return Undefined();
}

void 
Connection::OnReceive (const void *buf, size_t len)
{
  HandleScope scope;

  Local<Object> protocol = GetProtocol();
  Handle<Value> callback_v = protocol->Get(ON_RECEIVE_SYMBOL);
  if (!callback_v->IsFunction()) return; 

  Handle<Function> callback = Handle<Function>::Cast(callback_v);

  const int argc = 1;
  Handle<Value> argv[argc];

  if(len) {
    if(encoding_ == UTF8) {
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
  } else {
    argv[0] = Local<Value>::New(Null());  
  }

  TryCatch try_catch;
  callback->Call(protocol, argc, argv);

  if (try_catch.HasCaught())
    fatal_exception(try_catch); // XXX is this the right action to take?
}

void
Connection::OnError (oi_error e)
{
}

#define DEFINE_SIMPLE_CALLBACK(name, symbol) \
void name () \
{ \
  HandleScope scope; \
  Local<Object> protocol = GetProtocol();\
  Local<Value> callback_v = protocol->Get(symbol); \
  if (!callback_v->IsFunction()) return; \
  Handle<Function> callback = Handle<Function>::Cast(callback_v); \
  callback->Call(protocol, 0, NULL); \
}

DEFINE_SIMPLE_CALLBACK(Connection::OnConnect, ON_CONNECT_SYMBOL)
DEFINE_SIMPLE_CALLBACK(Connection::OnDrain, ON_DRAIN_SYMBOL)
DEFINE_SIMPLE_CALLBACK(Connection::OnDisconnect, ON_DISCONNECT_SYMBOL)
DEFINE_SIMPLE_CALLBACK(Connection::OnTimeout, ON_TIMEOUT_SYMBOL)
DEFINE_SIMPLE_CALLBACK(Connection::OnEOF, ON_EOF_SYMBOL)

void
Acceptor::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> tcp_server_template =
    FunctionTemplate::New(Acceptor::v8New);
  tcp_server_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("TCPServer"), tcp_server_template->GetFunction());

  NODE_SET_METHOD( tcp_server_template->InstanceTemplate()
                 , "listen"
                 , Acceptor::v8Listen
                 );
  NODE_SET_METHOD( tcp_server_template->InstanceTemplate()
                 , "close"
                 , Acceptor::v8Close
                 );
}

Acceptor::Acceptor (Handle<Object> handle, Handle<Object> options) 
  : ObjectWrap(handle) 
{
  HandleScope scope;

  int backlog = 1024; // default value
  Local<Value> backlog_v = options->Get(String::NewSymbol("backlog"));
  if (backlog_v->IsInt32()) {
    backlog = backlog_v->IntegerValue();
  }

  Local<Value> on_error_v = options->Get(ON_ERROR_SYMBOL);
  if (on_error_v->IsFunction()) {
    handle_->Set(ON_ERROR_SYMBOL, on_error_v);
  }

  oi_server_init(&server_, backlog);
  server_.on_connection = Acceptor::on_connection;
  server_.on_error      = Acceptor::on_error;
  server_.data = this;
}

Connection*
Acceptor::OnConnection (struct sockaddr *addr, socklen_t len)
{
  HandleScope scope;
  
  Local<Value> protocol_v = handle_->GetHiddenValue(PROTOCOL_SYMBOL);
  if (!protocol_v->IsFunction()) {
    Close();
    return NULL;
  }
  Local<Function> protocol = Local<Function>::Cast(protocol_v);

  Handle<Value> argv[] = { protocol, handle_ };
  Local<Object> connection_handle = tcp_connection_constructor->NewInstance(2, argv);

  Connection *connection = NODE_UNWRAP(Connection, connection_handle);

  return connection;
}

void
Acceptor::OnError (struct oi_error error)
{
  HandleScope scope;

  Local<Value> callback_v = handle_->Get(String::NewSymbol("onError"));
  if (!callback_v->IsFunction()) return;
  Local<Function> callback = Local<Function>::Cast(callback_v);
  callback->Call(handle_, 0, NULL); // TODO args
}

Handle<Value>
Acceptor::v8New (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1 || args[0]->IsFunction() == false)
    return ThrowException(String::New("Must at give connection handler as the first argument"));

  /// set the handler
  args.Holder()->SetHiddenValue(PROTOCOL_SYMBOL, args[0]);

  Local<Object> options;

  if (args.Length() > 1 && args[1]->IsObject()) {
    options = args[1]->ToObject();
  } else {
    options = Object::New();
  }

  new Acceptor(args.Holder(), options);

  Acceptor *acceptor = NODE_UNWRAP(Acceptor, args.Holder());

  return args.This();
}

Handle<Value>
Acceptor::v8Listen (const Arguments& args)
{
  Acceptor *acceptor = NODE_UNWRAP(Acceptor, args.Holder());

  if (args.Length() < 1)
    return ThrowException(String::New("Must give at least a port as argument."));

  char *host = NULL;

  HandleScope scope;
  String::AsciiValue port(args[0]->ToString());

  if (args[1]->IsString()) {
    String::Utf8Value host_sv(args[1]->ToString());
    host = *host_sv;
  }

  // For servers call getaddrinfo inline. This is blocking but it shouldn't
  // matter--ever. If someone actually complains then simply swap it out
  // with a libeio call.
  struct addrinfo *address = NULL;
  int r = getaddrinfo(host, *port, &tcp_hints, &address);
  if (r != 0) 
    return ThrowException(String::New(strerror(errno)));

  acceptor->Listen(address);
  return Undefined();
}

Handle<Value>
Acceptor::v8Close (const Arguments& args)
{
  Acceptor *acceptor = NODE_UNWRAP(Acceptor, args.Holder());
  acceptor->Close();
  return Undefined();
}
