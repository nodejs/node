#ifndef SRC_CRYPTO_CRYPTO_TURBOSHAKE_H_
#define SRC_CRYPTO_CRYPTO_TURBOSHAKE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_util.h"

namespace node::crypto {

enum class TurboShakeVariant { TurboSHAKE128, TurboSHAKE256 };

struct TurboShakeConfig final : public MemoryRetainer {
  CryptoJobMode job_mode;
  TurboShakeVariant variant;
  uint32_t output_length;     // Output length in bytes
  uint8_t domain_separation;  // Domain separation byte (0x01–0x7F)
  ByteSource data;

  TurboShakeConfig() = default;

  explicit TurboShakeConfig(TurboShakeConfig&& other) noexcept;

  TurboShakeConfig& operator=(TurboShakeConfig&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(TurboShakeConfig)
  SET_SELF_SIZE(TurboShakeConfig)
};

struct TurboShakeTraits final {
  using AdditionalParameters = TurboShakeConfig;
  static constexpr const char* JobName = "TurboShakeJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_DERIVEBITSREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      TurboShakeConfig* params);

  static bool DeriveBits(Environment* env,
                         const TurboShakeConfig& params,
                         ByteSource* out,
                         CryptoJobMode mode,
                         CryptoErrorStore* errors);

  static v8::MaybeLocal<v8::Value> EncodeOutput(Environment* env,
                                                const TurboShakeConfig& params,
                                                ByteSource* out);
};

using TurboShakeJob = DeriveBitsJob<TurboShakeTraits>;

enum class KangarooTwelveVariant { KT128, KT256 };

struct KangarooTwelveConfig final : public MemoryRetainer {
  CryptoJobMode job_mode;
  KangarooTwelveVariant variant;
  uint32_t output_length;  // Output length in bytes
  ByteSource data;
  ByteSource customization;

  KangarooTwelveConfig() = default;

  explicit KangarooTwelveConfig(KangarooTwelveConfig&& other) noexcept;

  KangarooTwelveConfig& operator=(KangarooTwelveConfig&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(KangarooTwelveConfig)
  SET_SELF_SIZE(KangarooTwelveConfig)
};

struct KangarooTwelveTraits final {
  using AdditionalParameters = KangarooTwelveConfig;
  static constexpr const char* JobName = "KangarooTwelveJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_DERIVEBITSREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      KangarooTwelveConfig* params);

  static bool DeriveBits(Environment* env,
                         const KangarooTwelveConfig& params,
                         ByteSource* out,
                         CryptoJobMode mode,
                         CryptoErrorStore* errors);

  static v8::MaybeLocal<v8::Value> EncodeOutput(
      Environment* env, const KangarooTwelveConfig& params, ByteSource* out);
};

using KangarooTwelveJob = DeriveBitsJob<KangarooTwelveTraits>;

namespace TurboShake {
void Initialize(Environment* env, v8::Local<v8::Object> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace TurboShake

}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_TURBOSHAKE_H_
