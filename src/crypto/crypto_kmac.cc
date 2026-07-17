#include "crypto/crypto_kmac.h"
#include "async_wrap-inl.h"
#include "crypto/crypto_hash.h"
#include "node_internals.h"
#include "threadpoolwork-inl.h"

#if OPENSSL_WITH_EVP_MAC
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <array>
#include <limits>
#include <utility>
#include "crypto/crypto_keys.h"
#include "crypto/crypto_sig.h"
#include "ncrypto.h"

namespace node::crypto {

using ncrypto::EVPMacCtxPointer;
using ncrypto::EVPMacPointer;
using node::Utf8Value;
using v8::Boolean;
using v8::FunctionCallbackInfo;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::Uint32;
using v8::Value;

KmacConfig::KmacConfig(KmacConfig&& other) noexcept
    : job_mode(other.job_mode),
      mode(other.mode),
      key(std::move(other.key)),
      data(std::move(other.data)),
      signature(std::move(other.signature)),
      customization(std::move(other.customization)),
      variant(other.variant),
      key_length(other.key_length),
      length(other.length) {}

KmacConfig& KmacConfig::operator=(KmacConfig&& other) noexcept {
  if (&other == this) return *this;
  this->~KmacConfig();
  return *new (this) KmacConfig(std::move(other));
}

void KmacConfig::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("key", key);
  // If the job is sync, then the KmacConfig does not own the data.
  if (IsCryptoJobAsync(job_mode)) {
    tracker->TrackFieldWithSize("data", data.size());
    tracker->TrackFieldWithSize("signature", signature.size());
    tracker->TrackFieldWithSize("customization", customization.size());
  }
}

Maybe<void> KmacTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    KmacConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  params->job_mode = mode;

  CHECK(args[offset]->IsUint32());  // SignConfiguration::Mode
  params->mode =
      static_cast<SignConfiguration::Mode>(args[offset].As<Uint32>()->Value());

  CHECK(args[offset + 1]->IsObject());  // Key
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args[offset + 1], Nothing<void>());
  params->key = key->Data().addRef();

  CHECK(args[offset + 2]->IsString());  // Algorithm name
  Utf8Value algorithm_name(env->isolate(), args[offset + 2]);
  std::string_view algorithm_str = algorithm_name.ToStringView();

  // Convert string to enum and validate
  if (algorithm_str == OSSL_MAC_NAME_KMAC128) {
    params->variant = KmacVariant::KMAC128;
  } else if (algorithm_str == OSSL_MAC_NAME_KMAC256) {
    params->variant = KmacVariant::KMAC256;
  } else {
    UNREACHABLE();
  }

  // Customization string (may be empty or undefined).
  if (!args[offset + 3]->IsUndefined()) {
    ArrayBufferOrViewContents<char> customization(args[offset + 3]);
    if (!customization.CheckSizeInt32()) [[unlikely]] {
      THROW_ERR_OUT_OF_RANGE(env, "customization is too big");
      return Nothing<void>();
    }
    params->customization = IsCryptoJobAsync(mode)
                                ? customization.ToCopy()
                                : customization.ToByteSource();
  }
  // If undefined, params->customization remains uninitialized (size 0).

  CHECK(args[offset + 4]->IsNumber());  // Key length
  double key_length = args[offset + 4].As<Number>()->Value();
  if (!(key_length >= 0) ||
      key_length > static_cast<double>(std::numeric_limits<size_t>::max())) {
    THROW_ERR_OUT_OF_RANGE(env, "key length is too big");
    return Nothing<void>();
  }
  params->key_length = static_cast<size_t>(key_length);

  CHECK(args[offset + 5]->IsUint32());  // Length
  params->length = args[offset + 5].As<Uint32>()->Value();

  ArrayBufferOrViewContents<char> data(args[offset + 6]);
  if (!data.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "data is too big");
    return Nothing<void>();
  }
  params->data = IsCryptoJobAsync(mode) ? data.ToCopy() : data.ToByteSource();

  if (!args[offset + 7]->IsUndefined()) {
    ArrayBufferOrViewContents<char> signature(args[offset + 7]);
    if (!signature.CheckSizeInt32()) [[unlikely]] {
      THROW_ERR_OUT_OF_RANGE(env, "signature is too big");
      return Nothing<void>();
    }
    params->signature =
        IsCryptoJobAsync(mode) ? signature.ToCopy() : signature.ToByteSource();
  }

  return JustVoid();
}

namespace {

static constexpr std::array<unsigned char, 4> kKmacFunctionName = {
    'K', 'M', 'A', 'C'};
static constexpr size_t kKmacMinOpenSSLKeySize = 4;
// Keep the bit-aware path within OpenSSL's KMAC provider limits.
static constexpr size_t kKmacMaxOpenSSLKeySize = 512;
static constexpr size_t kKmacMaxOpenSSLCustomizationSize = 512;
static constexpr size_t kKmacMaxOpenSSLOutputSize = 0xffffff / CHAR_BIT;

bool KmacParamsWithinOpenSSLLimits(const KmacConfig& params,
                                   size_t key_size,
                                   size_t length_bytes) {
  return key_size <= kKmacMaxOpenSSLKeySize &&
         NumBitsToBytes(params.key_length) <= kKmacMaxOpenSSLKeySize &&
         params.customization.size() <= kKmacMaxOpenSSLCustomizationSize &&
         length_bytes <= kKmacMaxOpenSSLOutputSize;
}

bool DeriveBitsWithCShake(const KmacConfig& params,
                          const void* key_data,
                          size_t key_size,
                          ByteSource* out) {
  const size_t key_length_bytes = NumBitsToBytes(params.key_length);
  if (key_size < key_length_bytes) return false;

  CShakeBytepadInput key_input = {
      .data = key_data,
      .byte_length = key_length_bytes,
      .bit_length = params.key_length,
  };
  CShakeParams cshake_params = {
      .variant = params.variant == KmacVariant::KMAC128
                     ? CShakeVariant::CSHAKE128
                     : CShakeVariant::CSHAKE256,
      .function_name_data = kKmacFunctionName.data(),
      .function_name_size = kKmacFunctionName.size(),
      .customization_data = params.customization.data(),
      .customization_size = params.customization.size(),
      .bytepad_input = &key_input,
      .input_data = params.data.data(),
      .input_size = params.data.size(),
      .append_output_length = true,
      .length = params.length,
  };
  return DeriveCShakeBits(cshake_params, out);
}

}  // namespace

bool KmacTraits::DeriveBits(Environment* env,
                            const KmacConfig& params,
                            ByteSource* out,
                            CryptoJobMode mode,
                            CryptoErrorStore*) {
  const bool truncate_to_bit_length = params.length % CHAR_BIT != 0;
  const size_t length_bytes =
      NumBitsToBytes(static_cast<size_t>(params.length));

  // Get the key data.
  const void* key_data = params.key.GetSymmetricKey();
  size_t key_size = params.key.GetSymmetricKeySize();

  if (!KmacParamsWithinOpenSSLLimits(params, key_size, length_bytes)) {
    return false;
  }

  if (params.length == 0) {
    *out = ByteSource();
    return true;
  }

  // OpenSSL's EVP_MAC provider rejects KMAC keys shorter than 4 bytes.
  if (params.length % CHAR_BIT != 0 || params.key_length % CHAR_BIT != 0 ||
      key_size < kKmacMinOpenSSLKeySize) {
    return DeriveBitsWithCShake(params, key_data, key_size, out);
  }

  // Fetch the KMAC algorithm
  auto mac = EVPMacPointer::Fetch((params.variant == KmacVariant::KMAC128)
                                      ? OSSL_MAC_NAME_KMAC128
                                      : OSSL_MAC_NAME_KMAC256);
  if (!mac) {
    return false;
  }

  // Create MAC context
  auto mac_ctx = EVPMacCtxPointer::New(mac.get());
  if (!mac_ctx) {
    return false;
  }

  // Set up parameters.
  OSSL_PARAM params_array[3];  // Max 3: size + customization + end
  size_t params_count = 0;

  // Set output length (always required for KMAC).
  size_t outlen = length_bytes;
  params_array[params_count++] =
      OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &outlen);

  // Set customization if provided.
  if (params.customization.size() > 0) {
    params_array[params_count++] = OSSL_PARAM_construct_octet_string(
        OSSL_MAC_PARAM_CUSTOM,
        const_cast<void*>(params.customization.data()),
        params.customization.size());
  }

  params_array[params_count] = OSSL_PARAM_construct_end();

  // Initialize the MAC context.
  if (!mac_ctx.init(ncrypto::Buffer<const void>(key_data, key_size),
                    params_array)) {
    return false;
  }

  // Update with data.
  if (!mac_ctx.update(ncrypto::Buffer<const void>(params.data.data(),
                                                  params.data.size()))) {
    return false;
  }

  // Finalize and get the result.
  auto result = mac_ctx.final(length_bytes);
  if (!result) {
    return false;
  }

  auto buffer = result.release();
  *out = ByteSource::Allocated(buffer.data, buffer.len);
  if (truncate_to_bit_length) TruncateToBitLength(params.length, out);
  return true;
}

MaybeLocal<Value> KmacTraits::EncodeOutput(Environment* env,
                                           const KmacConfig& params,
                                           ByteSource* out) {
  switch (params.mode) {
    case SignConfiguration::Mode::Sign:
      return out->ToArrayBuffer(env);
    case SignConfiguration::Mode::Verify:
      return Boolean::New(
          env->isolate(),
          out->size() > 0 && out->size() == params.signature.size() &&
              CRYPTO_memcmp(
                  out->data(), params.signature.data(), out->size()) == 0);
  }
  UNREACHABLE();
}

void Kmac::Initialize(Environment* env, Local<Object> target) {
  KmacJob::Initialize(env, target);
}

void Kmac::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  KmacJob::RegisterExternalReferences(registry);
}

}  // namespace node::crypto

#endif  // OPENSSL_WITH_EVP_MAC
