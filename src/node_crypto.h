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

#include "node.h"

#include "node_object_wrap.h"
#include "v8.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>

#ifdef OPENSSL_NPN_NEGOTIATED
#include "node_buffer.h"
#endif

#define EVP_F_EVP_DECRYPTFINAL 101


namespace node {
namespace crypto {

static X509_STORE* root_cert_store;

// Forward declaration
class Connection;

class SecureContext : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  SSL_CTX *ctx_;
  // TODO: ca_store_ should probably be removed, it's not used anywhere.
  X509_STORE *ca_store_;

 protected:
  static const int kMaxSessionSize = 10 * 1024;

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Init(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetKey(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetCert(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddCACert(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddCRL(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddRootCerts(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetCiphers(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetOptions(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetSessionIdContext(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetSessionTimeout(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);
  static v8::Handle<v8::Value> LoadPKCS12(const v8::Arguments& args);

  static SSL_SESSION* GetSessionCallback(SSL* s,
                                         unsigned char* key,
                                         int len,
                                         int* copy);
  static int NewSessionCallback(SSL* s, SSL_SESSION* sess);

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

class ClientHelloParser {
 public:
  enum FrameType {
    kChangeCipherSpec = 20,
    kAlert = 21,
    kHandshake = 22,
    kApplicationData = 23,
    kOther = 255
  };

  enum HandshakeType {
    kClientHello = 1
  };

  enum ParseState {
    kWaiting,
    kTLSHeader,
    kSSLHeader,
    kPaused,
    kEnded
  };

  ClientHelloParser(Connection* c) : conn_(c),
                                     state_(kWaiting),
                                     offset_(0),
                                     body_offset_(0) {
  }

  size_t Write(const uint8_t* data, size_t len);
  void Finish();

  inline bool ended() { return state_ == kEnded; }

 private:
  Connection* conn_;
  ParseState state_;
  size_t frame_len_;

  uint8_t data_[18432];
  size_t offset_;
  size_t body_offset_;
};

class Connection : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

#ifdef OPENSSL_NPN_NEGOTIATED
  v8::Persistent<v8::Object> npnProtos_;
  v8::Persistent<v8::Value> selectedNPNProto_;
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  v8::Persistent<v8::Object> sniObject_;
  v8::Persistent<v8::Value> sniContext_;
  v8::Persistent<v8::String> servername_;
#endif

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncIn(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearOut(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearPending(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncPending(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncOut(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearIn(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPeerCertificate(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetSession(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetSession(const v8::Arguments& args);
  static v8::Handle<v8::Value> LoadSession(const v8::Arguments& args);
  static v8::Handle<v8::Value> IsSessionReused(const v8::Arguments& args);
  static v8::Handle<v8::Value> IsInitFinished(const v8::Arguments& args);
  static v8::Handle<v8::Value> VerifyError(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetCurrentCipher(const v8::Arguments& args);
  static v8::Handle<v8::Value> Shutdown(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReceivedShutdown(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

#ifdef OPENSSL_NPN_NEGOTIATED
  // NPN
  static v8::Handle<v8::Value> GetNegotiatedProto(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetNPNProtocols(const v8::Arguments& args);
  static int AdvertiseNextProtoCallback_(SSL *s,
                                         const unsigned char **data,
                                         unsigned int *len,
                                         void *arg);
  static int SelectNextProtoCallback_(SSL *s,
                                      unsigned char **out, unsigned char *outlen,
                                      const unsigned char* in,
                                      unsigned int inlen, void *arg);
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  // SNI
  static v8::Handle<v8::Value> GetServername(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetSNICallback(const v8::Arguments& args);
  static int SelectSNIContextCallback_(SSL *s, int *ad, void* arg);
#endif

  int HandleBIOError(BIO *bio, const char* func, int rv);

  enum ZeroStatus {
    kZeroIsNotAnError,
    kZeroIsAnError
  };

  int HandleSSLError(const char* func, int rv, ZeroStatus zs);

  void ClearError();
  void SetShutdownFlags();

  static Connection* Unwrap(const v8::Arguments& args) {
    Connection* ss = ObjectWrap::Unwrap<Connection>(args.This());
    ss->ClearError();
    return ss;
  }

  Connection() : ObjectWrap(), hello_parser_(this) {
    bio_read_ = bio_write_ = NULL;
    ssl_ = NULL;
    next_sess_ = NULL;
  }

  ~Connection() {
    if (ssl_ != NULL) {
      SSL_free(ssl_);
      ssl_ = NULL;
    }

    if (next_sess_ != NULL) {
      SSL_SESSION_free(next_sess_);
      next_sess_ = NULL;
    }

#ifdef OPENSSL_NPN_NEGOTIATED
    if (!npnProtos_.IsEmpty()) npnProtos_.Dispose(node_isolate);
    if (!selectedNPNProto_.IsEmpty()) selectedNPNProto_.Dispose(node_isolate);
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
   if (!sniObject_.IsEmpty()) sniObject_.Dispose(node_isolate);
   if (!sniContext_.IsEmpty()) sniContext_.Dispose(node_isolate);
   if (!servername_.IsEmpty()) servername_.Dispose(node_isolate);
#endif
  }

 private:
  static void SSLInfoCallback(const SSL *ssl, int where, int ret);

  BIO *bio_read_;
  BIO *bio_write_;
  SSL *ssl_;

  ClientHelloParser hello_parser_;

  bool is_server_; /* coverity[member_decl] */
  SSL_SESSION* next_sess_;

  friend class ClientHelloParser;
  friend class SecureContext;
};

class CipherBase : public ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  enum CipherKind {
    kCipher,
    kDecipher
  };

  v8::Handle<v8::Value> Init(char* cipher_type, char* key_buf, int key_buf_len);
  v8::Handle<v8::Value> InitIv(char* cipher_type,
                               char* key,
                               int key_len,
                               char* iv,
                               int iv_len);
  bool Update(char* data, int len, unsigned char** out, int* out_len);
  bool Final(unsigned char** out, int *out_len);
  bool SetAutoPadding(bool auto_padding);

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Init(const v8::Arguments& args);
  static v8::Handle<v8::Value> InitIv(const v8::Arguments& args);
  static v8::Handle<v8::Value> Update(const v8::Arguments& args);
  static v8::Handle<v8::Value> Final(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetAutoPadding(const v8::Arguments& args);

  CipherBase(CipherKind kind) : cipher_(NULL),
                                initialised_(false),
                                kind_(kind) {
  }

  ~CipherBase() {
    if (!initialised_) return;
    EVP_CIPHER_CTX_cleanup(&ctx_);
  }

 private:
  EVP_CIPHER_CTX ctx_; /* coverity[member_decl] */
  const EVP_CIPHER* cipher_; /* coverity[member_decl] */
  bool initialised_;
  CipherKind kind_;
};

class Hmac : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);

 protected:
  v8::Handle<v8::Value> HmacInit(char* hashType, char* key, int key_len);
  bool HmacUpdate(char* data, int len);
  bool HmacDigest(unsigned char** md_value, unsigned int* md_len);

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> HmacInit(const v8::Arguments& args);
  static v8::Handle<v8::Value> HmacUpdate(const v8::Arguments& args);
  static v8::Handle<v8::Value> HmacDigest(const v8::Arguments& args);

  Hmac() : md_(NULL), initialised_(false) {
  }

  ~Hmac() {
    if (!initialised_) return;
    HMAC_CTX_cleanup(&ctx_);
  }

 private:
  HMAC_CTX ctx_; /* coverity[member_decl] */
  const EVP_MD* md_; /* coverity[member_decl] */
  bool initialised_;
};

class Hash : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);

  bool HashInit(const char* hashType);
  bool HashUpdate(char* data, int len);

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> HashUpdate(const v8::Arguments& args);
  static v8::Handle<v8::Value> HashDigest(const v8::Arguments& args);

  Hash() : md_(NULL), initialised_(false) {
  }

  ~Hash() {
    if (!initialised_) return;
    EVP_MD_CTX_cleanup(&mdctx_);
  }

 private:
  EVP_MD_CTX mdctx_; /* coverity[member_decl] */
  const EVP_MD* md_; /* coverity[member_decl] */
  bool initialised_;
};

class Sign : public ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  v8::Handle<v8::Value> SignInit(const char* sign_type);
  bool SignUpdate(char* data, int len);
  bool SignFinal(unsigned char** md_value,
                 unsigned int *md_len,
                 char* key_pem,
                 int key_pem_len);

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> SignInit(const v8::Arguments& args);
  static v8::Handle<v8::Value> SignUpdate(const v8::Arguments& args);
  static v8::Handle<v8::Value> SignFinal(const v8::Arguments& args);

  Sign() : md_(NULL), initialised_(false) {
  }

  ~Sign() {
    if (!initialised_) return;
    EVP_MD_CTX_cleanup(&mdctx_);
  }

 private:
  EVP_MD_CTX mdctx_; /* coverity[member_decl] */
  const EVP_MD* md_; /* coverity[member_decl] */
  bool initialised_;
};

class Verify : public ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);

  v8::Handle<v8::Value> VerifyInit(const char* verify_type);
  bool VerifyUpdate(char* data, int len);
  v8::Handle<v8::Value> VerifyFinal(char* key_pem,
                                    int key_pem_len,
                                    unsigned char* sig,
                                    int siglen);

 protected:
  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> VerifyInit(const v8::Arguments& args);
  static v8::Handle<v8::Value> VerifyUpdate(const v8::Arguments& args);
  static v8::Handle<v8::Value> VerifyFinal(const v8::Arguments& args);

  Verify() : md_(NULL), initialised_(false) {
  }

  ~Verify() {
    if (!initialised_) return;
    EVP_MD_CTX_cleanup(&mdctx_);
  }

 private:
  EVP_MD_CTX mdctx_; /* coverity[member_decl] */
  const EVP_MD* md_; /* coverity[member_decl] */
  bool initialised_;

};

class DiffieHellman : public ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  bool Init(int primeLength);
  bool Init(unsigned char* p, int p_len);
  bool Init(unsigned char* p, int p_len, unsigned char* g, int g_len);

 protected:
  static v8::Handle<v8::Value> DiffieHellmanGroup(const v8::Arguments& args);
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> GenerateKeys(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPrime(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetGenerator(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPublicKey(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPrivateKey(const v8::Arguments& args);
  static v8::Handle<v8::Value> ComputeSecret(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetPublicKey(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetPrivateKey(const v8::Arguments& args);

  DiffieHellman() : ObjectWrap(), initialised_(false), dh(NULL) {
  }

  ~DiffieHellman() {
    if (dh != NULL) {
      DH_free(dh);
    }
  }

 private:
  bool VerifyContext();

  bool initialised_;
  DH* dh;
};

void InitCrypto(v8::Handle<v8::Object> target);

}  // namespace crypto
}  // namespace node

#endif  // SRC_NODE_CRYPTO_H_
