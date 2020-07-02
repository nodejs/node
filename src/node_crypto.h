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

// ClientHelloParser
#include "node_crypto_clienthello.h"

#include "allocated_buffer.h"
#include "env.h"
#include "base_object.h"
#include "util.h"
#include "node_messaging.h"

#include "v8.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/rsa.h>

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
using SSLSessionPointer = DeleteFnPtr<SSL_SESSION, SSL_SESSION_free>;
using SSLPointer = DeleteFnPtr<SSL, SSL_free>;
using PKCS8Pointer = DeleteFnPtr<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free>;
using EVPKeyPointer = DeleteFnPtr<EVP_PKEY, EVP_PKEY_free>;
using EVPKeyCtxPointer = DeleteFnPtr<EVP_PKEY_CTX, EVP_PKEY_CTX_free>;
using EVPMDPointer = DeleteFnPtr<EVP_MD_CTX, EVP_MD_CTX_free>;
using RSAPointer = DeleteFnPtr<RSA, RSA_free>;
using ECPointer = DeleteFnPtr<EC_KEY, EC_KEY_free>;
using BignumPointer = DeleteFnPtr<BIGNUM, BN_free>;
using NetscapeSPKIPointer = DeleteFnPtr<NETSCAPE_SPKI, NETSCAPE_SPKI_free>;
using ECGroupPointer = DeleteFnPtr<EC_GROUP, EC_GROUP_free>;
using ECPointPointer = DeleteFnPtr<EC_POINT, EC_POINT_free>;
using ECKeyPointer = DeleteFnPtr<EC_KEY, EC_KEY_free>;
using DHPointer = DeleteFnPtr<DH, DH_free>;
using ECDSASigPointer = DeleteFnPtr<ECDSA_SIG, ECDSA_SIG_free>;

extern int VerifyCallback(int preverify_ok, X509_STORE_CTX* ctx);

extern void UseExtraCaCerts(const std::string& file);

void InitCryptoOnce();

class SecureContext final : public BaseObject {
 public:
  ~SecureContext() override;

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  SSL_CTX* operator*() const { return ctx_.get(); }

  // TODO(joyeecheung): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(SecureContext)
  SET_SELF_SIZE(SecureContext)

  SSLCtxPointer ctx_;
  X509Pointer cert_;
  X509Pointer issuer_;
#ifndef OPENSSL_NO_ENGINE
  bool client_cert_engine_provided_ = false;
  std::unique_ptr<ENGINE, std::function<void(ENGINE*)>> private_key_engine_;
#endif  // !OPENSSL_NO_ENGINE

  static const int kMaxSessionSize = 10 * 1024;

  // See TicketKeyCallback
  static const int kTicketKeyReturnIndex = 0;
  static const int kTicketKeyHMACIndex = 1;
  static const int kTicketKeyAESIndex = 2;
  static const int kTicketKeyNameIndex = 3;
  static const int kTicketKeyIVIndex = 4;

  unsigned char ticket_key_name_[16];
  unsigned char ticket_key_aes_[16];
  unsigned char ticket_key_hmac_[16];

 protected:
  // OpenSSL structures are opaque. This is sizeof(SSL_CTX) for OpenSSL 1.1.1b:
  static const int64_t kExternalSize = 1024;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Init(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetKey(const v8::FunctionCallbackInfo<v8::Value>& args);
#ifndef OPENSSL_NO_ENGINE
  static void SetEngineKey(const v8::FunctionCallbackInfo<v8::Value>& args);
#endif  // !OPENSSL_NO_ENGINE
  static void SetCert(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddCACert(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddCRL(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddRootCerts(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetCipherSuites(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetCiphers(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSigalgs(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetECDHCurve(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetDHParam(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetOptions(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSessionIdContext(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSessionTimeout(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetMinProto(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetMaxProto(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMinProto(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMaxProto(const v8::FunctionCallbackInfo<v8::Value>& args);
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
  static void CtxGetter(const v8::FunctionCallbackInfo<v8::Value>& info);

  template <bool primary>
  static void GetCertificate(const v8::FunctionCallbackInfo<v8::Value>& args);

  static int TicketKeyCallback(SSL* ssl,
                               unsigned char* name,
                               unsigned char* iv,
                               EVP_CIPHER_CTX* ectx,
                               HMAC_CTX* hctx,
                               int enc);

  static int TicketCompatibilityCallback(SSL* ssl,
                                         unsigned char* name,
                                         unsigned char* iv,
                                         EVP_CIPHER_CTX* ectx,
                                         HMAC_CTX* hctx,
                                         int enc);

  SecureContext(Environment* env, v8::Local<v8::Object> wrap);
  void Reset();
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
        awaiting_new_session_(false),
        cert_cb_(nullptr),
        cert_cb_arg_(nullptr),
        cert_cb_running_(false) {
    ssl_.reset(SSL_new(sc->ctx_.get()));
    CHECK(ssl_);
    env_->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
  }

  virtual ~SSLWrap() {
    DestroySSL();
  }

  inline void enable_session_callbacks() { session_callbacks_ = true; }
  inline bool is_server() const { return kind_ == kServer; }
  inline bool is_client() const { return kind_ == kClient; }
  inline bool is_awaiting_new_session() const { return awaiting_new_session_; }
  inline bool is_waiting_cert_cb() const { return cert_cb_ != nullptr; }

  void MemoryInfo(MemoryTracker* tracker) const;

 protected:
  typedef void (*CertCb)(void* arg);

  // OpenSSL structures are opaque. Estimate SSL memory size for OpenSSL 1.1.1b:
  //   SSL: 6224
  //   SSL->SSL3_STATE: 1040
  //   ...some buffers: 42 * 1024
  // NOTE: Actually it is much more than this
  static const int64_t kExternalSize = 6224 + 1040 + 42 * 1024;

  static void ConfigureSecureContext(SecureContext* sc);
  static void AddMethods(Environment* env, v8::Local<v8::FunctionTemplate> t);

  static SSL_SESSION* GetSessionCallback(SSL* s,
                                         const unsigned char* key,
                                         int len,
                                         int* copy);
  static int NewSessionCallback(SSL* s, SSL_SESSION* sess);
  static void KeylogCallback(const SSL* s, const char* line);
  static void OnClientHello(void* arg,
                            const ClientHelloParser::ClientHello& hello);

  static void GetPeerCertificate(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCertificate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetFinished(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPeerFinished(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoadSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsSessionReused(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyError(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCipher(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSharedSigalgs(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ExportKeyingMaterial(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EndParser(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CertCbDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Renegotiate(const v8::FunctionCallbackInfo<v8::Value>& args);
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

  void DestroySSL();
  void WaitForCertCb(CertCb cb, void* arg);
  int SetCACerts(SecureContext* sc);

  inline Environment* ssl_env() const {
    return env_;
  }

  Environment* const env_;
  Kind kind_;
  SSLSessionPointer next_sess_;
  SSLPointer ssl_;
  bool session_callbacks_;
  bool awaiting_new_session_;

  // SSL_set_cert_cb
  CertCb cert_cb_;
  void* cert_cb_arg_;
  bool cert_cb_running_;

  ClientHelloParser hello_parser_;

  v8::Global<v8::ArrayBufferView> ocsp_response_;
  BaseObjectPtr<SecureContext> sni_context_;

  friend class SecureContext;
};

// A helper class representing a read-only byte array. When deallocated, its
// contents are zeroed.
class ByteSource {
 public:
  ByteSource() = default;
  ByteSource(ByteSource&& other);
  ~ByteSource();

  ByteSource& operator=(ByteSource&& other);

  const char* get() const;
  size_t size() const;

  inline operator bool() const {
    return data_ != nullptr;
  }

  static ByteSource Allocated(char* data, size_t size);
  static ByteSource Foreign(const char* data, size_t size);

  static ByteSource FromStringOrBuffer(Environment* env,
                                       v8::Local<v8::Value> value);

  static ByteSource FromString(Environment* env,
                               v8::Local<v8::String> str,
                               bool ntc = false);

  static ByteSource FromBuffer(v8::Local<v8::Value> buffer,
                               bool ntc = false);

  static ByteSource NullTerminatedCopy(Environment* env,
                                       v8::Local<v8::Value> value);

  static ByteSource FromSymmetricKeyObjectHandle(v8::Local<v8::Value> handle);

  ByteSource(const ByteSource&) = delete;
  ByteSource& operator=(const ByteSource&) = delete;

 private:
  const char* data_ = nullptr;
  char* allocated_data_ = nullptr;
  size_t size_ = 0;

  ByteSource(const char* data, char* allocated_data, size_t size);
};

enum PKEncodingType {
  // RSAPublicKey / RSAPrivateKey according to PKCS#1.
  kKeyEncodingPKCS1,
  // PrivateKeyInfo or EncryptedPrivateKeyInfo according to PKCS#8.
  kKeyEncodingPKCS8,
  // SubjectPublicKeyInfo according to X.509.
  kKeyEncodingSPKI,
  // ECPrivateKey according to SEC1.
  kKeyEncodingSEC1
};

enum PKFormatType {
  kKeyFormatDER,
  kKeyFormatPEM
};

struct AsymmetricKeyEncodingConfig {
  bool output_key_object_;
  PKFormatType format_;
  v8::Maybe<PKEncodingType> type_ = v8::Nothing<PKEncodingType>();
};

typedef AsymmetricKeyEncodingConfig PublicKeyEncodingConfig;

struct PrivateKeyEncodingConfig : public AsymmetricKeyEncodingConfig {
  const EVP_CIPHER* cipher_;
  ByteSource passphrase_;
};

enum KeyType {
  kKeyTypeSecret,
  kKeyTypePublic,
  kKeyTypePrivate
};

// This uses the built-in reference counter of OpenSSL to manage an EVP_PKEY
// which is slightly more efficient than using a shared pointer and easier to
// use.
class ManagedEVPPKey {
 public:
  ManagedEVPPKey() = default;
  explicit ManagedEVPPKey(EVPKeyPointer&& pkey);
  ManagedEVPPKey(const ManagedEVPPKey& that);
  ManagedEVPPKey& operator=(const ManagedEVPPKey& that);

  operator bool() const;
  EVP_PKEY* get() const;

 private:
  EVPKeyPointer pkey_;
};

// Objects of this class can safely be shared among threads.
class KeyObjectData {
 public:
  static std::shared_ptr<KeyObjectData> CreateSecret(
      v8::Local<v8::ArrayBufferView> abv);
  static std::shared_ptr<KeyObjectData> CreateAsymmetric(
      KeyType type, const ManagedEVPPKey& pkey);

  KeyType GetKeyType() const;

  // These functions allow unprotected access to the raw key material and should
  // only be used to implement cryptographic operations requiring the key.
  ManagedEVPPKey GetAsymmetricKey() const;
  const char* GetSymmetricKey() const;
  size_t GetSymmetricKeySize() const;

 private:
  KeyObjectData(std::unique_ptr<char, std::function<void(char*)>> symmetric_key,
                unsigned int symmetric_key_len)
      : key_type_(KeyType::kKeyTypeSecret),
        symmetric_key_(std::move(symmetric_key)),
        symmetric_key_len_(symmetric_key_len),
        asymmetric_key_() {}

  KeyObjectData(KeyType type, const ManagedEVPPKey& pkey)
      : key_type_(type),
        symmetric_key_(),
        symmetric_key_len_(0),
        asymmetric_key_{pkey} {}

  const KeyType key_type_;
  const std::unique_ptr<char, std::function<void(char*)>> symmetric_key_;
  const unsigned int symmetric_key_len_;
  const ManagedEVPPKey asymmetric_key_;
};

class KeyObjectHandle : public BaseObject {
 public:
  static v8::Local<v8::Function> Initialize(Environment* env,
                                            v8::Local<v8::Object> target);

  static v8::MaybeLocal<v8::Object> Create(Environment* env,
                                           std::shared_ptr<KeyObjectData> data);

  // TODO(tniessen): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(KeyObjectHandle)
  SET_SELF_SIZE(KeyObjectHandle)

  const std::shared_ptr<KeyObjectData>& Data();

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Init(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void GetAsymmetricKeyType(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  v8::Local<v8::Value> GetAsymmetricKeyType() const;

  static void GetSymmetricKeySize(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Export(const v8::FunctionCallbackInfo<v8::Value>& args);
  v8::Local<v8::Value> ExportSecretKey() const;
  v8::MaybeLocal<v8::Value> ExportPublicKey(
      const PublicKeyEncodingConfig& config) const;
  v8::MaybeLocal<v8::Value> ExportPrivateKey(
      const PrivateKeyEncodingConfig& config) const;

  KeyObjectHandle(Environment* env,
                  v8::Local<v8::Object> wrap);

 private:
  std::shared_ptr<KeyObjectData> data_;
};

class NativeKeyObject : public BaseObject {
 public:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(NativeKeyObject)
  SET_SELF_SIZE(NativeKeyObject)

  class KeyObjectTransferData : public worker::TransferData {
   public:
    explicit KeyObjectTransferData(const std::shared_ptr<KeyObjectData>& data)
        : data_(data) {}

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    SET_MEMORY_INFO_NAME(KeyObjectTransferData)
    SET_SELF_SIZE(KeyObjectTransferData)
    SET_NO_MEMORY_INFO()

   private:
    std::shared_ptr<KeyObjectData> data_;
  };

  BaseObject::TransferMode GetTransferMode() const override;
  std::unique_ptr<worker::TransferData> CloneForMessaging() const override;

 private:
  NativeKeyObject(Environment* env,
                  v8::Local<v8::Object> wrap,
                  const std::shared_ptr<KeyObjectData>& handle_data)
    : BaseObject(env, wrap),
      handle_data_(handle_data) {}

  std::shared_ptr<KeyObjectData> handle_data_;
};

class CipherBase : public BaseObject {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  // TODO(joyeecheung): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(CipherBase)
  SET_SELF_SIZE(CipherBase)

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
  enum AuthTagState {
    kAuthTagUnknown,
    kAuthTagKnown,
    kAuthTagPassedToOpenSSL
  };
  static const unsigned kNoAuthTagLength = static_cast<unsigned>(-1);

  void CommonInit(const char* cipher_type,
                  const EVP_CIPHER* cipher,
                  const unsigned char* key,
                  int key_len,
                  const unsigned char* iv,
                  int iv_len,
                  unsigned int auth_tag_len);
  void Init(const char* cipher_type,
            const char* key_buf,
            int key_buf_len,
            unsigned int auth_tag_len);
  void InitIv(const char* cipher_type,
              const unsigned char* key,
              int key_len,
              const unsigned char* iv,
              int iv_len,
              unsigned int auth_tag_len);
  bool InitAuthenticated(const char* cipher_type, int iv_len,
                         unsigned int auth_tag_len);
  bool CheckCCMMessageLength(int message_len);
  UpdateResult Update(const char* data, int len, AllocatedBuffer* out);
  bool Final(AllocatedBuffer* out);
  bool SetAutoPadding(bool auto_padding);

  bool IsAuthenticatedMode() const;
  bool SetAAD(const char* data, unsigned int len, int plaintext_len);
  bool MaybePassAuthTagToOpenSSL();

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Init(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void InitIv(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Update(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Final(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAutoPadding(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void GetAuthTag(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAuthTag(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetAAD(const v8::FunctionCallbackInfo<v8::Value>& args);

  CipherBase(Environment* env, v8::Local<v8::Object> wrap, CipherKind kind);

 private:
  DeleteFnPtr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> ctx_;
  const CipherKind kind_;
  AuthTagState auth_tag_state_;
  unsigned int auth_tag_len_;
  char auth_tag_[EVP_GCM_TLS_TAG_LEN];
  bool pending_auth_failed_;
  int max_message_size_;
};

class Hmac : public BaseObject {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  // TODO(joyeecheung): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Hmac)
  SET_SELF_SIZE(Hmac)

 protected:
  void HmacInit(const char* hash_type, const char* key, int key_len);
  bool HmacUpdate(const char* data, int len);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HmacDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hmac(Environment* env, v8::Local<v8::Object> wrap);

 private:
  DeleteFnPtr<HMAC_CTX, HMAC_CTX_free> ctx_;
};

class Hash final : public BaseObject {
 public:
  ~Hash() override;

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  // TODO(joyeecheung): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(Hash)
  SET_SELF_SIZE(Hash)

  bool HashInit(const EVP_MD* md, v8::Maybe<unsigned int> xof_md_len);
  bool HashUpdate(const char* data, int len);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hash(Environment* env, v8::Local<v8::Object> wrap);

 private:
  EVPMDPointer mdctx_;
  bool has_md_;
  unsigned int md_len_;
  unsigned char* md_value_;
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
    kSignPublicKey,
    kSignMalformedSignature
  } Error;

  SignBase(Environment* env, v8::Local<v8::Object> wrap);

  Error Init(const char* sign_type);
  Error Update(const char* data, int len);

  // TODO(joyeecheung): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(SignBase)
  SET_SELF_SIZE(SignBase)

 protected:
  void CheckThrow(Error error);

  EVPMDPointer mdctx_;
};

enum DSASigEnc {
  kSigEncDER, kSigEncP1363
};

class Sign : public SignBase {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  struct SignResult {
    Error error;
    AllocatedBuffer signature;

    explicit SignResult(
        Error err,
        AllocatedBuffer&& sig = AllocatedBuffer())
      : error(err), signature(std::move(sig)) {}
  };

  SignResult SignFinal(
      const ManagedEVPPKey& pkey,
      int padding,
      const v8::Maybe<int>& saltlen,
      DSASigEnc dsa_sig_enc);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SignFinal(const v8::FunctionCallbackInfo<v8::Value>& args);

  Sign(Environment* env, v8::Local<v8::Object> wrap);
};

class Verify : public SignBase {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  Error VerifyFinal(const ManagedEVPPKey& key,
                    const ByteSource& sig,
                    int padding,
                    const v8::Maybe<int>& saltlen,
                    bool* verify_result);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyInit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyFinal(const v8::FunctionCallbackInfo<v8::Value>& args);

  Verify(Environment* env, v8::Local<v8::Object> wrap);
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
  static bool Cipher(Environment* env,
                     const ManagedEVPPKey& pkey,
                     int padding,
                     const EVP_MD* digest,
                     const void* oaep_label,
                     size_t oaep_label_size,
                     const unsigned char* data,
                     int len,
                     AllocatedBuffer* out);

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

  DiffieHellman(Environment* env, v8::Local<v8::Object> wrap);

  // TODO(joyeecheung): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(DiffieHellman)
  SET_SELF_SIZE(DiffieHellman)

 private:
  static void GetField(const v8::FunctionCallbackInfo<v8::Value>& args,
                       const BIGNUM* (*get_field)(const DH*),
                       const char* err_if_null);
  static void SetKey(const v8::FunctionCallbackInfo<v8::Value>& args,
                     int (*set_field)(DH*, BIGNUM*), const char* what);
  bool VerifyContext();

  int verifyError_;
  DHPointer dh_;
};

class ECDH final : public BaseObject {
 public:
  ~ECDH() override;

  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static ECPointPointer BufferToPoint(Environment* env,
                                      const EC_GROUP* group,
                                      v8::Local<v8::Value> buf);

  // TODO(joyeecheung): track the memory used by OpenSSL types
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(ECDH)
  SET_SELF_SIZE(ECDH)

 protected:
  ECDH(Environment* env, v8::Local<v8::Object> wrap, ECKeyPointer&& key);

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

void ThrowCryptoError(Environment* env,
                      unsigned long err,  // NOLINT(runtime/int)
                      const char* message = nullptr);

template <typename T>
inline T* MallocOpenSSL(size_t count) {
  void* mem = OPENSSL_malloc(MultiplyWithOverflowCheck(count, sizeof(T)));
  CHECK_IMPLIES(mem == nullptr, count == 0);
  return static_cast<T*>(mem);
}

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CRYPTO_H_
