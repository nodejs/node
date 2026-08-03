#ifndef SRC_CRYPTO_CRYPTO_KEM_H_
#define SRC_CRYPTO_CRYPTO_KEM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "memory_tracker.h"
#include "node_external_reference.h"

#if OPENSSL_VERSION_MAJOR >= 3

namespace node {
namespace crypto {

enum class KEMMode { Encapsulate, Decapsulate };

struct KEMConfiguration final : public MemoryRetainer {
  CryptoJobMode job_mode;
  KEMMode mode;
  KeyObjectData key;
  ByteSource ciphertext;

  KEMConfiguration() = default;
  explicit KEMConfiguration(KEMConfiguration&& other) noexcept;
  KEMConfiguration& operator=(KEMConfiguration&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(KEMConfiguration)
  SET_SELF_SIZE(KEMConfiguration)
};

struct KEMEncapsulateTraits final {
  using AdditionalParameters = KEMConfiguration;
  static constexpr const char* JobName = "KEMEncapsulateJob";

  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_DERIVEBITSREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      KEMConfiguration* params);

  static bool DeriveBits(Environment* env,
                         const KEMConfiguration& params,
                         ByteSource* out,
                         CryptoJobMode mode);

  static v8::MaybeLocal<v8::Value> EncodeOutput(Environment* env,
                                                const KEMConfiguration& params,
                                                ByteSource* out);
};

struct KEMDecapsulateTraits final {
  using AdditionalParameters = KEMConfiguration;
  static constexpr const char* JobName = "KEMDecapsulateJob";

  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_DERIVEBITSREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      KEMConfiguration* params);

  static bool DeriveBits(Environment* env,
                         const KEMConfiguration& params,
                         ByteSource* out,
                         CryptoJobMode mode);

  static v8::MaybeLocal<v8::Value> EncodeOutput(Environment* env,
                                                const KEMConfiguration& params,
                                                ByteSource* out);
};

using KEMEncapsulateJob = DeriveBitsJob<KEMEncapsulateTraits>;
using KEMDecapsulateJob = DeriveBitsJob<KEMDecapsulateTraits>;

void InitializeKEM(Environment* env, v8::Local<v8::Object> target);
void RegisterKEMExternalReferences(ExternalReferenceRegistry* registry);

namespace KEM {
void Initialize(Environment* env, v8::Local<v8::Object> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace KEM

}  // namespace crypto
}  // namespace node

#else

// Provide stub implementations when OpenSSL < 3.0
namespace node {
namespace crypto {
namespace KEM {
inline void Initialize(Environment* env, v8::Local<v8::Object> target) {
  // No-op when OpenSSL < 3.0
}
inline void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  // No-op when OpenSSL < 3.0
}
}  // namespace KEM
}  // namespace crypto
}  // namespace node

#endif
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_KEM_H_
