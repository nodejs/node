#include "crypto/crypto_kem.h"

#if OPENSSL_VERSION_MAJOR >= 3

#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
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
  if (job_mode == kCryptoJobAsync) {
    tracker->TrackFieldWithSize("ciphertext", ciphertext.size());
  }
}

namespace {

bool DoKEMEncapsulate(Environment* env,
                      const EVPKeyPointer& public_key,
                      ByteSource* out,
                      CryptoJobMode mode) {
  auto result = ncrypto::KEM::Encapsulate(public_key);
  if (!result) {
    if (mode == kCryptoJobSync) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to perform encapsulation");
    }
    return false;
  }

  // Pack the result: [ciphertext_len][shared_key_len][ciphertext][shared_key]
  size_t ciphertext_len = result->ciphertext.size();
  size_t shared_key_len = result->shared_key.size();
  size_t total_len =
      sizeof(uint32_t) + sizeof(uint32_t) + ciphertext_len + shared_key_len;

  auto data = ncrypto::DataPointer::Alloc(total_len);
  if (!data) {
    if (mode == kCryptoJobSync) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                        "Failed to allocate output buffer");
    }
    return false;
  }

  unsigned char* ptr = static_cast<unsigned char*>(data.get());

  // Write size headers
  *reinterpret_cast<uint32_t*>(ptr) = static_cast<uint32_t>(ciphertext_len);
  *reinterpret_cast<uint32_t*>(ptr + sizeof(uint32_t)) =
      static_cast<uint32_t>(shared_key_len);

  // Write ciphertext and shared key data
  unsigned char* ciphertext_ptr = ptr + 2 * sizeof(uint32_t);
  unsigned char* shared_key_ptr = ciphertext_ptr + ciphertext_len;

  std::memcpy(ciphertext_ptr, result->ciphertext.get(), ciphertext_len);
  std::memcpy(shared_key_ptr, result->shared_key.get(), shared_key_len);

  *out = ByteSource::Allocated(data.release());
  return true;
}

bool DoKEMDecapsulate(Environment* env,
                      const EVPKeyPointer& private_key,
                      const ByteSource& ciphertext,
                      ByteSource* out,
                      CryptoJobMode mode) {
  ncrypto::Buffer<const void> ciphertext_buf{ciphertext.data(),
                                             ciphertext.size()};
  auto shared_key = ncrypto::KEM::Decapsulate(private_key, ciphertext_buf);
  if (!shared_key) {
    if (mode == kCryptoJobSync) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to perform decapsulation");
    }
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

bool KEMEncapsulateTraits::DeriveBits(Environment* env,
                                      const KEMConfiguration& params,
                                      ByteSource* out,
                                      CryptoJobMode mode) {
  Mutex::ScopedLock lock(params.key.mutex());
  const auto& public_key = params.key.GetAsymmetricKey();

  return DoKEMEncapsulate(env, public_key, out, mode);
}

MaybeLocal<Value> KEMEncapsulateTraits::EncodeOutput(
    Environment* env, const KEMConfiguration& params, ByteSource* out) {
  // The output contains:
  // [ciphertext_len][shared_key_len][ciphertext][shared_key]
  const unsigned char* data = out->data<unsigned char>();

  uint32_t ciphertext_len = *reinterpret_cast<const uint32_t*>(data);
  uint32_t shared_key_len =
      *reinterpret_cast<const uint32_t*>(data + sizeof(uint32_t));

  const unsigned char* ciphertext_ptr = data + 2 * sizeof(uint32_t);
  const unsigned char* shared_key_ptr = ciphertext_ptr + ciphertext_len;

  MaybeLocal<Object> ciphertext_buf =
      node::Buffer::Copy(env->isolate(),
                         reinterpret_cast<const char*>(ciphertext_ptr),
                         ciphertext_len);

  MaybeLocal<Object> shared_key_buf =
      node::Buffer::Copy(env->isolate(),
                         reinterpret_cast<const char*>(shared_key_ptr),
                         shared_key_len);

  Local<Object> ciphertext_obj;
  Local<Object> shared_key_obj;
  if (!ciphertext_buf.ToLocal(&ciphertext_obj) ||
      !shared_key_buf.ToLocal(&shared_key_obj)) {
    return MaybeLocal<Value>();
  }

  // Return an array [sharedKey, ciphertext].
  Local<Array> result = Array::New(env->isolate(), 2);
  if (result->Set(env->context(), 0, shared_key_obj).IsNothing() ||
      result->Set(env->context(), 1, ciphertext_obj).IsNothing()) {
    return MaybeLocal<Value>();
  }

  return result;
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
      mode == kCryptoJobAsync ? ciphertext.ToCopy() : ciphertext.ToByteSource();

  return v8::JustVoid();
}

bool KEMDecapsulateTraits::DeriveBits(Environment* env,
                                      const KEMConfiguration& params,
                                      ByteSource* out,
                                      CryptoJobMode mode) {
  Mutex::ScopedLock lock(params.key.mutex());
  const auto& private_key = params.key.GetAsymmetricKey();

  return DoKEMDecapsulate(env, private_key, params.ciphertext, out, mode);
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
