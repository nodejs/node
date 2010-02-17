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

static Persistent<String> utf8_symbol;
static Persistent<String> binary_symbol;
static Persistent<String> ascii_symbol;

static Persistent<String> server_symbol;
static Persistent<String> remote_address_symbol;
static Persistent<String> fd_symbol;

static Persistent<String> ready_state_symbol;
static Persistent<String> open_symbol;
static Persistent<String> opening_symbol;
static Persistent<String> read_only_symbol;
static Persistent<String> write_only_symbol;
static Persistent<String> closing_symbol;
static Persistent<String> closed_symbol;

static Persistent<String> data_symbol;
static Persistent<String> connection_symbol;
static Persistent<String> connect_symbol;
static Persistent<String> timeout_symbol;
static Persistent<String> drain_symbol;
static Persistent<String> end_symbol;
static Persistent<String> close_symbol;

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

  utf8_symbol = NODE_PSYMBOL("utf8");
  binary_symbol = NODE_PSYMBOL("binary");
  ascii_symbol = NODE_PSYMBOL("ascii");

  server_symbol = NODE_PSYMBOL("server");
  remote_address_symbol = NODE_PSYMBOL("remoteAddress");
  fd_symbol = NODE_PSYMBOL("fd");

  ready_state_symbol = NODE_PSYMBOL("readyState");
  open_symbol = NODE_PSYMBOL("open");
  opening_symbol = NODE_PSYMBOL("opening");
  read_only_symbol = NODE_PSYMBOL("readOnly");
  write_only_symbol = NODE_PSYMBOL("writeOnly");
  closing_symbol = NODE_PSYMBOL("closing");
  closed_symbol = NODE_PSYMBOL("closed");

  data_symbol = NODE_PSYMBOL("data");
  connection_symbol = NODE_PSYMBOL("connection");
  connect_symbol = NODE_PSYMBOL("connect");
  timeout_symbol = NODE_PSYMBOL("timeout");
  drain_symbol = NODE_PSYMBOL("drain");
  end_symbol = NODE_PSYMBOL("end");
  close_symbol = NODE_PSYMBOL("close");

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
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "write", Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "forceClose", ForceClose);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setEncoding", SetEncoding);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "readPause", ReadPause);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "readResume", ReadResume);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setTimeout", SetTimeout);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setNoDelay", SetNoDelay);
  #if EVCOM_HAVE_GNUTLS
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setSecure", SetSecure);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "verifyPeer", VerifyPeer);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "getPeerCertificate",
                            GetPeerCertificate);
  gnutls_global_init();
  #endif

  // Getter for connection.readyState
  constructor_template->InstanceTemplate()->SetAccessor(
      ready_state_symbol,
      ReadyStateGetter);

  // Getter for connection.readyState
  constructor_template->InstanceTemplate()->SetAccessor(
      fd_symbol,
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

  assert(property == ready_state_symbol);

  // Resolving is not done in evcom, it's done in this file. Thus we add
  // this "opening" symbol to the native EVCOM ready states. "opening"
  // really means "resolving". 
  if (connection->resolving_) return scope.Close(opening_symbol);

  // Map between the evcom enum and V8 strings:
  switch (evcom_stream_state(&connection->stream_)) {
    case EVCOM_INITIALIZED:  return scope.Close(closed_symbol);
    case EVCOM_CONNECTING:   return scope.Close(opening_symbol);
    case EVCOM_CONNECTED_RW: return scope.Close(open_symbol);
    case EVCOM_CONNECTED_RO: return scope.Close(read_only_symbol);
    case EVCOM_CONNECTED_WO: return scope.Close(write_only_symbol);
    case EVCOM_CLOSING:      return scope.Close(closing_symbol);
    case EVCOM_CLOSED:       return scope.Close(closed_symbol);
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

  assert(property == fd_symbol);

  Local<Integer> fd = Integer::New(connection->stream_.recvfd);

  return scope.Close(fd);
}

// Constructor - these actions are not taken in the normal constructor
// (Connection::Connection) because sometimes the Connection needs to be
// reinitialized without destroying the object.
void Connection::Init() {
  resolving_ = false;
  secure_ = false;
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
  if (connection->ReadyState() != EVCOM_INITIALIZED || connection->resolving_) {
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

  // Ref the object so it doesn't get garbage collected.
  connection->Ref();

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

  assert(connection->refs_);
  assert(connection->resolving_);

  req->result = getaddrinfo(connection->host_, connection->port_,
                            &client_tcp_hints, &address);
  req->ptr2 = address;

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
  assert(connection->refs_);

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

  connection->Unref();

 out:
  return 0;
}

Handle<Value> Connection::SetEncoding(const Arguments& args) {
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);

  if (!args[0]->IsString()) {
    connection->encoding_ = BINARY;
    return scope.Close(binary_symbol);
  }

  switch (ParseEncoding(args[0])) {
    case ASCII:
      connection->encoding_ = ASCII;
      return scope.Close(ascii_symbol);

    case UTF8:
      connection->encoding_ = UTF8;
      return scope.Close(utf8_symbol);

    case BINARY:
      connection->encoding_ = BINARY;
      return scope.Close(binary_symbol);
  }
  assert(0 && "this shouldn't happen");
  return ThrowException(Exception::Error(
        String::New("Could not parse encoding. This is a Node bug.")));
}

#if EVCOM_HAVE_GNUTLS

Handle<Value> Connection::SetSecure(const Arguments& args) {
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);
  int r;

  connection->secure_ = true;

  // Create credentials

  gnutls_certificate_allocate_credentials(&connection->credentials);

  if (args[1]->IsString()) {
    String::Utf8Value caString(args[1]->ToString());
    gnutls_datum_t datum = { reinterpret_cast<unsigned char*>(*caString)
                           , caString.length() 
                           };
    r = gnutls_certificate_set_x509_trust_mem(connection->credentials,
                                              &datum, GNUTLS_X509_FMT_PEM);
  }

  if (args[2]->IsString()) {
    String::Utf8Value crlString(args[2]->ToString());
    gnutls_datum_t datum = { reinterpret_cast<unsigned char*>(*crlString)
                           , crlString.length()
                           };
    r = gnutls_certificate_set_x509_crl_mem(connection->credentials,
                                            &datum, GNUTLS_X509_FMT_PEM);
  }

  if (args[3]->IsString() && args[4]->IsString()) {
    String::Utf8Value keyString(args[3]->ToString());
    String::Utf8Value certString(args[4]->ToString());
    gnutls_datum_t datum_key = { reinterpret_cast<unsigned char*>(*keyString)
                               , keyString.length() 
                               };
    gnutls_datum_t datum_cert = { reinterpret_cast<unsigned char*>(*certString)
                                , certString.length()
                                };
    r = gnutls_certificate_set_x509_key_mem(connection->credentials,
                                            &datum_cert, &datum_key,
                                            GNUTLS_X509_FMT_PEM);
  }

  // Create the session object

  init_tls_session(&connection->stream_,
                   connection->credentials,
                   GNUTLS_CLIENT);

  return Undefined();
}


Handle<Value> Connection::VerifyPeer(const Arguments& args) {
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);

  const gnutls_datum_t * cert_chain;
  uint cert_chain_length;
  gnutls_x509_crl_t *crl_list;
  uint crl_list_size;
  gnutls_x509_crt_t *ca_list;
  uint ca_list_size;
  int r;

  if (!connection->secure_) {
    return Undefined();
  }

  cert_chain = gnutls_certificate_get_peers(connection->stream_.session,
                                            &cert_chain_length);

  gnutls_certificate_get_x509_crls(connection->credentials,
                                   &crl_list,
                                   &crl_list_size);

  gnutls_certificate_get_x509_cas(connection->credentials,
                                  &ca_list,
                                  &ca_list_size);

  r = verify_certificate_chain(connection->stream_.session,
                               connection->host_,
                               cert_chain,
                               cert_chain_length,
                               crl_list,
                               crl_list_size,
                               ca_list,
                               ca_list_size);

  return scope.Close(Integer::New(r));
}

Handle<Value> Connection::GetPeerCertificate(const Arguments& args) {
  HandleScope scope;

  Connection *connection = ObjectWrap::Unwrap<Connection>(args.This());
  assert(connection);

  if (!connection->secure_) {
    return Undefined();
  }

  const gnutls_datum_t * cert_chain;
  uint cert_chain_length;
  char *name;
  size_t name_size;
  gnutls_x509_crt_t cert;
  cert_chain = gnutls_certificate_get_peers(connection->stream_.session,
                                            &cert_chain_length);

  if ( (cert_chain_length == 0) || (cert_chain == NULL) ) {
    return Undefined();
  }

  gnutls_x509_crt_init(&cert);
  gnutls_x509_crt_import(cert, &cert_chain[0], GNUTLS_X509_FMT_DER);


  gnutls_x509_crt_get_dn(cert, NULL, &name_size);
  name = (char *)malloc(name_size);
  gnutls_x509_crt_get_dn(cert, name, &name_size);

  Local<String> dnString = String::New(name);
  free(name);
  gnutls_x509_crt_deinit(cert);
  return scope.Close(dnString);
}
#endif

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

  if (connection->host_ != NULL) {
    free(connection->host_);
    connection->host_ = NULL;
  }

  return Undefined();
}

Handle<Value> Connection::ForceClose(const Arguments& args) {
  HandleScope scope;
  Connection *connection = ObjectWrap::Unwrap<Connection>(args.Holder());
  assert(connection);

  connection->ForceClose();
  connection->Unref();
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
  return ThrowException(Exception::Error(String::New(
        "connection.send() has been renamed to connection.write(). "
        "(Also the 'receive' event has been renamed to 'data' and "
        "the 'eof' event has been renamed to 'end'.)")));
}


Handle<Value> Connection::Write(const Arguments& args) {
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
  connection->Write(buf, written);
  delete [] buf;

  return scope.Close(Integer::New(written));
}

void Connection::OnReceive(const void *buf, size_t len) {
  HandleScope scope;
  Local<Value> data = Encode(buf, len, encoding_);
  Emit(data_symbol, 1, &data);
}

void Connection::OnClose() {
  HandleScope scope;

  Handle<Value> argv[2] = {
    stream_.errorno == 0 ? False() : True(),
    String::New(strerror(stream_.errorno))
  };

  Emit(close_symbol, 2, argv);
}

void Connection::OnConnect() {
  HandleScope scope;

  if (stream_.server) {
    Server *server = static_cast<Server*>(stream_.server->data);
    Local<Value> value = Local<Value>::New(handle_);
    server->Emit(connection_symbol, 1, &value);
  }

  Emit(connect_symbol, 0, NULL);
}

void Connection::OnTimeout() {
  HandleScope scope;
  Emit(timeout_symbol, 0, NULL);
}

void Connection::OnDrain() {
  HandleScope scope;
  Emit(drain_symbol, 0, NULL);
}

void Connection::OnEOF() {
  HandleScope scope;
  Emit(end_symbol, 0, NULL);
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
  #if EVCOM_HAVE_GNUTLS
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "setSecure", SetSecure);
  #endif

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
  js_connection->Set(remote_address_symbol, remote_address);
  js_connection->Set(server_symbol, handle_);

  Connection *connection = UnwrapConnection(js_connection);
  if (!connection) return NULL;

  #if EVCOM_HAVE_GNUTLS
  if (secure_) {
    connection->secure_ = true;
    connection->credentials = credentials;
    init_tls_session(&connection->stream_,
                     connection->credentials,
                     GNUTLS_SERVER);
  }
  #endif

  connection->Ref();

  return connection;
}

void Server::OnClose(int errorno) {
  HandleScope scope;

  Handle<Value> argv[1] = { Integer::New(errorno) };

  Emit(close_symbol, 1, argv);
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

  if (server->server_.errorno) {
    Local<Value> e = Exception::Error(
        String::NewSymbol(strerror(server->server_.errorno)));
    Local<Object> obj = e->ToObject();
    obj->Set(String::NewSymbol("errno"), Integer::New(server->server_.errorno));
    return ThrowException(e);
  }

  return Undefined();
}

#if EVCOM_HAVE_GNUTLS

Handle<Value> Server::SetSecure(const Arguments& args) {
  Server *server = ObjectWrap::Unwrap<Server>(args.Holder());
  assert(server);

  int r;

  server->secure_ = true;
  gnutls_certificate_allocate_credentials(&server->credentials);


  if (args[1]->IsString()) {
    String::Utf8Value caString(args[1]->ToString());
    gnutls_datum_t datum = { reinterpret_cast<unsigned char*>(*caString)
                           , caString.length()
                           };
    r = gnutls_certificate_set_x509_trust_mem(server->credentials,
                                              &datum, GNUTLS_X509_FMT_PEM);
  }


  if (args[2]->IsString()) {
    String::Utf8Value crlString(args[2]->ToString());
    gnutls_datum_t datum = { reinterpret_cast<unsigned char*>(*crlString)
                           , crlString.length()
                           };
    r = gnutls_certificate_set_x509_crl_mem(server->credentials,
                                            &datum, GNUTLS_X509_FMT_PEM);
  }

  if (args[3]->IsString() && args[4]->IsString()) {
    String::Utf8Value keyString(args[3]->ToString());
    String::Utf8Value certString(args[4]->ToString());
    gnutls_datum_t datum_key  = { reinterpret_cast<unsigned char*>(*keyString)
                                , keyString.length() 
                                };
    gnutls_datum_t datum_cert = { reinterpret_cast<unsigned char*>(*certString)
                                , certString.length()
                                };
    r = gnutls_certificate_set_x509_key_mem(server->credentials,
                                              &datum_cert, &datum_key,
                                              GNUTLS_X509_FMT_PEM);
  }

  return Undefined();
}

#endif

Handle<Value> Server::Close(const Arguments& args) {
  Server *server = ObjectWrap::Unwrap<Server>(args.Holder());
  assert(server);

  server->Close();
  return Undefined();
}

}  // namespace node




#if EVCOM_HAVE_GNUTLS
void init_tls_session(evcom_stream* stream_,
                      gnutls_certificate_credentials_t credentials,
                      gnutls_connection_end_t session_type) {
  gnutls_init(&stream_->session,
              session_type);
  if (session_type == GNUTLS_SERVER) {
    gnutls_certificate_server_set_request(stream_->session,
                                          GNUTLS_CERT_REQUEST);
  }
  gnutls_set_default_priority(stream_->session);
  const int cert_type_priority[] = { GNUTLS_CRT_X509, 0 };
  const int proto_type_priority[] = { GNUTLS_TLS1_0,
                                      GNUTLS_TLS1_1,
                                      GNUTLS_SSL3,
                                      0};
  gnutls_certificate_type_set_priority(stream_->session,
                                       cert_type_priority);
  gnutls_protocol_set_priority(stream_->session,
                               proto_type_priority);
  gnutls_credentials_set(stream_->session,
                         GNUTLS_CRD_CERTIFICATE,
                         credentials);
  evcom_stream_set_secure_session(stream_,
                                  stream_->session);
}


/* This function will try to verify the peer's certificate chain, and
 * also check if the hostname matches, and the activation, expiration dates.
 */
int verify_certificate_chain(gnutls_session_t session,
                             const char *hostname,
                             const gnutls_datum_t * cert_chain,
                             int cert_chain_length,
                             gnutls_x509_crl_t *crl_list,
                             int crl_list_size,
                             gnutls_x509_crt_t *ca_list,
                             int ca_list_size) {
  int r = 0;
  int i;
  int ss = 0;
  gnutls_x509_crt_t *cert;

  if ((cert_chain_length == 0) || (cert_chain == NULL)) {
    return JS_GNUTLS_CERT_UNDEFINED;
  }
  cert = (gnutls_x509_crt_t *)malloc(sizeof(*cert) * cert_chain_length);

  /* Import all the certificates in the chain to
   * native certificate format.
   */
  for (i = 0; i < cert_chain_length; i++) {
    gnutls_x509_crt_init(&cert[i]);
    gnutls_x509_crt_import(cert[i], &cert_chain[i], GNUTLS_X509_FMT_DER);
  }

  /* If the last certificate in the chain is self signed ignore it.
   * That is because we want to check against our trusted certificate
   * list.
   */

  if (gnutls_x509_crt_check_issuer(cert[cert_chain_length - 1],
                                    cert[cert_chain_length - 1]) > 0
                                    && cert_chain_length > 0) {
    cert_chain_length--;
    ss = 1;
  }

  /* Now verify the certificates against their issuers
   * in the chain.
   */
  for (i = 1; i < cert_chain_length; i++) {
    r = verify_cert2(cert[i - 1], cert[i], crl_list, crl_list_size);
    if (r < 0) goto out;
  }

  /* Here we must verify the last certificate in the chain against
   * our trusted CA list.
   */

  if (cert_chain_length>0) {
    r = verify_last_cert(cert[cert_chain_length - 1], ca_list, ca_list_size,
                       crl_list, crl_list_size);
    if (r < 0) goto out;
  } else {
    r = verify_last_cert(cert[0], ca_list, ca_list_size,
                          crl_list, crl_list_size);
    if (r < 0) goto out;
  }

  /* Check if the name in the first certificate matches our destination!
   */
  if (hostname != NULL) {
    if (!gnutls_x509_crt_check_hostname(cert[0], hostname)) {
      r = JS_GNUTLS_CERT_DOES_NOT_MATCH_HOSTNAME;
    }
  }

 out:

  for (i = 0; i < cert_chain_length+ss; i++) {
    gnutls_x509_crt_deinit(cert[i]);
  }

  return r;
}


/* Verifies a certificate against an other certificate
 * which is supposed to be it's issuer. Also checks the
 * crl_list if the certificate is revoked.
 */
int verify_cert2(gnutls_x509_crt_t crt,
                 gnutls_x509_crt_t issuer,
                 gnutls_x509_crl_t * crl_list,
                 int crl_list_size) {
  unsigned int output;
  int ret;
  time_t now = time(0);

  gnutls_x509_crt_verify(crt, &issuer, 1, 0, &output);

  if (output & GNUTLS_CERT_INVALID) {
    if (output & GNUTLS_CERT_SIGNER_NOT_FOUND) {
      return JS_GNUTLS_CERT_SIGNER_NOT_FOUND;
    }
    if (output & GNUTLS_CERT_SIGNER_NOT_CA) {
      return JS_GNUTLS_CERT_SIGNER_NOT_CA;
    }
    return JS_GNUTLS_CERT_SIGNER_NOT_CA;
  }


  /* Now check the expiration dates.
   */
  if (gnutls_x509_crt_get_activation_time(crt) > now) {
    return JS_GNUTLS_CERT_NOT_ACTIVATED;
  }

  if (gnutls_x509_crt_get_expiration_time(crt) < now) {
    return JS_GNUTLS_CERT_EXPIRED;
  }

  /* Check if the certificate is revoked.
   */
  ret = gnutls_x509_crt_check_revocation(crt, crl_list, crl_list_size);
  if (ret == 1) {
    return JS_GNUTLS_CERT_REVOKED;
  }

  return JS_GNUTLS_CERT_VALIDATED;
}


/* Verifies a certificate against our trusted CA list.
 * Also checks the crl_list if the certificate is revoked.
 */
int verify_last_cert(gnutls_x509_crt_t crt,
                     gnutls_x509_crt_t * ca_list,
                     int ca_list_size,
                     gnutls_x509_crl_t * crl_list,
                     int crl_list_size) {
  unsigned int output;
  int ret;
  time_t now = time(0);

  gnutls_x509_crt_verify(crt, ca_list, ca_list_size,
                         GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT, &output);

  if (output & GNUTLS_CERT_INVALID) {
    if (output & GNUTLS_CERT_SIGNER_NOT_CA) {
      return JS_GNUTLS_CERT_SIGNER_NOT_CA;
    }
    return JS_GNUTLS_CERT_INVALID;
  }


  /* Now check the expiration dates.
   */
  if (gnutls_x509_crt_get_activation_time(crt) > now) {
    return JS_GNUTLS_CERT_NOT_ACTIVATED;
  }

  if (gnutls_x509_crt_get_expiration_time(crt) < now) {
    return JS_GNUTLS_CERT_EXPIRED;
  }

  /* Check if the certificate is revoked.
   */
  ret = gnutls_x509_crt_check_revocation(crt, crl_list, crl_list_size);
  if (ret == 1) {
    return JS_GNUTLS_CERT_REVOKED;
  }

  return JS_GNUTLS_CERT_VALIDATED;
}
#endif  // EVCOM_HAVE_GNUTLS

