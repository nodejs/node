#ifndef SRC_CRYPTO_CRYPTO_HASH_H_
#define SRC_CRYPTO_CRYPTO_HASH_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "memory_tracker.h"
#include "v8.h"

namespace node {
namespace crypto {
class Hash final : public BaseObject {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Hash)
  SET_SELF_SIZE(Hash)

  bool HashInit(const EVP_MD* md, v8::Maybe<unsigned int> xof_md_len);
  bool HashUpdate(const char* data, size_t len);

  static void GetHashes(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCachedAliases(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void OneShotDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

 protected:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void HashDigest(const v8::FunctionCallbackInfo<v8::Value>& args);

  Hash(Environment* env, v8::Local<v8::Object> wrap);

 private:
  ncrypto::EVPMDCtxPointer mdctx_{};
  unsigned int md_len_ = 0;
  ByteSource digest_;
};

struct HashConfig final : public MemoryRetainer {
  CryptoJobMode mode;
  ByteSource in;
  const EVP_MD* digest;
  unsigned int length;

  HashConfig() = default;

  explicit HashConfig(HashConfig&& other) noexcept;

  HashConfig& operator=(HashConfig&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(HashConfig)
  SET_SELF_SIZE(HashConfig)
};

struct HashTraits final {
  using AdditionalParameters = HashConfig;
  static constexpr const char* JobName = "HashJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_HASHREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      HashConfig* params);

  static bool DeriveBits(Environment* env,
                         const HashConfig& params,
                         ByteSource* out,
                         CryptoJobMode mode,
                         CryptoErrorStore* errors);

  static v8::MaybeLocal<v8::Value> EncodeOutput(Environment* env,
                                                const HashConfig& params,
                                                ByteSource* out);
};

using HashJob = DeriveBitsJob<HashTraits>;

#if OPENSSL_WITH_EVP_MAC
enum class CShakeVariant { CSHAKE128, CSHAKE256 };

struct CShakeBytepadInput final {
  const void* data;
  size_t byte_length;
  size_t bit_length;
};

struct CShakeParams final {
  CShakeVariant variant;
  const void* function_name_data;
  size_t function_name_size;
  const void* customization_data;
  size_t customization_size;
  const CShakeBytepadInput* bytepad_input;
  const void* input_data;
  size_t input_size;
  bool append_output_length;
  uint32_t length;  // Output length in bits
};

bool DeriveCShakeBits(const CShakeParams& params, ByteSource* out);

struct CShakeConfig final : public MemoryRetainer {
  CryptoJobMode mode;
  ByteSource in;
  ByteSource function_name;
  ByteSource customization;
  CShakeVariant variant;
  uint32_t length;  // Output length in bits

  CShakeConfig() = default;

  explicit CShakeConfig(CShakeConfig&& other) noexcept;

  CShakeConfig& operator=(CShakeConfig&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(CShakeConfig)
  SET_SELF_SIZE(CShakeConfig)
};

struct CShakeTraits final {
  using AdditionalParameters = CShakeConfig;
  static constexpr const char* JobName = "CShakeJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_HASHREQUEST;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      CShakeConfig* params);

  static bool DeriveBits(Environment* env,
                         const CShakeConfig& params,
                         ByteSource* out,
                         CryptoJobMode mode,
                         CryptoErrorStore* errors);

  static v8::MaybeLocal<v8::Value> EncodeOutput(Environment* env,
                                                const CShakeConfig& params,
                                                ByteSource* out);
};

using CShakeJob = DeriveBitsJob<CShakeTraits>;
#endif  // OPENSSL_WITH_EVP_MAC

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_HASH_H_
