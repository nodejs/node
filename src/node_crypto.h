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

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
// ClientHelloParser
#include "node_crypto_clienthello-inl.h"

#include "node_buffer.h"

#include "env.h"
#include "async-wrap-inl.h"
#include "base-object-inl.h"

#include "v8.h"

#include <openssl/ssl.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#ifndef OPENSSL_NO_ENGINE
# include <openssl/engine.h>
#endif  // !OPENSSL_NO_ENGINE
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>

#define EVP_F_EVP_DECRYPTFINAL 101

#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)
# define NODE__HAVE_TLSEXT_STATUS_CB
#endif  // !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)

namespace node {
namespace crypto {

// Forcibly clear OpenSSL's error stack on return. This stops stale errors
// from popping up later in the lifecycle of crypto operations where they
// would cause spurious failures. It's a rather blunt method, though.
// ERR_clear_error() isn't necessarily cheap either.
struct ClearErrorOnReturn {
  ~ClearErrorOnReturn() { ERR_clear_error(); }
};

// Pop errors from OpenSSL's error stack that were added
// between when this was constructed and destructed.
struct MarkPopErrorOnReturn {
  MarkPopErrorOnReturn() { ERR_set_mark(); }
  ~MarkPopErrorOnReturn() { ERR_pop_to_mark(); }
};

enum CheckResult {
  CHECK_CERT_REVOKED = 0,
  CHECK_OK = 1
};

extern int VerifyCallback(int preverify_ok, X509_STORE_CTX* ctx);

extern void UseExtraCaCerts(const std::string& file);

class SecureContext : public BaseObject {
 public:
  ~SecureContext() override {
    FreeCTXMem();
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  SSL_CTX* ctx_;
  X509* cert_;
  X509* issuer_;

  static const int kMaxSessionSize = 10 * 1024;

  // See TicketKeyCallback
  static const int kTicketKeyReturnIndex = 0;
  static const int kTicketKeyHMACIndex = 1;
  static const int kTicketKeyAESIndex = 2;
  static const int kTicketKeyNameIndex = 3;
  static const int kTicketKeyIVIndex = 4;

 protected:
  static const int64_t kExternalSize = sizeof(SSL_CTX);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Init(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetCert(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddCACert(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddCRL(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddRootCerts(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetCiphers(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetECDHCurve(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetDHParam(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetOptions(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSessionIdContext(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSessionTimeout(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoadPKCS12(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetTicketKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTicketKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetFreeListLength(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnableTicketKeyCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CtxGetter(v8::Local<v8::String> property,
                        const v8::PropertyCallbackInfo<v8::Value>& info);

  template <bool primary>
  static void GetCertificate(const v8::FunctionCallbackInfo<v8::Value>& args);

  static int TicketKeyCallback(SSL* ssl,
                               unsigned char* name,
                               unsigned char* iv,
                               EVP_CIPHER_CTX* ectx,
                               HMAC_CTX* hctx,
                               int enc);

  SecureContext(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        ctx_(nullptr),
        cert_(nullptr),
        issuer_(nullptr) {
    MakeWeak<SecureContext>(this);
    env->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
  }

  void FreeCTXMem() {
    if (!ctx_) {
      return;
    }

    env()->isolate()->AdjustAmountOfExternalAllocatedMemory(-kExternalSize);
    SSL_CTX_free(ctx_);
    if (cert_ != nullptr)
      X509_free(cert_);
    if (issuer_ != nullptr)
      X509_free(issuer_);
    ctx_ = nullptr;
    cert_ = nullptr;
    issuer_ = nullptr;
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
        next_sess_(nullptr),
        session_callbacks_(false),
        new_session_wait_(false),
        cert_cb_(nullptr),
        cert_cb_arg_(nullptr),
        cert_cb_running_(false) {
    ssl_ = SSL_new(sc->ctx_);
    env_->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
    CHECK_NE(ssl_, nullptr);
  }

  virtual ~SSLWrap() {
    DestroySSL();
    if (next_sess_ != nullptr) {
      SSL_SESSION_free(next_sess_);
      next_sess_ = nullptr;
    }

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
    sni_context_.Reset();
#endif

#ifdef NODE__HAVE_TLSEXT_STATUS_CB
    ocsp_response_.Reset();
#endif  // NODE__HAVE_TLSEXT_STATUS_CB
  }

  inline SSL* ssl() const { return ssl_; }
  inline void enable_session_callbacks() { session_callbacks_ = true; }
  inline bool is_server() const { return kind_ == kServer; }
  inline bool is_client() const { return kind_ == kClient; }
  inline bool is_waiting_new_session() const { return new_session_wait_; }
  inline bool is_waiting_cert_cb() const { return cert_cb_ != nullptr; }

 protected:
  typedef void (*CertCb)(void* arg);

  // Size allocated by OpenSSL: one for SSL structure, one for SSL3_STATE and
  // some for buffers.
  // NOTE: Actually it is much more than this
  static const int64_t kExternalSize =
      sizeof(SSL) + sizeof(SSL3_STATE) + 42 * 1024;

  static void InitNPN(SecureContext* sc);
  static void AddMethods(Environment* env, v8::Local<v8::FunctionTemplate> t);

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
  static void CertCbDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Renegotiate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetTLSTicket(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NewSessionDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetOCSPResponse(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RequestOCSP(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetEphemeralKeyInfo(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetProtocol(const v8::FunctionCallbackInfo<v8::Value>& args);

#ifdef SSL_set_max_send_fragment
  static void SetMaxSendFragment(
      const v8::FunctionCallbackInfo<v8::Value>& args);
#endif  // SSL_set_max_send_fragment

#ifndef OPENSSL_NO_NEXTPROTONEG
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
#endif  // OPENSSL_NO_NEXTPROTONEG

  static void GetALPNNegotiatedProto(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetALPNProtocols(const v8::FunctionCallbackInfo<v8::Value>& args);
  static int SelectALPNCallback(SSL* s,
                                const unsigned char** out,
                                unsigned char* outlen,
                                const unsigned char* in,
                                unsigned int inlen,
                                void* arg);
  static int TLSExtStatusCallback(SSL* s, void* arg);
  static int SSLCertCallback(SSL* s, void* arg);
  static void SSLGetter(v8::Local<v8::String> property,
                        const v8::PropertyCallbackInfo<v8::Value>& info);

  void DestroySSL();
  void WaitForCertCb(CertCb cb, void* arg);
  void SetSNIContext(SecureContext* sc);
  int SetCACerts(SecureContext* sc);

  inline Environment* ssl_env() const {
    return env_;
  }

  Environment* const env_;
  Kind kind_;
  SSL_SESSION* next_sess_;
  SSL* ssl_;
  bool session_callbacks_;
  bool new_session_wait_;

  // SSL_set_cert_cb
  CertCb cert_cb_;
  void* cert_cb_arg_;
  bool cert_cb_running_;

  ClientHelloParser hello_parser_;

#ifdef NODE__HAVE_TLSEXT_STATUS_CB
  v8::Persistent<v8::Object> ocsp_response_;
#endif  // NODE__HAVE_TLSEXT_STATUS_CB

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  v8::Persistent<v8::Value> sni_context_;
#endif

  friend class SecureContext;
};

// Connection inherits from AsyncWrap because SSLWrap makes calls to
// MakeCallback, but SSLWrap doesn't store the handle itself. Instead it
// assumes that any args.This() called will be the handle from Connection.
class Connection : public AsyncWrap, public SSLWrap<Connection> {
 public:
  ~Connection() override {
#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
    sniObject_.Reset();
    servername_.Reset();
#endif
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  void NewSessionDoneCb();

#ifndef OPENSSL_NO_NEXTPROTONEG
  v8::Persistent<v8::Object> npnProtos_;
  v8::Persistent<v8::Value> selectedNPNProto_;
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  v8::Persistent<v8::Object> sniObject_;
  v8::Persistent<v8::String> servername_;
#endif

  size_t self_size() const override { return sizeof(*this); }

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

  void SetShutdownFlags();

  Connection(Environment* env,
             v8::Local<v8::Object> wrap,
             SecureContext* sc,
             SSLWrap<Connection>::Kind kind)
      : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_SSLCONNECTION),
        SSLWrap<Connection>(env, sc, kind),
        bio_read_(nullptr),
        bio_write_(nullptr),
        hello_offset_(0) {
    MakeWeak<Connection>(this);
    Wrap(wrap, this);
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
  ~CipherBase() override {
    if (!initialised_)
      return;
    EVP_CIPHER_CTX_cleanup(&ctx_);
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

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
  bool SetAAD(const char* data, unsigned int len);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Init(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void InitIv(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Update(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Final(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAutoPadding(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void GetAuthTag(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAuthTag(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAAD(const v8::FunctionCallbackInfo<v8::Value>& args);

  CipherBase(Environment* env,
             v8::Local<v8::Object> wrap,
             CipherKind kind)
      : BaseObject(env, wrap),
        initialised_(false),
        kind_(kind),
        auth_tag_len_(0) {
    MakeWeak<CipherBase>(this);
  }

 private:
  EVP_CIPHER_CTX ctx_; /* coverity[member_decl] */
  bool initialised_;
  const CipherKind kind_;
  unsigned int auth_tag_len_;
  char auth_tag_[EVP_GCM_TLS_TAG_LEN];
};

class Hmac : public BaseObject {
 public:
  ~Hmac() override {
    if (!initialised_)
      return;
    HMAC_CTX_cleanup(&ctx_);
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

 protected:
  void HmacInit(const char* hash_type, const char* key, int key_len);
  bool HmacUpdate(const char* data, int len);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hmac(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        initialised_(false) {
    MakeWeak<Hmac>(this);
  }

 private:
  HMAC_CTX ctx_; /* coverity[member_decl] */
  bool initialised_;
};

class Hash : public BaseObject {
 public:
  ~Hash() override {
    if (!initialised_)
      return;
    EVP_MD_CTX_cleanup(&mdctx_);
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  bool HashInit(const char* hash_type);
  bool HashUpdate(const char* data, int len);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hash(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        initialised_(false) {
    MakeWeak<Hash>(this);
  }

 private:
  EVP_MD_CTX mdctx_; /* coverity[member_decl] */
  bool initialised_;
  bool finalized_;
};

class SignBase : public BaseObject {
 public:
  typedef enum {
    kSignOk,
    kSignUnknownDigest,
    kSignInit,
    kSignNotInitialised,
    kSignUpdate,
    kSignPrivateKey,
    kSignPublicKey
  } Error;

  SignBase(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        initialised_(false) {
  }

  ~SignBase() override {
    if (!initialised_)
      return;
    EVP_MD_CTX_cleanup(&mdctx_);
  }

 protected:
  void CheckThrow(Error error);

  EVP_MD_CTX mdctx_; /* coverity[member_decl] */
  bool initialised_;
};

class Sign : public SignBase {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  Error SignInit(const char* sign_type);
  Error SignUpdate(const char* data, int len);
  Error SignFinal(const char* key_pem,
                  int key_pem_len,
                  const char* passphrase,
                  unsigned char* sig,
                  unsigned int *sig_len,
                  int padding,
                  int saltlen);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignFinal(const v8::FunctionCallbackInfo<v8::Value>& args);

  Sign(Environment* env, v8::Local<v8::Object> wrap) : SignBase(env, wrap) {
    MakeWeak<Sign>(this);
  }
};

class Verify : public SignBase {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  Error VerifyInit(const char* verify_type);
  Error VerifyUpdate(const char* data, int len);
  Error VerifyFinal(const char* key_pem,
                    int key_pem_len,
                    const char* sig,
                    int siglen,
                    int padding,
                    int saltlen,
                    bool* verify_result);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyFinal(const v8::FunctionCallbackInfo<v8::Value>& args);

  Verify(Environment* env, v8::Local<v8::Object> wrap) : SignBase(env, wrap) {
    MakeWeak<Verify>(this);
  }
};

class PublicKeyCipher {
 public:
  typedef int (*EVP_PKEY_cipher_init_t)(EVP_PKEY_CTX *ctx);
  typedef int (*EVP_PKEY_cipher_t)(EVP_PKEY_CTX *ctx,
                                   unsigned char *out, size_t *outlen,
                                   const unsigned char *in, size_t inlen);

  enum Operation {
    kPublic,
    kPrivate
  };

  template <Operation operation,
            EVP_PKEY_cipher_init_t EVP_PKEY_cipher_init,
            EVP_PKEY_cipher_t EVP_PKEY_cipher>
  static bool Cipher(const char* key_pem,
                     int key_pem_len,
                     const char* passphrase,
                     int padding,
                     const unsigned char* data,
                     int len,
                     unsigned char** out,
                     size_t* out_len);

  template <Operation operation,
            EVP_PKEY_cipher_init_t EVP_PKEY_cipher_init,
            EVP_PKEY_cipher_t EVP_PKEY_cipher>
  static void Cipher(const v8::FunctionCallbackInfo<v8::Value>& args);
};

class DiffieHellman : public BaseObject {
 public:
  ~DiffieHellman() override {
    if (dh != nullptr) {
      DH_free(dh);
    }
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  bool Init(int primeLength, int g);
  bool Init(const char* p, int p_len, int g);
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
  static void VerifyErrorGetter(
      v8::Local<v8::String> property,
      const v8::PropertyCallbackInfo<v8::Value>& args);

  DiffieHellman(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        initialised_(false),
        verifyError_(0),
        dh(nullptr) {
    MakeWeak<DiffieHellman>(this);
  }

 private:
  static void GetField(const v8::FunctionCallbackInfo<v8::Value>& args,
                       BIGNUM* (DH::*field), const char* err_if_null);
  static void SetKey(const v8::FunctionCallbackInfo<v8::Value>& args,
                     BIGNUM* (DH::*field), const char* what);
  bool VerifyContext();

  bool initialised_;
  int verifyError_;
  DH* dh;
};

class ECDH : public BaseObject {
 public:
  ~ECDH() override {
    if (key_ != nullptr)
      EC_KEY_free(key_);
    key_ = nullptr;
    group_ = nullptr;
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

 protected:
  ECDH(Environment* env, v8::Local<v8::Object> wrap, EC_KEY* key)
      : BaseObject(env, wrap),
        key_(key),
        group_(EC_KEY_get0_group(key_)) {
    MakeWeak<ECDH>(this);
    CHECK_NE(group_, nullptr);
  }

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GenerateKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ComputeSecret(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPrivateKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPrivateKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);

  EC_POINT* BufferToPoint(char* data, size_t len);

  bool IsKeyPairValid();
  bool IsKeyValidForCurve(const BIGNUM* private_key);

  EC_KEY* key_;
  const EC_GROUP* group_;
};

bool EntropySource(unsigned char* buffer, size_t length);
#ifndef OPENSSL_NO_ENGINE
void SetEngine(const v8::FunctionCallbackInfo<v8::Value>& args);
#endif  // !OPENSSL_NO_ENGINE
void InitCrypto(v8::Local<v8::Object> target);

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CRYPTO_H_
