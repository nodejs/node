#include "crypto/crypto_hash.h"
#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "string_bytes.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#include <cstdio>

namespace node {

using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Uint32;
using v8::Value;

namespace crypto {
Hash::Hash(Environment* env, Local<Object> wrap) : BaseObject(env, wrap) {
  MakeWeak();
}

void Hash::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("mdctx", mdctx_ ? kSizeOf_EVP_MD_CTX : 0);
  tracker->TrackFieldWithSize("md", digest_ ? md_len_ : 0);
}

void Hash::GetHashes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CipherPushContext ctx(env);
  EVP_MD_do_all_sorted(array_push_back<EVP_MD>, &ctx);
  args.GetReturnValue().Set(ctx.ToJSArray());
}

void Hash::Initialize(Environment* env, Local<Object> target) {
  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  t->InstanceTemplate()->SetInternalFieldCount(
      Hash::kInternalFieldCount);
  t->Inherit(BaseObject::GetConstructorTemplate(env));

  env->SetProtoMethod(t, "update", HashUpdate);
  env->SetProtoMethod(t, "digest", HashDigest);

  env->SetConstructorFunction(target, "Hash", t);

  env->SetMethodNoSideEffect(target, "getHashes", GetHashes);

  HashJob::Initialize(env, target);
}

void Hash::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const Hash* orig = nullptr;
  const EVP_MD* md = nullptr;

  if (args[0]->IsObject()) {
    ASSIGN_OR_RETURN_UNWRAP(&orig, args[0].As<Object>());
    md = EVP_MD_CTX_md(orig->mdctx_.get());
  } else {
    const Utf8Value hash_type(env->isolate(), args[0]);
    md = EVP_get_digestbyname(*hash_type);
  }

  Maybe<unsigned int> xof_md_len = Nothing<unsigned int>();
  if (!args[1]->IsUndefined()) {
    CHECK(args[1]->IsUint32());
    xof_md_len = Just<unsigned int>(args[1].As<Uint32>()->Value());
  }

  Hash* hash = new Hash(env, args.This());
  if (md == nullptr || !hash->HashInit(md, xof_md_len)) {
    return ThrowCryptoError(env, ERR_get_error(),
                            "Digest method not supported");
  }

  if (orig != nullptr &&
      0 >= EVP_MD_CTX_copy(hash->mdctx_.get(), orig->mdctx_.get())) {
    return ThrowCryptoError(env, ERR_get_error(), "Digest copy error");
  }
}

bool Hash::HashInit(const EVP_MD* md, Maybe<unsigned int> xof_md_len) {
  mdctx_.reset(EVP_MD_CTX_new());
  if (!mdctx_ || EVP_DigestInit_ex(mdctx_.get(), md, nullptr) <= 0) {
    mdctx_.reset();
    return false;
  }

  md_len_ = EVP_MD_size(md);
  if (xof_md_len.IsJust() && xof_md_len.FromJust() != md_len_) {
    // This is a little hack to cause createHash to fail when an incorrect
    // hashSize option was passed for a non-XOF hash function.
    if ((EVP_MD_flags(md) & EVP_MD_FLAG_XOF) == 0) {
      EVPerr(EVP_F_EVP_DIGESTFINALXOF, EVP_R_NOT_XOF_OR_INVALID_LENGTH);
      return false;
    }
    md_len_ = xof_md_len.FromJust();
  }

  return true;
}

bool Hash::HashUpdate(const char* data, size_t len) {
  if (!mdctx_)
    return false;
  EVP_DigestUpdate(mdctx_.get(), data, len);
  return true;
}

void Hash::HashUpdate(const FunctionCallbackInfo<Value>& args) {
  Decode<Hash>(args, [](Hash* hash, const FunctionCallbackInfo<Value>& args,
                        const char* data, size_t size) {
    Environment* env = Environment::GetCurrent(args);
    if (UNLIKELY(size > INT_MAX))
      return THROW_ERR_OUT_OF_RANGE(env, "data is too long");
    bool r = hash->HashUpdate(data, size);
    args.GetReturnValue().Set(r);
  });
}

void Hash::HashDigest(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Hash* hash;
  ASSIGN_OR_RETURN_UNWRAP(&hash, args.Holder());

  enum encoding encoding = BUFFER;
  if (args.Length() >= 1) {
    encoding = ParseEncoding(env->isolate(), args[0], BUFFER);
  }

  unsigned int len = hash->md_len_;

  // TODO(tniessen): SHA3_squeeze does not work for zero-length outputs on all
  // platforms and will cause a segmentation fault if called. This workaround
  // causes hash.digest() to correctly return an empty buffer / string.
  // See https://github.com/openssl/openssl/issues/9431.

  if (!hash->digest_ && len > 0) {
    // Some hash algorithms such as SHA3 do not support calling
    // EVP_DigestFinal_ex more than once, however, Hash._flush
    // and Hash.digest can both be used to retrieve the digest,
    // so we need to cache it.
    // See https://github.com/nodejs/node/issues/28245.

    char* md_value = MallocOpenSSL<char>(len);
    ByteSource digest = ByteSource::Allocated(md_value, len);

    size_t default_len = EVP_MD_CTX_size(hash->mdctx_.get());
    int ret;
    if (len == default_len) {
      ret = EVP_DigestFinal_ex(
          hash->mdctx_.get(),
          reinterpret_cast<unsigned char*>(md_value),
          &len);
      // The output length should always equal hash->md_len_
      CHECK_EQ(len, hash->md_len_);
    } else {
      ret = EVP_DigestFinalXOF(
          hash->mdctx_.get(),
          reinterpret_cast<unsigned char*>(md_value),
          len);
    }

    if (ret != 1)
      return ThrowCryptoError(env, ERR_get_error());

    hash->digest_ = std::move(digest);
  }

  Local<Value> error;
  MaybeLocal<Value> rc =
      StringBytes::Encode(env->isolate(),
                          hash->digest_.get(),
                          len,
                          encoding,
                          &error);
  if (rc.IsEmpty()) {
    CHECK(!error.IsEmpty());
    env->isolate()->ThrowException(error);
    return;
  }
  args.GetReturnValue().Set(rc.FromMaybe(Local<Value>()));
}

HashConfig::HashConfig(HashConfig&& other) noexcept
    : mode(other.mode),
      in(std::move(other.in)),
      digest(other.digest),
      length(other.length) {}

HashConfig& HashConfig::operator=(HashConfig&& other) noexcept {
  if (&other == this) return *this;
  this->~HashConfig();
  return *new (this) HashConfig(std::move(other));
}

void HashConfig::MemoryInfo(MemoryTracker* tracker) const {
  // If the Job is sync, then the HashConfig does not own the data.
  if (mode == kCryptoJobAsync)
    tracker->TrackFieldWithSize("in", in.size());
}

Maybe<bool> HashTraits::EncodeOutput(
    Environment* env,
    const HashConfig& params,
    ByteSource* out,
    v8::Local<v8::Value>* result) {
  *result = out->ToArrayBuffer(env);
  return Just(!result->IsEmpty());
}

Maybe<bool> HashTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    HashConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  params->mode = mode;

  CHECK(args[offset]->IsString());  // Hash algorithm
  Utf8Value digest(env->isolate(), args[offset]);
  params->digest = EVP_get_digestbyname(*digest);
  if (UNLIKELY(params->digest == nullptr)) {
    THROW_ERR_CRYPTO_INVALID_DIGEST(env, "Invalid digest: %s", *digest);
    return Nothing<bool>();
  }

  ArrayBufferOrViewContents<char> data(args[offset + 1]);
  if (UNLIKELY(!data.CheckSizeInt32())) {
    THROW_ERR_OUT_OF_RANGE(env, "data is too big");
    return Nothing<bool>();
  }
  params->in = mode == kCryptoJobAsync
      ? data.ToCopy()
      : data.ToByteSource();

  unsigned int expected = EVP_MD_size(params->digest);
  params->length = expected;
  if (UNLIKELY(args[offset + 2]->IsUint32())) {
    // length is expressed in terms of bits
    params->length =
        static_cast<uint32_t>(args[offset + 2]
            .As<Uint32>()->Value()) / CHAR_BIT;
    if (params->length != expected) {
      if ((EVP_MD_flags(params->digest) & EVP_MD_FLAG_XOF) == 0) {
        THROW_ERR_CRYPTO_INVALID_DIGEST(env, "Digest method not supported");
        return Nothing<bool>();
      }
    }
  }

  return Just(true);
}

bool HashTraits::DeriveBits(
    Environment* env,
    const HashConfig& params,
    ByteSource* out) {
  EVPMDPointer ctx(EVP_MD_CTX_new());

  if (UNLIKELY(!ctx ||
               EVP_DigestInit_ex(ctx.get(), params.digest, nullptr) <= 0 ||
               EVP_DigestUpdate(
                   ctx.get(),
                   params.in.get(),
                   params.in.size()) <= 0)) {
    return false;
  }

  if (LIKELY(params.length > 0)) {
    unsigned int length = params.length;
    char* data = MallocOpenSSL<char>(length);
    ByteSource buf = ByteSource::Allocated(data, length);
    unsigned char* ptr = reinterpret_cast<unsigned char*>(data);

    size_t expected = EVP_MD_CTX_size(ctx.get());

    int ret = (length == expected)
        ? EVP_DigestFinal_ex(ctx.get(), ptr, &length)
        : EVP_DigestFinalXOF(ctx.get(), ptr, length);

    if (UNLIKELY(ret != 1))
      return false;

    *out = std::move(buf);
  }

  return true;
}

}  // namespace crypto
}  // namespace node
