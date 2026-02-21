#ifndef SRC_CRYPTO_CRYPTO_AES_H_
#define SRC_CRYPTO_CRYPTO_AES_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_cipher.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "v8.h"

namespace node::crypto {
constexpr unsigned kNoAuthTagLength = static_cast<unsigned>(-1);

#define VARIANTS_COMMON(V)                                                     \
  V(CTR_128, AES_CTR_Cipher, ncrypto::Cipher::AES_128_CTR)                     \
  V(CTR_192, AES_CTR_Cipher, ncrypto::Cipher::AES_192_CTR)                     \
  V(CTR_256, AES_CTR_Cipher, ncrypto::Cipher::AES_256_CTR)                     \
  V(CBC_128, AES_Cipher, ncrypto::Cipher::AES_128_CBC)                         \
  V(CBC_192, AES_Cipher, ncrypto::Cipher::AES_192_CBC)                         \
  V(CBC_256, AES_Cipher, ncrypto::Cipher::AES_256_CBC)                         \
  V(GCM_128, AES_Cipher, ncrypto::Cipher::AES_128_GCM)                         \
  V(GCM_192, AES_Cipher, ncrypto::Cipher::AES_192_GCM)                         \
  V(GCM_256, AES_Cipher, ncrypto::Cipher::AES_256_GCM)                         \
  V(KW_128, AES_Cipher, ncrypto::Cipher::AES_128_KW)                           \
  V(KW_192, AES_Cipher, ncrypto::Cipher::AES_192_KW)                           \
  V(KW_256, AES_Cipher, ncrypto::Cipher::AES_256_KW)

#if OPENSSL_VERSION_MAJOR >= 3
#define VARIANTS_OCB(V)                                                        \
  V(OCB_128, AES_Cipher, ncrypto::Cipher::AES_128_OCB)                         \
  V(OCB_192, AES_Cipher, ncrypto::Cipher::AES_192_OCB)                         \
  V(OCB_256, AES_Cipher, ncrypto::Cipher::AES_256_OCB)
#else
#define VARIANTS_OCB(V)
#endif

#define VARIANTS(V)                                                            \
  VARIANTS_COMMON(V)                                                           \
  VARIANTS_OCB(V)

enum class AESKeyVariant {
#define V(name, _, __) name,
  VARIANTS(V)
#undef V
};

struct AESCipherConfig final : public MemoryRetainer {
  CryptoJobMode mode;
  AESKeyVariant variant;
  ncrypto::Cipher cipher;
  size_t length;
  ByteSource iv;  // Used for both iv or counter
  ByteSource additional_data;
  ByteSource tag;  // Used only for authenticated modes (GCM)

  AESCipherConfig() = default;

  AESCipherConfig(AESCipherConfig&& other) noexcept;

  AESCipherConfig& operator=(AESCipherConfig&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(AESCipherConfig)
  SET_SELF_SIZE(AESCipherConfig)
};

struct AESCipherTraits final {
  static constexpr const char* JobName = "AESCipherJob";

  using AdditionalParameters = AESCipherConfig;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      WebCryptoCipherMode cipher_mode,
      AESCipherConfig* config);

  static WebCryptoCipherStatus DoCipher(Environment* env,
                                        const KeyObjectData& key_data,
                                        WebCryptoCipherMode cipher_mode,
                                        const AESCipherConfig& params,
                                        const ByteSource& in,
                                        ByteSource* out);
};

using AESCryptoJob = CipherJob<AESCipherTraits>;

namespace AES {
void Initialize(Environment* env, v8::Local<v8::Object> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace AES
}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_AES_H_
