#include "crypto/crypto_argon2.h"

#include <openssl/core_names.h> /* OSSL_KDF_* */

namespace node::crypto {

using v8::FunctionCallbackInfo;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Uint32;
using v8::Value;

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

Maybe<bool> Argon2Traits::EncodeOutput(Environment* env,
                                       const Argon2Config& params,
                                       ByteSource* out,
                                       Local<Value>* result) {
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
  if (UNLIKELY(!params->kdf)) {
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

  return Just(true);
}

bool Argon2Traits::DeriveBits(Environment* env,
                              const Argon2Config& params,
                              ByteSource* out) {
  ByteSource::Builder buf(params.keylen);

  auto kctx = EVPKdfCtxPointer{EVP_KDF_CTX_new(params.kdf.get())};
  if (!kctx) {
    return false;
  }

  auto ossl_params = std::vector<OSSL_PARAM>{
      OSSL_PARAM_octet_string(OSSL_KDF_PARAM_PASSWORD,
                              const_cast<char*>(params.pass.data<char>()),
                              params.pass.size()),
      OSSL_PARAM_octet_string(OSSL_KDF_PARAM_SALT,
                              const_cast<char*>(params.salt.data<char>()),
                              params.salt.size()),
      OSSL_PARAM_uint32(OSSL_KDF_PARAM_ITER,
                        const_cast<uint32_t*>(&params.iter)),
      OSSL_PARAM_uint32(OSSL_KDF_PARAM_ARGON2_LANES,
                        const_cast<uint32_t*>(&params.lanes)),
      OSSL_PARAM_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST,
                        const_cast<uint32_t*>(&params.memcost)),
  };

  if (params.secret.size() > 0) {
    ossl_params.push_back(
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_SECRET,
                                const_cast<char*>(params.secret.data<char>()),
                                params.secret.size()));
  }

  if (params.ad.size() > 0) {
    ossl_params.push_back(
        OSSL_PARAM_octet_string(OSSL_KDF_PARAM_ARGON2_AD,
                                const_cast<char*>(params.ad.data<char>()),
                                params.ad.size()));
  }

  ossl_params.push_back(OSSL_PARAM_END);

  // Both the pass and salt may be zero-length at this point
  if (EVP_KDF_derive(kctx.get(),
                     buf.data<unsigned char>(),
                     params.keylen,
                     ossl_params.data()) != 1) {
    return false;
  }
  *out = std::move(buf).release();
  return true;
}

#endif  // !OPENSSL_NO_ARGON2

}  // namespace node::crypto
