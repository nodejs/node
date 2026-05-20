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
using PqcKeyTypeGetter = Local<String> (Environment::*)() const;

enum PqcAlgorithmFlag {
  kPqcRawPrivate = 1 << 0,
  kPqcRawSeed = 1 << 1,
  kPqcSignature = 1 << 2,
};

struct PqcAlgorithm {
  int id;
  const char* name;
  PqcKeyTypeGetter key_type;
  int flags;
};

// ML-DSA and ML-KEM carry private material as a seed. SLH-DSA uses the
// expanded private key and is only exposed by OpenSSL.
constexpr int kPqcMlDsaFlags = kPqcRawSeed | kPqcSignature;
constexpr int kPqcMlKemFlags = kPqcRawSeed;
constexpr int kPqcSlhDsaFlags = kPqcRawPrivate | kPqcSignature;

constexpr PqcAlgorithm kPqcAlgorithms[] = {
    {EVP_PKEY_ML_DSA_44,
     "ML-DSA-44",
     &Environment::crypto_ml_dsa_44_string,
     kPqcMlDsaFlags},
    {EVP_PKEY_ML_DSA_65,
     "ML-DSA-65",
     &Environment::crypto_ml_dsa_65_string,
     kPqcMlDsaFlags},
    {EVP_PKEY_ML_DSA_87,
     "ML-DSA-87",
     &Environment::crypto_ml_dsa_87_string,
     kPqcMlDsaFlags},
    {EVP_PKEY_ML_KEM_768,
     "ML-KEM-768",
     &Environment::crypto_ml_kem_768_string,
     kPqcMlKemFlags},
    {EVP_PKEY_ML_KEM_1024,
     "ML-KEM-1024",
     &Environment::crypto_ml_kem_1024_string,
     kPqcMlKemFlags},

#if OPENSSL_WITH_PQC_ML_KEM_512
    {EVP_PKEY_ML_KEM_512,
     "ML-KEM-512",
     &Environment::crypto_ml_kem_512_string,
     kPqcMlKemFlags},
#endif
#if OPENSSL_WITH_PQC_SLH_DSA
    {EVP_PKEY_SLH_DSA_SHA2_128F,
     "SLH-DSA-SHA2-128f",
     &Environment::crypto_slh_dsa_sha2_128f_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHA2_128S,
     "SLH-DSA-SHA2-128s",
     &Environment::crypto_slh_dsa_sha2_128s_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHA2_192F,
     "SLH-DSA-SHA2-192f",
     &Environment::crypto_slh_dsa_sha2_192f_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHA2_192S,
     "SLH-DSA-SHA2-192s",
     &Environment::crypto_slh_dsa_sha2_192s_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHA2_256F,
     "SLH-DSA-SHA2-256f",
     &Environment::crypto_slh_dsa_sha2_256f_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHA2_256S,
     "SLH-DSA-SHA2-256s",
     &Environment::crypto_slh_dsa_sha2_256s_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHAKE_128F,
     "SLH-DSA-SHAKE-128f",
     &Environment::crypto_slh_dsa_shake_128f_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHAKE_128S,
     "SLH-DSA-SHAKE-128s",
     &Environment::crypto_slh_dsa_shake_128s_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHAKE_192F,
     "SLH-DSA-SHAKE-192f",
     &Environment::crypto_slh_dsa_shake_192f_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHAKE_192S,
     "SLH-DSA-SHAKE-192s",
     &Environment::crypto_slh_dsa_shake_192s_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHAKE_256F,
     "SLH-DSA-SHAKE-256f",
     &Environment::crypto_slh_dsa_shake_256f_string,
     kPqcSlhDsaFlags},
    {EVP_PKEY_SLH_DSA_SHAKE_256S,
     "SLH-DSA-SHAKE-256s",
     &Environment::crypto_slh_dsa_shake_256s_string,
     kPqcSlhDsaFlags},
#endif
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

bool HasPqcAlgorithmFlag(const PqcAlgorithm* alg, PqcAlgorithmFlag flag) {
  return alg != nullptr && (alg->flags & flag) != 0;
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
    const bool uses_seed = HasPqcAlgorithmFlag(alg, kPqcRawSeed);
    DataPointer priv_data = uses_seed ? pkey.rawSeed() : pkey.rawPrivateKey();
    if (uses_seed && !priv_data) {
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
    pkey = HasPqcAlgorithmFlag(alg, kPqcRawSeed)
               ? EVPKeyPointer::NewRawSeed(alg->id, buf)
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

bool IsPqcKeyId(int id) {
  return FindPqcAlgorithmById(id) != nullptr;
}

bool IsPqcRawPrivateKeyId(int id) {
  const PqcAlgorithm* alg = FindPqcAlgorithmById(id);
  return HasPqcAlgorithmFlag(alg, kPqcRawPrivate);
}

bool IsPqcSeedKeyId(int id) {
  const PqcAlgorithm* alg = FindPqcAlgorithmById(id);
  return HasPqcAlgorithmFlag(alg, kPqcRawSeed);
}

bool IsPqcSignatureKeyId(int id) {
  const PqcAlgorithm* alg = FindPqcAlgorithmById(id);
  return HasPqcAlgorithmFlag(alg, kPqcSignature);
}

int GetPqcNidFromName(const char* name) {
  for (const auto& alg : kPqcAlgorithms) {
    if (StringEqualNoCase(name, alg.name)) return alg.id;
  }
  return NID_undef;
}

Local<Value> GetPqcAsymmetricKeyType(Environment* env, int id) {
  const PqcAlgorithm* alg = FindPqcAlgorithmById(id);
  if (alg == nullptr) return v8::Undefined(env->isolate());

  Local<String> key_type = (env->*(alg->key_type))();
  return key_type.As<Value>();
}
#endif
}  // namespace crypto
}  // namespace node
