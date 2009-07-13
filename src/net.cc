#include "net.h"
#include "events.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> /* inet_ntop */
#include <netinet/in.h> /* sockaddr_in, sockaddr_in6 */

using namespace v8;
using namespace node;

#define UTF8_SYMBOL           String::NewSymbol("utf8")
#define RAW_SYMBOL            String::NewSymbol("raw")
#define ASCII_SYMBOL          String::NewSymbol("ascii")

#define ENCODING_SYMBOL       String::NewSymbol("encoding")
#define TIMEOUT_SYMBOL        String::NewSymbol("timeout")
#define SERVER_SYMBOL         String::NewSymbol("server")
#define REMOTE_ADDRESS_SYMBOL String::NewSymbol("remoteAddress")

#define PROTOCOL_SYMBOL           String::NewSymbol("protocol")

#define READY_STATE_SYMBOL  String::NewSymbol("readyState")
#define OPEN_SYMBOL         String::NewSymbol("open")
#define OPENING_SYMBOL      String::NewSymbol("opening")
#define READ_ONLY_SYMBOL    String::NewSymbol("readOnly")
#define WRITE_ONLY_SYMBOL   String::NewSymbol("writeOnly")
#define CLOSED_SYMBOL       String::NewSymbol("closed")

static const struct addrinfo server_tcp_hints = 
/* ai_flags      */ { AI_PASSIVE 
/* ai_family     */ , AF_UNSPEC
/* ai_socktype   */ , SOCK_STREAM
                    , 0
                    };

static const struct addrinfo client_tcp_hints = 
/* ai_flags      */ { 0
/* ai_family     */ , AF_UNSPEC
/* ai_socktype   */ , SOCK_STREAM
                    , 0
                    };

Persistent<FunctionTemplate> Connection::constructor_template;

void 
Connection::Initialize (v8::Handle<v8::Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
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
Connection::ReadyStateGetter (Local<String> property, const AccessorInfo& info)
{
  Connection *connection = ObjectWrap::Unwrap<Connection>(info.This());
  assert(connection);

  HandleScope scope;

  assert(property == READY_STATE_SYMBOL);

  switch(connection->ReadyState()) {
    case OPEN: return scope.Close(OPEN_SYMBOL);
    case OPENING: return scope.Close(OPENING_SYMBOL);
    case CLOSED: return scope.Close(CLOSED_SYMBOL);
    case READ_ONLY: return scope.Close(READ_ONLY_SYMBOL);
    case WRITE_ONLY: return scope.Close(WRITE_ONLY_SYMBOL);
  }

  assert(0 && "This shouldnt happen");
  return ThrowException(String::New("This shouldn't happen."));
}

void
Connection::Init (void)
{
  opening = false;
  double timeout = 60.0; // default
  evnet_socket_init(&socket_, timeout);
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
  if (socket_.fd > 0) {
    printf("%d garbage collecting open Connection : %d\n", i++, socket_.fd);
    printf("  socket->read_action: %p\n", socket_.read_action);
    printf("  socket->write_action: %p\n", socket_.write_action);
  }
  ForceClose();
}

Handle<Value>
Connection::New (const Arguments& args)
{
  HandleScope scope;

  Connection *connection = new Connection();
  connection->Wrap(args.This());

  return args.This();
}

enum Connection::readyState
Connection::ReadyState (void)
{
  if (socket_.got_full_close)
    return CLOSED;

  if (socket_.got_half_close)
    return (socket_.read_action == NULL ? CLOSED : READ_ONLY);

  if (socket_.read_action && socket_.write_action)
    return OPEN;

  else if (socket_.write_action)
    return WRITE_ONLY;

  else if (socket_.read_action)
    return READ_ONLY;

  else if (opening)
    return OPENING;
  
  return CLOSED;
}

Handle<Value>
Connection::Connect (const Arguments& args)
{
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());

  assert(connection);

  HandleScope scope;

  if (connection->ReadyState() != CLOSED) {
    return ThrowException(String::New("Socket is not in CLOSED state."));
  } else {
    // XXX ugly.
    connection->Init(); // in case we're reusing the socket... ?
  }

  assert(connection->socket_.fd < 0);
  assert(connection->socket_.read_action == NULL);
  assert(connection->socket_.write_action == NULL);

  if (args.Length() == 0)
    return ThrowException(String::New("Must specify a port."));

  String::AsciiValue port_sv(args[0]->ToString());
  if (connection->port_) printf("connection->port_ = '%s'\n", connection->port_);
  assert(connection->port_ == NULL);
  connection->port_ = strdup(*port_sv);

  assert(connection->host_ == NULL);
  if (args.Length() > 1 && args[1]->IsString()) {
    String::Utf8Value host_sv(args[1]->ToString());
    connection->host_ = strdup(*host_sv);
  }

  connection->opening = true;

  ev_ref(EV_DEFAULT_UC);
  
#ifdef __APPLE__
  /* HACK: Bypass the thread pool and do it sync on Macintosh.
   * Experiecing strange error where execution halts on 
   * getaddrinfo() and CPU goes to 100%. FIXME.
   */
  eio_req *req = static_cast<eio_req*>(malloc(sizeof(eio_req)));
  req->data = connection;
  Connection::Resolve(req);
#else
  /* For the moment I will do DNS lookups in the eio thread pool. This is
   * sub-optimal and cannot handle massive numbers of requests. 
   * In the future I will move to a system using adns or udns: 
   * http://lists.schmorp.de/pipermail/libev/2009q1/000632.html
   */
  connection->Attach();
  eio_custom( Connection::Resolve
            , EIO_PRI_DEFAULT
            , Connection::AfterResolve
            , connection
            );
#endif // __APPLE__
  return Undefined();
}

int
Connection::Resolve (eio_req *req)
{
  Connection *connection = static_cast<Connection*> (req->data);
  struct addrinfo *address = NULL;

  assert(connection->attached_);
  assert(connection->opening);

  req->result = getaddrinfo(connection->host_, connection->port_, 
                            &client_tcp_hints, &address);
  req->ptr2 = address;

  free(connection->host_);
  connection->host_ = NULL;

  free(connection->port_);
  connection->port_ = NULL;

#ifdef __APPLE__
  Connection::AfterResolve(req);
#endif

  return 0;
}

static struct addrinfo *
AddressDefaultToIPv4 (struct addrinfo *address_list)
{
  struct addrinfo *address = NULL;

  for (address = address_list; address != NULL; address = address->ai_next) {
    if (address->ai_addr->sa_family == AF_INET) break;
  }

  return address == NULL ? address_list : address;
}

int
Connection::AfterResolve (eio_req *req)
{
  ev_unref(EV_DEFAULT_UC);

  Connection *connection = static_cast<Connection*> (req->data);

  assert(connection->opening);
  assert(connection->attached_);

  struct addrinfo *address = NULL,
                  *address_list = static_cast<struct addrinfo *>(req->ptr2);

  address = AddressDefaultToIPv4(address_list);

  connection->opening = false;

  int r = 0;
  if (req->result == 0) r = connection->Connect(address);

  if (address_list) freeaddrinfo(address_list); 

  // no error. return.
  if (r == 0 && req->result == 0) {
    evnet_socket_attach (EV_DEFAULT_UC_ &connection->socket_);
    goto out;
  }

  /* RESOLVE ERROR */

  /* TODO: the whole resolve process should be moved into evnet_socket.
   * The fact that I'm modifying a read-only variable here should be 
   * good evidence of this.
   */
  connection->socket_.errorno = r | req->result;

  connection->OnDisconnect();

  connection->Detach();

out:
#ifdef __APPLE__
  free(req);
#endif
  return 0;
}

Handle<Value>
Connection::SetEncoding (const Arguments& args)
{
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);

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

Handle<Value>
Connection::Close (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  connection->Close();
  return Undefined();
}

Handle<Value>
Connection::FullClose (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  connection->FullClose();
  return Undefined();
}

Handle<Value>
Connection::ForceClose (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  connection->ForceClose();
  connection->Detach();
  return Undefined();
}


Handle<Value>
Connection::Send (const Arguments& args)
{
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  if ( connection->ReadyState() != OPEN 
    && connection->ReadyState() != WRITE_ONLY
     ) 
    return ThrowException(String::New("Socket is not open for writing"));

  // XXX
  // A lot of improvement can be made here. First of all we're allocating
  // evnet_bufs for every send which is clearly inefficent - it should use a
  // memory pool or ring buffer. Of course, expressing binary data as an
  // array of integers is extremely inefficent. This can improved when v8
  // bug 270 (http://code.google.com/p/v8/issues/detail?id=270) has been
  // addressed. 

  if (args[0]->IsString()) {
    enum encoding enc = ParseEncoding(args[1]);
    Local<String> s = args[0]->ToString();
    size_t len = s->Utf8Length();
    evnet_buf *buf = node::buf_new(len);
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
    evnet_buf *buf = node::buf_new(len);
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

  Emit("receive", argc, argv);
}

void 
Connection::OnDisconnect ()
{
  HandleScope scope;

  Handle<Value> argv[1];
  argv[0] = socket_.errorno == 0 ? False() : True();

  Emit("disconnect", 1, argv);
}

#define DEFINE_SIMPLE_CALLBACK(name, type)                          \
void name ()                                                        \
{                                                                   \
  HandleScope scope;                                                \
  Emit (type, 0, NULL);                                             \
}

DEFINE_SIMPLE_CALLBACK(Connection::OnConnect, "connect")
DEFINE_SIMPLE_CALLBACK(Connection::OnDrain, "drain")
DEFINE_SIMPLE_CALLBACK(Connection::OnTimeout, "timeout")
DEFINE_SIMPLE_CALLBACK(Connection::OnEOF, "eof")

Persistent<FunctionTemplate> Server::constructor_template;

void
Server::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "listen", Listen);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);

  target->Set(String::NewSymbol("Server"), constructor_template->GetFunction());
}


static Local<String>
GetAddressString (struct sockaddr *addr)
{
  HandleScope scope;
  char ip[INET6_ADDRSTRLEN];
  Local<String> remote_address;

  if (addr->sa_family == AF_INET) {
    struct sockaddr_in *sa = reinterpret_cast<struct sockaddr_in*>(addr);
    inet_ntop(AF_INET, &(sa->sin_addr), ip, INET6_ADDRSTRLEN);
    remote_address = String::New(ip);

  } else if (addr->sa_family == AF_INET6) {
    struct sockaddr_in6 *sa6 = reinterpret_cast<struct sockaddr_in6*>(addr);
    inet_ntop(AF_INET6, &(sa6->sin6_addr), ip, INET6_ADDRSTRLEN);
    remote_address = String::New(ip);

  } else assert(0 && "received a bad sa_family");

  return scope.Close(remote_address);
}

Handle<FunctionTemplate>
Server::GetConnectionTemplate (void)
{
  return Connection::constructor_template;
}

Connection*
Server::UnwrapConnection (Local<Object> connection)
{
  HandleScope scope;
  return ObjectWrap::Unwrap<Connection>(connection);
}

Connection*
Server::OnConnection (struct sockaddr *addr)
{
  HandleScope scope;

  TryCatch try_catch;

  Local<Object> js_connection =
    GetConnectionTemplate()->GetFunction()->NewInstance(0, NULL);
  
  if (js_connection.IsEmpty()) {
    FatalException(try_catch);
    return NULL;
  }

  Local<String> remote_address = GetAddressString(addr);
  js_connection->Set(REMOTE_ADDRESS_SYMBOL, remote_address);
  js_connection->Set(SERVER_SYMBOL, handle_);

  Connection *connection = UnwrapConnection(js_connection);
  if (!connection) return NULL;

  Handle<Value> argv[1] = { js_connection };

  Emit("connection", 1, argv);

  return connection;
}

void
Server::OnClose (int errorno)
{
  HandleScope scope;

  Handle<Value> argv[1] = { Integer::New(errorno) };

  Emit("close", 1, argv);
}

// TODO Server->SetOptions
// TODO Server -> Server rename

Handle<Value>
Server::New (const Arguments& args)
{
  HandleScope scope;

  Server *server = new Server();
  server->Wrap(args.This());

  return args.This();
}

Handle<Value>
Server::Listen (const Arguments& args)
{
  Server *server = ObjectWrap::Unwrap<Server>(args.Holder());
  assert(server);

  if (args.Length() == 0)
    return ThrowException(String::New("Must give at least a port as argument."));

  HandleScope scope;
  String::AsciiValue port(args[0]->ToString());

  char *host = NULL;
  if (args[1]->IsString()) {
    String::Utf8Value host_sv(args[1]->ToString());
    host = strdup(*host_sv);
  }

  // For servers call getaddrinfo inline. This is blocking but it shouldn't
  // matter much. If someone actually complains then simply swap it out
  // with a libeio call.
  struct addrinfo *address = NULL,
                  *address_list = NULL;
  int r = getaddrinfo(host, *port, &server_tcp_hints, &address_list);
  free(host);
  if (r != 0)
    return ThrowException(String::New(strerror(errno)));

  address = AddressDefaultToIPv4(address_list);

  server->Listen(address);

  if (address_list) freeaddrinfo(address_list); 

  return Undefined();
}

Handle<Value>
Server::Close (const Arguments& args)
{
  Server *server = ObjectWrap::Unwrap<Server>(args.Holder());
  assert(server);

  server->Close();
  return Undefined();
}
