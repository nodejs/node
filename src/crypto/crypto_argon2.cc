#include "crypto/crypto_argon2.h"

namespace node::crypto {
#if OPENSSL_VERSION_MAJOR >= 3 && OPENSSL_VERSION_MINOR >= 2
#ifndef OPENSSL_NO_ARGON2
#include <openssl/core_names.h>

using v8::FunctionCallbackInfo;
using v8::JustVoid;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Uint32;
using v8::Value;

Argon2Config::Argon2Config(Argon2Config&& other) noexcept
    : mode{other.mode},
      pass{std::move(other.pass)},
      salt{std::move(other.salt)},
      secret{std::move(other.secret)},
      ad{std::move(other.ad)},
      type{other.type},
      iter{other.iter},
      lanes{other.lanes},
      memcost{other.memcost},
      keylen{other.keylen} {}

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

MaybeLocal<Value> Argon2Traits::EncodeOutput(Environment* env,
                                             const Argon2Config& config,
                                             ByteSource* out) {
  return out->ToArrayBuffer(env);
}

Maybe<void> Argon2Traits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    Argon2Config* config) {
  Environment* env = Environment::GetCurrent(args);

  config->mode = mode;

  ArrayBufferOrViewContents<char> pass(args[offset]);
  ArrayBufferOrViewContents<char> salt(args[offset + 1]);
  ArrayBufferOrViewContents<char> secret(args[offset + 6]);
  ArrayBufferOrViewContents<char> ad(args[offset + 7]);
  Utf8Value type_(env->isolate(), args[offset + 8]);

  if (!pass.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "pass is too large");
    return Nothing<void>();
  }

  if (!salt.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "salt is too large");
    return Nothing<void>();
  }

  if (!secret.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "secret is too large");
    return Nothing<void>();
  }

  if (!ad.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "ad is too large");
    return Nothing<void>();
  }

  const bool isAsync = mode == kCryptoJobAsync;
  config->pass = isAsync ? pass.ToCopy() : pass.ToByteSource();
  config->salt = isAsync ? salt.ToCopy() : salt.ToByteSource();
  config->secret = isAsync ? secret.ToCopy() : secret.ToByteSource();
  config->ad = isAsync ? ad.ToCopy() : ad.ToByteSource();

  if (type_.ToStringView() == "argon2i") {
    config->type = ncrypto::Argon2Type::ARGON2I;
  } else if (type_.ToStringView() == "argon2d") {
    config->type = ncrypto::Argon2Type::ARGON2D;
  } else if (type_.ToStringView() == "argon2id") {
    config->type = ncrypto::Argon2Type::ARGON2ID;
  } else {
    THROW_ERR_CRYPTO_INVALID_ARGON2_PARAMS(env);
    return Nothing<void>();
  }

  CHECK(args[offset + 2]->IsUint32());  // lanes
  CHECK(args[offset + 3]->IsUint32());  // keylen
  CHECK(args[offset + 4]->IsUint32());  // memcost
  CHECK(args[offset + 5]->IsUint32());  // iter

  config->lanes = args[offset + 2].As<Uint32>()->Value();
  config->keylen = args[offset + 3].As<Uint32>()->Value();
  config->memcost = args[offset + 4].As<Uint32>()->Value();
  config->iter = args[offset + 5].As<Uint32>()->Value();

  if (!ncrypto::argon2(config->pass,
                       config->salt,
                       config->lanes,
                       config->keylen,
                       config->memcost,
                       config->iter,
                       config->version,
                       config->secret,
                       config->ad,
                       config->type)) {
    THROW_ERR_CRYPTO_INVALID_ARGON2_PARAMS(env);
    return Nothing<void>();
  }

  return JustVoid();
}

bool Argon2Traits::DeriveBits(Environment* env,
                              const Argon2Config& config,
                              ByteSource* out,
                              CryptoJobMode mode) {
  // If the config.length is zero-length, just return an empty buffer.
  // It's useless, yes, but allowed via the API.
  if (config.keylen == 0) {
    *out = ByteSource();
    return true;
  }

  // Both the pass and salt may be zero-length at this point
  auto dp = ncrypto::argon2(
      ncrypto::Buffer<const char>{
          .data = config.pass.data<char>(),
          .len = config.pass.size(),
      },
      ncrypto::Buffer{
          .data = config.salt.data<unsigned char>(),
          .len = config.salt.size(),
      },
      config.lanes,
      config.keylen,
      config.memcost,
      config.iter,
      config.version,
        ncrypto::Buffer{
          .data = config.secret.data<unsigned char>(),
          .len = config.secret.size(),
      },
      ncrypto::Buffer{
          .data = config.ad.data<unsigned char>(),
          .len = config.ad.size(),
      },
      config.type);

  if (!dp) return false;
  DCHECK(!dp.isSecure());
  *out = ByteSource::Allocated(dp.release());
  return true;
}

#endif  // !OPENSSL_NO_ARGON2
#endif
}  // namespace node::crypto
