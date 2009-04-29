#ifndef node_net_h
#define node_net_h

#include "node.h"
#include <v8.h>
#include <oi_socket.h>

namespace node {

class Server : ObjectWrap {
public:
  Server (v8::Handle<v8::Object> handle, int backlog);
  ~Server ();

  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> ListenTCP (const v8::Arguments& args);
  static v8::Handle<v8::Value> Close (const v8::Arguments& args);

  static void Initialize (v8::Handle<v8::Object> target);
private:
  static oi_socket* OnConnection (oi_server *, struct sockaddr *, socklen_t);
  oi_server server_;
};

static v8::Persistent<v8::FunctionTemplate> socket_template;

class Socket : ObjectWrap {
public:
  Socket (v8::Handle<v8::Object> handle, double timeout);
  ~Socket ();
  // also a constructor
  static Socket* NewConnection (double timeout);


  void SetEncoding (v8::Handle<v8::Value>);
  void SetTimeout (double);

  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> Write (const v8::Arguments& args);
  static v8::Handle<v8::Value> Close (const v8::Arguments& args);
  static v8::Handle<v8::Value> ConnectTCP (const v8::Arguments& args);
  static v8::Handle<v8::Value> SetEncoding (const v8::Arguments& args);

  static void Initialize (v8::Handle<v8::Object> target);
private:
  static void OnConnect (oi_socket *socket);
  static void OnRead (oi_socket *s, const void *buf, size_t count);
  static void OnDrain (oi_socket *s);
  static void OnError (oi_socket *s, oi_error e);
  static void OnClose (oi_socket *s);
  static void OnTimeout (oi_socket *s);

  static int Resolve (eio_req *req);
  static int AfterResolve (eio_req *req);

  enum encoding encoding_;
  oi_socket socket_;

  char *host_;
  char *port_;

  friend class Server;
};

} // namespace node
#endif
