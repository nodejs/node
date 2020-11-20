#include "crypto/crypto_dsa.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#include <openssl/bn.h>
#include <openssl/dsa.h>

#include <cstdio>

namespace node {

using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

namespace crypto {
EVPKeyCtxPointer DsaKeyGenTraits::Setup(DsaKeyPairGenConfig* params) {
  EVPKeyCtxPointer param_ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_DSA, nullptr));
  EVP_PKEY* raw_params = nullptr;

  if (!param_ctx ||
      EVP_PKEY_paramgen_init(param_ctx.get()) <= 0 ||
      EVP_PKEY_CTX_set_dsa_paramgen_bits(
          param_ctx.get(),
          params->params.modulus_bits) <= 0) {
    return EVPKeyCtxPointer();
  }

  if (params->params.divisor_bits != -1) {
    if (EVP_PKEY_CTX_ctrl(
            param_ctx.get(),
            EVP_PKEY_DSA,
            EVP_PKEY_OP_PARAMGEN,
            EVP_PKEY_CTRL_DSA_PARAMGEN_Q_BITS,
            params->params.divisor_bits,
            nullptr) <= 0) {
      return EVPKeyCtxPointer();
    }
  }

  if (EVP_PKEY_paramgen(param_ctx.get(), &raw_params) <= 0)
    return EVPKeyCtxPointer();

  EVPKeyPointer key_params(raw_params);
  EVPKeyCtxPointer key_ctx(EVP_PKEY_CTX_new(key_params.get(), nullptr));

  if (!key_ctx || EVP_PKEY_keygen_init(key_ctx.get()) <= 0)
    return EVPKeyCtxPointer();

  return key_ctx;
}

// Input arguments for DsaKeyPairGenJob
//   1. CryptoJobMode
//   2. Modulus Bits
//   3. Divisor Bits
//   4. Public Format
//   5. Public Type
//   6. Private Format
//   7. Private Type
//   8. Cipher
//   9. Passphrase
Maybe<bool> DsaKeyGenTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int* offset,
    DsaKeyPairGenConfig* params) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[*offset]->IsUint32());  // modulus bits
  CHECK(args[*offset + 1]->IsInt32());  // divisor bits

  params->params.modulus_bits = args[*offset].As<Uint32>()->Value();
  params->params.divisor_bits = args[*offset + 1].As<Int32>()->Value();
  if (params->params.divisor_bits < -1) {
    char msg[1024];
    snprintf(msg, sizeof(msg), "invalid value for divisor_bits");
    THROW_ERR_OUT_OF_RANGE(env, msg);
    return Nothing<bool>();
  }

  *offset += 2;

  return Just(true);
}

Maybe<bool> DSAKeyExportTraits::AdditionalConfig(
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    DSAKeyExportConfig* params) {
  return Just(true);
}

WebCryptoKeyExportStatus DSAKeyExportTraits::DoExport(
    std::shared_ptr<KeyObjectData> key_data,
    WebCryptoKeyFormat format,
    const DSAKeyExportConfig& params,
    ByteSource* out) {
  CHECK_NE(key_data->GetKeyType(), kKeyTypeSecret);

  switch (format) {
    case kWebCryptoKeyFormatRaw:
      // Not supported for RSA keys of either type
      return WebCryptoKeyExportStatus::FAILED;
    case kWebCryptoKeyFormatPKCS8:
      if (key_data->GetKeyType() != kKeyTypePrivate)
        return WebCryptoKeyExportStatus::INVALID_KEY_TYPE;
      return PKEY_PKCS8_Export(key_data.get(), out);
    case kWebCryptoKeyFormatSPKI:
      if (key_data->GetKeyType() != kKeyTypePublic)
        return WebCryptoKeyExportStatus::INVALID_KEY_TYPE;
      return PKEY_SPKI_Export(key_data.get(), out);
    default:
      UNREACHABLE();
  }
}

Maybe<bool> ExportJWKDsaKey(
    Environment* env,
    std::shared_ptr<KeyObjectData> key,
    Local<Object> target) {
  ManagedEVPPKey pkey = key->GetAsymmetricKey();
  CHECK_EQ(EVP_PKEY_id(pkey.get()), EVP_PKEY_DSA);

  DSA* dsa = EVP_PKEY_get0_DSA(pkey.get());
  CHECK_NOT_NULL(dsa);

  const BIGNUM* y;
  const BIGNUM* x;
  const BIGNUM* p;
  const BIGNUM* q;
  const BIGNUM* g;

  DSA_get0_key(dsa, &y, &x);
  DSA_get0_pqg(dsa, &p, &q, &g);

  if (target->Set(
          env->context(),
          env->jwk_kty_string(),
          env->jwk_dsa_string()).IsNothing()) {
    return Nothing<bool>();
  }

  if (SetEncodedValue(env, target, env->jwk_y_string(), y).IsNothing() ||
      SetEncodedValue(env, target, env->jwk_p_string(), p).IsNothing() ||
      SetEncodedValue(env, target, env->jwk_q_string(), q).IsNothing() ||
      SetEncodedValue(env, target, env->jwk_g_string(), g).IsNothing()) {
    return Nothing<bool>();
  }

  if (key->GetKeyType() == kKeyTypePrivate &&
      SetEncodedValue(env, target, env->jwk_x_string(), x).IsNothing()) {
    return Nothing<bool>();
  }

  return Just(true);
}

std::shared_ptr<KeyObjectData> ImportJWKDsaKey(
    Environment* env,
    Local<Object> jwk,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset) {
  Local<Value> y_value;
  Local<Value> p_value;
  Local<Value> q_value;
  Local<Value> g_value;
  Local<Value> x_value;

  if (!jwk->Get(env->context(), env->jwk_y_string()).ToLocal(&y_value) ||
      !jwk->Get(env->context(), env->jwk_p_string()).ToLocal(&p_value) ||
      !jwk->Get(env->context(), env->jwk_q_string()).ToLocal(&q_value) ||
      !jwk->Get(env->context(), env->jwk_g_string()).ToLocal(&g_value) ||
      !jwk->Get(env->context(), env->jwk_x_string()).ToLocal(&x_value)) {
    return std::shared_ptr<KeyObjectData>();
  }

  if (!y_value->IsString() ||
      !p_value->IsString() ||
      !q_value->IsString() ||
      !q_value->IsString() ||
      (!x_value->IsUndefined() && !x_value->IsString())) {
    THROW_ERR_CRYPTO_INVALID_JWK(env, "Invalid JWK DSA key");
    return std::shared_ptr<KeyObjectData>();
  }

  KeyType type = x_value->IsString() ? kKeyTypePrivate : kKeyTypePublic;

  DsaPointer dsa(DSA_new());

  ByteSource y = ByteSource::FromEncodedString(env, y_value.As<String>());
  ByteSource p = ByteSource::FromEncodedString(env, p_value.As<String>());
  ByteSource q = ByteSource::FromEncodedString(env, q_value.As<String>());
  ByteSource g = ByteSource::FromEncodedString(env, g_value.As<String>());

  if (!DSA_set0_key(dsa.get(), y.ToBN().release(), nullptr) ||
      !DSA_set0_pqg(dsa.get(),
                    p.ToBN().release(),
                    q.ToBN().release(),
                    g.ToBN().release())) {
    THROW_ERR_CRYPTO_INVALID_JWK(env, "Invalid JWK DSA key");
    return std::shared_ptr<KeyObjectData>();
  }

  if (type == kKeyTypePrivate) {
    ByteSource x = ByteSource::FromEncodedString(env, x_value.As<String>());
    if (!DSA_set0_key(dsa.get(), nullptr, x.ToBN().release())) {
      THROW_ERR_CRYPTO_INVALID_JWK(env, "Invalid JWK DSA key");
      return std::shared_ptr<KeyObjectData>();
    }
  }

  EVPKeyPointer pkey(EVP_PKEY_new());
  CHECK_EQ(EVP_PKEY_set1_DSA(pkey.get(), dsa.get()), 1);

  return KeyObjectData::CreateAsymmetric(type, ManagedEVPPKey(std::move(pkey)));
}

Maybe<bool> GetDsaKeyDetail(
    Environment* env,
    std::shared_ptr<KeyObjectData> key,
    Local<Object> target) {
  const BIGNUM* p;  // Modulus length
  const BIGNUM* q;  // Divisor length

  ManagedEVPPKey pkey = key->GetAsymmetricKey();
  int type = EVP_PKEY_id(pkey.get());
  CHECK(type == EVP_PKEY_DSA);

  DSA* dsa = EVP_PKEY_get0_DSA(pkey.get());
  CHECK_NOT_NULL(dsa);

  DSA_get0_pqg(dsa, &p, &q, nullptr);

  size_t modulus_length = BN_num_bytes(p) * CHAR_BIT;
  size_t divisor_length = BN_num_bytes(q) * CHAR_BIT;

  if (target->Set(
          env->context(),
          env->modulus_length_string(),
          Number::New(env->isolate(), modulus_length)).IsNothing() ||
      target->Set(
          env->context(),
          env->divisor_length_string(),
          Number::New(env->isolate(), divisor_length)).IsNothing()) {
    return Nothing<bool>();
  }

  return Just(true);
}

namespace DSAAlg {
void Initialize(Environment* env, Local<Object> target) {
  DsaKeyPairGenJob::Initialize(env, target);
  DSAKeyExportJob::Initialize(env, target);
}
}  // namespace DSAAlg
}  // namespace crypto
}  // namespace node

