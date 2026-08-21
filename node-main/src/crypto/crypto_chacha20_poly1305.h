#ifndef SRC_CRYPTO_CRYPTO_CHACHA20_POLY1305_H_
#define SRC_CRYPTO_CRYPTO_CHACHA20_POLY1305_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_cipher.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "v8.h"

namespace node::crypto {
constexpr unsigned kChaCha20Poly1305AuthTagLength = 16;

struct ChaCha20Poly1305CipherConfig final : public MemoryRetainer {
  CryptoJobMode mode;
  ncrypto::Cipher cipher;
  ByteSource iv;
  ByteSource additional_data;
  ByteSource tag;

  ChaCha20Poly1305CipherConfig() = default;

  ChaCha20Poly1305CipherConfig(ChaCha20Poly1305CipherConfig&& other) noexcept;

  ChaCha20Poly1305CipherConfig& operator=(
      ChaCha20Poly1305CipherConfig&& other) noexcept;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ChaCha20Poly1305CipherConfig)
  SET_SELF_SIZE(ChaCha20Poly1305CipherConfig)
};

struct ChaCha20Poly1305CipherTraits final {
  static constexpr const char* JobName = "ChaCha20Poly1305CipherJob";

  using AdditionalParameters = ChaCha20Poly1305CipherConfig;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      WebCryptoCipherMode cipher_mode,
      ChaCha20Poly1305CipherConfig* config);

  static WebCryptoCipherStatus DoCipher(
      Environment* env,
      const KeyObjectData& key_data,
      WebCryptoCipherMode cipher_mode,
      const ChaCha20Poly1305CipherConfig& params,
      const ByteSource& in,
      ByteSource* out);
};

using ChaCha20Poly1305CryptoJob = CipherJob<ChaCha20Poly1305CipherTraits>;

namespace ChaCha20Poly1305 {
void Initialize(Environment* env, v8::Local<v8::Object> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace ChaCha20Poly1305
}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_CHACHA20_POLY1305_H_
