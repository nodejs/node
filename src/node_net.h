// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef SRC_NET_H_
#define SRC_NET_H_

#include <node.h>
#include <node_events.h>
#include <v8.h>
#include <evcom.h>

#if EVCOM_HAVE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif


namespace node {

class Server;

class Connection : public EventEmitter {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  /* v8 interface */
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect(const v8::Arguments& args);
  static v8::Handle<v8::Value> Send(const v8::Arguments& args);
  static v8::Handle<v8::Value> Write(const v8::Arguments& args);
  static v8::Handle<v8::Value> SendUtf8(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);
  static v8::Handle<v8::Value> ForceClose(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetEncoding(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadPause(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadResume(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetTimeout(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetNoDelay(const v8::Arguments& args);

  static v8::Handle<v8::Value> ReadyStateGetter(v8::Local<v8::String> _,
      const v8::AccessorInfo& info);
  static v8::Handle<v8::Value> FDGetter(v8::Local<v8::String> _,
      const v8::AccessorInfo& info);

  #if EVCOM_HAVE_GNUTLS
  static v8::Handle<v8::Value> SetSecure(const v8::Arguments& args);
  static v8::Handle<v8::Value> VerifyPeer(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPeerCertificate(const v8::Arguments& args);
  #endif

  Connection() : EventEmitter() {
    encoding_ = BINARY;

    host_ = NULL;
    port_ = NULL;

    Init();
  }
  virtual ~Connection();

  int Connect(struct sockaddr *address) {
    return evcom_stream_connect(&stream_, address);
  }

  void Write(const char *buf, size_t len) {
    evcom_stream_write(&stream_, buf, len);
  }

  void Close() {
    evcom_stream_close(&stream_);
  }

  void ForceClose() {
    evcom_stream_force_close(&stream_);
  }

  void ReadPause() {
    evcom_stream_read_pause(&stream_);
  }

  void ReadResume() {
    evcom_stream_read_resume(&stream_);
  }

  void SetTimeout(float timeout) {
    evcom_stream_reset_timeout(&stream_, timeout);
  }

  void SetNoDelay(bool no_delay) {
    evcom_stream_set_no_delay(&stream_, no_delay);
  }

  virtual void OnConnect();
  virtual void OnReceive(const void *buf, size_t len);
  virtual void OnEOF();
  virtual void OnClose();
  virtual void OnTimeout();
  virtual void OnDrain();

  v8::Local<v8::Object> GetProtocol();

  enum evcom_stream_state ReadyState() {
    return evcom_stream_state(&stream_);
  }

  enum encoding encoding_;
  bool resolving_;
  bool secure_;
  #if EVCOM_HAVE_GNUTLS
  gnutls_certificate_credentials_t credentials;
  #endif

 private:

  /* liboi callbacks */
  static void on_connect(evcom_stream *s) {
    Connection *connection = static_cast<Connection*>(s->data);
    connection->OnConnect();
  }

  static void on_read(evcom_stream *s, const void *buf, size_t len) {
    Connection *connection = static_cast<Connection*>(s->data);
    assert(connection->refs_);
    if (len == 0)
      connection->OnEOF();
    else
      connection->OnReceive(buf, len);
  }

  static void on_close(evcom_stream *s) {
    Connection *connection = static_cast<Connection*>(s->data);

    evcom_stream_detach(s);

    assert(connection->stream_.recvfd < 0);
    assert(connection->stream_.sendfd < 0);

    #if EVCOM_HAVE_GNUTLS
    if (connection->secure_) {
      if (connection->stream_.session) {
        gnutls_deinit(connection->stream_.session);
        connection->stream_.session = NULL;
      }
      if (!connection->stream_.server && connection->credentials) {
        gnutls_certificate_free_credentials(connection->credentials);
        connection->credentials = NULL;
      }
    }
    #endif

    connection->OnClose();

    assert(connection->refs_);

    connection->Unref();
  }

  static void on_timeout(evcom_stream *s) {
    Connection *connection = static_cast<Connection*>(s->data);
    connection->OnTimeout();
  }

  static void on_drain(evcom_stream *s) {
    Connection *connection = static_cast<Connection*>(s->data);
    connection->OnDrain();
  }

  void Init();  // constructor helper.

  static int Resolve(eio_req *req);
  static int AfterResolve(eio_req *req);
  char *host_;
  char *port_;
  evcom_stream stream_;

  friend class Server;
};

class Server : public EventEmitter {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Listen(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);
  #if EVCOM_HAVE_GNUTLS
  static v8::Handle<v8::Value> SetSecure(const v8::Arguments& args);
  #endif

  Server() : EventEmitter() {
    evcom_server_init(&server_);
    server_.on_connection = Server::on_connection;
    server_.on_close = Server::on_close;
    server_.data = this;
    secure_ = false;
  }

  virtual ~Server() {
    assert(server_.fd >= 0);
  }

  int Listen(struct sockaddr *address, int backlog) {
    int r = evcom_server_listen(&server_, address, backlog);
    if (r != 0) return r;
    evcom_server_attach(EV_DEFAULT_ &server_);
    Ref();
    return 0;
  }

  void Close() {
    evcom_server_close(&server_);
  }

  virtual v8::Handle<v8::FunctionTemplate> GetConnectionTemplate();
  virtual Connection* UnwrapConnection(v8::Local<v8::Object> connection);

 private:
  Connection* OnConnection(struct sockaddr *addr);

  static evcom_stream* on_connection(evcom_server *s, struct sockaddr *addr) {
    Server *server = static_cast<Server*>(s->data);
    Connection *connection = server->OnConnection(addr);
    return &connection->stream_;
  }

  void OnClose(int errorno);

  static void on_close(evcom_server *s) {
    Server *server = static_cast<Server*>(s->data);
    evcom_server_detach(s);
    server->OnClose(s->errorno);
    server->Unref();
  }

  evcom_server server_;

  #if EVCOM_HAVE_GNUTLS
  gnutls_certificate_credentials_t credentials;
  #endif
  bool secure_;
};

}  // namespace node
#endif  // SRC_NET_H_

#if EVCOM_HAVE_GNUTLS
void init_tls_session(evcom_stream* stream_,
                      gnutls_certificate_credentials_t credentials,
                      gnutls_connection_end_t session_type);

int verify_certificate_chain(gnutls_session_t session,
                             const char *hostname,
                             const gnutls_datum_t * cert_chain,
                             int cert_chain_length,
                             gnutls_x509_crl_t *crl_list,
                             int crl_list_size,
                             gnutls_x509_crt_t *ca_list,
                             int ca_list_size);

int verify_cert2(gnutls_x509_crt_t crt,
                 gnutls_x509_crt_t issuer,
                 gnutls_x509_crl_t * crl_list,
                 int crl_list_size);

int verify_last_cert(gnutls_x509_crt_t crt,
                     gnutls_x509_crt_t * ca_list,
                     int ca_list_size,
                     gnutls_x509_crl_t * crl_list,
                     int crl_list_size);

#define JS_GNUTLS_CERT_VALIDATED 1
#define JS_GNUTLS_CERT_UNDEFINED 0

#define JS_GNUTLS_CERT_SIGNER_NOT_FOUND -100
#define JS_GNUTLS_CERT_SIGNER_NOT_CA -101
#define JS_GNUTLS_CERT_INVALID -102
#define JS_GNUTLS_CERT_NOT_ACTIVATED -103
#define JS_GNUTLS_CERT_EXPIRED -104
#define JS_GNUTLS_CERT_REVOKED -105
#define JS_GNUTLS_CERT_DOES_NOT_MATCH_HOSTNAME -106

#endif
