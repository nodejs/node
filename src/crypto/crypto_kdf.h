#ifndef SRC_CRYPTO_CRYPTO_KDF_H_
#define SRC_CRYPTO_CRYPTO_KDF_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_util.h"
#include "v8.h"

#include <variant>

namespace node {
namespace crypto {

struct KDFConfig final : public MemoryRetainer {
  CryptoJobMode mode;
  ncrypto::KdfCtxPointer kdfCtx;
  ncrypto::KdfCtxPointer::Params kdfParams;
  uint32_t length;

  KDFConfig() = default;

  explicit KDFConfig(KDFConfig&& other) noexcept = default;

  KDFConfig& operator=(KDFConfig&& other) noexcept = default;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(KDFConfig)
  SET_SELF_SIZE(KDFConfig)
};

struct KDFTraits final {
  using AdditionalParameters = KDFConfig;
  static constexpr const char* JobName = "KDFJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_DERIVEBITSREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      KDFConfig* params);

  static bool DeriveBits(
      Environment* env,
      const KDFConfig& params,
      ByteSource* out);

  static v8::MaybeLocal<v8::Value> EncodeOutput(Environment* env,
                                                const KDFConfig& params,
                                                ByteSource* out);
};

using KDFJob = DeriveBitsJob<KDFTraits>;

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_KDF_H_
