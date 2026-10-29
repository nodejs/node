#ifndef SRC_CRYPTO_CRYPTO_DSA_H_
#define SRC_CRYPTO_CRYPTO_DSA_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_keys.h"
#include "crypto/crypto_keygen.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "memory_tracker.h"
#include "v8.h"

namespace node::crypto {
struct DsaKeyPairParams final : public MemoryRetainer {
  unsigned int modulus_bits;
  int divisor_bits;
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(DsaKeyPairParams)
  SET_SELF_SIZE(DsaKeyPairParams)
};

using DsaKeyPairGenConfig = KeyPairGenConfig<DsaKeyPairParams>;

struct DsaKeyGenTraits final {
  using AdditionalParameters = DsaKeyPairGenConfig;
  static constexpr const char* JobName = "DsaKeyPairGenJob";

  static ncrypto::EVPKeyCtxPointer Setup(DsaKeyPairGenConfig* params);

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int* offset,
      DsaKeyPairGenConfig* params);
};

using DsaKeyPairGenJob = KeyGenJob<KeyPairGenTraits<DsaKeyGenTraits>>;

bool GetDsaKeyDetail(Environment* env,
                     const KeyObjectData& key,
                     v8::Local<v8::Object> target);

namespace DSAAlg {
void Initialize(Environment* env, v8::Local<v8::Object> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace DSAAlg
}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_DSA_H_
