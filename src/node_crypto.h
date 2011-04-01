// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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

static X509_STORE* root_cert_store;

class SecureContext : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  SSL_CTX *ctx_;
  // TODO: ca_store_ should probably be removed, it's not used anywhere.
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

  void FreeCTXMem() {
    if (ctx_) {
      if (ctx_->cert_store == root_cert_store) {
        // SSL_CTX_free() will attempt to free the cert_store as well.
        // Since we want our root_cert_store to stay around forever
        // we just clear the field. Hopefully OpenSSL will not modify this
        // struct in future versions.
        ctx_->cert_store = NULL;
      }
      SSL_CTX_free(ctx_);
      ctx_ = NULL;
      ca_store_ = NULL;
    } else {
      assert(ca_store_ == NULL);
    }
  }

  ~SecureContext() {
    FreeCTXMem();
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
