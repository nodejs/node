#include "crypto/crypto_ml_dsa.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "string_bytes.h"
#include "v8.h"

namespace node {

using ncrypto::DataPointer;
using ncrypto::EVPKeyPointer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

namespace crypto {

#if OPENSSL_WITH_PQC
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

/**
 * Exports an ML-DSA key to JWK format.
 *
 * The resulting JWK object contains:
 * - "kty": "AKP" (Asymmetric Key Pair - required)
 * - "alg": "ML-DSA-XX" (Algorithm identifier - required for "AKP")
 * - "pub": "<Base64URL-encoded raw public key>" (required)
 * - "priv": <"Base64URL-encoded raw seed>" (required for private keys only)
 */
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

KeyObjectData ImportJWKAkpKey(Environment* env, Local<Object> jwk) {
  Local<Value> alg_value;
  Local<Value> pub_value;
  Local<Value> priv_value;

  if (!jwk->Get(env->context(), env->jwk_alg_string()).ToLocal(&alg_value) ||
      !jwk->Get(env->context(), env->jwk_pub_string()).ToLocal(&pub_value) ||
      !jwk->Get(env->context(), env->jwk_priv_string()).ToLocal(&priv_value)) {
    return {};
  }

  static constexpr int kMlDsaIds[] = {
      EVP_PKEY_ML_DSA_44, EVP_PKEY_ML_DSA_65, EVP_PKEY_ML_DSA_87};

  Utf8Value alg(env->isolate(),
                alg_value->IsString() ? alg_value.As<String>()
                                      : String::Empty(env->isolate()));

  int id = NID_undef;
  for (int candidate : kMlDsaIds) {
    if (strcmp(*alg, GetMlDsaAlgorithmName(candidate)) == 0) {
      id = candidate;
      break;
    }
  }

  if (id == NID_undef) {
    THROW_ERR_CRYPTO_INVALID_JWK(env, "Unsupported JWK AKP \"alg\"");
    return {};
  }

  if (!pub_value->IsString() ||
      (!priv_value->IsUndefined() && !priv_value->IsString())) {
    THROW_ERR_CRYPTO_INVALID_JWK(env, "Invalid JWK AKP key");
    return {};
  }

  KeyType type = priv_value->IsString() ? kKeyTypePrivate : kKeyTypePublic;

  EVPKeyPointer pkey;
  if (type == kKeyTypePrivate) {
    ByteSource seed =
        ByteSource::FromEncodedString(env, priv_value.As<String>());
    pkey =
        EVPKeyPointer::NewRawSeed(id,
                                  ncrypto::Buffer<const unsigned char>{
                                      .data = seed.data<const unsigned char>(),
                                      .len = seed.size(),
                                  });
  } else {
    ByteSource pub = ByteSource::FromEncodedString(env, pub_value.As<String>());
    pkey =
        EVPKeyPointer::NewRawPublic(id,
                                    ncrypto::Buffer<const unsigned char>{
                                        .data = pub.data<const unsigned char>(),
                                        .len = pub.size(),
                                    });
  }

  if (!pkey) {
    THROW_ERR_CRYPTO_INVALID_JWK(env, "Invalid JWK AKP key");
    return {};
  }

  // When importing a private key, verify that the JWK's pub field matches
  // the public key derived from the seed.
  if (type == kKeyTypePrivate && pub_value->IsString()) {
    ByteSource pub = ByteSource::FromEncodedString(env, pub_value.As<String>());
    auto derived_pub = pkey.rawPublicKey();
    if (!derived_pub || derived_pub.size() != pub.size() ||
        CRYPTO_memcmp(derived_pub.get(), pub.data(), pub.size()) != 0) {
      THROW_ERR_CRYPTO_INVALID_JWK(env, "Invalid JWK AKP key");
      return {};
    }
  }

  return KeyObjectData::CreateAsymmetric(type, std::move(pkey));
}
#endif
}  // namespace crypto
}  // namespace node
