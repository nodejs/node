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

class SecureContext : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  SSL_CTX *pCtx;
  X509_STORE *caStore;

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Init(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetKey(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetCert(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddCACert(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetCiphers(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

  SecureContext() : ObjectWrap() {
    pCtx = NULL;
    caStore = NULL;
  }

  ~SecureContext() {
    // Free up
  }

 private:
};

class SecureStream : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadInject(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadExtract(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadPending(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteCanExtract(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteExtract(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteInject(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPeerCertificate(const v8::Arguments& args);
  static v8::Handle<v8::Value> IsInitFinished(const v8::Arguments& args);
  static v8::Handle<v8::Value> VerifyPeer(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetCurrentCipher(const v8::Arguments& args);
  static v8::Handle<v8::Value> Shutdown(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

  SecureStream() : ObjectWrap() {
    pbioRead = pbioWrite = NULL;
    pSSL = NULL;
  }

  ~SecureStream() {
  }

 private:
  BIO *pbioRead;
  BIO *pbioWrite;
  SSL *pSSL;
  bool server; /* coverity[member_decl] */
  bool shouldVerify; /* coverity[member_decl] */
};

void InitCrypto(v8::Handle<v8::Object> target);
}

#endif  // SRC_NODE_CRYPTO_H_
