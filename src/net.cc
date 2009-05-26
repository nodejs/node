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

#define UTF8_SYMBOL           String::NewSymbol("utf8")
#define RAW_SYMBOL            String::NewSymbol("raw")
#define ASCII_SYMBOL          String::NewSymbol("ascii")

#define ON_RECEIVE_SYMBOL     String::NewSymbol("onReceive")
#define ON_DISCONNECT_SYMBOL  String::NewSymbol("onDisconnect")
#define ON_CONNECT_SYMBOL     String::NewSymbol("onConnect")
#define ON_DRAIN_SYMBOL       String::NewSymbol("onDrain")
#define ON_TIMEOUT_SYMBOL     String::NewSymbol("onTimeout")
#define ON_ERROR_SYMBOL       String::NewSymbol("onError")
#define ON_EOF_SYMBOL         String::NewSymbol("onEOF")

#define ENCODING_SYMBOL       String::NewSymbol("encoding")
#define TIMEOUT_SYMBOL        String::NewSymbol("timeout")
#define SERVER_SYMBOL         String::NewSymbol("server")

#define PROTOCOL_SYMBOL           String::NewSymbol("protocol")
#define CONNECTION_HANDLER_SYMBOL String::NewSymbol("connection_handler")

#define READY_STATE_SYMBOL  String::NewSymbol("readyState")
#define OPEN_SYMBOL         String::NewSymbol("open")
#define READ_ONLY_SYMBOL    String::NewSymbol("readOnly")
#define WRITE_ONLY_SYMBOL   String::NewSymbol("writeOnly")
#define CLOSED_SYMBOL       String::NewSymbol("closed")

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

Persistent<FunctionTemplate> Connection::constructor_template;

void 
Connection::Initialize (v8::Handle<v8::Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "connect", Connect);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "send", Send);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "fullClose", FullClose);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "forceClose", ForceClose);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setEncoding", SetEncoding);

  constructor_template->PrototypeTemplate()->SetAccessor(
      READY_STATE_SYMBOL,
      ReadyStateGetter);

  target->Set(String::NewSymbol("Connection"), constructor_template->GetFunction());
}

Handle<Value>
Connection::ReadyStateGetter (Local<String> _, const AccessorInfo& info)
{
  Connection *connection = NODE_UNWRAP(Connection, info.This());
  if (!connection) return Handle<Value>();

  HandleScope scope;

  switch(connection->ReadyState()) {
    case OPEN: return scope.Close(OPEN_SYMBOL);
    case CLOSED: return scope.Close(CLOSED_SYMBOL);
    case READ_ONLY: return scope.Close(READ_ONLY_SYMBOL);
    case WRITE_ONLY: return scope.Close(WRITE_ONLY_SYMBOL);
  }

  assert(0 && "This shouldnt happen");
  return ThrowException(String::New("This shouldn't happen."));
}

Connection::Connection (Handle<Object> handle)
  : ObjectWrap(handle) 
{
  encoding_ = RAW;

  host_ = NULL;
  port_ = NULL;

  Init();
}

void
Connection::Init (void)
{
  double timeout = 60.0; // default
  oi_socket_init(&socket_, timeout);
  socket_.on_connect = Connection::on_connect;
  socket_.on_read    = Connection::on_read;
  socket_.on_drain   = Connection::on_drain;
  socket_.on_close   = Connection::on_close;
  socket_.on_timeout = Connection::on_timeout;
  socket_.data = this;
}


Connection::~Connection ()
{
  static int i = 0;
  if(socket_.fd > 0) {
    printf("garbage collecting open Connection : %d\n", i++);
    printf("  socket->read_action: %p\n", socket_.read_action);
    printf("  socket->write_action: %p\n", socket_.write_action);
  }
  ForceClose();
}

void
Connection::SetAcceptor (Handle<Object> acceptor_handle)
{
  HandleScope scope;
  handle_->Set(SERVER_SYMBOL, acceptor_handle);
  
  Attach();
}

Handle<Value>
Connection::New (const Arguments& args)
{
  HandleScope scope;

  Connection *c = new Connection(args.This());
  ObjectWrap::InformV8ofAllocation(c);

  return args.This();
}

Handle<Value>
Connection::Connect (const Arguments& args)
{
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());
  if (!connection) return Handle<Value>();

  HandleScope scope;

  if (connection->ReadyState() != CLOSED) {
    return ThrowException(String::New("Socket is already connected."));
  } else {
    // XXX ugly.
    connection->Init(); // in case we're reusing the socket... ?
  }

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

  connection->Attach();
  return Undefined();
}

Handle<Value>
Connection::SetEncoding (const Arguments& args)
{
  HandleScope scope;

  Connection *connection = NODE_UNWRAP(Connection, args.This());
  if (!connection) return Handle<Value>();

  if (!args[0]->IsString()) {
    connection->encoding_ = RAW;
    return scope.Close(RAW_SYMBOL);
  }

  switch (ParseEncoding(args[0])) {
    case ASCII:
      connection->encoding_ = ASCII;
      return scope.Close(ASCII_SYMBOL);

    case UTF8:
      connection->encoding_ = UTF8;
      return scope.Close(UTF8_SYMBOL);

    case RAW:
    default:
      connection->encoding_ = RAW;
      return scope.Close(RAW_SYMBOL);
  }
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

  puts("net.cc: resolve failed");

  connection->OnDisconnect();
  connection->Detach();

  return 0;
}

Handle<Value>
Connection::Close (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());
  if (!connection) return Handle<Value>();

  connection->Close();
  return Undefined();
}

Handle<Value>
Connection::FullClose (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());
  if (!connection) return Handle<Value>();

  connection->FullClose();
  return Undefined();
}

Handle<Value>
Connection::ForceClose (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());
  if (!connection) return Handle<Value>();

  connection->ForceClose();
  connection->Detach();
  return Undefined();
}

static void
free_buf (oi_buf *b)
{
  V8::AdjustAmountOfExternalAllocatedMemory(-b->len);
  free(b);
}

static oi_buf *
new_buf (size_t size)
{
  size_t total = sizeof(oi_buf) + size;
  void *p = malloc(total);
  if (p == NULL) return NULL;

  oi_buf *b = static_cast<oi_buf*>(p);
  b->base = static_cast<char*>(p) + sizeof(oi_buf);

  b->len = size;
  b->release = free_buf;
  V8::AdjustAmountOfExternalAllocatedMemory(total);

  return b;
}

Handle<Value>
Connection::Send (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = NODE_UNWRAP(Connection, args.Holder());
  if (!connection) return Handle<Value>();

  if ( connection->ReadyState() != OPEN 
    && connection->ReadyState() != WRITE_ONLY
     ) 
    return ThrowException(String::New("Socket is not open for writing"));

  // XXX
  // A lot of improvement can be made here. First of all we're allocating
  // oi_bufs for every send which is clearly inefficent - it should use a
  // memory pool or ring buffer. Of course, expressing binary data as an
  // array of integers is extremely inefficent. This can improved when v8
  // bug 270 (http://code.google.com/p/v8/issues/detail?id=270) has been
  // addressed. 

  if (args[0]->IsString()) {
    enum encoding enc = ParseEncoding(args[1]);
    Local<String> s = args[0]->ToString();
    size_t len = s->Utf8Length();
    oi_buf *buf = new_buf(len);
    switch (enc) {
      case RAW:
      case ASCII:
        s->WriteAscii(buf->base, 0, len);
        break;

      case UTF8:
        s->WriteUtf8(buf->base, len);
        break;

      default:
        assert(0 && "unhandled string encoding");
    }
    connection->Send(buf);

  } else if (args[0]->IsArray()) {
    Handle<Array> array = Handle<Array>::Cast(args[0]);
    size_t len = array->Length();
    oi_buf *buf = new_buf(len);
    for (size_t i = 0; i < len; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf->base[i] = int_value->IntegerValue();
    }
    connection->Send(buf);

  } else return ThrowException(String::New("Bad argument"));

  return Undefined();  
}

void 
Connection::OnReceive (const void *buf, size_t len)
{
  HandleScope scope;

  Handle<Value> callback_v = handle_->Get(ON_RECEIVE_SYMBOL);
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
  callback->Call(handle_, argc, argv);

  if (try_catch.HasCaught())
    FatalException(try_catch);
}

#define DEFINE_SIMPLE_CALLBACK(name, symbol)                        \
void name ()                                                        \
{                                                                   \
  HandleScope scope;                                                \
  Local<Value> callback_v = handle_->Get(symbol);                   \
  if (!callback_v->IsFunction()) return;                            \
  Handle<Function> callback = Handle<Function>::Cast(callback_v);   \
  TryCatch try_catch;                                               \
  callback->Call(handle_, 0, NULL);                                 \
  if (try_catch.HasCaught())                                        \
    node::FatalException(try_catch);                               \
}

DEFINE_SIMPLE_CALLBACK(Connection::OnConnect, ON_CONNECT_SYMBOL)
DEFINE_SIMPLE_CALLBACK(Connection::OnDrain, ON_DRAIN_SYMBOL)
DEFINE_SIMPLE_CALLBACK(Connection::OnDisconnect, ON_DISCONNECT_SYMBOL)
DEFINE_SIMPLE_CALLBACK(Connection::OnTimeout, ON_TIMEOUT_SYMBOL)
DEFINE_SIMPLE_CALLBACK(Connection::OnEOF, ON_EOF_SYMBOL)

Persistent<FunctionTemplate> Acceptor::constructor_template;

void
Acceptor::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructor_template = Persistent<FunctionTemplate>::New(t);

  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "listen", Listen);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);

  target->Set(String::NewSymbol("Server"), constructor_template->GetFunction());
}

Acceptor::Acceptor (Handle<Object> handle, Handle<Function> connection_handler,  Handle<Object> options) 
  : ObjectWrap(handle) 
{
  HandleScope scope;

  handle_->SetHiddenValue(CONNECTION_HANDLER_SYMBOL, connection_handler);

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
  server_.data = this;
}

Connection*
Acceptor::OnConnection (struct sockaddr *addr, socklen_t len)
{
  HandleScope scope;
  
  Local<Function> connection_handler = GetConnectionHandler();
  if (connection_handler.IsEmpty()) {
    printf("Connection handler was empty!");
    Close();
    return NULL;
  }

  TryCatch try_catch;

  Local<Object> connection_handle =
    Connection::constructor_template->GetFunction()->NewInstance(0, NULL);
  
  if (connection_handle.IsEmpty()) {
    FatalException(try_catch);
    return NULL;
  }

  Connection *connection = NODE_UNWRAP(Connection, connection_handle);
  if (!connection) return NULL;

  connection->SetAcceptor(handle_);

  Handle<Value> argv[1] = { connection_handle };

  Local<Value> ret = connection_handler->Call(handle_, 1, argv);

  if (ret.IsEmpty())
    FatalException(try_catch);

  return connection;
}

Handle<Value>
Acceptor::New (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1 || args[0]->IsFunction() == false)
    return ThrowException(String::New("Must at give connection handler as the first argument"));

  Local<Function> connection_handler = Local<Function>::Cast(args[0]);
  Local<Object> options;

  if (args.Length() > 1 && args[1]->IsObject()) {
    options = args[1]->ToObject();
  } else {
    options = Object::New();
  }

  Acceptor *a = new Acceptor(args.This(), connection_handler, options);
  ObjectWrap::InformV8ofAllocation(a);

  return args.This();
}

Handle<Value>
Acceptor::Listen (const Arguments& args)
{
  Acceptor *acceptor = NODE_UNWRAP(Acceptor, args.Holder());
  if (!acceptor) return Handle<Value>();

  if (args.Length() < 1)
    return ThrowException(String::New("Must give at least a port as argument."));


  HandleScope scope;
  String::AsciiValue port(args[0]->ToString());

  char *host = NULL;
  if (args[1]->IsString()) {
    String::Utf8Value host_sv(args[1]->ToString());
    host = strdup(*host_sv);
  }

  // For servers call getaddrinfo inline. This is blocking but it shouldn't
  // matter--ever. If someone actually complains then simply swap it out
  // with a libeio call.
  struct addrinfo *address = NULL;
  int r = getaddrinfo(host, *port, &tcp_hints, &address);
  if (r != 0) 
    return ThrowException(String::New(strerror(errno)));

  free(host);

  acceptor->Listen(address);
  return Undefined();
}

Handle<Value>
Acceptor::Close (const Arguments& args)
{
  Acceptor *acceptor = NODE_UNWRAP(Acceptor, args.Holder());
  if (!acceptor) return Handle<Value>();

  acceptor->Close();
  return Undefined();
}

Local<v8::Function>
Acceptor::GetConnectionHandler (void)
{
  HandleScope scope;

  Local<Value> connection_handler_v = handle_->GetHiddenValue(CONNECTION_HANDLER_SYMBOL);
  if (connection_handler_v->IsFunction()) {
    Local<Function> connection_handler = Local<Function>::Cast(connection_handler_v);
    return scope.Close(connection_handler);
  }

  return Local<Function>();
}

