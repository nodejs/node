#ifndef SRC_CRYPTO_CRYPTO_X509_GEN_H_
#define SRC_CRYPTO_CRYPTO_X509_GEN_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "memory_tracker.h"
#include "v8.h"

#include <optional>

namespace node::crypto {

using X509RequestPointer = ncrypto::DeleteFnPtr<X509_REQ, X509_REQ_free>;

struct X509GenerateConfig final : public MemoryRetainer {
  ncrypto::X509Pointer certificate;
  X509RequestPointer request;
  ncrypto::EVPKeyPointer request_public_key;
  KeyObjectData signing_key;
  ncrypto::Digest digest;
  std::optional<int> salt_length;
  std::optional<int> padding;

  X509GenerateConfig() = default;
  explicit X509GenerateConfig(X509GenerateConfig&& other) noexcept;
  X509GenerateConfig& operator=(X509GenerateConfig&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(X509GenerateConfig)
  SET_SELF_SIZE(X509GenerateConfig)
};

struct X509GenerateTraits final {
  using AdditionalParameters = X509GenerateConfig;
  static constexpr const char* JobName = "X509GenerateJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_SIGNREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      X509GenerateConfig* params);

  static bool DeriveBits(Environment* env,
                         const X509GenerateConfig& params,
                         ByteSource* out,
                         CryptoJobMode mode,
                         CryptoErrorStore* errors);

  static v8::MaybeLocal<v8::Value> EncodeOutput(
      Environment* env, const X509GenerateConfig& params, ByteSource* out);
};

using X509GenerateJob = DeriveBitsJob<X509GenerateTraits>;

struct X509Generation final {
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);
};

}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_X509_GEN_H_
