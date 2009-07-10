#ifndef node_net_h
#define node_net_h

#include "node.h"
#include "events.h"
#include <v8.h>
#include <oi_socket.h>

namespace node {

class Server;

class Connection : public EventEmitter {
public:
  static void Initialize (v8::Handle<v8::Object> target);

protected:
  /* v8 interface */
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect (const v8::Arguments& args);
  static v8::Handle<v8::Value> Send (const v8::Arguments& args);
  static v8::Handle<v8::Value> SendUtf8 (const v8::Arguments& args);
  static v8::Handle<v8::Value> Close (const v8::Arguments& args);
  static v8::Handle<v8::Value> FullClose (const v8::Arguments& args);
  static v8::Handle<v8::Value> ForceClose (const v8::Arguments& args);
  static v8::Handle<v8::Value> SetEncoding (const v8::Arguments& args);

  static v8::Handle<v8::Value> ReadyStateGetter (v8::Local<v8::String> _,
      const v8::AccessorInfo& info);

  Connection (void) : EventEmitter() 
  {
    encoding_ = RAW;

    host_ = NULL;
    port_ = NULL;

    Init();
  }
  virtual ~Connection (void);

  int Connect (struct addrinfo *address) {
    return oi_socket_connect (&socket_, address);
  }
  void Send (oi_buf *buf) { oi_socket_write(&socket_, buf); }
  void Close (void) { oi_socket_close(&socket_); }
  void FullClose (void) { oi_socket_full_close(&socket_); }
  void ForceClose (void) { oi_socket_force_close(&socket_); }

  virtual void OnConnect (void);
  virtual void OnReceive (const void *buf, size_t len);
  virtual void OnDrain (void);
  virtual void OnEOF (void);
  virtual void OnDisconnect (void);
  virtual void OnTimeout (void);

  v8::Local<v8::Object> GetProtocol (void);

  enum encoding encoding_;

  enum readyState { OPEN, OPENING, CLOSED, READ_ONLY, WRITE_ONLY };
  bool opening;
  enum readyState ReadyState (void);

private:

  /* liboi callbacks */
  static void on_connect (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnConnect();
  }

  static void on_read (oi_socket *s, const void *buf, size_t len) {
    Connection *connection = static_cast<Connection*> (s->data);
    if (len == 0)
      connection->OnEOF();
    else
      connection->OnReceive(buf, len);
  }

  static void on_drain (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnDrain();
  }

  static void on_close (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnDisconnect();

    /*
    if (s->errorno)
      printf("socket died with error %d\n", s->errorno);
    */

    connection->Detach();
  }

  static void on_timeout (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnTimeout();
  }

  void Init (void); // constructor helper.

  static int Resolve (eio_req *req);
  static int AfterResolve (eio_req *req);
  char *host_;
  char *port_;
  oi_socket socket_;

  friend class Server;
};

class Server : public EventEmitter {
public:
  static void Initialize (v8::Handle<v8::Object> target);

protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> Listen (const v8::Arguments& args);
  static v8::Handle<v8::Value> Close (const v8::Arguments& args);

  Server (void) : EventEmitter() 
  {
    oi_server_init(&server_, 1024);
    server_.on_connection = Server::on_connection;
    server_.data = this;
  }
  virtual ~Server () { Close(); }

  int Listen (struct addrinfo *address) { 
    int r = oi_server_listen (&server_, address); 
    if(r != 0) return r;
    oi_server_attach (EV_DEFAULT_ &server_); 
    Attach();
    return 0;
  }

  void Close ( ) { 
    oi_server_close (&server_); 
    Detach();
  }

  virtual v8::Handle<v8::FunctionTemplate> GetConnectionTemplate (void);
  virtual Connection* UnwrapConnection (v8::Local<v8::Object> connection);

private:
  Connection* OnConnection (struct sockaddr *addr, socklen_t len);
  static oi_socket* on_connection (oi_server *s, struct sockaddr *addr, socklen_t len) {
    Server *server = static_cast<Server*> (s->data);
    Connection *connection = server->OnConnection (addr, len);
    return &connection->socket_;
  }

  oi_server server_;
};

} // namespace node
#endif
