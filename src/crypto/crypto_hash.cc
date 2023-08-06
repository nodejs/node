#include "crypto/crypto_hash.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "string_bytes.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#include <cstdio>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
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
  MarkPopErrorOnReturn mark_pop_error_on_return;
  CipherPushContext ctx(env);
  EVP_MD_do_all_sorted(
#if OPENSSL_VERSION_MAJOR >= 3
    array_push_back<EVP_MD,
                    EVP_MD_fetch,
                    EVP_MD_free,
                    EVP_get_digestbyname,
                    EVP_MD_get0_name>,
#else
    array_push_back<EVP_MD>,
#endif
    &ctx);
  args.GetReturnValue().Set(ctx.ToJSArray());
}

void Hash::Initialize(Environment* env, Local<Object> target) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<FunctionTemplate> t = NewFunctionTemplate(isolate, New);

  t->InstanceTemplate()->SetInternalFieldCount(Hash::kInternalFieldCount);

  SetProtoMethod(isolate, t, "update", HashUpdate);
  SetProtoMethod(isolate, t, "digest", HashDigest);

  SetConstructorFunction(context, target, "Hash", t);

  SetMethodNoSideEffect(context, target, "getHashes", GetHashes);

  HashJob::Initialize(env, target);

  SetMethodNoSideEffect(
      context, target, "internalVerifyIntegrity", InternalVerifyIntegrity);
}

void Hash::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(HashUpdate);
  registry->Register(HashDigest);
  registry->Register(GetHashes);

  HashJob::RegisterExternalReferences(registry);

  registry->Register(InternalVerifyIntegrity);
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
  return EVP_DigestUpdate(mdctx_.get(), data, len) == 1;
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

    ByteSource::Builder digest(len);

    size_t default_len = EVP_MD_CTX_size(hash->mdctx_.get());
    int ret;
    if (len == default_len) {
      ret = EVP_DigestFinal_ex(
          hash->mdctx_.get(), digest.data<unsigned char>(), &len);
      // The output length should always equal hash->md_len_
      CHECK_EQ(len, hash->md_len_);
    } else {
      ret = EVP_DigestFinalXOF(
          hash->mdctx_.get(), digest.data<unsigned char>(), len);
    }

    if (ret != 1)
      return ThrowCryptoError(env, ERR_get_error());

    hash->digest_ = std::move(digest).release();
  }

  Local<Value> error;
  MaybeLocal<Value> rc = StringBytes::Encode(
      env->isolate(), hash->digest_.data<char>(), len, encoding, &error);
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
                   ctx.get(), params.in.data<char>(), params.in.size()) <= 0)) {
    return false;
  }

  if (LIKELY(params.length > 0)) {
    unsigned int length = params.length;
    ByteSource::Builder buf(length);

    size_t expected = EVP_MD_CTX_size(ctx.get());

    int ret =
        (length == expected)
            ? EVP_DigestFinal_ex(ctx.get(), buf.data<unsigned char>(), &length)
            : EVP_DigestFinalXOF(ctx.get(), buf.data<unsigned char>(), length);

    if (UNLIKELY(ret != 1))
      return false;

    *out = std::move(buf).release();
  }

  return true;
}

void InternalVerifyIntegrity(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(args.Length(), 3);

  CHECK(args[0]->IsString());
  Utf8Value algorithm(env->isolate(), args[0]);

  CHECK(args[1]->IsString() || IsAnyBufferSource(args[1]));
  ByteSource content = ByteSource::FromStringOrBuffer(env, args[1]);

  CHECK(args[2]->IsArrayBufferView());
  ArrayBufferOrViewContents<unsigned char> expected(args[2]);

  const EVP_MD* md_type = EVP_get_digestbyname(*algorithm);
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_size;
  if (md_type == nullptr || EVP_Digest(content.data(),
                                       content.size(),
                                       digest,
                                       &digest_size,
                                       md_type,
                                       nullptr) != 1) {
    return ThrowCryptoError(
        env, ERR_get_error(), "Digest method not supported");
  }

  if (digest_size != expected.size() ||
      CRYPTO_memcmp(digest, expected.data(), digest_size) != 0) {
    Local<Value> error;
    MaybeLocal<Value> rc =
        StringBytes::Encode(env->isolate(),
                            reinterpret_cast<const char*>(digest),
                            digest_size,
                            BASE64,
                            &error);
    if (rc.IsEmpty()) {
      CHECK(!error.IsEmpty());
      env->isolate()->ThrowException(error);
      return;
    }
    args.GetReturnValue().Set(rc.FromMaybe(Local<Value>()));
  }
}

}  // namespace crypto
}  // namespace node
