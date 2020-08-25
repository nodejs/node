#ifndef SRC_CRYPTO_CRYPTO_UTIL_H_
#define SRC_CRYPTO_CRYPTO_UTIL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "async_wrap.h"
#include "allocated_buffer.h"
#include "node_errors.h"
#include "node_internals.h"
#include "util.h"
#include "v8.h"
#include "string_bytes.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/kdf.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/ssl.h>
#ifndef OPENSSL_NO_ENGINE
#  include <openssl/engine.h>
#endif  // !OPENSSL_NO_ENGINE

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <climits>
#include <cstdio>

namespace node {
namespace crypto {
// Currently known sizes of commonly used OpenSSL struct sizes.
// OpenSSL considers it's various structs to be opaque and the
// sizes may change from one version of OpenSSL to another, so
// these values should not be trusted to remain static. These
// are provided to allow for some close to reasonable memory
// tracking.
constexpr size_t kSizeOf_DH = 144;
constexpr size_t kSizeOf_EC_KEY = 80;
constexpr size_t kSizeOf_EVP_CIPHER_CTX = 168;
constexpr size_t kSizeOf_EVP_MD_CTX = 48;
constexpr size_t kSizeOf_EVP_PKEY = 72;
constexpr size_t kSizeOf_EVP_PKEY_CTX = 80;
constexpr size_t kSizeOf_HMAC_CTX = 32;

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
using HMACCtxPointer = DeleteFnPtr<HMAC_CTX, HMAC_CTX_free>;
using CipherCtxPointer = DeleteFnPtr<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free>;
using RsaPointer = DeleteFnPtr<RSA, RSA_free>;
using DsaPointer = DeleteFnPtr<DSA, DSA_free>;
using EcdsaSigPointer = DeleteFnPtr<ECDSA_SIG, ECDSA_SIG_free>;

// Our custom implementation of the certificate verify callback
// used when establishing a TLS handshake. Because we cannot perform
// I/O quickly enough with X509_STORE_CTX_ APIs in this callback,
// we ignore preverify_ok errors here and let the handshake continue.
// In other words, this VerifyCallback is a non-op. It is imperative
// that the user user Connection::VerifyError after the `secure`
// callback has been made.
extern int VerifyCallback(int preverify_ok, X509_STORE_CTX* ctx);

void InitCryptoOnce();

void InitCrypto(v8::Local<v8::Object> target);

extern void UseExtraCaCerts(const std::string& file);

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

// Ensure that OpenSSL has enough entropy (at least 256 bits) for its PRNG.
// The entropy pool starts out empty and needs to fill up before the PRNG
// can be used securely.  Once the pool is filled, it never dries up again;
// its contents is stirred and reused when necessary.
//
// OpenSSL normally fills the pool automatically but not when someone starts
// generating random numbers before the pool is full: in that case OpenSSL
// keeps lowering the entropy estimate to thwart attackers trying to guess
// the initial state of the PRNG.
//
// When that happens, we will have to wait until enough entropy is available.
// That should normally never take longer than a few milliseconds.
//
// OpenSSL draws from /dev/random and /dev/urandom.  While /dev/random may
// block pending "true" randomness, /dev/urandom is a CSPRNG that doesn't
// block under normal circumstances.
//
// The only time when /dev/urandom may conceivably block is right after boot,
// when the whole system is still low on entropy.  That's not something we can
// do anything about.
void CheckEntropy();

// Generate length bytes of random data. If this returns false, the data
// may not be truly random but it's still generally good enough.
bool EntropySource(unsigned char* buffer, size_t length);

int PasswordCallback(char* buf, int size, int rwflag, void* u);

int NoPasswordCallback(char* buf, int size, int rwflag, void* u);

// Decode is used by the various stream-based crypto utilities to decode
// string input.
template <typename T>
void Decode(const v8::FunctionCallbackInfo<v8::Value>& args,
            void (*callback)(T*, const v8::FunctionCallbackInfo<v8::Value>&,
                             const char*, size_t)) {
  T* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  if (args[0]->IsString()) {
    StringBytes::InlineDecoder decoder;
    Environment* env = Environment::GetCurrent(args);
    enum encoding enc = ParseEncoding(env->isolate(), args[1], UTF8);
    if (decoder.Decode(env, args[0].As<v8::String>(), enc).IsNothing())
      return;
    callback(ctx, args, decoder.out(), decoder.size());
  } else {
    ArrayBufferViewContents<char> buf(args[0]);
    callback(ctx, args, buf.data(), buf.length());
  }
}

// Utility struct used to harvest error information from openssl's error stack
struct CryptoErrorVector : public std::vector<std::string> {
  void Capture();

  v8::MaybeLocal<v8::Value> ToException(
      Environment* env,
      v8::Local<v8::String> exception_string = v8::Local<v8::String>()) const;
};

template <typename T>
T* MallocOpenSSL(size_t count) {
  void* mem = OPENSSL_malloc(MultiplyWithOverflowCheck(count, sizeof(T)));
  CHECK_IMPLIES(mem == nullptr, count == 0);
  return static_cast<T*>(mem);
}

template <typename T>
T* ReallocOpenSSL(T* buf, size_t count) {
  void* mem = OPENSSL_realloc(buf, MultiplyWithOverflowCheck(count, sizeof(T)));
  CHECK_IMPLIES(mem == nullptr, count == 0);
  return static_cast<T*>(mem);
}

// A helper class representing a read-only byte array. When deallocated, its
// contents are zeroed.
class ByteSource {
 public:
  ByteSource() = default;
  ByteSource(ByteSource&& other) noexcept;
  ~ByteSource();

  ByteSource& operator=(ByteSource&& other) noexcept;

  const char* get() const;

  template <typename T>
  const T* data() const { return reinterpret_cast<const T*>(get()); }

  size_t size() const;

  operator bool() const { return data_ != nullptr; }

  BignumPointer ToBN() const {
    return BignumPointer(BN_bin2bn(
        reinterpret_cast<const unsigned char*>(get()),
        size(),
        nullptr));
  }

  // Creates a v8::BackingStore that takes over responsibility for
  // any allocated data. The ByteSource will be reset with size = 0
  // after being called.
  std::unique_ptr<v8::BackingStore> ReleaseToBackingStore();

  v8::Local<v8::ArrayBuffer> ToArrayBuffer(Environment* env);

  void reset();

  // Allows an Allocated ByteSource to be truncated.
  void Resize(size_t newsize) {
    CHECK_LE(newsize, size_);
    CHECK_NOT_NULL(allocated_data_);
    char* new_data_ = ReallocOpenSSL<char>(allocated_data_, newsize);
    data_ = allocated_data_ = new_data_;
    size_ = newsize;
  }

  static ByteSource Allocated(char* data, size_t size);
  static ByteSource Foreign(const char* data, size_t size);

  static ByteSource FromEncodedString(Environment* env,
                                      v8::Local<v8::String> value,
                                      enum encoding enc = BASE64);

  static ByteSource FromStringOrBuffer(Environment* env,
                                       v8::Local<v8::Value> value);

  static ByteSource FromString(Environment* env,
                               v8::Local<v8::String> str,
                               bool ntc = false);

  static ByteSource FromBuffer(v8::Local<v8::Value> buffer,
                               bool ntc = false);

  static ByteSource FromBIO(const BIOPointer& bio);

  static ByteSource NullTerminatedCopy(Environment* env,
                                       v8::Local<v8::Value> value);

  static ByteSource FromSymmetricKeyObjectHandle(v8::Local<v8::Value> handle);

  ByteSource(const ByteSource&) = delete;
  ByteSource& operator=(const ByteSource&) = delete;

  static ByteSource FromSecretKeyBytes(
      Environment* env, v8::Local<v8::Value> value);

 private:
  const char* data_ = nullptr;
  char* allocated_data_ = nullptr;
  size_t size_ = 0;

  ByteSource(const char* data, char* allocated_data, size_t size);
};

enum CryptoJobMode {
  kCryptoJobAsync,
  kCryptoJobSync
};

CryptoJobMode GetCryptoJobMode(v8::Local<v8::Value> args);

template <typename CryptoJobTraits>
class CryptoJob : public AsyncWrap, public ThreadPoolWork {
 public:
  using AdditionalParams = typename CryptoJobTraits::AdditionalParameters;

  explicit CryptoJob(
      Environment* env,
      v8::Local<v8::Object> object,
      AsyncWrap::ProviderType type,
      CryptoJobMode mode,
      AdditionalParams&& params)
      : AsyncWrap(env, object, type),
        ThreadPoolWork(env),
        mode_(mode),
        params_(std::move(params)) {
    // If the CryptoJob is async, then the instance will be
    // cleaned up when AfterThreadPoolWork is called.
    if (mode == kCryptoJobSync) MakeWeak();
  }

  bool IsNotIndicativeOfMemoryLeakAtExit() const override {
    // CryptoJobs run a work in the libuv thread pool and may still
    // exist when the event loop empties and starts to exit.
    return true;
  }

  void AfterThreadPoolWork(int status) override {
    Environment* env = AsyncWrap::env();
    CHECK_EQ(mode_, kCryptoJobAsync);
    CHECK(status == 0 || status == UV_ECANCELED);
    std::unique_ptr<CryptoJob> ptr(this);
    // If the job was canceled do not execute the callback.
    // TODO(@jasnell): We should likely revisit skipping the
    // callback on cancel as that could leave the JS in a pending
    // state (e.g. unresolved promises...)
    if (status == UV_ECANCELED) return;
    v8::HandleScope handle_scope(env->isolate());
    v8::Context::Scope context_scope(env->context());
    v8::Local<v8::Value> args[2];
    if (ptr->ToResult(&args[0], &args[1]).FromJust())
      ptr->MakeCallback(env->ondone_string(), arraysize(args), args);
  }

  virtual v8::Maybe<bool> ToResult(
      v8::Local<v8::Value>* err,
      v8::Local<v8::Value>* result) = 0;

  CryptoJobMode mode() const { return mode_; }

  CryptoErrorVector* errors() { return &errors_; }

  AdditionalParams* params() { return &params_; }

  std::string MemoryInfoName() const override {
    return CryptoJobTraits::JobName;
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("params", params_);
    tracker->TrackField("errors", errors_);
  }

  static void Run(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    CryptoJob<CryptoJobTraits>* job;
    ASSIGN_OR_RETURN_UNWRAP(&job, args.Holder());
    if (job->mode() == kCryptoJobAsync)
      return job->ScheduleWork();

    v8::Local<v8::Value> ret[2];
    env->PrintSyncTrace();
    job->DoThreadPoolWork();
    if (job->ToResult(&ret[0], &ret[1]).FromJust()) {
      args.GetReturnValue().Set(
          v8::Array::New(env->isolate(), ret, arraysize(ret)));
    }
  }

  static void Initialize(
      v8::FunctionCallback new_fn,
      Environment* env,
      v8::Local<v8::Object> target) {
    v8::Local<v8::FunctionTemplate> job = env->NewFunctionTemplate(new_fn);
    v8::Local<v8::String> class_name =
        OneByteString(env->isolate(), CryptoJobTraits::JobName);
    job->SetClassName(class_name);
    job->Inherit(AsyncWrap::GetConstructorTemplate(env));
    job->InstanceTemplate()->SetInternalFieldCount(
        AsyncWrap::kInternalFieldCount);
    env->SetProtoMethod(job, "run", Run);
    target->Set(
        env->context(),
        class_name,
        job->GetFunction(env->context()).ToLocalChecked()).Check();
  }

 private:
  const CryptoJobMode mode_;
  CryptoErrorVector errors_;
  AdditionalParams params_;
};

template <typename DeriveBitsTraits>
class DeriveBitsJob final : public CryptoJob<DeriveBitsTraits> {
 public:
  using AdditionalParams = typename DeriveBitsTraits::AdditionalParameters;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    CryptoJobMode mode = GetCryptoJobMode(args[0]);

    AdditionalParams params;
    if (DeriveBitsTraits::AdditionalConfig(mode, args, 1, &params)
            .IsNothing()) {
      // The DeriveBitsTraits::AdditionalConfig is responsible for
      // calling an appropriate THROW_CRYPTO_* variant reporting
      // whatever error caused initialization to fail.
      return;
    }

    new DeriveBitsJob(env, args.This(), mode, std::move(params));
  }

  static void Initialize(
      Environment* env,
      v8::Local<v8::Object> target) {
    CryptoJob<DeriveBitsTraits>::Initialize(New, env, target);
  }

  DeriveBitsJob(
      Environment* env,
      v8::Local<v8::Object> object,
      CryptoJobMode mode,
      AdditionalParams&& params)
      : CryptoJob<DeriveBitsTraits>(
            env,
            object,
            DeriveBitsTraits::Provider,
            mode,
            std::move(params)) {}

  void DoThreadPoolWork() override {
    if (!DeriveBitsTraits::DeriveBits(
            AsyncWrap::env(),
            *CryptoJob<DeriveBitsTraits>::params(), &out_)) {
      CryptoErrorVector* errors = CryptoJob<DeriveBitsTraits>::errors();
      errors->Capture();
      if (errors->empty())
        errors->push_back("Deriving bits failed");
      return;
    }
    success_ = true;
  }

  v8::Maybe<bool> ToResult(
      v8::Local<v8::Value>* err,
      v8::Local<v8::Value>* result) override {
    Environment* env = AsyncWrap::env();
    CryptoErrorVector* errors = CryptoJob<DeriveBitsTraits>::errors();
    if (success_) {
      CHECK(errors->empty());
      *err = v8::Undefined(env->isolate());
      return DeriveBitsTraits::EncodeOutput(
          env,
          *CryptoJob<DeriveBitsTraits>::params(),
          &out_,
          result);
    }

    if (errors->empty())
      errors->Capture();
    CHECK(!errors->empty());
    *result = v8::Undefined(env->isolate());
    return v8::Just(errors->ToException(env).ToLocal(err));
  }

  SET_SELF_SIZE(DeriveBitsJob);
  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize("out", out_.size());
    CryptoJob<DeriveBitsTraits>::MemoryInfo(tracker);
  }

 private:
  ByteSource out_;
  bool success_ = false;
};

void ThrowCryptoError(Environment* env,
                      unsigned long err,  // NOLINT(runtime/int)
                      const char* message = nullptr);

#ifndef OPENSSL_NO_ENGINE
struct EnginePointer {
  ENGINE* engine = nullptr;
  bool finish_on_exit = false;

  inline EnginePointer() = default;

  inline explicit EnginePointer(ENGINE* engine_, bool finish_on_exit_ = false)
    : engine(engine_),
      finish_on_exit(finish_on_exit_) {}

  inline EnginePointer(EnginePointer&& other) noexcept
      : engine(other.engine),
        finish_on_exit(other.finish_on_exit) {
    other.release();
  }

  inline ~EnginePointer() { reset(); }

  inline EnginePointer& operator=(EnginePointer&& other) noexcept {
    if (this == &other) return *this;
    this->~EnginePointer();
    return *new (this) EnginePointer(std::move(other));
  }

  inline operator bool() const { return engine != nullptr; }

  inline ENGINE* get() { return engine; }

  inline void reset(ENGINE* engine_ = nullptr, bool finish_on_exit_ = false) {
    if (engine != nullptr) {
      if (finish_on_exit)
        ENGINE_finish(engine);
      ENGINE_free(engine);
    }
    engine = engine_;
    finish_on_exit = finish_on_exit_;
  }

  inline ENGINE* release() {
    ENGINE* ret = engine;
    engine = nullptr;
    finish_on_exit = false;
    return ret;
  }
};

EnginePointer LoadEngineById(const char* id, CryptoErrorVector* errors);

bool SetEngine(
    const char* id,
    uint32_t flags,
    CryptoErrorVector* errors = nullptr);

void SetEngine(const v8::FunctionCallbackInfo<v8::Value>& args);
#endif  // !OPENSSL_NO_ENGINE

#ifdef NODE_FIPS_MODE
void GetFipsCrypto(const v8::FunctionCallbackInfo<v8::Value>& args);

void SetFipsCrypto(const v8::FunctionCallbackInfo<v8::Value>& args);
#endif /* NODE_FIPS_MODE */

class CipherPushContext {
 public:
  inline explicit CipherPushContext(Environment* env) : env_(env) {}

  inline void push_back(const char* str) {
    list_.emplace_back(OneByteString(env_->isolate(), str));
  }

  inline v8::Local<v8::Array> ToJSArray() {
    return v8::Array::New(env_->isolate(), list_.data(), list_.size());
  }

 private:
  std::vector<v8::Local<v8::Value>> list_;
  Environment* env_;
};

template <class TypeName>
void array_push_back(const TypeName* md,
                     const char* from,
                     const char* to,
                     void* arg) {
  static_cast<CipherPushContext*>(arg)->push_back(from);
}

inline bool IsAnyByteSource(v8::Local<v8::Value> arg) {
  return arg->IsArrayBufferView() ||
         arg->IsArrayBuffer() ||
         arg->IsSharedArrayBuffer();
}

template <typename T>
class ArrayBufferOrViewContents {
 public:
  ArrayBufferOrViewContents() = default;

  inline explicit ArrayBufferOrViewContents(v8::Local<v8::Value> buf) {
    CHECK(IsAnyByteSource(buf));
    if (buf->IsArrayBufferView()) {
      auto view = buf.As<v8::ArrayBufferView>();
      offset_ = view->ByteOffset();
      length_ = view->ByteLength();
      store_ = view->Buffer()->GetBackingStore();
    } else if (buf->IsArrayBuffer()) {
      auto ab = buf.As<v8::ArrayBuffer>();
      offset_ = 0;
      length_ = ab->ByteLength();
      store_ = ab->GetBackingStore();
    } else {
      auto sab = buf.As<v8::SharedArrayBuffer>();
      offset_ = 0;
      length_ = sab->ByteLength();
      store_ = sab->GetBackingStore();
    }
  }

  inline const T* data() const {
    // Ideally, these would return nullptr if IsEmpty() or length_ is zero,
    // but some of the openssl API react badly if given a nullptr even when
    // length is zero, so we have to return something.
    if (size() == 0)
      return &buf;
    return reinterpret_cast<T*>(store_->Data()) + offset_;
  }

  inline T* data() {
    // Ideally, these would return nullptr if IsEmpty() or length_ is zero,
    // but some of the openssl API react badly if given a nullptr even when
    // length is zero, so we have to return something.
    if (size() == 0)
      return &buf;
    return reinterpret_cast<T*>(store_->Data()) + offset_;
  }

  inline size_t size() const { return length_; }

  // In most cases, input buffer sizes passed in to openssl need to
  // be limited to <= INT_MAX. This utility method helps us check.
  inline bool CheckSizeInt32() { return size() <= INT_MAX; }

  inline ByteSource ToByteSource() const {
    return ByteSource::Foreign(data(), size());
  }

  inline ByteSource ToCopy() const {
    if (size() == 0) return ByteSource();
    char* buf = MallocOpenSSL<char>(size());
    CHECK_NOT_NULL(buf);
    memcpy(buf, data(), size());
    return ByteSource::Allocated(buf, size());
  }

  inline ByteSource ToNullTerminatedCopy() const {
    if (size() == 0) return ByteSource();
    char* buf = MallocOpenSSL<char>(size() + 1);
    CHECK_NOT_NULL(buf);
    buf[size()] = 0;
    memcpy(buf, data(), size());
    return ByteSource::Allocated(buf, size());
  }

  template <typename M>
  void CopyTo(M* dest, size_t len) const {
    static_assert(sizeof(M) == 1, "sizeof(M) must equal 1");
    len = std::min(len, size());
    if (len > 0 && data() != nullptr)
      memcpy(dest, data(), len);
  }

 private:
  T buf = 0;
  size_t offset_ = 0;
  size_t length_ = 0;
  std::shared_ptr<v8::BackingStore> store_;
};

template <typename T>
std::vector<T> CopyBuffer(const ArrayBufferOrViewContents<T>& buf) {
  std::vector<T> vec;
  vec->resize(buf.size());
  if (vec->size() > 0 && buf.data() != nullptr)
    memcpy(vec->data(), buf.data(), vec->size());
  return vec;
}

template <typename T>
std::vector<T> CopyBuffer(v8::Local<v8::Value> buf) {
  return CopyBuffer(ArrayBufferOrViewContents<T>(buf));
}

v8::MaybeLocal<v8::Value> EncodeBignum(
    Environment* env,
    const BIGNUM* bn,
    v8::Local<v8::Value>* error);

v8::Maybe<bool> SetEncodedValue(
    Environment* env,
    v8::Local<v8::Object> target,
    v8::Local<v8::String> name,
    const BIGNUM* bn,
    int size = 0);

namespace Util {
void Initialize(Environment* env, v8::Local<v8::Object> target);
}  // namespace Util

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_UTIL_H_
