#ifndef node_net_h
#define node_net_h

#include "node.h"
#include <v8.h>
#include <oi_socket.h>

namespace node {

class Acceptor;

class Connection : public ObjectWrap {
public:
  static void Initialize (v8::Handle<v8::Object> target);

  virtual size_t size (void) { return sizeof(Connection); };

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

  static v8::Handle<v8::Value> EncodingGetter (v8::Local<v8::String> _,
      const v8::AccessorInfo& info);
  static void EncodingSetter (v8::Local<v8::String> _,
      v8::Local<v8::Value> value, const v8::AccessorInfo& info);

  static v8::Handle<v8::Value> ReadyStateGetter (v8::Local<v8::String> _,
      const v8::AccessorInfo& info);

  Connection (v8::Handle<v8::Object> handle); 
  virtual ~Connection ();

  int Connect (struct addrinfo *address) { return oi_socket_connect (&socket_, address); }
  void Send (oi_buf *buf) { oi_socket_write(&socket_, buf); }
  void Close (void) { oi_socket_close(&socket_); }
  void FullClose (void) { oi_socket_full_close(&socket_); }
  void ForceClose (void) { oi_socket_force_close(&socket_); }

  void SetAcceptor (v8::Handle<v8::Object> acceptor_handle);

  virtual void OnConnect (void);
  virtual void OnReceive (const void *buf, size_t len);
  virtual void OnDrain (void);
  virtual void OnEOF (void);
  virtual void OnDisconnect (void);
  virtual void OnTimeout (void);

  v8::Local<v8::Object> GetProtocol (void);

  enum encoding encoding_;

  enum readyState { OPEN, CLOSED, READ_ONLY, WRITE_ONLY };
  enum readyState ReadyState ( )
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
    
    return CLOSED;
  }

private:

  /* liboi callbacks */
  static void on_connect (oi_socket *s) {
    Connection *connection = static_cast<Connection*> (s->data);
    connection->OnConnect();
  }

  static void on_read (oi_socket *s, const void *buf, size_t len) {
    Connection *connection = static_cast<Connection*> (s->data);
    v8::V8::ResumeProfiler();
    if (len == 0)
      connection->OnEOF();
    else
      connection->OnReceive(buf, len);
    v8::V8::PauseProfiler();
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

  static int Resolve (eio_req *req);
  static int AfterResolve (eio_req *req);
  char *host_;
  char *port_;
  oi_socket socket_;

  friend class Acceptor;
};

class Acceptor : public ObjectWrap {
public:
  static void Initialize (v8::Handle<v8::Object> target);

  virtual size_t size (void) { return sizeof(Acceptor); };

protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> Listen (const v8::Arguments& args);
  static v8::Handle<v8::Value> Close (const v8::Arguments& args);

  Acceptor (v8::Handle<v8::Object> handle, 
            v8::Handle<v8::Function> connection_handler, 
            v8::Handle<v8::Object> options);
  virtual ~Acceptor () { Close(); }

  v8::Local<v8::Function> GetConnectionHandler (void);

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

  virtual Connection* OnConnection (struct sockaddr *addr, socklen_t len);

private:
  static oi_socket* on_connection (oi_server *s, struct sockaddr *addr, socklen_t len) {
    Acceptor *acceptor = static_cast<Acceptor*> (s->data);
    Connection *connection = acceptor->OnConnection (addr, len);
    if (connection)
      return &connection->socket_;
    else
      return NULL;
  }

  oi_server server_;
};

} // namespace node
#endif
