#include "crypto/crypto_kem.h"

#if OPENSSL_WITH_KEM

#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

namespace node {

using ncrypto::EVPKeyPointer;
using v8::Array;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Uint8Array;
using v8::Value;

namespace crypto {

KEMConfiguration::KEMConfiguration(KEMConfiguration&& other) noexcept
    : job_mode(other.job_mode),
      mode(other.mode),
      key(std::move(other.key)),
      ciphertext(std::move(other.ciphertext)) {}

KEMConfiguration& KEMConfiguration::operator=(
    KEMConfiguration&& other) noexcept {
  if (&other == this) return *this;
  this->~KEMConfiguration();
  return *new (this) KEMConfiguration(std::move(other));
}

void KEMConfiguration::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("key", key);
  if (IsCryptoJobAsync(job_mode)) {
    tracker->TrackFieldWithSize("ciphertext", ciphertext.size());
  }
}

namespace {

bool DoKEMDecapsulate(Environment* env,
                      const EVPKeyPointer& private_key,
                      const ByteSource& ciphertext,
                      ByteSource* out,
                      CryptoJobMode mode,
                      CryptoErrorStore* errors) {
  ncrypto::Buffer<const void> ciphertext_buf{ciphertext.data(),
                                             ciphertext.size()};
  auto shared_key = ncrypto::KEM::Decapsulate(private_key, ciphertext_buf);
  if (!shared_key) {
    errors->Insert(NodeCryptoError::DECAPSULATION_FAILED);
    errors->SetNodeErrorCode("ERR_CRYPTO_OPERATION_FAILED");
    return false;
  }

  *out = ByteSource::Allocated(shared_key.release());
  return true;
}

}  // anonymous namespace

// KEMEncapsulateTraits implementation
Maybe<void> KEMEncapsulateTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    KEMConfiguration* params) {
  params->job_mode = mode;
  params->mode = KEMMode::Encapsulate;

  unsigned int key_offset = offset;
  auto public_key_data =
      KeyObjectData::GetPublicOrPrivateKeyFromJs(args, &key_offset);
  if (!public_key_data) {
    return Nothing<void>();
  }
  params->key = std::move(public_key_data);

  return v8::JustVoid();
}

void KEMEncapsulateJob::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());

  CryptoJobMode mode = GetCryptoJobMode(args[0]);
  AdditionalParams params;
  if (KEMEncapsulateTraits::AdditionalConfig(mode, args, 1, &params)
          .IsNothing()) {
    return;
  }

  new KEMEncapsulateJob(env, args.This(), mode, std::move(params));
}

void KEMEncapsulateJob::Initialize(Environment* env, Local<Object> target) {
  CryptoJob<KEMEncapsulateTraits>::Initialize(New, env, target);
}

void KEMEncapsulateJob::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  CryptoJob<KEMEncapsulateTraits>::RegisterExternalReferences(New, registry);
}

KEMEncapsulateJob::KEMEncapsulateJob(Environment* env,
                                     Local<Object> object,
                                     CryptoJobMode mode,
                                     AdditionalParams&& params)
    : CryptoJob<KEMEncapsulateTraits>(env,
                                      object,
                                      KEMEncapsulateTraits::Provider,
                                      mode,
                                      std::move(params)) {}

void KEMEncapsulateJob::DoThreadPoolWork() {
  ncrypto::ClearErrorOnReturn clear_error_on_return;
  AdditionalParams* params = CryptoJob<KEMEncapsulateTraits>::params();
  Mutex::ScopedLock lock(params->key.mutex());
  out_ = ncrypto::KEM::Encapsulate(params->key.GetAsymmetricKey());
  if (!out_) {
    CryptoErrorStore* errors = CryptoJob<KEMEncapsulateTraits>::errors();
    errors->Insert(NodeCryptoError::ENCAPSULATION_FAILED);
    errors->SetNodeErrorCode("ERR_CRYPTO_OPERATION_FAILED");
  }
}

Maybe<void> KEMEncapsulateJob::ToResult(Local<Value>* err,
                                        Local<Value>* result) {
  Environment* env = AsyncWrap::env();
  CryptoErrorStore* errors = CryptoJob<KEMEncapsulateTraits>::errors();
  if (!out_) {
    if (errors->Empty()) errors->Capture();
    CHECK(!errors->Empty());
    *result = v8::Undefined(env->isolate());
    if (!errors->ToException(env).ToLocal(err)) return Nothing<void>();
    return v8::JustVoid();
  }

  CHECK(errors->Empty());
  *err = v8::Undefined(env->isolate());

  ByteSource ciphertext = ByteSource::Allocated(out_->ciphertext.release());
  ByteSource shared_key = ByteSource::Allocated(out_->shared_key.release());

  if (mode() == kCryptoJobWebCrypto) {
    Local<Object> output = Object::New(env->isolate());
    if (!output
             ->DefineOwnProperty(env->context(),
                                 OneByteString(env->isolate(), "sharedKey"),
                                 shared_key.ToArrayBuffer(env))
             .FromMaybe(false) ||
        !output
             ->DefineOwnProperty(env->context(),
                                 OneByteString(env->isolate(), "ciphertext"),
                                 ciphertext.ToArrayBuffer(env))
             .FromMaybe(false)) {
      return Nothing<void>();
    }
    *result = output;
    return v8::JustVoid();
  }

  Local<Uint8Array> shared_key_buf;
  Local<Uint8Array> ciphertext_buf;
  if (!shared_key.ToBuffer(env).ToLocal(&shared_key_buf) ||
      !ciphertext.ToBuffer(env).ToLocal(&ciphertext_buf)) {
    return Nothing<void>();
  }

  Local<Array> output = Array::New(env->isolate(), 2);
  if (output->Set(env->context(), 0, shared_key_buf).IsNothing() ||
      output->Set(env->context(), 1, ciphertext_buf).IsNothing()) {
    return Nothing<void>();
  }
  *result = output;
  return v8::JustVoid();
}

void KEMEncapsulateJob::MemoryInfo(MemoryTracker* tracker) const {
  if (out_) {
    tracker->TrackFieldWithSize("ciphertext", out_->ciphertext.size());
    tracker->TrackFieldWithSize("shared_key", out_->shared_key.size());
  }
  CryptoJob<KEMEncapsulateTraits>::MemoryInfo(tracker);
}

// KEMDecapsulateTraits implementation
Maybe<void> KEMDecapsulateTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    KEMConfiguration* params) {
  Environment* env = Environment::GetCurrent(args);

  params->job_mode = mode;
  params->mode = KEMMode::Decapsulate;

  unsigned int key_offset = offset;
  auto private_key_data =
      KeyObjectData::GetPrivateKeyFromJs(args, &key_offset, true);
  if (!private_key_data) {
    return Nothing<void>();
  }
  params->key = std::move(private_key_data);

  ArrayBufferOrViewContents<unsigned char> ciphertext(args[key_offset]);
  if (!ciphertext.CheckSizeInt32()) {
    THROW_ERR_OUT_OF_RANGE(env, "ciphertext is too big");
    return Nothing<void>();
  }

  params->ciphertext =
      IsCryptoJobAsync(mode) ? ciphertext.ToCopy() : ciphertext.ToByteSource();

  return v8::JustVoid();
}

bool KEMDecapsulateTraits::DeriveBits(Environment* env,
                                      const KEMConfiguration& params,
                                      ByteSource* out,
                                      CryptoJobMode mode,
                                      CryptoErrorStore* errors) {
  Mutex::ScopedLock lock(params.key.mutex());
  const auto& private_key = params.key.GetAsymmetricKey();

  return DoKEMDecapsulate(
      env, private_key, params.ciphertext, out, mode, errors);
}

MaybeLocal<Value> KEMDecapsulateTraits::EncodeOutput(
    Environment* env, const KEMConfiguration& params, ByteSource* out) {
  return out->ToBuffer(env);
}

void InitializeKEM(Environment* env, Local<Object> target) {
  KEMEncapsulateJob::Initialize(env, target);
  KEMDecapsulateJob::Initialize(env, target);

  constexpr int kKEMEncapsulate = static_cast<int>(KEMMode::Encapsulate);
  constexpr int kKEMDecapsulate = static_cast<int>(KEMMode::Decapsulate);

  NODE_DEFINE_CONSTANT(target, kKEMEncapsulate);
  NODE_DEFINE_CONSTANT(target, kKEMDecapsulate);
}

void RegisterKEMExternalReferences(ExternalReferenceRegistry* registry) {
  KEMEncapsulateJob::RegisterExternalReferences(registry);
  KEMDecapsulateJob::RegisterExternalReferences(registry);
}

namespace KEM {
void Initialize(Environment* env, Local<Object> target) {
  InitializeKEM(env, target);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  RegisterKEMExternalReferences(registry);
}
}  // namespace KEM

}  // namespace crypto
}  // namespace node

#endif
