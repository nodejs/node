#include "crypto/crypto_pqc.h"
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
namespace {
struct PqcAlgorithm {
  int id;
  const char* name;
  bool
      use_seed;  // true: rawSeed/NewRawSeed, false: rawPrivateKey/NewRawPrivate
};

constexpr PqcAlgorithm kPqcAlgorithms[] = {
    {EVP_PKEY_ML_DSA_44, "ML-DSA-44", true},
    {EVP_PKEY_ML_DSA_65, "ML-DSA-65", true},
    {EVP_PKEY_ML_DSA_87, "ML-DSA-87", true},
    {EVP_PKEY_ML_KEM_512, "ML-KEM-512", true},
    {EVP_PKEY_ML_KEM_768, "ML-KEM-768", true},
    {EVP_PKEY_ML_KEM_1024, "ML-KEM-1024", true},
    {EVP_PKEY_SLH_DSA_SHA2_128F, "SLH-DSA-SHA2-128f", false},
    {EVP_PKEY_SLH_DSA_SHA2_128S, "SLH-DSA-SHA2-128s", false},
    {EVP_PKEY_SLH_DSA_SHA2_192F, "SLH-DSA-SHA2-192f", false},
    {EVP_PKEY_SLH_DSA_SHA2_192S, "SLH-DSA-SHA2-192s", false},
    {EVP_PKEY_SLH_DSA_SHA2_256F, "SLH-DSA-SHA2-256f", false},
    {EVP_PKEY_SLH_DSA_SHA2_256S, "SLH-DSA-SHA2-256s", false},
    {EVP_PKEY_SLH_DSA_SHAKE_128F, "SLH-DSA-SHAKE-128f", false},
    {EVP_PKEY_SLH_DSA_SHAKE_128S, "SLH-DSA-SHAKE-128s", false},
    {EVP_PKEY_SLH_DSA_SHAKE_192F, "SLH-DSA-SHAKE-192f", false},
    {EVP_PKEY_SLH_DSA_SHAKE_192S, "SLH-DSA-SHAKE-192s", false},
    {EVP_PKEY_SLH_DSA_SHAKE_256F, "SLH-DSA-SHAKE-256f", false},
    {EVP_PKEY_SLH_DSA_SHAKE_256S, "SLH-DSA-SHAKE-256s", false},
};

const PqcAlgorithm* FindPqcAlgorithmById(int id) {
  for (const auto& alg : kPqcAlgorithms) {
    if (alg.id == id) return &alg;
  }
  return nullptr;
}

const PqcAlgorithm* FindPqcAlgorithmByName(const char* name) {
  for (const auto& alg : kPqcAlgorithms) {
    if (strcmp(name, alg.name) == 0) return &alg;
  }
  return nullptr;
}

bool TrySetEncodedKey(Environment* env,
                      DataPointer data,
                      Local<Object> target,
                      Local<String> key) {
  Local<Value> encoded;
  if (!data) return false;
  const ncrypto::Buffer<const char> out = data;
  return StringBytes::Encode(env->isolate(), out.data, out.len, BASE64URL)
             .ToLocal(&encoded) &&
         target->Set(env->context(), key, encoded).IsJust();
}
}  // namespace

bool ExportJwkPqcKey(Environment* env,
                     const KeyObjectData& key,
                     Local<Object> target) {
  Mutex::ScopedLock lock(key.mutex());
  const auto& pkey = key.GetAsymmetricKey();

  const PqcAlgorithm* alg = FindPqcAlgorithmById(pkey.id());
  CHECK(alg);

  if (key.GetKeyType() == kKeyTypePrivate) {
    DataPointer priv_data =
        alg->use_seed ? pkey.rawSeed() : pkey.rawPrivateKey();
    if (alg->use_seed && !priv_data) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                        "key does not have an available seed");
      return false;
    }
    if (!TrySetEncodedKey(
            env, std::move(priv_data), target, env->jwk_priv_string())) {
      return false;
    }
  }

  return !(
      target->Set(env->context(), env->jwk_kty_string(), env->jwk_akp_string())
          .IsNothing() ||
      target
          ->Set(env->context(),
                env->jwk_alg_string(),
                OneByteString(env->isolate(), alg->name))
          .IsNothing() ||
      !TrySetEncodedKey(
          env, pkey.rawPublicKey(), target, env->jwk_pub_string()));
}

KeyObjectData ImportJWKPqcKey(Environment* env, Local<Object> jwk) {
  Local<Value> alg_value;
  Local<Value> pub_value;
  Local<Value> priv_value;

  if (!jwk->Get(env->context(), env->jwk_alg_string()).ToLocal(&alg_value) ||
      !jwk->Get(env->context(), env->jwk_pub_string()).ToLocal(&pub_value) ||
      !jwk->Get(env->context(), env->jwk_priv_string()).ToLocal(&priv_value)) {
    return {};
  }

  Utf8Value alg_str(env->isolate(),
                    alg_value->IsString() ? alg_value.As<String>()
                                          : String::Empty(env->isolate()));

  const PqcAlgorithm* alg = FindPqcAlgorithmByName(*alg_str);
  if (!alg) {
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
    ByteSource priv =
        ByteSource::FromEncodedString(env, priv_value.As<String>());
    ncrypto::Buffer<const unsigned char> buf{
        .data = priv.data<const unsigned char>(),
        .len = priv.size(),
    };
    pkey = alg->use_seed ? EVPKeyPointer::NewRawSeed(alg->id, buf)
                         : EVPKeyPointer::NewRawPrivate(alg->id, buf);
  } else {
    ByteSource pub = ByteSource::FromEncodedString(env, pub_value.As<String>());
    pkey =
        EVPKeyPointer::NewRawPublic(alg->id,
                                    ncrypto::Buffer<const unsigned char>{
                                        .data = pub.data<const unsigned char>(),
                                        .len = pub.size(),
                                    });
  }

  if (!pkey) {
    THROW_ERR_CRYPTO_INVALID_JWK(env, "Invalid JWK AKP key");
    return {};
  }

  // When importing a private key, verify that the pub field matches
  // the public key derived from the private key material.
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
