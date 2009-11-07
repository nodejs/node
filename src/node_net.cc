// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node_net.h>

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

namespace node {

#define UTF8_SYMBOL           String::NewSymbol("utf8")
#define BINARY_SYMBOL         String::NewSymbol("binary")
#define ASCII_SYMBOL          String::NewSymbol("ascii")

#define SERVER_SYMBOL         String::NewSymbol("server")
#define REMOTE_ADDRESS_SYMBOL String::NewSymbol("remoteAddress")

#define FD_SYMBOL             String::NewSymbol("fd")

#define READY_STATE_SYMBOL  String::NewSymbol("readyState")
#define OPEN_SYMBOL         String::NewSymbol("open")
#define OPENING_SYMBOL      String::NewSymbol("opening")
#define READ_ONLY_SYMBOL    String::NewSymbol("readOnly")
#define WRITE_ONLY_SYMBOL   String::NewSymbol("writeOnly")
#define CLOSING_SYMBOL      String::NewSymbol("closing")
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

// Initialize the tcp.Connection object.
void Connection::Initialize(v8::Handle<v8::Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  constructor_template = Persistent<FunctionTemplate>::New(t);
  // Inherits from node.EventEmitter
  constructor_template->Inherit(EventEmitter::constructor_template);
  // All native node objects have 1 internal field (required by ObjectWrap)
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  // Set class name for debugging output
  constructor_template->SetClassName(String::NewSymbol("Connection"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "connect", Connect);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "send", Send);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "forceClose", ForceClose);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setEncoding", SetEncoding);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "readPause", ReadPause);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "readResume", ReadResume);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setTimeout", SetTimeout);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setNoDelay", SetNoDelay);

  // Getter for connection.readyState
  constructor_template->PrototypeTemplate()->SetAccessor(
      READY_STATE_SYMBOL,
      ReadyStateGetter);

  // Getter for connection.readyState
  constructor_template->PrototypeTemplate()->SetAccessor(
      FD_SYMBOL,
      FDGetter);

  // Assign class to its place as tcp.Connection
  target->Set(String::NewSymbol("Connection"),
      constructor_template->GetFunction());
}

Handle<Value> Connection::ReadyStateGetter(Local<String> property,
                                           const AccessorInfo& info) {
  // Unwrap the javascript object to get the C++ object
  Connection *connection = ObjectWrap::Unwrap<Connection>(info.This());
  assert(connection);

  HandleScope scope;

  assert(property == READY_STATE_SYMBOL);

  // Resolving is not done in evcom, it's done in this file. Thus we add
  // this "opening" symbol to the native EVCOM ready states. "opening"
  // really means "resolving". 
  if (connection->resolving_) return scope.Close(OPENING_SYMBOL);

  // Map between the evcom enum and V8 strings:
  switch (evcom_stream_state(&connection->stream_)) {
    case EVCOM_INITIALIZED:  return scope.Close(CLOSED_SYMBOL);
    case EVCOM_CONNECTING:   return scope.Close(OPENING_SYMBOL);
    case EVCOM_CONNECTED_RW: return scope.Close(OPEN_SYMBOL);
    case EVCOM_CONNECTED_RO: return scope.Close(READ_ONLY_SYMBOL);
    case EVCOM_CONNECTED_WO: return scope.Close(WRITE_ONLY_SYMBOL);
    case EVCOM_CLOSING:      return scope.Close(CLOSING_SYMBOL);
    case EVCOM_CLOSED:       return scope.Close(CLOSED_SYMBOL);
  }

  assert(0 && "This shouldnt happen");
  return ThrowException(Exception::Error(
        String::New("This shouldn't happen.")));
}

Handle<Value> Connection::FDGetter(Local<String> property,
                                   const AccessorInfo& info) {
  // Unwrap the javascript object to get the C++ object
  Connection *connection = ObjectWrap::Unwrap<Connection>(info.This());
  assert(connection);

  HandleScope scope;

  assert(property == FD_SYMBOL);

  Local<Integer> fd = Integer::New(connection->stream_.recvfd);

  return scope.Close(fd);
}

// Constructor - these actions are not taken in the normal constructor
// (Connection::Connection) because sometimes the Connection needs to be
// reinitialized without destroying the object.
void Connection::Init() {
  resolving_ = false;
  evcom_stream_init(&stream_);
  stream_.on_connect = Connection::on_connect;
  stream_.on_read    = Connection::on_read;
  stream_.on_close   = Connection::on_close;
  stream_.on_timeout = Connection::on_timeout;
  stream_.on_drain   = Connection::on_drain;
  stream_.data = this;
}

Connection::~Connection() {
  assert(stream_.recvfd < 0 && "garbage collecting open Connection");
  assert(stream_.sendfd < 0 && "garbage collecting open Connection");
}

// V8 contstructor
Handle<Value> Connection::New(const Arguments& args) {
  HandleScope scope;

  // All constructors in node look similar to this.

  // allocate the C++ object
  Connection *connection = new Connection();

  // Use ObjectWrap::Wrap to assign it to the internal field in the V8
  // object.
  connection->Wrap(args.This());

  return args.This();
}

// Open a connection. Starts resolving the hostname in the libeio
// thread pool, when that completes in Connection::AfterResolve, actually
// open the connection.
Handle<Value> Connection::Connect(const Arguments& args) {
  // Unwrap V8 object into C++ object
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  HandleScope scope;

  // If the connection is closed, reinitialize it.
  if (connection->ReadyState() == EVCOM_CLOSED) {
    connection->Init();  // in case we're reusing the socket
    assert(connection->ReadyState() == EVCOM_INITIALIZED);
  }

  // If connect() is called on an open connection, raise an error.
  if (connection->ReadyState() != EVCOM_INITIALIZED) {
    Local<Value> exception = Exception::Error(
        String::New("Socket is not in CLOSED state."));
    return ThrowException(exception);
  }

  assert(connection->stream_.recvfd < 0);
  assert(connection->stream_.sendfd < 0);

  // Make sure we have at least one argument (a port)
  if (args.Length() == 0) {
    Local<Value> exception = Exception::TypeError(
        String::New("First argument must be a port number"));
    return ThrowException(exception);
  }

  // We need the port as a string for getaddrinfo().
  // Thus cast it to a string. The strdup()'d value will be freed in
  // Resolve().
  String::AsciiValue port_sv(args[0]->ToString());
  assert(connection->port_ == NULL);
  connection->port_ = strdup(*port_sv);

  // Get the host, if any is present.
  assert(connection->host_ == NULL);
  if (args.Length() > 1 && args[1]->IsString()) {
    String::Utf8Value host_sv(args[1]->ToString());
    connection->host_ = strdup(*host_sv);
  }

  connection->resolving_ = true; // This is done so that readyState can return
                                 // "opening" while getaddinfo() runs.

  // There will not be any active watchers from this object on the event
  // loop while getaddrinfo() runs. If the only thing happening in the
  // script was this hostname resolution, then the event loop would drop
  // out. Thus we need to add ev_ref() until AfterResolve().
  ev_ref(EV_DEFAULT_UC);

  // Attach the object so it doesn't get garbage collected.
  connection->Attach();

  // For the moment I will do DNS lookups in the eio thread pool. This is
  // sub-optimal and cannot handle massive numbers of requests.
  // In the future I will move to a system using adns or udns:
  // http://lists.schmorp.de/pipermail/libev/2009q1/000632.html
  eio_custom(Connection::Resolve, EIO_PRI_DEFAULT, Connection::AfterResolve,
      connection);

  return Undefined();
}

// This function is executed in the thread pool. It cannot modify any
// members of the connection object. 
int Connection::Resolve(eio_req *req) {
  Connection *connection = static_cast<Connection*> (req->data);
  struct addrinfo *address = NULL;

  assert(connection->attached_);
  assert(connection->resolving_);

  req->result = getaddrinfo(connection->host_, connection->port_,
                            &client_tcp_hints, &address);
  req->ptr2 = address;

  free(connection->host_);
  connection->host_ = NULL;

  free(connection->port_);
  connection->port_ = NULL;

  return 0;
}

static struct addrinfo * AddressDefaultToIPv4(struct addrinfo *address_list) {
  struct addrinfo *address = NULL;

  for (address = address_list; address != NULL; address = address->ai_next) {
    if (address->ai_addr->sa_family == AF_INET) break;
  }

  return address == NULL ? address_list : address;
}

int Connection::AfterResolve(eio_req *req) {
  ev_unref(EV_DEFAULT_UC);

  Connection *connection = static_cast<Connection*> (req->data);

  assert(connection->resolving_);
  assert(connection->attached_);

  struct addrinfo *address = NULL,
                  *address_list = static_cast<struct addrinfo *>(req->ptr2);

  address = AddressDefaultToIPv4(address_list);

  connection->resolving_ = false;

  int r = 0;
  if (req->result == 0) r = connection->Connect(address->ai_addr);

  if (address_list) freeaddrinfo(address_list);

  // no error. return.
  if (req->result == 0) {
    evcom_stream_attach(EV_DEFAULT_UC_ &connection->stream_);
    goto out;
  }

  /* RESOLVE ERROR */

  /* TODO: the whole resolve process should be moved into evcom_stream.
   * The fact that I'm modifying a read-only variable here should be
   * good evidence of this.
   */
  connection->stream_.errorno = req->result;

  connection->OnClose();

  connection->Detach();

 out:
  return 0;
}

Handle<Value> Connection::SetEncoding(const Arguments& args) {
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);

  if (!args[0]->IsString()) {
    connection->encoding_ = BINARY;
    return scope.Close(BINARY_SYMBOL);
  }

  switch (ParseEncoding(args[0])) {
    case ASCII:
      connection->encoding_ = ASCII;
      return scope.Close(ASCII_SYMBOL);

    case UTF8:
      connection->encoding_ = UTF8;
      return scope.Close(UTF8_SYMBOL);

    case BINARY:
      connection->encoding_ = BINARY;
      return scope.Close(BINARY_SYMBOL);
  }
  assert(0 && "this shouldn't happen");
  return ThrowException(Exception::Error(
        String::New("Could not parse encoding. This is a Node bug.")));
}

Handle<Value> Connection::ReadPause(const Arguments& args) {
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);

  connection->ReadPause();

  return Undefined();
}

Handle<Value> Connection::ReadResume(const Arguments& args) {
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);

  connection->ReadResume();

  return Undefined();
}

Handle<Value> Connection::SetTimeout(const Arguments& args) {
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);

  float timeout = NODE_V8_UNIXTIME(args[0]);

  connection->SetTimeout(timeout);

  return Undefined();
}

Handle<Value> Connection::Close(const Arguments& args) {
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  connection->Close();
  return Undefined();
}

Handle<Value> Connection::ForceClose(const Arguments& args) {
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  connection->ForceClose();
  connection->Detach();
  return Undefined();
}

Handle<Value> Connection::SetNoDelay(const Arguments& args) {
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());

  bool no_delay = true;
  if (args.Length() > 0) {
    no_delay = args[0]->IsTrue();
  }

  connection->SetNoDelay(no_delay);

  return Undefined();
}

Handle<Value> Connection::Send(const Arguments& args) {
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  if (connection->ReadyState() != EVCOM_CONNECTED_RW &&
      connection->ReadyState() != EVCOM_CONNECTED_WO) {
    Local<Value> exception = Exception::Error(
        String::New("Socket is not open for writing"));
    return ThrowException(exception);
  }

  enum encoding enc = ParseEncoding(args[1]);
  ssize_t len = DecodeBytes(args[0], enc);

  if (len < 0) {
    Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
    return ThrowException(exception);
  }

  char * buf = new char[len];
  ssize_t written = DecodeWrite(buf, len, args[0], enc);
  assert(written == len);
  connection->Send(buf, written);
  delete [] buf;

  return scope.Close(Integer::New(written));
}

void Connection::OnReceive(const void *buf, size_t len) {
  HandleScope scope;
  Local<Value> data = Encode(buf, len, encoding_);
  Emit("receive", 1, &data);
}

void Connection::OnClose() {
  HandleScope scope;

  Handle<Value> argv[1];
  argv[0] = stream_.errorno == 0 ? False() : True();

  Emit("close", 1, argv);
}

void Connection::OnConnect() {
  HandleScope scope;

  if (stream_.server) {
    Server *server = static_cast<Server*>(stream_.server->data);
    Local<Value> value = Local<Value>::New(handle_);
    server->Emit("connection", 1, &value);
  }

  Emit("connect", 0, NULL);
}

void Connection::OnTimeout() {
  HandleScope scope;
  Emit("timeout", 0, NULL);
}

void Connection::OnDrain() {
  HandleScope scope;
  Emit("drain", 0, NULL);
}

void Connection::OnEOF() {
  HandleScope scope;
  Emit("eof", 0, NULL);
}

Persistent<FunctionTemplate> Server::constructor_template;

void Server::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Server"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "listen", Listen);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);

  target->Set(String::NewSymbol("Server"), constructor_template->GetFunction());
}

static Local<String> GetAddressString(struct sockaddr *addr) {
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

  } else {
    assert(0 && "received a bad sa_family");
  }

  return scope.Close(remote_address);
}

Handle<FunctionTemplate> Server::GetConnectionTemplate() {
  return Connection::constructor_template;
}

Connection* Server::UnwrapConnection(Local<Object> connection) {
  HandleScope scope;
  return ObjectWrap::Unwrap<Connection>(connection);
}

Connection* Server::OnConnection(struct sockaddr *addr) {
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

  connection->Attach();

  return connection;
}

void Server::OnClose(int errorno) {
  HandleScope scope;

  Handle<Value> argv[1] = { Integer::New(errorno) };

  Emit("close", 1, argv);
}

Handle<Value> Server::New(const Arguments& args) {
  HandleScope scope;

  Server *server = new Server();
  server->Wrap(args.This());

  return args.This();
}

Handle<Value> Server::Listen(const Arguments& args) {
  Server *server = ObjectWrap::Unwrap<Server>(args.Holder());
  assert(server);

  HandleScope scope;

  if (args.Length() == 0) {
    Local<Value> exception = Exception::TypeError(
        String::New("First argument must be a port number"));
    return ThrowException(exception);
  }

  String::AsciiValue port(args[0]->ToString());

#ifndef DNS_MAXNAME
# define DNS_MAXNAME 1024
#endif

  char host[DNS_MAXNAME+1] = "\0";
  int backlog = 128;

  if (args.Length() == 2) {
    if (args[1]->IsInt32()) {
      backlog = args[1]->Int32Value();
    } else if (args[1]->IsString()) {
      args[1]->ToString()->WriteAscii(host, 0, DNS_MAXNAME+1);
    }
  } else if (args.Length() > 2) {
    if (args[1]->IsString()) {
      args[1]->ToString()->WriteAscii(host, 0, DNS_MAXNAME+1);
    }

    if (!args[2]->IsInt32()) {
      return ThrowException(
          Exception::TypeError(String::New("backlog must be an integer")));
    }

    backlog = args[2]->Int32Value();
  }

  // For servers call getaddrinfo inline. This is blocking but it shouldn't
  // matter much. If someone actually complains then simply swap it out
  // with a libeio call.
  struct addrinfo * address = NULL,
                  * address_list = NULL;

  int r = getaddrinfo(strlen(host) ?
      host : NULL, *port, &server_tcp_hints, &address_list);

  if (r != 0) {
    Local<Value> exception = Exception::Error(String::New(strerror(errno)));
    return ThrowException(exception);
  }

  address = AddressDefaultToIPv4(address_list);

  server->Listen(address->ai_addr, backlog);

  if (address_list) freeaddrinfo(address_list);

  return Undefined();
}

Handle<Value> Server::Close(const Arguments& args) {
  Server *server = ObjectWrap::Unwrap<Server>(args.Holder());
  assert(server);

  server->Close();
  return Undefined();
}

}  // namespace node
