#include "net.h"
#include "node.h"

#include <oi_socket.h>
#include <oi_buf.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <strings.h>

using namespace v8;

static Persistent<String> readyState_str; 

static Persistent<Integer> readyStateCONNECTING; 
static Persistent<Integer> readyStateOPEN; 
static Persistent<Integer> readyStateCLOSED; 

enum encoding {UTF8, RAW};

class Socket {
public:
  Socket (Handle<Object> obj, double timeout);
  ~Socket ();

  int ConnectTCP (char *port, char *host);
  void Write (Handle<Value> arg);
  void Disconnect ();

  void SetEncoding (enum encoding);
  void SetTimeout (double);

  void OnConnect ();
  void OnRead (const void *buf, size_t count);
  void OnDrain ();
  void OnError (oi_error e);
  void OnClose ();

private:
  oi_socket socket_;
  struct addrinfo *address_;
  Persistent<Object> js_object_;
};

static void
on_connect (oi_socket *socket)
{
  Socket *s = static_cast<Socket*> (socket->data);
  s->OnConnect();
}

static void
on_read (oi_socket *socket, const void *buf, size_t count)
{
  Socket *s = static_cast<Socket*> (socket->data);
  s->OnRead(buf, count);
}

static void
on_drain (oi_socket *socket)
{
  Socket *s = static_cast<Socket*> (socket->data);
  s->OnDrain();
}

static void
on_error (oi_socket *socket, oi_error e)
{
  Socket *s = static_cast<Socket*> (socket->data);
  s->OnError(e);
}

static void
on_timeout (oi_socket *socket)
{
  Socket *s = static_cast<Socket*> (socket->data);
  s->OnTimeout(e);
}

static void
on_close (oi_socket *socket)
{
  Socket *s = static_cast<Socket*> (socket->data);
  s->OnClose();
}


static Handle<Value>
NewSocket (const Arguments& args)
{
  if (args.Length() > 1)
    return Undefined();

  HandleScope scope;

  // Default options 
  double timeout = 60.0; // in seconds
  enum {RAW, UTF8} encoding = RAW;

  // Set options from argument.
  if (args.Length() == 1 && args[0]->IsObject()) {
    Local<Object> options = args[0]->ToObject();
    Local<Value> timeout_value = options->Get(String::NewSymbol("timeout"));
    Local<Value> encoding_value = options->Get(String::NewSymbol("encoding"));

    if (timeout_value->IsNumber()) {
      // timeout is specified in milliseconds like other time
      // values in javascript
      timeout = timeout_value->NumberValue() / 1000;
    }

    if (encoding_value->IsString()) {
      Local<String> encoding_string = encoding_value->ToString();
      char buf[5]; // need enough room for "utf8" or "raw"
      encoding_string->WriteAscii(buf, 0, 4);
      buf[4] = '\0';
      if(strcasecmp(buf, "utf8") == 0) encoding = UTF8;
    }
  }

  Socket *s = new Socket(args.This(), timeout);
  if(s == NULL)
    return Undefined(); // XXX raise error?

  return args.This();
}

static Socket*
Unwrapsocket (Handle<Object> obj) 
{
  HandleScope scope;
  Handle<External> field = Handle<External>::Cast(obj->GetInternalField(0));
  Socket* socket = static_cast<Socket*>(field->Value());
  return socket;
}

static Handle<Value>
SocketConnectTCPCallback (const Arguments& args)
{
  if (args.Length() < 1)
    return Undefined();

  HandleScope scope;
  Socket *socket = Unwrapsocket(args.Holder());

  String::AsciiValue port(args[0]);

  char *host = NULL; 
  String::AsciiValue host_v(args[1]->ToString());
  if(args[1]->IsString()) {
    host = *host_v;
  }

  int r = socket->ConnectTCP(*port, host);
  // TODO raise error if r != 0
    
  return Undefined(); 
}

static Handle<Value>
SocketWriteCallback (const Arguments& args) 
{
  HandleScope scope;
  Socket *socket = Unwrapsocket(args.Holder());
  socket->Write(args[0]);
  return Undefined();
}

static Handle<Value>
SocketCloseCallback (const Arguments& args) 
{
  HandleScope scope;
  Socket *socket = Unwrapsocket(args.Holder());
  socket->Disconnect();
  return Undefined();
}

static void
DestroySocket (Persistent<Value> _, void *data)
{
  Socket *s = static_cast<Socket*> (data);
  delete s;
}

Socket::Socket(Handle<Object> js_object, double timeout)
{
  oi_socket_init(&socket_, timeout);
  socket_.on_connect = on_connect;
  socket_.on_read    = on_read;
  socket_.on_drain   = on_drain;
  socket_.on_error   = on_error;
  socket_.on_close   = on_close;
  socket_.on_timeout = on_timeout;
  socket_.data = this;

  HandleScope scope;
  js_object_ = Persistent<Object>::New(js_object);
  js_object_->SetInternalField (0, External::New(this));
  js_object_.MakeWeak (this, DestroySocket);
}

Socket::~Socket ()
{
  Disconnect();
  oi_socket_detach(&socket_);
  js_object_.Dispose();
  js_object_.Clear(); // necessary? 
}

static struct addrinfo tcp_hints = 
/* ai_flags      */ { AI_PASSIVE
/* ai_family     */ , AF_UNSPEC
/* ai_socktype   */ , SOCK_STREAM
/* ai_protocol   */ , 0
/* ai_addrlen    */ , 0
/* ai_addr       */ , 0
/* ai_canonname  */ , 0
/* ai_next       */ , 0
                    };

int
Socket::ConnectTCP(char *port, char *host)
{
  int r;

  HandleScope scope;

  js_object_->Set(readyState_str, readyStateCONNECTING);

  /* FIXME Blocking DNS resolution. */
  printf("resolving host: %s, port: %s\n", host, port);
  r = getaddrinfo (host, port, &tcp_hints, &address);
  if(r != 0)  {
    perror("getaddrinfo");
    return r;
  }

  r = oi_socket_connect (&socket, address);
  if(r != 0)  {
    perror("oi_socket_connect");
    return r;
  }
  oi_socket_attach (&socket, node_loop());

  freeaddrinfo(address);
  address = NULL;
}

void Socket::Write (Handle<Value> arg)
{
  HandleScope scope;
 
  if (arg == Null()) {

    oi_socket_write_eof(&socket);

  } else if (arg->IsString()) {
    Local<String> s = arg->ToString();

    size_t l1 = s->Utf8Length(), l2;
    oi_buf *buf = oi_buf_new2(l1);
    l2 = s->WriteUtf8(buf->base, l1);
    assert(l1 == l2);

    oi_socket_write(&socket, buf);

  } else if (arg->IsArray()) {
    size_t length = array->Length();
    Handle<Array> array = Handle<Array>::Cast(arg);
    oi_buf *buf = oi_buf_new2(length);
    for (int i = 0; i < length; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf[i] = int_value->Int32Value();
    }

    oi_socket_write(&socket, buf);

  } else {
    // raise error bad argument.
    assert(0);
  }
}
 
void
Socket::Disconnect()
{
  oi_socket_close(&socket);
}

void
Socket::OnConnect()
{
  HandleScope scope;

  assert(READY_STATE_CONNECTING == ReadyState());
  js_object_->Set(readyState_str, readyStateOPEN);

  Handle<Value> on_connect_value = js_object_->Get( String::NewSymbol("on_connect") );
  if (!on_connect_value->IsFunction())
    return; 
  Handle<Function> on_connect = Handle<Function>::Cast(on_connect_value);

  TryCatch try_catch;

  Handle<Value> r = on_connect->Call(js_object_, 0, NULL);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}

void
Socket::OnRead (const void *buf, size_t count)
{
  HandleScope scope;

  assert(READY_STATE_OPEN == ReadyState());

  Handle<Value> onread_value = js_object_->Get( String::NewSymbol("on_read") );
  if (!onread_value->IsFunction()) return; 
  Handle<Function> onread = Handle<Function>::Cast(onread_value);

  const int argc = 1;
  Handle<Value> argv[argc];

  if(count) {
    Handle<String> chunk = String::New((const char*)buf, count); // TODO binary data?
    argv[0] = chunk;
  } else {
    // TODO eof? delete write method?
    argv[0] = Null();
  }

  TryCatch try_catch;

  Handle<Value> r = onread->Call(js_object_, argc, argv);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}

void
Socket::OnClose ()
{
  HandleScope scope;

  printf("onclose readyState %d\n", ReadyState());

  assert(READY_STATE_OPEN == ReadyState());
  js_object_->Set(readyState_str, readyStateCLOSED);

  Handle<Value> onclose_value = js_object_->Get( String::NewSymbol("on_close") );
  if (!onclose_value->IsFunction()) return; 
  Handle<Function> onclose = Handle<Function>::Cast(onclose_value);

  TryCatch try_catch;

  Handle<Value> r = onclose->Call(js_object_, 0, NULL);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}

void
NodeInit_net (Handle<Object> target)
{
  HandleScope scope;

  //
  // Socket
  //
  Local<FunctionTemplate> socket_template = FunctionTemplate::New(NewSocket);
  target->Set(String::NewSymbol("Socket"), socket_template->GetFunction());
  socket_template->InstanceTemplate()->SetInternalFieldCount(1);

  // socket.connectTCP()
  Local<FunctionTemplate> socket_connect_tcp = 
      FunctionTemplate::New(SocketConnectTCPCallback);
  socket_template->InstanceTemplate()->Set(String::NewSymbol("connectTCP"), 
                                           socket_connect_tcp->GetFunction());

  // socket.connectUNIX()
  Local<FunctionTemplate> socket_connect_unix = 
      FunctionTemplate::New(SocketConnectUNIXCallback);
  socket_template->InstanceTemplate()->Set(String::NewSymbol("connectUNIX"), 
                                           socket_connect_unix->GetFunction());

  // socket.write()
  Local<FunctionTemplate> socket_write =
      FunctionTemplate::New(SocketWriteCallback);
  socket_template->InstanceTemplate()->Set(String::NewSymbol("write"), 
                                           socket_write->GetFunction());

  // socket.close()
  Local<FunctionTemplate> socket_close = 
      FunctionTemplate::New(SocketCloseCallback);
  socket_template->InstanceTemplate()->Set(String::NewSymbol("close"), 
                                           socket_close->GetFunction());

  //
  // Server
  //
  Local<FunctionTemplate> server_template = FunctionTemplate::New(NewServer);
  target->Set(String::NewSymbol("Server"), server_template->GetFunction());
  server_template->InstanceTemplate()->SetInternalFieldCount(1);

  // server.listenTCP()
  Local<FunctionTemplate> server_listenTCP =
      FunctionTemplate::New(ServerListenTCPCallback);
  server_template->InstanceTemplate()->Set(String::NewSymbol("listenTCP"), 
                                           server_listenTCP->GetFunction());

  // server.listenUNIX()
  Local<FunctionTemplate> server_listenUNIX =
      FunctionTemplate::New(ServerListenUNIXCallback);
  server_template->InstanceTemplate()->Set(String::NewSymbol("listenUNIX"), 
                                           server_listenTCP->GetFunction());

  // server.close()
  Local<FunctionTemplate> server_close = FunctionTemplate::New(ServerCloseCallback);
  server_template->InstanceTemplate()->Set(String::NewSymbol("close"), 
                                           server_close->GetFunction());
}

