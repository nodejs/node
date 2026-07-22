#include "crypto/crypto_argon2.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "crypto/crypto_keys.h"
#include "memory_tracker-inl.h"
#include "threadpoolwork-inl.h"

#if OPENSSL_WITH_ARGON2
#include <openssl/core_names.h>

namespace node::crypto {

using v8::FunctionCallbackInfo;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Uint32;
using v8::Value;

Argon2Config::Argon2Config(Argon2Config&& other) noexcept
    : mode{other.mode},
      key{std::move(other.key)},
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
  if (key) tracker->TrackField("key", key);
  if (IsCryptoJobAsync(mode)) {
    if (!key) tracker->TrackFieldWithSize("pass", pass.size());
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

  CHECK(KeyObjectHandle::HasInstance(env, args[offset]) ||
        IsAnyBufferSource(args[offset]));  // pass
  ArrayBufferOrViewContents<char> salt(args[offset + 1]);
  ArrayBufferOrViewContents<char> secret(args[offset + 6]);
  ArrayBufferOrViewContents<char> ad(args[offset + 7]);

  if (KeyObjectHandle::HasInstance(env, args[offset])) {
    KeyObjectHandle* key;
    ASSIGN_OR_RETURN_UNWRAP(&key, args[offset], Nothing<void>());
    config->key = key->Data().addRef();
  } else {
    ArrayBufferOrViewContents<char> pass(args[offset]);
    if (!pass.CheckSizeInt32()) [[unlikely]] {
      THROW_ERR_OUT_OF_RANGE(env, "pass is too large");
      return Nothing<void>();
    }
    config->pass = IsCryptoJobAsync(mode) ? pass.ToCopy() : pass.ToByteSource();
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

  const bool isAsync = IsCryptoJobAsync(mode);
  config->salt = isAsync ? salt.ToCopy() : salt.ToByteSource();
  config->secret = isAsync ? secret.ToCopy() : secret.ToByteSource();
  config->ad = isAsync ? ad.ToCopy() : ad.ToByteSource();

  CHECK(args[offset + 2]->IsUint32());  // lanes
  CHECK(args[offset + 3]->IsUint32());  // keylen
  CHECK(args[offset + 4]->IsUint32());  // memcost
  CHECK(args[offset + 5]->IsUint32());  // iter
  CHECK(args[offset + 8]->IsUint32());  // type

  config->lanes = args[offset + 2].As<Uint32>()->Value();
  config->keylen = args[offset + 3].As<Uint32>()->Value();
  config->memcost = args[offset + 4].As<Uint32>()->Value();
  config->iter = args[offset + 5].As<Uint32>()->Value();
  config->type =
      static_cast<ncrypto::Argon2Type>(args[offset + 8].As<Uint32>()->Value());

  return JustVoid();
}

bool Argon2Traits::DeriveBits(Environment* env,
                              const Argon2Config& config,
                              ByteSource* out,
                              CryptoJobMode mode,
                              CryptoErrorStore* errors) {
  // If the config.length is zero-length, just return an empty buffer.
  // It's useless, yes, but allowed via the API.
  if (config.keylen == 0) {
    *out = ByteSource();
    return true;
  }

  // Both the pass and salt may be zero-length at this point
  const ncrypto::Buffer<const char> pass{
      .data = config.key ? config.key.GetSymmetricKey()
                         : config.pass.data<const char>(),
      .len = config.key ? config.key.GetSymmetricKeySize() : config.pass.size(),
  };

  auto dp = ncrypto::argon2(pass,
                            config.salt,
                            config.lanes,
                            config.keylen,
                            config.memcost,
                            config.iter,
                            config.version,
                            config.secret,
                            config.ad,
                            config.type);

  if (!dp) {
    errors->Insert(NodeCryptoError::ARGON2_FAILED);
    return false;
  }
  DCHECK(!dp.isSecure());
  *out = ByteSource::Allocated(dp.release());
  return true;
}

static constexpr auto kTypeArgon2d = ncrypto::Argon2Type::ARGON2D;
static constexpr auto kTypeArgon2i = ncrypto::Argon2Type::ARGON2I;
static constexpr auto kTypeArgon2id = ncrypto::Argon2Type::ARGON2ID;

void Argon2::Initialize(Environment* env, Local<Object> target) {
  Argon2Job::Initialize(env, target);

  NODE_DEFINE_CONSTANT(target, kTypeArgon2d);
  NODE_DEFINE_CONSTANT(target, kTypeArgon2i);
  NODE_DEFINE_CONSTANT(target, kTypeArgon2id);
}

void Argon2::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  Argon2Job::RegisterExternalReferences(registry);
}

}  // namespace node::crypto

#endif
