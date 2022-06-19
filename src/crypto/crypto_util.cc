#include "crypto/crypto_util.h"
#include "async_wrap-inl.h"
#include "crypto/crypto_bio.h"
#include "crypto/crypto_keys.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "node_options-inl.h"
#include "string_bytes.h"
#include "threadpoolwork-inl.h"
#include "util-inl.h"
#include "v8.h"

#include "math.h"

#if OPENSSL_VERSION_MAJOR >= 3
#include "openssl/provider.h"
#endif

#include <openssl/rand.h>

namespace node {

using v8::ArrayBuffer;
using v8::BackingStore;
using v8::BigInt;
using v8::Context;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Nothing;
using v8::Object;
using v8::String;
using v8::TryCatch;
using v8::Uint32;
using v8::Uint8Array;
using v8::Value;

namespace crypto {
int VerifyCallback(int preverify_ok, X509_STORE_CTX* ctx) {
  // From https://www.openssl.org/docs/man1.1.1/man3/SSL_verify_cb:
  //
  //   If VerifyCallback returns 1, the verification process is continued. If
  //   VerifyCallback always returns 1, the TLS/SSL handshake will not be
  //   terminated with respect to verification failures and the connection will
  //   be established. The calling process can however retrieve the error code
  //   of the last verification error using SSL_get_verify_result(3) or by
  //   maintaining its own error storage managed by VerifyCallback.
  //
  // Since we cannot perform I/O quickly enough with X509_STORE_CTX_ APIs in
  // this callback, we ignore all preverify_ok errors and let the handshake
  // continue. It is imperative that the user use Connection::VerifyError after
  // the 'secure' callback has been made.
  return 1;
}

void CheckEntropy() {
  for (;;) {
    int status = RAND_status();
    CHECK_GE(status, 0);  // Cannot fail.
    if (status != 0)
      break;

    // Give up, RAND_poll() not supported.
    if (RAND_poll() == 0)
      break;
  }
}

bool EntropySource(unsigned char* buffer, size_t length) {
  // Ensure that OpenSSL's PRNG is properly seeded.
  CheckEntropy();
  // RAND_bytes() can return 0 to indicate that the entropy data is not truly
  // random. That's okay, it's still better than V8's stock source of entropy,
  // which is /dev/urandom on UNIX platforms and the current time on Windows.
  return RAND_bytes(buffer, length) != -1;
}

int PasswordCallback(char* buf, int size, int rwflag, void* u) {
  const ByteSource* passphrase = *static_cast<const ByteSource**>(u);
  if (passphrase != nullptr) {
    size_t buflen = static_cast<size_t>(size);
    size_t len = passphrase->size();
    if (buflen < len)
      return -1;
    memcpy(buf, passphrase->data(), len);
    return len;
  }

  return -1;
}

// This callback is used to avoid the default passphrase callback in OpenSSL
// which will typically prompt for the passphrase. The prompting is designed
// for the OpenSSL CLI, but works poorly for Node.js because it involves
// synchronous interaction with the controlling terminal, something we never
// want, and use this function to avoid it.
int NoPasswordCallback(char* buf, int size, int rwflag, void* u) {
  return 0;
}

bool ProcessFipsOptions() {
  /* Override FIPS settings in configuration file, if needed. */
  if (per_process::cli_options->enable_fips_crypto ||
      per_process::cli_options->force_fips_crypto) {
#if OPENSSL_VERSION_MAJOR >= 3
    OSSL_PROVIDER* fips_provider = OSSL_PROVIDER_load(nullptr, "fips");
    if (fips_provider == nullptr)
      return false;
    OSSL_PROVIDER_unload(fips_provider);

    return EVP_default_properties_enable_fips(nullptr, 1) &&
           EVP_default_properties_is_fips_enabled(nullptr);
#else
    return FIPS_mode() == 0 && FIPS_mode_set(1);
#endif
  }
  return true;
}

bool InitCryptoOnce(Isolate* isolate) {
  static uv_once_t init_once = UV_ONCE_INIT;
  TryCatch try_catch{isolate};
  uv_once(&init_once, InitCryptoOnce);
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    try_catch.ReThrow();
    return false;
  }
  return true;
}

// Protect accesses to FIPS state with a mutex. This should potentially
// be part of a larger mutex for global OpenSSL state.
static Mutex fips_mutex;

void InitCryptoOnce() {
  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  Mutex::ScopedLock fips_lock(fips_mutex);
#ifndef OPENSSL_IS_BORINGSSL
  OPENSSL_INIT_SETTINGS* settings = OPENSSL_INIT_new();

#if OPENSSL_VERSION_MAJOR < 3
  // --openssl-config=...
  if (!per_process::cli_options->openssl_config.empty()) {
    const char* conf = per_process::cli_options->openssl_config.c_str();
    OPENSSL_INIT_set_config_filename(settings, conf);
  }
#endif

#if OPENSSL_VERSION_MAJOR >= 3
  // --openssl-legacy-provider
  if (per_process::cli_options->openssl_legacy_provider) {
    OSSL_PROVIDER* legacy_provider = OSSL_PROVIDER_load(nullptr, "legacy");
    if (legacy_provider == nullptr) {
      fprintf(stderr, "Unable to load legacy provider.\n");
    }
  }
#endif

  OPENSSL_init_ssl(0, settings);
  OPENSSL_INIT_free(settings);
  settings = nullptr;

#ifndef _WIN32
  if (per_process::cli_options->secure_heap != 0) {
    switch (CRYPTO_secure_malloc_init(
                per_process::cli_options->secure_heap,
                static_cast<int>(per_process::cli_options->secure_heap_min))) {
      case 0:
        fprintf(stderr, "Unable to initialize openssl secure heap.\n");
        break;
      case 2:
        // Not a fatal error but worthy of a warning.
        fprintf(stderr, "Unable to memory map openssl secure heap.\n");
        break;
      case 1:
        // OK!
        break;
    }
  }
#endif

#endif  // OPENSSL_IS_BORINGSSL

  // Turn off compression. Saves memory and protects against CRIME attacks.
  // No-op with OPENSSL_NO_COMP builds of OpenSSL.
  sk_SSL_COMP_zero(SSL_COMP_get_compression_methods());

#ifndef OPENSSL_NO_ENGINE
  ERR_load_ENGINE_strings();
  ENGINE_load_builtin_engines();
#endif  // !OPENSSL_NO_ENGINE

  NodeBIO::GetMethod();
}

void GetFipsCrypto(const FunctionCallbackInfo<Value>& args) {
  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  Mutex::ScopedLock fips_lock(fips_mutex);

#if OPENSSL_VERSION_MAJOR >= 3
  args.GetReturnValue().Set(EVP_default_properties_is_fips_enabled(nullptr) ?
      1 : 0);
#else
  args.GetReturnValue().Set(FIPS_mode() ? 1 : 0);
#endif
}

void SetFipsCrypto(const FunctionCallbackInfo<Value>& args) {
  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  Mutex::ScopedLock fips_lock(fips_mutex);

  CHECK(!per_process::cli_options->force_fips_crypto);
  Environment* env = Environment::GetCurrent(args);
  // TODO(addaleax): This should not be possible to set from worker threads.
  // CHECK(env->owns_process_state());
  bool enable = args[0]->BooleanValue(env->isolate());

#if OPENSSL_VERSION_MAJOR >= 3
  if (enable == EVP_default_properties_is_fips_enabled(nullptr))
#else
  if (static_cast<int>(enable) == FIPS_mode())
#endif
    return;  // No action needed.

#if OPENSSL_VERSION_MAJOR >= 3
  if (!EVP_default_properties_enable_fips(nullptr, enable)) {
#else
  if (!FIPS_mode_set(enable)) {
#endif
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)
    return ThrowCryptoError(env, err);
  }
}

void TestFipsCrypto(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  Mutex::ScopedLock fips_lock(fips_mutex);

#ifdef OPENSSL_FIPS
#if OPENSSL_VERSION_MAJOR >= 3
  OSSL_PROVIDER* fips_provider = nullptr;
  if (OSSL_PROVIDER_available(nullptr, "fips")) {
    fips_provider = OSSL_PROVIDER_load(nullptr, "fips");
  }
  const auto enabled = fips_provider == nullptr ? 0 :
      OSSL_PROVIDER_self_test(fips_provider) ? 1 : 0;
#else
  const auto enabled = FIPS_selftest() ? 1 : 0;
#endif
#else  // OPENSSL_FIPS
  const auto enabled = 0;
#endif  // OPENSSL_FIPS

  args.GetReturnValue().Set(enabled);
}

void CryptoErrorStore::Capture() {
  errors_.clear();
  while (const uint32_t err = ERR_get_error()) {
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    errors_.emplace_back(buf);
  }
  std::reverse(std::begin(errors_), std::end(errors_));
}

bool CryptoErrorStore::Empty() const {
  return errors_.empty();
}

MaybeLocal<Value> CryptoErrorStore::ToException(
    Environment* env,
    Local<String> exception_string) const {
  if (exception_string.IsEmpty()) {
    CryptoErrorStore copy(*this);
    if (copy.Empty()) {
      // But possibly a bug...
      copy.Insert(NodeCryptoError::OK);
    }
    // Use last element as the error message, everything else goes
    // into the .opensslErrorStack property on the exception object.
    const std::string& last_error_string = copy.errors_.back();
    Local<String> exception_string;
    if (!String::NewFromUtf8(
            env->isolate(),
            last_error_string.data(),
            NewStringType::kNormal,
            last_error_string.size()).ToLocal(&exception_string)) {
      return MaybeLocal<Value>();
    }
    copy.errors_.pop_back();
    return copy.ToException(env, exception_string);
  }

  Local<Value> exception_v = Exception::Error(exception_string);
  CHECK(!exception_v.IsEmpty());

  if (!Empty()) {
    CHECK(exception_v->IsObject());
    Local<Object> exception = exception_v.As<Object>();
    Local<Value> stack;
    if (!ToV8Value(env->context(), errors_).ToLocal(&stack) ||
        exception->Set(env->context(), env->openssl_error_stack(), stack)
            .IsNothing()) {
      return MaybeLocal<Value>();
    }
  }

  return exception_v;
}

ByteSource::ByteSource(ByteSource&& other) noexcept
    : data_(other.data_),
      allocated_data_(other.allocated_data_),
      size_(other.size_) {
  other.allocated_data_ = nullptr;
}

ByteSource::~ByteSource() {
  OPENSSL_clear_free(allocated_data_, size_);
}

ByteSource& ByteSource::operator=(ByteSource&& other) noexcept {
  if (&other != this) {
    OPENSSL_clear_free(allocated_data_, size_);
    data_ = other.data_;
    allocated_data_ = other.allocated_data_;
    other.allocated_data_ = nullptr;
    size_ = other.size_;
  }
  return *this;
}

std::unique_ptr<BackingStore> ByteSource::ReleaseToBackingStore() {
  // It's ok for allocated_data_ to be nullptr but
  // only if size_ is zero.
  CHECK_IMPLIES(size_ > 0, allocated_data_ != nullptr);
  std::unique_ptr<BackingStore> ptr = ArrayBuffer::NewBackingStore(
      allocated_data_,
      size(),
      [](void* data, size_t length, void* deleter_data) {
        OPENSSL_clear_free(deleter_data, length);
      }, allocated_data_);
  CHECK(ptr);
  allocated_data_ = nullptr;
  data_ = nullptr;
  size_ = 0;
  return ptr;
}

Local<ArrayBuffer> ByteSource::ToArrayBuffer(Environment* env) {
  std::unique_ptr<BackingStore> store = ReleaseToBackingStore();
  return ArrayBuffer::New(env->isolate(), std::move(store));
}

MaybeLocal<Uint8Array> ByteSource::ToBuffer(Environment* env) {
  Local<ArrayBuffer> ab = ToArrayBuffer(env);
  return Buffer::New(env, ab, 0, ab->ByteLength());
}

ByteSource ByteSource::FromBIO(const BIOPointer& bio) {
  CHECK(bio);
  BUF_MEM* bptr;
  BIO_get_mem_ptr(bio.get(), &bptr);
  ByteSource::Builder out(bptr->length);
  memcpy(out.data<void>(), bptr->data, bptr->length);
  return std::move(out).release();
}

ByteSource ByteSource::FromEncodedString(Environment* env,
                                         Local<String> key,
                                         enum encoding enc) {
  size_t length = 0;
  ByteSource out;

  if (StringBytes::Size(env->isolate(), key, enc).To(&length) && length > 0) {
    ByteSource::Builder buf(length);
    size_t actual =
        StringBytes::Write(env->isolate(), buf.data<char>(), length, key, enc);
    out = std::move(buf).release(actual);
  }

  return out;
}

ByteSource ByteSource::FromStringOrBuffer(Environment* env,
                                          Local<Value> value) {
  return IsAnyByteSource(value) ? FromBuffer(value)
                                : FromString(env, value.As<String>());
}

ByteSource ByteSource::FromString(Environment* env, Local<String> str,
                                  bool ntc) {
  CHECK(str->IsString());
  size_t size = str->Utf8Length(env->isolate());
  size_t alloc_size = ntc ? size + 1 : size;
  ByteSource::Builder out(alloc_size);
  int opts = String::NO_OPTIONS;
  if (!ntc) opts |= String::NO_NULL_TERMINATION;
  str->WriteUtf8(env->isolate(), out.data<char>(), alloc_size, nullptr, opts);
  return std::move(out).release();
}

ByteSource ByteSource::FromBuffer(Local<Value> buffer, bool ntc) {
  ArrayBufferOrViewContents<char> buf(buffer);
  return ntc ? buf.ToNullTerminatedCopy() : buf.ToByteSource();
}

ByteSource ByteSource::FromSecretKeyBytes(
    Environment* env,
    Local<Value> value) {
  // A key can be passed as a string, buffer or KeyObject with type 'secret'.
  // If it is a string, we need to convert it to a buffer. We are not doing that
  // in JS to avoid creating an unprotected copy on the heap.
  return value->IsString() || IsAnyByteSource(value) ?
           ByteSource::FromStringOrBuffer(env, value) :
           ByteSource::FromSymmetricKeyObjectHandle(value);
}

ByteSource ByteSource::NullTerminatedCopy(Environment* env,
                                          Local<Value> value) {
  return Buffer::HasInstance(value) ? FromBuffer(value, true)
                                    : FromString(env, value.As<String>(), true);
}

ByteSource ByteSource::FromSymmetricKeyObjectHandle(Local<Value> handle) {
  CHECK(handle->IsObject());
  KeyObjectHandle* key = Unwrap<KeyObjectHandle>(handle.As<Object>());
  CHECK_NOT_NULL(key);
  return Foreign(key->Data()->GetSymmetricKey(),
                 key->Data()->GetSymmetricKeySize());
}

ByteSource ByteSource::Allocated(void* data, size_t size) {
  return ByteSource(data, data, size);
}

ByteSource ByteSource::Foreign(const void* data, size_t size) {
  return ByteSource(data, nullptr, size);
}

namespace error {
Maybe<bool> Decorate(Environment* env, Local<Object> obj,
              unsigned long err) {  // NOLINT(runtime/int)
  if (err == 0) return Just(true);  // No decoration necessary.

  const char* ls = ERR_lib_error_string(err);
  const char* fs = ERR_func_error_string(err);
  const char* rs = ERR_reason_error_string(err);

  Isolate* isolate = env->isolate();
  Local<Context> context = isolate->GetCurrentContext();

  if (ls != nullptr) {
    if (obj->Set(context, env->library_string(),
                 OneByteString(isolate, ls)).IsNothing()) {
      return Nothing<bool>();
    }
  }
  if (fs != nullptr) {
    if (obj->Set(context, env->function_string(),
                 OneByteString(isolate, fs)).IsNothing()) {
      return Nothing<bool>();
    }
  }
  if (rs != nullptr) {
    if (obj->Set(context, env->reason_string(),
                 OneByteString(isolate, rs)).IsNothing()) {
      return Nothing<bool>();
    }

    // SSL has no API to recover the error name from the number, so we
    // transform reason strings like "this error" to "ERR_SSL_THIS_ERROR",
    // which ends up being close to the original error macro name.
    std::string reason(rs);

    for (auto& c : reason) {
      if (c == ' ')
        c = '_';
      else
        c = ToUpper(c);
    }

#define OSSL_ERROR_CODES_MAP(V)                                               \
    V(SYS)                                                                    \
    V(BN)                                                                     \
    V(RSA)                                                                    \
    V(DH)                                                                     \
    V(EVP)                                                                    \
    V(BUF)                                                                    \
    V(OBJ)                                                                    \
    V(PEM)                                                                    \
    V(DSA)                                                                    \
    V(X509)                                                                   \
    V(ASN1)                                                                   \
    V(CONF)                                                                   \
    V(CRYPTO)                                                                 \
    V(EC)                                                                     \
    V(SSL)                                                                    \
    V(BIO)                                                                    \
    V(PKCS7)                                                                  \
    V(X509V3)                                                                 \
    V(PKCS12)                                                                 \
    V(RAND)                                                                   \
    V(DSO)                                                                    \
    V(ENGINE)                                                                 \
    V(OCSP)                                                                   \
    V(UI)                                                                     \
    V(COMP)                                                                   \
    V(ECDSA)                                                                  \
    V(ECDH)                                                                   \
    V(OSSL_STORE)                                                             \
    V(FIPS)                                                                   \
    V(CMS)                                                                    \
    V(TS)                                                                     \
    V(HMAC)                                                                   \
    V(CT)                                                                     \
    V(ASYNC)                                                                  \
    V(KDF)                                                                    \
    V(SM2)                                                                    \
    V(USER)                                                                   \

#define V(name) case ERR_LIB_##name: lib = #name "_"; break;
    const char* lib = "";
    const char* prefix = "OSSL_";
    switch (ERR_GET_LIB(err)) { OSSL_ERROR_CODES_MAP(V) }
#undef V
#undef OSSL_ERROR_CODES_MAP
    // Don't generate codes like "ERR_OSSL_SSL_".
    if (lib && strcmp(lib, "SSL_") == 0)
      prefix = "";

    // All OpenSSL reason strings fit in a single 80-column macro definition,
    // all prefix lengths are <= 10, and ERR_OSSL_ is 9, so 128 is more than
    // sufficient.
    char code[128];
    snprintf(code, sizeof(code), "ERR_%s%s%s", prefix, lib, reason.c_str());

    if (obj->Set(env->isolate()->GetCurrentContext(),
             env->code_string(),
             OneByteString(env->isolate(), code)).IsNothing())
      return Nothing<bool>();
  }

  return Just(true);
}
}  // namespace error

void ThrowCryptoError(Environment* env,
                      unsigned long err,  // NOLINT(runtime/int)
                      // Default, only used if there is no SSL `err` which can
                      // be used to create a long-style message string.
                      const char* message) {
  char message_buffer[128] = {0};
  if (err != 0 || message == nullptr) {
    ERR_error_string_n(err, message_buffer, sizeof(message_buffer));
    message = message_buffer;
  }
  HandleScope scope(env->isolate());
  Local<String> exception_string;
  Local<Value> exception;
  Local<Object> obj;
  if (!String::NewFromUtf8(env->isolate(), message).ToLocal(&exception_string))
    return;
  CryptoErrorStore errors;
  errors.Capture();
  if (!errors.ToException(env, exception_string).ToLocal(&exception) ||
      !exception->ToObject(env->context()).ToLocal(&obj) ||
      error::Decorate(env, obj, err).IsNothing()) {
    return;
  }
  env->isolate()->ThrowException(exception);
}

#ifndef OPENSSL_NO_ENGINE
EnginePointer LoadEngineById(const char* id, CryptoErrorStore* errors) {
  MarkPopErrorOnReturn mark_pop_error_on_return;

  EnginePointer engine(ENGINE_by_id(id));
  if (!engine) {
    // Engine not found, try loading dynamically.
    engine = EnginePointer(ENGINE_by_id("dynamic"));
    if (engine) {
      if (!ENGINE_ctrl_cmd_string(engine.get(), "SO_PATH", id, 0) ||
          !ENGINE_ctrl_cmd_string(engine.get(), "LOAD", nullptr, 0)) {
        engine.reset();
      }
    }
  }

  if (!engine && errors != nullptr) {
    errors->Capture();
    if (errors->Empty()) {
      errors->Insert(NodeCryptoError::ENGINE_NOT_FOUND, id);
    }
  }

  return engine;
}

bool SetEngine(const char* id, uint32_t flags, CryptoErrorStore* errors) {
  ClearErrorOnReturn clear_error_on_return;
  EnginePointer engine = LoadEngineById(id, errors);
  if (!engine)
    return false;

  if (!ENGINE_set_default(engine.get(), flags)) {
    if (errors != nullptr)
      errors->Capture();
    return false;
  }

  return true;
}

void SetEngine(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.Length() >= 2 && args[0]->IsString());
  uint32_t flags;
  if (!args[1]->Uint32Value(env->context()).To(&flags)) return;

  const node::Utf8Value engine_id(env->isolate(), args[0]);

  args.GetReturnValue().Set(SetEngine(*engine_id, flags));
}
#endif  // !OPENSSL_NO_ENGINE

MaybeLocal<Value> EncodeBignum(
    Environment* env,
    const BIGNUM* bn,
    int size,
    Local<Value>* error) {
  std::vector<uint8_t> buf(size);
  CHECK_EQ(BN_bn2binpad(bn, buf.data(), size), size);
  return StringBytes::Encode(
      env->isolate(),
      reinterpret_cast<const char*>(buf.data()),
      buf.size(),
      BASE64URL,
      error);
}

Maybe<bool> SetEncodedValue(
    Environment* env,
    Local<Object> target,
    Local<String> name,
    const BIGNUM* bn,
    int size) {
  Local<Value> value;
  Local<Value> error;
  CHECK_NOT_NULL(bn);
  if (size == 0)
    size = BN_num_bytes(bn);
  if (!EncodeBignum(env, bn, size, &error).ToLocal(&value)) {
    if (!error.IsEmpty())
      env->isolate()->ThrowException(error);
    return Nothing<bool>();
  }
  return target->Set(env->context(), name, value);
}

CryptoJobMode GetCryptoJobMode(v8::Local<v8::Value> args) {
  CHECK(args->IsUint32());
  uint32_t mode = args.As<v8::Uint32>()->Value();
  CHECK_LE(mode, kCryptoJobSync);
  return static_cast<CryptoJobMode>(mode);
}

namespace {
// SecureBuffer uses openssl to allocate a Uint8Array using
// OPENSSL_secure_malloc. Because we do not yet actually
// make use of secure heap, this has the same semantics as
// using OPENSSL_malloc. However, if the secure heap is
// initialized, SecureBuffer will automatically use it.
void SecureBuffer(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsUint32());
  Environment* env = Environment::GetCurrent(args);
  uint32_t len = args[0].As<Uint32>()->Value();
  char* data = static_cast<char*>(OPENSSL_secure_malloc(len));
  if (data == nullptr) {
    // There's no memory available for the allocation.
    // Return nothing.
    return;
  }
  memset(data, 0, len);
  std::shared_ptr<BackingStore> store =
      ArrayBuffer::NewBackingStore(
          data,
          len,
          [](void* data, size_t len, void* deleter_data) {
            OPENSSL_secure_clear_free(data, len);
          },
          data);
  Local<ArrayBuffer> buffer = ArrayBuffer::New(env->isolate(), store);
  args.GetReturnValue().Set(Uint8Array::New(buffer, 0, len));
}

void SecureHeapUsed(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (CRYPTO_secure_malloc_initialized())
    args.GetReturnValue().Set(
        BigInt::New(env->isolate(), CRYPTO_secure_used()));
}
}  // namespace

namespace Util {
void Initialize(Environment* env, Local<Object> target) {
#ifndef OPENSSL_NO_ENGINE
  env->SetMethod(target, "setEngine", SetEngine);
#endif  // !OPENSSL_NO_ENGINE

  env->SetMethodNoSideEffect(target, "getFipsCrypto", GetFipsCrypto);
  env->SetMethod(target, "setFipsCrypto", SetFipsCrypto);
  env->SetMethodNoSideEffect(target, "testFipsCrypto", TestFipsCrypto);

  NODE_DEFINE_CONSTANT(target, kCryptoJobAsync);
  NODE_DEFINE_CONSTANT(target, kCryptoJobSync);

  env->SetMethod(target, "secureBuffer", SecureBuffer);
  env->SetMethod(target, "secureHeapUsed", SecureHeapUsed);
}
void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
#ifndef OPENSSL_NO_ENGINE
  registry->Register(SetEngine);
#endif  // !OPENSSL_NO_ENGINE

  registry->Register(GetFipsCrypto);
  registry->Register(SetFipsCrypto);
  registry->Register(TestFipsCrypto);
  registry->Register(SecureBuffer);
  registry->Register(SecureHeapUsed);
}

}  // namespace Util

}  // namespace crypto
}  // namespace node
