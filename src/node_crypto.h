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
#include "node_crypto_clienthello.h"  // ClientHelloParser
#include "node_crypto_clienthello-inl.h"

#ifdef OPENSSL_NPN_NEGOTIATED
#include "node_buffer.h"
#endif

#include "env.h"
#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "base-object.h"
#include "base-object-inl.h"

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

#define EVP_F_EVP_DECRYPTFINAL 101


namespace node {
namespace crypto {

extern int VerifyCallback(int preverify_ok, X509_STORE_CTX* ctx);

extern X509_STORE* root_cert_store;

// Forward declaration
class Connection;

class SecureContext : public BaseObject {
 public:
  ~SecureContext() {
    FreeCTXMem();
  }

  static void Initialize(Environment* env, v8::Handle<v8::Object> target);

  X509_STORE* ca_store_;
  SSL_CTX* ctx_;

  static const int kMaxSessionSize = 10 * 1024;

 protected:

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Init(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetCert(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddCACert(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddCRL(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddRootCerts(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetCiphers(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetECDHCurve(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetOptions(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSessionIdContext(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSessionTimeout(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoadPKCS12(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetTicketKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTicketKeys(const v8::FunctionCallbackInfo<v8::Value>& args);

  SecureContext(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        ca_store_(NULL),
        ctx_(NULL) {
    MakeWeak<SecureContext>(this);
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
};

// SSLWrap implicitly depends on the inheriting class' handle having an
// internal pointer to the Base class.
template <class Base>
class SSLWrap {
 public:
  enum Kind {
    kClient,
    kServer
  };

  SSLWrap(Environment* env, SecureContext* sc, Kind kind)
      : env_(env),
        kind_(kind),
        next_sess_(NULL),
        session_callbacks_(false) {
    ssl_ = SSL_new(sc->ctx_);
    assert(ssl_ != NULL);
  }

  ~SSLWrap() {
    if (ssl_ != NULL) {
      SSL_free(ssl_);
      ssl_ = NULL;
    }
    if (next_sess_ != NULL) {
      SSL_SESSION_free(next_sess_);
      next_sess_ = NULL;
    }

#ifdef OPENSSL_NPN_NEGOTIATED
    npn_protos_.Dispose();
    selected_npn_proto_.Dispose();
#endif
  }

  inline SSL* ssl() const { return ssl_; }
  inline void enable_session_callbacks() { session_callbacks_ = true; }
  inline bool is_server() const { return kind_ == kServer; }
  inline bool is_client() const { return kind_ == kClient; }

 protected:
  static void InitNPN(SecureContext* sc, Base* base);
  static void AddMethods(v8::Handle<v8::FunctionTemplate> t);

  static SSL_SESSION* GetSessionCallback(SSL* s,
                                         unsigned char* key,
                                         int len,
                                         int* copy);
  static int NewSessionCallback(SSL* s, SSL_SESSION* sess);
  static void OnClientHello(void* arg,
                            const ClientHelloParser::ClientHello& hello);

  static void GetPeerCertificate(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoadSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsSessionReused(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsInitFinished(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyError(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCurrentCipher(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EndParser(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Renegotiate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args);

#ifdef OPENSSL_NPN_NEGOTIATED
  static void GetNegotiatedProto(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetNPNProtocols(const v8::FunctionCallbackInfo<v8::Value>& args);
  static int AdvertiseNextProtoCallback(SSL* s,
                                        const unsigned char** data,
                                        unsigned int* len,
                                        void* arg);
  static int SelectNextProtoCallback(SSL* s,
                                     unsigned char** out,
                                     unsigned char* outlen,
                                     const unsigned char* in,
                                     unsigned int inlen,
                                     void* arg);
#endif  // OPENSSL_NPN_NEGOTIATED

  inline Environment* ssl_env() const {
    return env_;
  }

  Environment* const env_;
  Kind kind_;
  SSL_SESSION* next_sess_;
  SSL* ssl_;
  bool session_callbacks_;
  ClientHelloParser hello_parser_;

#ifdef OPENSSL_NPN_NEGOTIATED
  v8::Persistent<v8::Object> npn_protos_;
  v8::Persistent<v8::Value> selected_npn_proto_;
#endif  // OPENSSL_NPN_NEGOTIATED

  friend class SecureContext;
};

// Connection inherits from AsyncWrap because SSLWrap makes calls to
// MakeCallback, but SSLWrap doesn't store the handle itself. Instead it
// assumes that any args.This() called will be the handle from Connection.
class Connection : public SSLWrap<Connection>, public AsyncWrap {
 public:
  ~Connection() {
#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
    sniObject_.Dispose();
    sniContext_.Dispose();
    servername_.Dispose();
#endif
  }

  static void Initialize(Environment* env, v8::Handle<v8::Object> target);

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
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EncIn(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ClearOut(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ClearPending(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EncPending(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EncOut(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ClearIn(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  // SNI
  static void GetServername(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSNICallback(const v8::FunctionCallbackInfo<v8::Value>& args);
  static int SelectSNIContextCallback_(SSL* s, int* ad, void* arg);
#endif

  static void OnClientHelloParseEnd(void* arg);

  int HandleBIOError(BIO* bio, const char* func, int rv);

  enum ZeroStatus {
    kZeroIsNotAnError,
    kZeroIsAnError
  };

  enum SyscallStatus {
    kIgnoreSyscall,
    kSyscallError
  };

  int HandleSSLError(const char* func, int rv, ZeroStatus zs, SyscallStatus ss);

  void ClearError();
  void SetShutdownFlags();

  Connection(Environment* env,
             v8::Local<v8::Object> wrap,
             SecureContext* sc,
             SSLWrap<Connection>::Kind kind)
      : SSLWrap<Connection>(env, sc, kind),
        AsyncWrap(env, wrap),
        bio_read_(NULL),
        bio_write_(NULL),
        hello_offset_(0) {
    MakeWeak<Connection>(this);
    hello_parser_.Start(SSLWrap<Connection>::OnClientHello,
                        OnClientHelloParseEnd,
                        this);
    enable_session_callbacks();
  }

 private:
  static void SSLInfoCallback(const SSL *ssl, int where, int ret);

  BIO *bio_read_;
  BIO *bio_write_;

  uint8_t hello_data_[18432];
  size_t hello_offset_;

  friend class ClientHelloParser;
  friend class SecureContext;
};

class CipherBase : public BaseObject {
 public:
  ~CipherBase() {
    if (!initialised_)
      return;
    delete[] auth_tag_;
    EVP_CIPHER_CTX_cleanup(&ctx_);
  }

  static void Initialize(Environment* env, v8::Handle<v8::Object> target);

 protected:
  enum CipherKind {
    kCipher,
    kDecipher
  };

  void Init(const char* cipher_type, const char* key_buf, int key_buf_len);
  void InitIv(const char* cipher_type,
              const char* key,
              int key_len,
              const char* iv,
              int iv_len);
  bool Update(const char* data, int len, unsigned char** out, int* out_len);
  bool Final(unsigned char** out, int *out_len);
  bool SetAutoPadding(bool auto_padding);

  bool IsAuthenticatedMode() const;
  bool GetAuthTag(char** out, unsigned int* out_len) const;
  bool SetAuthTag(const char* data, unsigned int len);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Init(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void InitIv(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Update(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Final(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAutoPadding(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void GetAuthTag(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAuthTag(const v8::FunctionCallbackInfo<v8::Value>& args);

  CipherBase(Environment* env,
             v8::Local<v8::Object> wrap,
             CipherKind kind)
      : BaseObject(env, wrap),
        cipher_(NULL),
        initialised_(false),
        kind_(kind),
        auth_tag_(NULL),
        auth_tag_len_(0) {
    MakeWeak<CipherBase>(this);
  }

 private:
  EVP_CIPHER_CTX ctx_; /* coverity[member_decl] */
  const EVP_CIPHER* cipher_; /* coverity[member_decl] */
  bool initialised_;
  CipherKind kind_;
  char* auth_tag_;
  unsigned int auth_tag_len_;
};

class Hmac : public BaseObject {
 public:
  ~Hmac() {
    if (!initialised_)
      return;
    HMAC_CTX_cleanup(&ctx_);
  }

  static void Initialize(Environment* env, v8::Handle<v8::Object> target);

 protected:
  void HmacInit(const char* hash_type, const char* key, int key_len);
  bool HmacUpdate(const char* data, int len);
  bool HmacDigest(unsigned char** md_value, unsigned int* md_len);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hmac(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        md_(NULL),
        initialised_(false) {
    MakeWeak<Hmac>(this);
  }

 private:
  HMAC_CTX ctx_; /* coverity[member_decl] */
  const EVP_MD* md_; /* coverity[member_decl] */
  bool initialised_;
};

class Hash : public BaseObject {
 public:
  ~Hash() {
    if (!initialised_)
      return;
    EVP_MD_CTX_cleanup(&mdctx_);
  }

  static void Initialize(Environment* env, v8::Handle<v8::Object> target);

  bool HashInit(const char* hash_type);
  bool HashUpdate(const char* data, int len);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hash(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        md_(NULL),
        initialised_(false) {
    MakeWeak<Hash>(this);
  }

 private:
  EVP_MD_CTX mdctx_; /* coverity[member_decl] */
  const EVP_MD* md_; /* coverity[member_decl] */
  bool initialised_;
};

class Sign : public BaseObject {
 public:
  ~Sign() {
    if (!initialised_)
      return;
    EVP_MD_CTX_cleanup(&mdctx_);
  }

  static void Initialize(Environment* env, v8::Handle<v8::Object> target);

  void SignInit(const char* sign_type);
  bool SignUpdate(const char* data, int len);
  bool SignFinal(const char* key_pem,
                 int key_pem_len,
                 const char* passphrase,
                 unsigned char** sig,
                 unsigned int *sig_len);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignFinal(const v8::FunctionCallbackInfo<v8::Value>& args);

  Sign(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        md_(NULL),
        initialised_(false) {
    MakeWeak<Sign>(this);
  }

 private:
  EVP_MD_CTX mdctx_; /* coverity[member_decl] */
  const EVP_MD* md_; /* coverity[member_decl] */
  bool initialised_;
};

class Verify : public BaseObject {
 public:
  ~Verify() {
    if (!initialised_)
      return;
    EVP_MD_CTX_cleanup(&mdctx_);
  }

  static void Initialize(Environment* env, v8::Handle<v8::Object> target);

  void VerifyInit(const char* verify_type);
  bool VerifyUpdate(const char* data, int len);
  bool VerifyFinal(const char* key_pem,
                   int key_pem_len,
                   const char* sig,
                   int siglen);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyFinal(const v8::FunctionCallbackInfo<v8::Value>& args);

  Verify(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        md_(NULL),
        initialised_(false) {
    MakeWeak<Verify>(this);
  }

 private:
  EVP_MD_CTX mdctx_; /* coverity[member_decl] */
  const EVP_MD* md_; /* coverity[member_decl] */
  bool initialised_;
};

class DiffieHellman : public BaseObject {
 public:
  ~DiffieHellman() {
    if (dh != NULL) {
      DH_free(dh);
    }
  }

  static void Initialize(Environment* env, v8::Handle<v8::Object> target);

  bool Init(int primeLength);
  bool Init(const char* p, int p_len);
  bool Init(const char* p, int p_len, const char* g, int g_len);

 protected:
  static void DiffieHellmanGroup(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GenerateKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPrime(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetGenerator(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPrivateKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ComputeSecret(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPrivateKey(const v8::FunctionCallbackInfo<v8::Value>& args);

  DiffieHellman(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        initialised_(false),
        dh(NULL) {
    MakeWeak<DiffieHellman>(this);
  }

 private:
  bool VerifyContext();

  bool initialised_;
  DH* dh;
};

class Certificate : public AsyncWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  v8::Handle<v8::Value> CertificateInit(const char* sign_type);
  bool VerifySpkac(const char* data, unsigned int len);
  const char* ExportPublicKey(const char* data, int len);
  const char* ExportChallenge(const char* data, int len);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifySpkac(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ExportPublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ExportChallenge(const v8::FunctionCallbackInfo<v8::Value>& args);

  Certificate(Environment* env, v8::Local<v8::Object> wrap)
      : AsyncWrap(env, wrap) {
    MakeWeak<Certificate>(this);
  }
};

bool EntropySource(unsigned char* buffer, size_t length);
void InitCrypto(v8::Handle<v8::Object> target);

}  // namespace crypto
}  // namespace node

#endif  // SRC_NODE_CRYPTO_H_
