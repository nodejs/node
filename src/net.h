#ifndef node_net_h
#define node_net_h

#include "node.h"
#include <v8.h>
#include <oi_socket.h>

namespace node {

class Connection : public ObjectWrap {
public:
  static void Initialize (v8::Handle<v8::Object> target);

  Connection (v8::Handle<v8::Object> handle); 
  ~Connection () {
    Disconnect();
  }

  int Connect (struct addrinfo *address) {
    return oi_socket_connect (&socket_, address);
  }

  void Send (oi_buf *buf) { 
    oi_socket_write (&socket_, buf);
  }

  void SendEOF (void) { 
    oi_socket_write_eof (&socket_);
  }

  void Disconnect (void) {
    oi_socket_close (&socket_);
  }

protected:
  static v8::Handle<v8::Value> v8New (const v8::Arguments& args);
  static v8::Handle<v8::Value> v8Connect (const v8::Arguments& args);
  static v8::Handle<v8::Value> v8Send (const v8::Arguments& args);
  static v8::Handle<v8::Value> v8SendEOF (const v8::Arguments& args);
  static v8::Handle<v8::Value> v8Disconnect (const v8::Arguments& args);

  void OnConnect (void);
  void OnReceive (const void *buf, size_t len);
  void OnDrain (void);
  void OnEOF (void);
  void OnDisconnect (void);
  void OnError (oi_error e);
  void OnTimeout (void);

  v8::Local<v8::Object> GetProtocol (void);
  static v8::Local<v8::Object> NewServerSideInstance ( 
      v8::Local<v8::Function> protocol, 
      v8::Handle<v8::Object> server);

private:
  static void _OnConnect (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnConnect();
  }

  static void _OnReceive (oi_socket *s, const void *buf, size_t len) {
    Connection *connection = static_cast<Connection*> (s->data);
    if (len == 0)
      connection->OnEOF();
    else
      connection->OnReceive(buf, len);
  }

  static void _OnDrain (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnDrain();
  }

  static void _OnError (oi_socket *s, oi_error e) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnError(e);
  }

  static void _OnDisconnect (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnDisconnect();
  }

  static void _OnTimeout (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnTimeout();
  }

  static int Resolve (eio_req *req);
  static int AfterResolve (eio_req *req);

  enum encoding encoding_;
  char *host_;
  char *port_;
  oi_socket socket_;

  friend class Acceptor;
};

class Acceptor : public ObjectWrap {
public:
  static void Initialize (v8::Handle<v8::Object> target);

  Acceptor (v8::Handle<v8::Object> handle, v8::Handle<v8::Object> options);
  ~Acceptor () { Close(); }

  int Listen (struct addrinfo *address) { 
    int r = oi_server_listen (&server_, address); 
    if(r != 0) return r;
    oi_server_attach (EV_DEFAULT_ &server_); 
    return 0;
  }

  void Close ( ) { 
    oi_server_close (&server_); 
  }

protected:
  Connection* OnConnection (struct sockaddr *addr, socklen_t len);
  void OnError (struct oi_error error);

  static v8::Handle<v8::Value> v8New (const v8::Arguments& args);
  static v8::Handle<v8::Value> v8Listen (const v8::Arguments& args);
  static v8::Handle<v8::Value> v8Close (const v8::Arguments& args);

private:
  static oi_socket* _OnConnection (oi_server *s, struct sockaddr *addr, socklen_t len) {
    Acceptor *acceptor = static_cast<Acceptor*> (s->data);
    Connection *connection = acceptor->OnConnection (addr, len);
    if (connection)
      return &connection->socket_;
    else
      return NULL;
  }

  static void _OnError (oi_server *s, struct oi_error error) {
    Acceptor *acceptor = static_cast<Acceptor*> (s->data);
    acceptor->OnError (error);
  }

  oi_server server_;
};

} // namespace node
#endif
