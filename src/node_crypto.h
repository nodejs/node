#ifndef SRC_NODE_CRYPTO_H_
#define SRC_NODE_CRYPTO_H_

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/hmac.h>

#define EVP_F_EVP_DECRYPTFINAL 101


namespace node {
namespace crypto {

class SecureContext : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  SSL_CTX *ctx_;
  X509_STORE *ca_store_;

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Init(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetKey(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetCert(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddCACert(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddCRL(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddRootCerts(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetCiphers(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

  SecureContext() : ObjectWrap() {
    ctx_ = NULL;
    ca_store_ = NULL;
  }

  ~SecureContext() {
    if (ctx_) {
      SSL_CTX_free(ctx_);
      ctx_ = NULL;
      ca_store_ = NULL;
    } else {
      assert(ca_store_ == NULL);
    }
  }

 private:
};

class Connection : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncIn(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearOut(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearPending(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncPending(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncOut(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearIn(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPeerCertificate(const v8::Arguments& args);
  static v8::Handle<v8::Value> IsInitFinished(const v8::Arguments& args);
  static v8::Handle<v8::Value> VerifyError(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetCurrentCipher(const v8::Arguments& args);
  static v8::Handle<v8::Value> Shutdown(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReceivedShutdown(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

  int HandleBIOError(BIO *bio, const char* func, int rv);
  int HandleSSLError(const char* func, int rv);

  void ClearError();
  void SetShutdownFlags();

  static Connection* Unwrap(const v8::Arguments& args) {
    Connection* ss = ObjectWrap::Unwrap<Connection>(args.Holder());
    ss->ClearError();
    return ss;
  }

  Connection() : ObjectWrap() {
    bio_read_ = bio_write_ = NULL;
    ssl_ = NULL;
  }

  ~Connection() {
    if (ssl_ != NULL) {
      SSL_free(ssl_);
      ssl_ = NULL;
    }
  }

 private:
  BIO *bio_read_;
  BIO *bio_write_;
  SSL *ssl_;
  bool is_server_; /* coverity[member_decl] */
};

void InitCrypto(v8::Handle<v8::Object> target);

}  // namespace crypto
}  // namespace node

#endif  // SRC_NODE_CRYPTO_H_
