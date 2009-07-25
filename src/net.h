#ifndef node_net_h
#define node_net_h

#include "node.h"
#include "events.h"
#include <v8.h>
#include <evcom.h>

#include <string.h>

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
    return evcom_socket_connect (&socket_, address);
  }
  void Send (evcom_buf *buf) { evcom_socket_write(&socket_, buf); }
  void Close (void) { evcom_socket_close(&socket_); }
  void FullClose (void) { evcom_socket_full_close(&socket_); }
  void ForceClose (void) { evcom_socket_force_close(&socket_); }

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
  static void on_connect (evcom_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnConnect();
  }

  static void on_read (evcom_socket *s, const void *buf, size_t len) {
    Connection *connection = static_cast<Connection*> (s->data);
    assert(connection->attached_);
    if (len == 0)
      connection->OnEOF();
    else
      connection->OnReceive(buf, len);
  }

  static void on_drain (evcom_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnDrain();
  }

  static void on_close (evcom_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);

    assert(connection->socket_.fd < 0);
    assert(connection->socket_.read_action == NULL);
    assert(connection->socket_.write_action == NULL);

    connection->OnDisconnect();

    if (s->errorno) {
      printf("socket died: %s\n", strerror(s->errorno));
    }

    assert(connection->attached_);

    connection->Detach();
  }

  static void on_timeout (evcom_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnTimeout();
  }

  void Init (void); // constructor helper.

  static int Resolve (eio_req *req);
  static int AfterResolve (eio_req *req);
  char *host_;
  char *port_;
  evcom_socket socket_;

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
    evcom_server_init(&server_);
    server_.on_connection = Server::on_connection;
    server_.on_close = Server::on_close;
    server_.data = this;
  }

  virtual ~Server () {
    evcom_server_close (&server_); 
  }

  int Listen (struct addrinfo *address) { 
    int r = evcom_server_listen (&server_, address, 1024); 
    if(r != 0) return r;
    evcom_server_attach (EV_DEFAULT_ &server_); 
    Attach();
    return 0;
  }

  void Close ( ) {
    evcom_server_close (&server_); 
  }

  virtual v8::Handle<v8::FunctionTemplate> GetConnectionTemplate (void);
  virtual Connection* UnwrapConnection (v8::Local<v8::Object> connection);

private:
  Connection* OnConnection (struct sockaddr *addr);
  static evcom_socket* on_connection (evcom_server *s, struct sockaddr *addr) {
    Server *server = static_cast<Server*> (s->data);
    Connection *connection = server->OnConnection (addr);
    return &connection->socket_;
  }

  void OnClose (int errorno);
  static void on_close (evcom_server *s, int errorno) {
    Server *server = static_cast<Server*> (s->data);
    server->OnClose(errorno);
    server->Detach();
  }

  evcom_server server_;
};

} // namespace node
#endif
