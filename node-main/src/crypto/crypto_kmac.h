#ifndef SRC_CRYPTO_CRYPTO_KMAC_H_
#define SRC_CRYPTO_CRYPTO_KMAC_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>
#include "crypto/crypto_keys.h"
#include "crypto/crypto_sig.h"
#include "crypto/crypto_util.h"

namespace node::crypto {

// KMAC (Keccak Message Authentication Code) is available since OpenSSL 3.0.
#if OPENSSL_VERSION_MAJOR >= 3

enum class KmacVariant { KMAC128, KMAC256 };

struct KmacConfig final : public MemoryRetainer {
  CryptoJobMode job_mode;
  SignConfiguration::Mode mode;
  KeyObjectData key;
  ByteSource data;
  ByteSource signature;
  ByteSource customization;
  KmacVariant variant;
  uint32_t length;  // Output length in bytes

  KmacConfig() = default;

  explicit KmacConfig(KmacConfig&& other) noexcept;

  KmacConfig& operator=(KmacConfig&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(KmacConfig)
  SET_SELF_SIZE(KmacConfig)
};

struct KmacTraits final {
  using AdditionalParameters = KmacConfig;
  static constexpr const char* JobName = "KmacJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_SIGNREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      KmacConfig* params);

  static bool DeriveBits(Environment* env,
                         const KmacConfig& params,
                         ByteSource* out,
                         CryptoJobMode mode);

  static v8::MaybeLocal<v8::Value> EncodeOutput(Environment* env,
                                                const KmacConfig& params,
                                                ByteSource* out);
};

using KmacJob = DeriveBitsJob<KmacTraits>;

namespace Kmac {
void Initialize(Environment* env, v8::Local<v8::Object> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace Kmac

#else
// If there is no KMAC support, provide empty namespace functions.
namespace Kmac {
void Initialize(Environment* env, v8::Local<v8::Object> target) {}
void RegisterExternalReferences(ExternalReferenceRegistry* registry) {}
}  // namespace Kmac
#endif

}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_KMAC_H_
