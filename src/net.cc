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


class Socket {
public:
  Socket (Handle<Object> obj, double timeout);
  ~Socket ();

  void SetEncoding (enum encoding);
  void SetTimeout (double);

  static Handle<Value> New (const Arguments& args);
  static Handle<Value> Write (const Arguments& args);
  static Handle<Value> Close (const Arguments& args);
  static Handle<Value> ConnectTCP (const Arguments& args);

  static void OnConnect (oi_socket *socket);
  static void OnRead (oi_socket *s, const void *buf, size_t count);
  static void OnDrain (oi_socket *s);
  static void OnError (oi_socket *s, oi_error e);
  static void OnClose (oi_socket *s);
  static void OnTimeout (oi_socket *s);

private:
  static Socket* Unwrap (Handle<Object> handle);
  static void MakeWeak (Persistent<Value> _, void *data);
  enum {UTF8, RAW} encoding_;
  oi_socket socket_;
  struct addrinfo *address_;
  Persistent<Object> handle_;
};

Handle<Value>
Socket::New(const Arguments& args)
{
  if (args.Length() > 1)
    return Undefined();

  HandleScope scope;

  // Default options 
  double timeout = 60.0; // in seconds
  enum {UTF8, RAW} encoding ;

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

  Socket *s = new Socket(args.Holder(), timeout);
  if(s == NULL)
    return Undefined(); // XXX raise error?

  return args.This();
}

Socket*
Socket::Unwrap (Handle<Object> handle) 
{
  HandleScope scope;
  Handle<External> field = Handle<External>::Cast(handle->GetInternalField(0));
  Socket* socket = static_cast<Socket*>(field->Value());
  return socket;
}

Handle<Value>
Socket::ConnectTCP (const Arguments& args)
{
  if (args.Length() < 1)
    return Undefined();

  HandleScope scope;
  Socket *socket = Socket::Unwrap(args.Holder());

  String::AsciiValue port(args[0]);

  char *host = NULL; 
  String::AsciiValue host_v(args[1]->ToString());
  if(args[1]->IsString()) {
    host = *host_v;
  }

  int r;

  /* FIXME Blocking DNS resolution. */
  printf("resolving host: %s, port: %s\n", host, *port);
  r = getaddrinfo (host, *port, &tcp_hints, &socket->address_);
  if(r != 0)  {
    perror("getaddrinfo");
    return Undefined(); 
  }

  r = oi_socket_connect (&socket->socket_, socket->address_);
  if(r != 0)  {
    perror("oi_socket_connect");
    return Undefined(); 
  }
  oi_socket_attach (&socket->socket_, node_loop());

  freeaddrinfo(socket->address_);
  socket->address_ = NULL;
  // TODO raise error if r != 0
    
  return Undefined(); 
}

Handle<Value>
Socket::Close (const Arguments& args) 
{
  HandleScope scope;
  Socket *socket = Socket::Unwrap(args.Holder());
  oi_socket_close(&socket->socket_);
  return Undefined();
}

void
Socket::MakeWeak (Persistent<Value> _, void *data)
{
  Socket *s = static_cast<Socket*> (data);
  delete s;
}

Socket::Socket(Handle<Object> handle, double timeout)
{
  oi_socket_init(&socket_, timeout);
  socket_.on_connect = Socket::OnConnect;
  socket_.on_read    = Socket::OnRead;
  socket_.on_drain   = Socket::OnDrain;
  socket_.on_error   = Socket::OnError;
  socket_.on_close   = Socket::OnClose;
  socket_.on_timeout = Socket::OnTimeout;
  socket_.data = this;

  HandleScope scope;
  handle_ = Persistent<Object>::New(handle);
  handle_->SetInternalField (0, External::New(this));
  handle_.MakeWeak (this, Socket::MakeWeak);

  encoding_ = UTF8;
}

Socket::~Socket ()
{
  oi_socket_close(&socket_);
  oi_socket_detach(&socket_);
  handle_.Dispose();
  handle_.Clear(); // necessary? 
}

Handle<Value>
Socket::Write (const Arguments& args) 
{
  HandleScope scope;

  Socket *socket = Socket::Unwrap(args.Holder());
 
  if (args[0] == Null()) {
    oi_socket_write_eof(&socket->socket_);

  } else if (args[0]->IsString()) {
    // utf8 encoding
    Local<String> s = args[0]->ToString();
    size_t length = s->Utf8Length();
    oi_buf *buf = oi_buf_new2(length);
    s->WriteUtf8(buf->base, length);
    oi_socket_write(&socket->socket_, buf);

  } else if (args[0]->IsArray()) {
    // raw encoding
    Handle<Array> array = Handle<Array>::Cast(args[0]);
    size_t length = array->Length();
    oi_buf *buf = oi_buf_new2(length);
    for (int i = 0; i < length; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf->base[i] = int_value->Int32Value();
    }
    oi_socket_write(&socket->socket_, buf);

  } else {
    // raise error bad argument.
    assert(0);
  }

  return Undefined();
}

void
Socket::OnConnect (oi_socket *s)
{
  Socket *socket = static_cast<Socket*> (s->data);

  HandleScope scope;

  Handle<Value> on_connect_value = socket->handle_->Get( String::NewSymbol("onConnect") );
  if (!on_connect_value->IsFunction())
    return; 
  Handle<Function> on_connect = Handle<Function>::Cast(on_connect_value);

  TryCatch try_catch;

  Handle<Value> r = on_connect->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}

void
Socket::OnRead (oi_socket *s, const void *buf, size_t count)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> onread_value = socket->handle_->Get( String::NewSymbol("onRead") );
  if (!onread_value->IsFunction()) return; 
  Handle<Function> onread = Handle<Function>::Cast(onread_value);

  const int argc = 1;
  Handle<Value> argv[argc];

  if(count) {
    if(socket->encoding_ == UTF8) {
      // utf8 encoding
      Handle<String> chunk = String::New((const char*)buf, count);
      argv[0] = chunk;
    } else {
      // raw encoding
      Local<Array> array = Array::New(count);
      for (int i = 0; i < count; i++) {
        int val = static_cast<const int*>(buf)[i];
        array->Set(Integer::New(i), Integer::New(val));
      }
      argv[0] = array;
    }
  } else {
    argv[0] = Local<Value>::New(Null());
  }

  TryCatch try_catch;

  Handle<Value> r = onread->Call(socket->handle_, argc, argv);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}

void
Socket::OnClose (oi_socket *s)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> onclose_value = socket->handle_->Get( String::NewSymbol("onClose") );
  if (!onclose_value->IsFunction()) return; 
  Handle<Function> onclose = Handle<Function>::Cast(onclose_value);

  TryCatch try_catch;

  Handle<Value> r = onclose->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}

void
Socket::OnDrain (oi_socket *s)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> onclose_value = socket->handle_->Get( String::NewSymbol("onDrain") );
  if (!onclose_value->IsFunction()) return; 
  Handle<Function> onclose = Handle<Function>::Cast(onclose_value);

  TryCatch try_catch;

  Handle<Value> r = onclose->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}


void
Socket::OnError (oi_socket *s, oi_error e)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> onclose_value = socket->handle_->Get( String::NewSymbol("onError") );
  if (!onclose_value->IsFunction()) return; 
  Handle<Function> onclose = Handle<Function>::Cast(onclose_value);

  TryCatch try_catch;

  Handle<Value> r = onclose->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}

void
Socket::OnTimeout (oi_socket *s)
{
  Socket *socket = static_cast<Socket*> (s->data);
  HandleScope scope;

  Handle<Value> onclose_value = socket->handle_->Get( String::NewSymbol("onTimeout") );
  if (!onclose_value->IsFunction()) return; 
  Handle<Function> onclose = Handle<Function>::Cast(onclose_value);

  TryCatch try_catch;

  Handle<Value> r = onclose->Call(socket->handle_, 0, NULL);

  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);
}


void
NodeInit_net (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> socket_template = FunctionTemplate::New(Socket::New);
  socket_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("Socket"), socket_template->GetFunction());

  NODE_SET_METHOD(socket_template->InstanceTemplate(), "connectTCP", Socket::ConnectTCP);
  //NODE_SET_METHOD(socket_template->InstanceTemplate(), "connectUNIX", Socket::ConnectUNIX);
  NODE_SET_METHOD(socket_template->InstanceTemplate(), "write", Socket::Write);
  NODE_SET_METHOD(socket_template->InstanceTemplate(), "close", Socket::Close);

}

