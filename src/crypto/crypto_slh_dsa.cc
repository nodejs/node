#include "crypto/crypto_slh_dsa.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "string_bytes.h"
#include "v8.h"

namespace node {

using ncrypto::DataPointer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

namespace crypto {

#if OPENSSL_WITH_PQC
constexpr const char* GetSlhDsaAlgorithmName(int id) {
  switch (id) {
    case EVP_PKEY_SLH_DSA_SHA2_128F:
      return "SLH-DSA-SHA2-128f";
    case EVP_PKEY_SLH_DSA_SHA2_128S:
      return "SLH-DSA-SHA2-128s";
    case EVP_PKEY_SLH_DSA_SHA2_192F:
      return "SLH-DSA-SHA2-192f";
    case EVP_PKEY_SLH_DSA_SHA2_192S:
      return "SLH-DSA-SHA2-192s";
    case EVP_PKEY_SLH_DSA_SHA2_256F:
      return "SLH-DSA-SHA2-256f";
    case EVP_PKEY_SLH_DSA_SHA2_256S:
      return "SLH-DSA-SHA2-256s";
    case EVP_PKEY_SLH_DSA_SHAKE_128F:
      return "SLH-DSA-SHAKE-128f";
    case EVP_PKEY_SLH_DSA_SHAKE_128S:
      return "SLH-DSA-SHAKE-128s";
    case EVP_PKEY_SLH_DSA_SHAKE_192F:
      return "SLH-DSA-SHAKE-192f";
    case EVP_PKEY_SLH_DSA_SHAKE_192S:
      return "SLH-DSA-SHAKE-192s";
    case EVP_PKEY_SLH_DSA_SHAKE_256F:
      return "SLH-DSA-SHAKE-256f";
    case EVP_PKEY_SLH_DSA_SHAKE_256S:
      return "SLH-DSA-SHAKE-256s";
    default:
      return nullptr;
  }
}

/**
 * Exports an SLH-DSA key to JWK format.
 *
 * The resulting JWK object contains:
 * - "kty": "AKP" (Asymmetric Key Pair - required)
 * - "alg": "SLH-DSA-XX-XX" (Algorithm identifier - required for "AKP")
 * - "pub": "<Base64URL-encoded raw public key>" (required)
 * - "priv": "<Base64URL-encoded raw private key>" (required for private keys)
 */
bool ExportJwkSlhDsaKey(Environment* env,
                        const KeyObjectData& key,
                        Local<Object> target) {
  Mutex::ScopedLock lock(key.mutex());
  const auto& pkey = key.GetAsymmetricKey();

  const char* alg = GetSlhDsaAlgorithmName(pkey.id());
  CHECK(alg);

  static constexpr auto trySetKey = [](Environment* env,
                                       DataPointer data,
                                       Local<Object> target,
                                       Local<String> key) {
    Local<Value> encoded;
    if (!data) return false;
    const ncrypto::Buffer<const char> out = data;
    return StringBytes::Encode(env->isolate(), out.data, out.len, BASE64URL)
               .ToLocal(&encoded) &&
           target->Set(env->context(), key, encoded).IsJust();
  };

  if (key.GetKeyType() == kKeyTypePrivate) {
    if (!trySetKey(env, pkey.rawPrivateKey(), target, env->jwk_priv_string())) {
      return false;
    }
  }

  return !(
      target->Set(env->context(), env->jwk_kty_string(), env->jwk_akp_string())
          .IsNothing() ||
      target
          ->Set(env->context(),
                env->jwk_alg_string(),
                OneByteString(env->isolate(), alg))
          .IsNothing() ||
      !trySetKey(env, pkey.rawPublicKey(), target, env->jwk_pub_string()));
}
#endif
}  // namespace crypto
}  // namespace node
