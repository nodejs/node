#include "crypto/crypto_ml_dsa.h"
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

#if OPENSSL_VERSION_MAJOR >= 3 && OPENSSL_VERSION_MINOR >= 5
constexpr const char* GetMlDsaAlgorithmName(int id) {
  switch (id) {
    case EVP_PKEY_ML_DSA_44:
      return "ML-DSA-44";
    case EVP_PKEY_ML_DSA_65:
      return "ML-DSA-65";
    case EVP_PKEY_ML_DSA_87:
      return "ML-DSA-87";
    default:
      return nullptr;
  }
}

bool ExportJwkMlDsaKey(Environment* env,
                       const KeyObjectData& key,
                       Local<Object> target) {
  Mutex::ScopedLock lock(key.mutex());
  const auto& pkey = key.GetAsymmetricKey();

  const char* alg = GetMlDsaAlgorithmName(pkey.id());
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
    auto seed = pkey.rawSeed();
    if (!seed) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                        "key does not have an available seed");
      return false;
    }
    if (!trySetKey(env, pkey.rawSeed(), target, env->jwk_priv_string())) {
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
