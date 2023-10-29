#include "crypto/crypto_argon2.h"
#include "async_wrap-inl.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#include <openssl/core_names.h> /* OSSL_KDF_* */

namespace node {

using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Just;
using v8::Maybe;
using v8::Nothing;
using v8::Uint32;
using v8::Value;

namespace crypto {
#ifndef OPENSSL_NO_ARGON2

Argon2Config::Argon2Config(Argon2Config&& other) noexcept
    : mode(other.mode),
      kdf(std::move(other.kdf)),
      pass(std::move(other.pass)),
      salt(std::move(other.salt)),
      secret(std::move(other.secret)),
      ad(std::move(other.ad)),
      iter(other.iter),
      // threads(other.threads),
      lanes(other.lanes),
      memcost(other.memcost),
      keylen(other.keylen) {}

Argon2Config& Argon2Config::operator=(Argon2Config&& other) noexcept {
  if (&other == this) return *this;
  this->~Argon2Config();
  return *new (this) Argon2Config(std::move(other));
}

void Argon2Config::MemoryInfo(MemoryTracker* tracker) const {
  if (mode == kCryptoJobAsync) {
    tracker->TrackFieldWithSize("pass", pass.size());
    tracker->TrackFieldWithSize("salt", salt.size());
    tracker->TrackFieldWithSize("secret", secret.size());
    tracker->TrackFieldWithSize("ad", ad.size());
  }
}

static bool argon2_hash(const EVPKdfPointer& kdf,
                        const char* pass,
                        size_t passlen,
                        const char* salt,
                        size_t saltlen,
                        const char* secret,
                        size_t secretlen,
                        const char* ad,
                        size_t adlen,
                        uint32_t iter,
                        uint32_t lanes,
                        uint32_t memcost,
                        unsigned char* key,
                        size_t keylen) {
  auto kctx = EVPKdfCtxPointer{EVP_KDF_CTX_new(kdf.get())};
  if (!kctx) {
    return false;
  }

  std::vector<OSSL_PARAM> params;

  params.push_back(OSSL_PARAM_construct_octet_string(
      OSSL_KDF_PARAM_PASSWORD, const_cast<char*>(pass), passlen));

  params.push_back(OSSL_PARAM_construct_octet_string(
      OSSL_KDF_PARAM_SALT, const_cast<char*>(salt), saltlen));

  if (secretlen > 0) {
    params.push_back(OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_SECRET, const_cast<char*>(secret), secretlen));
  }

  params.push_back(OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iter));
  params.push_back(
      OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes));

  if (adlen > 0) {
    params.push_back(OSSL_PARAM_construct_octet_string(
        OSSL_KDF_PARAM_ARGON2_AD, const_cast<char*>(ad), adlen));
  }

  params.push_back(
      OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memcost));

  params.push_back(OSSL_PARAM_construct_end());

  return EVP_KDF_derive(kctx.get(), key, keylen, params.data());
}

Maybe<bool> Argon2Traits::EncodeOutput(Environment* env,
                                       const Argon2Config& params,
                                       ByteSource* out,
                                       v8::Local<v8::Value>* result) {
  *result = out->ToArrayBuffer(env);
  return Just(!result->IsEmpty());
}

Maybe<bool> Argon2Traits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    Argon2Config* params) {
  Environment* env = Environment::GetCurrent(args);

  params->mode = mode;

  ArrayBufferOrViewContents<char> pass(args[offset]);
  ArrayBufferOrViewContents<char> salt(args[offset + 1]);
  Utf8Value algorithm(env->isolate(), args[offset + 2]);
  ArrayBufferOrViewContents<char> secret(args[offset + 3]);
  ArrayBufferOrViewContents<char> ad(args[offset + 4]);

  if (UNLIKELY(!pass.CheckSizeInt32())) {
    THROW_ERR_OUT_OF_RANGE(env, "pass is too large");
    return Nothing<bool>();
  }

  if (UNLIKELY(!salt.CheckSizeInt32())) {
    THROW_ERR_OUT_OF_RANGE(env, "salt is too large");
    return Nothing<bool>();
  }

  if (UNLIKELY(!secret.CheckSizeInt32())) {
    THROW_ERR_OUT_OF_RANGE(env, "secret is too large");
    return Nothing<bool>();
  }

  if (UNLIKELY(!ad.CheckSizeInt32())) {
    THROW_ERR_OUT_OF_RANGE(env, "ad is too large");
    return Nothing<bool>();
  }

  params->kdf = EVPKdfPointer{EVP_KDF_fetch(nullptr, algorithm.out(), nullptr)};
  if (!params->kdf) {
    THROW_ERR_CRYPTO_INVALID_ARGON2_PARAMS(env);
    return Nothing<bool>();
  }

  const bool isAsync = mode == kCryptoJobAsync;
  params->pass = isAsync ? pass.ToCopy() : pass.ToByteSource();
  params->salt = isAsync ? salt.ToCopy() : salt.ToByteSource();
  params->secret = isAsync ? secret.ToCopy() : secret.ToByteSource();
  params->ad = isAsync ? ad.ToCopy() : ad.ToByteSource();

  CHECK(args[offset + 5]->IsUint32());  // iter
  CHECK(args[offset + 6]->IsUint32());  // lanes
  CHECK(args[offset + 7]->IsUint32());  // memcost
  CHECK(args[offset + 8]->IsUint32());  // keylen

  params->iter = args[offset + 5].As<Uint32>()->Value();
  params->lanes = args[offset + 6].As<Uint32>()->Value();
  params->memcost = args[offset + 7].As<Uint32>()->Value();
  params->keylen = args[offset + 8].As<Uint32>()->Value();

  if (!argon2_hash(params->kdf,
                   params->pass.data<char>(),
                   params->pass.size(),
                   params->salt.data<char>(),
                   params->salt.size(),
                   params->secret.data<char>(),
                   params->secret.size(),
                   params->ad.data<char>(),
                   params->ad.size(),
                   params->iter,
                   params->lanes,
                   params->memcost,
                   nullptr,
                   params->keylen)) {
    THROW_ERR_CRYPTO_INVALID_ARGON2_PARAMS(env);
    return Nothing<bool>();
  }

  return Just(true);
}

bool Argon2Traits::DeriveBits(Environment* env,
                              const Argon2Config& params,
                              ByteSource* out) {
  ByteSource::Builder buf(params.keylen);

  // Both the pass and salt may be zero-length at this point
  if (!argon2_hash(params.kdf,
                   params.pass.data<char>(),
                   params.pass.size(),
                   params.salt.data<char>(),
                   params.salt.size(),
                   params.secret.data<char>(),
                   params.secret.size(),
                   params.ad.data<char>(),
                   params.ad.size(),
                   params.iter,
                   params.lanes,
                   params.memcost,
                   buf.data<unsigned char>(),
                   params.keylen)) {
    return false;
  }
  *out = std::move(buf).release();
  return true;
}

#endif  // !OPENSSL_NO_ARGON2

}  // namespace crypto
}  // namespace node
