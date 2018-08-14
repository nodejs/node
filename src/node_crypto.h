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
#include "node_crypto_clienthello.h"

#include "node_buffer.h"

#include "env.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"

#include "v8.h"

#include <openssl/ssl.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#ifndef OPENSSL_NO_ENGINE
# include <openssl/engine.h>
#endif  // !OPENSSL_NO_ENGINE
#include <openssl/err.h>
#include <openssl/evp.h>
// TODO(shigeki) Remove this after upgrading to 1.1.1
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>

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

// Define smart pointers for the most commonly used OpenSSL types:
using X509Pointer = DeleteFnPtr<X509, X509_free>;
using BIOPointer = DeleteFnPtr<BIO, BIO_free_all>;
using SSLCtxPointer = DeleteFnPtr<SSL_CTX, SSL_CTX_free>;
using SSLPointer = DeleteFnPtr<SSL, SSL_free>;
using EVPKeyPointer = DeleteFnPtr<EVP_PKEY, EVP_PKEY_free>;
using EVPKeyCtxPointer = DeleteFnPtr<EVP_PKEY_CTX, EVP_PKEY_CTX_free>;
using EVPMDPointer = DeleteFnPtr<EVP_MD_CTX, EVP_MD_CTX_free>;
using RSAPointer = DeleteFnPtr<RSA, RSA_free>;
using BignumPointer = DeleteFnPtr<BIGNUM, BN_free>;
using NetscapeSPKIPointer = DeleteFnPtr<NETSCAPE_SPKI, NETSCAPE_SPKI_free>;
using ECGroupPointer = DeleteFnPtr<EC_GROUP, EC_GROUP_free>;
using ECPointPointer = DeleteFnPtr<EC_POINT, EC_POINT_free>;
using ECKeyPointer = DeleteFnPtr<EC_KEY, EC_KEY_free>;
using DHPointer = DeleteFnPtr<DH, DH_free>;

struct StackOfX509Deleter {
  void operator()(STACK_OF(X509)* p) const { sk_X509_pop_free(p, X509_free); }
};
using StackOfX509 = std::unique_ptr<STACK_OF(X509), StackOfX509Deleter>;

struct StackOfXASN1Deleter {
  void operator()(STACK_OF(ASN1_OBJECT)* p) const {
    sk_ASN1_OBJECT_pop_free(p, ASN1_OBJECT_free);
  }
};
using StackOfASN1 = std::unique_ptr<STACK_OF(ASN1_OBJECT), StackOfXASN1Deleter>;

// OPENSSL_free is a macro, so we need a wrapper function.
struct OpenSSLBufferDeleter {
  void operator()(char* pointer) const { OPENSSL_free(pointer); }
};
using OpenSSLBuffer = std::unique_ptr<char[], OpenSSLBufferDeleter>;

enum CheckResult {
  CHECK_CERT_REVOKED = 0,
  CHECK_OK = 1
};

extern void UseExtraCaCerts(const std::string& file);
void ThrowCryptoError(Environment* env,
                      unsigned long err,  // NOLINT(runtime/int)
                      const char* message = nullptr);
void InitCryptoOnce();

class SecureContext : public BaseObject {
 public:
  ~SecureContext() override {
    Reset();
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackThis(this);
  }

  ADD_MEMORY_INFO_NAME(SecureContext)

  SSLCtxPointer ctx_;
  X509Pointer cert_;
  X509Pointer issuer_;
#ifndef OPENSSL_NO_ENGINE
  bool client_cert_engine_provided_ = false;
#endif  // !OPENSSL_NO_ENGINE

  static const int kMaxSessionSize = 10 * 1024;

  // See TicketKeyCallback
  static const int kTicketKeyReturnIndex = 0;
  static const int kTicketKeyHMACIndex = 1;
  static const int kTicketKeyAESIndex = 2;
  static const int kTicketKeyNameIndex = 3;
  static const int kTicketKeyIVIndex = 4;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  unsigned char ticket_key_name_[16];
  unsigned char ticket_key_aes_[16];
  unsigned char ticket_key_hmac_[16];
#endif

 protected:
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  static const int64_t kExternalSize = sizeof(SSL_CTX);
#else
  // OpenSSL 1.1.0 has opaque structures. This is an estimate based on the size
  // as of OpenSSL 1.1.0f.
  static const int64_t kExternalSize = 872;
#endif

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
#ifndef OPENSSL_NO_ENGINE
  static void SetClientCertEngine(
      const v8::FunctionCallbackInfo<v8::Value>& args);
#endif  // !OPENSSL_NO_ENGINE
  static void GetTicketKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTicketKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetFreeListLength(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnableTicketKeyCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  template <bool primary>
  static void GetCertificate(const v8::FunctionCallbackInfo<v8::Value>& args);

  static int TicketKeyCallback(SSL* ssl,
                               unsigned char* name,
                               unsigned char* iv,
                               EVP_CIPHER_CTX* ectx,
                               HMAC_CTX* hctx,
                               int enc);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  static int TicketCompatibilityCallback(SSL* ssl,
                                         unsigned char* name,
                                         unsigned char* iv,
                                         EVP_CIPHER_CTX* ectx,
                                         HMAC_CTX* hctx,
                                         int enc);
#endif

  SecureContext(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap) {
    MakeWeak();
    env->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
  }

  inline void Reset() {
    env()->isolate()->AdjustAmountOfExternalAllocatedMemory(-kExternalSize);
    ctx_.reset();
    cert_.reset();
    issuer_.reset();
  }
};

class CipherBase : public BaseObject {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackThis(this);
  }

  ADD_MEMORY_INFO_NAME(CipherBase)

 protected:
  enum CipherKind {
    kCipher,
    kDecipher
  };
  enum UpdateResult {
    kSuccess,
    kErrorMessageSize,
    kErrorState
  };
  static const unsigned kNoAuthTagLength = static_cast<unsigned>(-1);

  void Init(const char* cipher_type,
            const char* key_buf,
            int key_buf_len,
            unsigned int auth_tag_len);
  void InitIv(const char* cipher_type,
              const char* key,
              int key_len,
              const char* iv,
              int iv_len,
              unsigned int auth_tag_len);
  bool InitAuthenticated(const char* cipher_type, int iv_len,
                         unsigned int auth_tag_len);
  bool CheckCCMMessageLength(int message_len);
  UpdateResult Update(const char* data, int len, unsigned char** out,
                      int* out_len);
  bool Final(unsigned char** out, int* out_len);
  bool SetAutoPadding(bool auto_padding);

  bool IsAuthenticatedMode() const;
  bool SetAAD(const char* data, unsigned int len, int plaintext_len);

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
        ctx_(nullptr),
        kind_(kind),
        auth_tag_set_(false),
        auth_tag_len_(kNoAuthTagLength),
        pending_auth_failed_(false) {
    MakeWeak();
  }

 private:
  DeleteFnPtr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> ctx_;
  const CipherKind kind_;
  bool auth_tag_set_;
  unsigned int auth_tag_len_;
  char auth_tag_[EVP_GCM_TLS_TAG_LEN];
  bool pending_auth_failed_;
  int max_message_size_;
};

class Hmac : public BaseObject {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackThis(this);
  }

  ADD_MEMORY_INFO_NAME(Hmac)

 protected:
  void HmacInit(const char* hash_type, const char* key, int key_len);
  bool HmacUpdate(const char* data, int len);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hmac(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        ctx_(nullptr) {
    MakeWeak();
  }

 private:
  DeleteFnPtr<HMAC_CTX, HMAC_CTX_free> ctx_;
};

class Hash : public BaseObject {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackThis(this);
  }

  ADD_MEMORY_INFO_NAME(Hash)

  bool HashInit(const char* hash_type);
  bool HashUpdate(const char* data, int len);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hash(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        mdctx_(nullptr),
        finalized_(false) {
    MakeWeak();
  }

 private:
  EVPMDPointer mdctx_;
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
      : BaseObject(env, wrap) {
  }

  Error Init(const char* sign_type);
  Error Update(const char* data, int len);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackThis(this);
  }

  ADD_MEMORY_INFO_NAME(SignBase)

 protected:
  void CheckThrow(Error error);

  EVPMDPointer mdctx_;
};

class Sign : public SignBase {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  Error SignFinal(const char* key_pem,
                  int key_pem_len,
                  const char* passphrase,
                  unsigned char* sig,
                  unsigned int* sig_len,
                  int padding,
                  int saltlen);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignFinal(const v8::FunctionCallbackInfo<v8::Value>& args);

  Sign(Environment* env, v8::Local<v8::Object> wrap) : SignBase(env, wrap) {
    MakeWeak();
  }
};

class Verify : public SignBase {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

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
    MakeWeak();
  }
};

class PublicKeyCipher {
 public:
  typedef int (*EVP_PKEY_cipher_init_t)(EVP_PKEY_CTX* ctx);
  typedef int (*EVP_PKEY_cipher_t)(EVP_PKEY_CTX* ctx,
                                   unsigned char* out, size_t* outlen,
                                   const unsigned char* in, size_t inlen);

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
      const v8::FunctionCallbackInfo<v8::Value>& args);

  DiffieHellman(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        initialised_(false),
        verifyError_(0) {
    MakeWeak();
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackThis(this);
  }

  ADD_MEMORY_INFO_NAME(DiffieHellman)

 private:
  static void GetField(const v8::FunctionCallbackInfo<v8::Value>& args,
                       const BIGNUM* (*get_field)(const DH*),
                       const char* err_if_null);
  static void SetKey(const v8::FunctionCallbackInfo<v8::Value>& args,
                     int (*set_field)(DH*, BIGNUM*), const char* what);
  bool VerifyContext();

  bool initialised_;
  int verifyError_;
  DHPointer dh_;
};

class ECDH : public BaseObject {
 public:
  ~ECDH() override {
    group_ = nullptr;
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static ECPointPointer BufferToPoint(Environment* env,
                                      const EC_GROUP* group,
                                      char* data,
                                      size_t len);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackThis(this);
  }

  ADD_MEMORY_INFO_NAME(ECDH)

 protected:
  ECDH(Environment* env, v8::Local<v8::Object> wrap, ECKeyPointer&& key)
      : BaseObject(env, wrap),
        key_(std::move(key)),
        group_(EC_KEY_get0_group(key_.get())) {
    MakeWeak();
    CHECK_NOT_NULL(group_);
  }

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GenerateKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ComputeSecret(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPrivateKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPrivateKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPublicKey(const v8::FunctionCallbackInfo<v8::Value>& args);

  bool IsKeyPairValid();
  bool IsKeyValidForCurve(const BignumPointer& private_key);

  ECKeyPointer key_;
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
