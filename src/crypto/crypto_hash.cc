#include "crypto/crypto_hash.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "string_bytes.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#if OPENSSL_WITH_EVP_MAC
#include <openssl/core_names.h>
#include <openssl/evp.h>
#endif

#if NCRYPTO_USE_BORINGSSL_EVP_DO_ALL_FALLBACK
#include <openssl/digest.h>
#endif

#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

namespace node {

using ncrypto::DataPointer;
using ncrypto::EVPMDCtxPointer;
using ncrypto::MarkPopErrorOnReturn;
using v8::ArrayBuffer;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Int32;
using v8::Isolate;
using v8::Just;
using v8::JustVoid;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::Nothing;
using v8::Null;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Uint8Array;
using v8::Value;

namespace crypto {
Hash::Hash(Environment* env, Local<Object> wrap) : BaseObject(env, wrap) {
  MakeWeak();
}

void Hash::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("mdctx", mdctx_ ? kSizeOf_EVP_MD_CTX : 0);
  tracker->TrackFieldWithSize("md", digest_ ? md_len_ : 0);
}

#if NCRYPTO_USE_BORINGSSL_EVP_DO_ALL_FALLBACK
struct BoringSSLDigest {
  const EVP_MD* (*get)();
  const char* name;
};

constexpr BoringSSLDigest kBoringSSLDigests[] = {
    {EVP_md4, "md4"},
    {EVP_md5, "md5"},
    {EVP_sha1, "sha1"},
    {EVP_sha224, "sha224"},
    {EVP_sha256, "sha256"},
    {EVP_sha384, "sha384"},
    {EVP_sha512, "sha512"},
    {EVP_sha512_256, "sha512-256"},
};
#endif

#if OPENSSL_VERSION_MAJOR >= 3
void PushAliases(const char* name, void* data) {
  static_cast<std::vector<std::string>*>(data)->push_back(name);
}

EVP_MD* GetCachedMDByID(Environment* env, size_t id) {
  CHECK_LT(id, env->evp_md_cache.size());
  EVP_MD* result = env->evp_md_cache[id].get();
  CHECK_NOT_NULL(result);
  return result;
}

struct MaybeCachedMD {
  EVP_MD* explicit_md = nullptr;
  const EVP_MD* implicit_md = nullptr;
  int32_t cache_id = -1;
};

MaybeCachedMD FetchAndMaybeCacheMD(Environment* env, const char* search_name) {
  const EVP_MD* implicit_md = ncrypto::getDigestByName(search_name);
  if (!implicit_md) return {nullptr, nullptr, -1};

  const char* real_name = EVP_MD_get0_name(implicit_md);
  if (!real_name) return {nullptr, implicit_md, -1};

  auto it = env->alias_to_md_id_map.find(real_name);
  if (it != env->alias_to_md_id_map.end()) {
    size_t id = it->second;
    return {GetCachedMDByID(env, id), implicit_md, static_cast<int32_t>(id)};
  }

  // EVP_*_fetch() does not support alias names, so we need to pass it the
  // real/original algorithm name.
  // We use EVP_*_fetch() as a filter here because it will only return an
  // instance if the algorithm is supported by the public OpenSSL APIs (some
  // algorithms are used internally by OpenSSL and are also passed to this
  // callback).
  EVP_MD* explicit_md = EVP_MD_fetch(nullptr, real_name, nullptr);
  if (!explicit_md) return {nullptr, implicit_md, -1};

  // Cache the EVP_MD* fetched.
  env->evp_md_cache.emplace_back(explicit_md);
  size_t id = env->evp_md_cache.size() - 1;

  // Add all the aliases to the map to speed up next lookup.
  std::vector<std::string> aliases;
  EVP_MD_names_do_all(explicit_md, PushAliases, &aliases);
  for (const auto& alias : aliases) {
    env->alias_to_md_id_map.emplace(alias, id);
  }
  env->alias_to_md_id_map.emplace(search_name, id);

  return {explicit_md, implicit_md, static_cast<int32_t>(id)};
}

void SaveSupportedHashAlgorithmsAndCacheMD(const EVP_MD* md,
                                           const char* from,
                                           const char* to,
                                           void* arg) {
  if (!from) return;
  Environment* env = static_cast<Environment*>(arg);
  auto result = FetchAndMaybeCacheMD(env, from);
  if (result.explicit_md) {
    env->supported_hash_algorithms.push_back(from);
  }
}

#else
void SaveSupportedHashAlgorithms(const EVP_MD* md,
                                 const char* from,
                                 const char* to,
                                 void* arg) {
  if (!from) return;
  Environment* env = static_cast<Environment*>(arg);
  env->supported_hash_algorithms.push_back(from);
}
#endif  // OPENSSL_VERSION_MAJOR >= 3

const std::vector<std::string>& GetSupportedHashAlgorithms(Environment* env) {
  if (env->supported_hash_algorithms.empty()) {
    MarkPopErrorOnReturn mark_pop_error_on_return;
#if NCRYPTO_USE_BORINGSSL_EVP_DO_ALL_FALLBACK
    for (const auto& digest : kBoringSSLDigests) {
      static_cast<void>(digest.get);
      env->supported_hash_algorithms.emplace_back(digest.name);
    }
#elif OPENSSL_VERSION_MAJOR >= 3
    // Since we'll fetch the EVP_MD*, cache them along the way to speed up
    // later lookups instead of throwing them away immediately.
    EVP_MD_do_all_sorted(SaveSupportedHashAlgorithmsAndCacheMD, env);
#else
    EVP_MD_do_all_sorted(SaveSupportedHashAlgorithms, env);
#endif
  }
  return env->supported_hash_algorithms;
}

void Hash::GetHashes(const FunctionCallbackInfo<Value>& args) {
  Local<Context> context = args.GetIsolate()->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);
  const std::vector<std::string>& results = GetSupportedHashAlgorithms(env);

  Local<Value> ret;
  if (ToV8Value(context, results).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void Hash::GetCachedAliases(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = args.GetIsolate()->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);
  size_t size = env->alias_to_md_id_map.size();
  LocalVector<Name> names(isolate);
  LocalVector<Value> values(isolate);
#if OPENSSL_VERSION_MAJOR >= 3
  names.reserve(size);
  values.reserve(size);
  for (auto& [alias, id] : env->alias_to_md_id_map) {
    names.push_back(OneByteString(isolate, alias));
    values.push_back(Uint32::New(isolate, id));
  }
#else
  CHECK(env->alias_to_md_id_map.empty());
#endif
  Local<Value> prototype = Null(isolate);
  Local<Object> result =
      Object::New(isolate, prototype, names.data(), values.data(), size);
  args.GetReturnValue().Set(result);
}

const EVP_MD* GetDigestImplementation(Environment* env,
                                      Local<Value> algorithm,
                                      Local<Value> cache_id_val,
                                      Local<Value> algorithm_cache) {
  CHECK(algorithm->IsString());
  CHECK(cache_id_val->IsInt32());
  CHECK(algorithm_cache->IsObject());

#if OPENSSL_VERSION_MAJOR >= 3
  int32_t cache_id = cache_id_val.As<Int32>()->Value();
  if (cache_id != -1) {  // Alias already cached, return the cached EVP_MD*.
    return GetCachedMDByID(env, cache_id);
  }

  // Only decode the algorithm when we don't have it cached to avoid
  // unnecessary overhead.
  Isolate* isolate = env->isolate();
  Utf8Value utf8(isolate, algorithm);

  auto result = FetchAndMaybeCacheMD(env, *utf8);
  if (result.cache_id != -1) {
    // Add the alias to both C++ side and JS side to speedup the lookup
    // next time.
    env->alias_to_md_id_map.emplace(*utf8, result.cache_id);
    if (algorithm_cache.As<Object>()
            ->Set(isolate->GetCurrentContext(),
                  algorithm,
                  Int32::New(isolate, result.cache_id))
            .IsNothing()) {
      return nullptr;
    }
  }

  return result.explicit_md ? result.explicit_md : result.implicit_md;
#else
  Utf8Value utf8(env->isolate(), algorithm);
  return ncrypto::getDigestByName(*utf8);
#endif
}

void MarkInvalidXofLength() {
#if NCRYPTO_USE_OPENSSL3_PROVIDER
  ERR_raise(ERR_LIB_EVP, EVP_R_NOT_XOF_OR_INVALID_LENGTH);
#else
  EVPerr(EVP_F_EVP_DIGESTFINALXOF, EVP_R_NOT_XOF_OR_INVALID_LENGTH);
#endif
}

// DEP0198 EOL requires XOFs without an OpenSSL-defined default output length
// to fail when outputLength is omitted. OpenSSL 3.4 and later report a digest
// size of 0 for such XOFs, including SHAKE, which had weak historical defaults
// before OpenSSL 3.4. For older OpenSSL versions, identify those resolved
// EVP_MD values explicitly to keep the missing-outputLength error
// version-independent.
#if !OPENSSL_VERSION_PREREQ(3, 4)
bool IsShakeDigest(const EVP_MD* md) {
#if OPENSSL_VERSION_MAJOR >= 3
  return EVP_MD_is_a(md, "SHAKE128") || EVP_MD_is_a(md, "SHAKE256");
#else
  const char* name = OBJ_nid2sn(EVP_MD_type(md));
  return name != nullptr &&
         (strcmp(name, "SHAKE128") == 0 || strcmp(name, "SHAKE256") == 0);
#endif
}
#endif

bool ShouldRejectMissingXofLength(const EVP_MD* md, size_t default_length) {
  if (default_length == 0) return true;

#if !OPENSSL_VERSION_PREREQ(3, 4)
  return IsShakeDigest(md);
#else
  static_cast<void>(md);
  return false;
#endif
}

// crypto.digest(algorithm, algorithmId, algorithmCache,
//               input, outputEncoding, outputEncodingId, outputLength)
void Hash::OneShotDigest(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK_EQ(args.Length(), 7);
  CHECK(args[0]->IsString());                                  // algorithm
  CHECK(args[1]->IsInt32());                                   // algorithmId
  CHECK(args[2]->IsObject());                                  // algorithmCache
  CHECK(args[3]->IsString() || args[3]->IsArrayBufferView());  // input
  CHECK(args[4]->IsString());                                  // outputEncoding
  CHECK(args[5]->IsUint32() || args[5]->IsUndefined());  // outputEncodingId
  CHECK(args[6]->IsUint32() || args[6]->IsUndefined());  // outputLength

  const EVP_MD* md = GetDigestImplementation(env, args[0], args[1], args[2]);
  if (md == nullptr) [[unlikely]] {
    Utf8Value method(isolate, args[0]);
    std::string message =
        "Digest method " + method.ToString() + " is not supported";
    return ThrowCryptoError(env, ERR_get_error(), message.c_str());
  }

  enum encoding output_enc = ParseEncoding(isolate, args[4], args[5], HEX);

  bool is_xof = (EVP_MD_flags(md) & EVP_MD_FLAG_XOF) != 0;
  int output_length = EVP_MD_size(md);

  // This is to cause hash() to fail when an incorrect
  // outputLength option was passed for a non-XOF hash function.
  if (!is_xof && !args[6]->IsUndefined()) {
    output_length = args[6].As<Uint32>()->Value();
    if (output_length != EVP_MD_size(md)) {
      Utf8Value method(isolate, args[0]);
      std::string message =
          "Output length " + std::to_string(output_length) + " is invalid for ";
      message += method.ToString() + ", which does not support XOF";
      return ThrowCryptoError(env, ERR_get_error(), message.c_str());
    }
  } else if (is_xof) {
    if (!args[6]->IsUndefined()) {
      output_length = args[6].As<Uint32>()->Value();
    } else if (ShouldRejectMissingXofLength(md, output_length)) {
      MarkInvalidXofLength();
      return ThrowCryptoError(
          env, ERR_get_error(), "Digest method not supported");
    }
  }

  if (output_length == 0) {
    if (output_enc == BUFFER) {
      Local<Uint8Array> u8;
      if (Buffer::New(isolate, ArrayBuffer::New(isolate, 0), 0, 0)
              .ToLocal(&u8)) {
        args.GetReturnValue().Set(u8);
      }
    } else {
      args.GetReturnValue().Set(String::Empty(isolate));
    }
    return;
  }

  DataPointer output = ([&]() -> DataPointer {
    if (args[3]->IsString()) {
      Utf8Value utf8(isolate, args[3]);
      ncrypto::Buffer<const unsigned char> buf = {
          .data = reinterpret_cast<const unsigned char*>(utf8.out()),
          .len = utf8.length(),
      };
      return is_xof ? ncrypto::xofHashDigest(buf, md, output_length)
                    : ncrypto::hashDigest(buf, md);
    }

    ArrayBufferViewContents<unsigned char> input(args[3]);
    ncrypto::Buffer<const unsigned char> buf = {
        .data = reinterpret_cast<const unsigned char*>(input.data()),
        .len = input.length(),
    };
    return is_xof ? ncrypto::xofHashDigest(buf, md, output_length)
                  : ncrypto::hashDigest(buf, md);
  })();

  if (!output) [[unlikely]] {
    return ThrowCryptoError(env, ERR_get_error());
  }

  Local<Value> ret;
  if (StringBytes::Encode(env->isolate(),
                          static_cast<const char*>(output.get()),
                          output.size(),
                          output_enc)
          .ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
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
  SetMethodNoSideEffect(context, target, "getCachedAliases", GetCachedAliases);
  SetMethodNoSideEffect(context, target, "oneShotDigest", OneShotDigest);

  HashJob::Initialize(env, target);
#if OPENSSL_WITH_EVP_MAC
  CShakeJob::Initialize(env, target);
#endif
}

void Hash::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(HashUpdate);
  registry->Register(HashDigest);
  registry->Register(GetHashes);
  registry->Register(GetCachedAliases);
  registry->Register(OneShotDigest);

  HashJob::RegisterExternalReferences(registry);
#if OPENSSL_WITH_EVP_MAC
  CShakeJob::RegisterExternalReferences(registry);
#endif
}

// new Hash(algorithm, algorithmId, xofLen, algorithmCache)
void Hash::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Maybe<unsigned int> xof_md_len = Nothing<unsigned int>();
  if (!args[1]->IsUndefined()) {
    CHECK(args[1]->IsUint32());
    xof_md_len = Just<unsigned int>(args[1].As<Uint32>()->Value());
  }

  const Hash* orig = nullptr;
  const EVP_MD* md = nullptr;
  if (args[0]->IsObject()) {
    ASSIGN_OR_RETURN_UNWRAP(&orig, args[0].As<Object>());
    CHECK_NOT_NULL(orig);
    md = orig->mdctx_.getDigest();
  } else {
    md = GetDigestImplementation(env, args[0], args[2], args[3]);
  }

  Hash* hash = new Hash(env, args.This());
  if (md == nullptr || !hash->HashInit(md, xof_md_len)) {
    return ThrowCryptoError(env, ERR_get_error(),
                            "Digest method not supported");
  }

  if (orig != nullptr && !orig->mdctx_.copyTo(hash->mdctx_)) {
    return ThrowCryptoError(env, ERR_get_error(), "Digest copy error");
  }
}

bool Hash::HashInit(const EVP_MD* md, Maybe<unsigned int> xof_md_len) {
  mdctx_ = EVPMDCtxPointer::New();
  if (!mdctx_.digestInit(md)) [[unlikely]] {
    mdctx_.reset();
    return false;
  }

  md_len_ = mdctx_.getDigestSize();

  if (mdctx_.hasXofFlag() && !xof_md_len.IsJust() &&
      ShouldRejectMissingXofLength(md, md_len_)) {
    MarkInvalidXofLength();
    mdctx_.reset();
    return false;
  }

  if (xof_md_len.IsJust() && xof_md_len.FromJust() != md_len_) {
    // This is a little hack to cause createHash to fail when an incorrect
    // hashSize option was passed for a non-XOF hash function.
    if (!mdctx_.hasXofFlag()) [[unlikely]] {
      MarkInvalidXofLength();
      mdctx_.reset();
      return false;
    }
    md_len_ = xof_md_len.FromJust();
  }

  return true;
}

bool Hash::HashUpdate(const char* data, size_t len) {
  if (!mdctx_) return false;
  return mdctx_.digestUpdate(ncrypto::Buffer<const void>{
      .data = data,
      .len = len,
  });
}

void Hash::HashUpdate(const FunctionCallbackInfo<Value>& args) {
  Decode<Hash>(args,
               [](Hash* hash,
                  const FunctionCallbackInfo<Value>& args,
                  const char* data,
                  size_t size) {
                 Environment* env = Environment::GetCurrent(args);
                 if (size > INT_MAX) [[unlikely]]
                   return THROW_ERR_OUT_OF_RANGE(env, "data is too long");
                 bool r = hash->HashUpdate(data, size);
                 args.GetReturnValue().Set(r);
               });
}

void Hash::HashDigest(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Hash* hash;
  ASSIGN_OR_RETURN_UNWRAP(&hash, args.This());

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
    auto data = hash->mdctx_.digestFinal(len);
    if (!data) [[unlikely]] {
      return ThrowCryptoError(env, ERR_get_error());
    }
    DCHECK(!data.isSecure());

    hash->digest_ = ByteSource::Allocated(data.release());
  }

  Local<Value> ret;
  if (StringBytes::Encode(
          env->isolate(), hash->digest_.data<char>(), len, encoding)
          .ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
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
  if (IsCryptoJobAsync(mode)) tracker->TrackFieldWithSize("in", in.size());
}

MaybeLocal<Value> HashTraits::EncodeOutput(Environment* env,
                                           const HashConfig& params,
                                           ByteSource* out) {
  return out->ToArrayBuffer(env);
}

Maybe<void> HashTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    HashConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  params->mode = mode;

  CHECK(args[offset]->IsString());  // Hash algorithm
  Utf8Value digest(env->isolate(), args[offset]);
  params->digest = ncrypto::getDigestByName(*digest);
  if (params->digest == nullptr) [[unlikely]] {
    THROW_ERR_CRYPTO_INVALID_DIGEST(env, "Invalid digest: %s", digest);
    return Nothing<void>();
  }

  ArrayBufferOrViewContents<char> data(args[offset + 1]);
  if (!data.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "data is too big");
    return Nothing<void>();
  }
  params->in = IsCryptoJobAsync(mode) ? data.ToCopy() : data.ToByteSource();

  unsigned int expected = EVP_MD_size(params->digest);
  params->length = expected;
  if (args[offset + 2]->IsUint32()) [[unlikely]] {
    // length is expressed in terms of bits
    params->length =
        static_cast<uint32_t>(args[offset + 2].As<Uint32>()->Value()) /
        CHAR_BIT;
    if (params->length != expected) {
      if ((EVP_MD_flags(params->digest) & EVP_MD_FLAG_XOF) == 0) [[unlikely]] {
        THROW_ERR_CRYPTO_INVALID_DIGEST(env, "Digest method not supported");
        return Nothing<void>();
      }
    }
  }

  return JustVoid();
}

bool HashTraits::DeriveBits(Environment* env,
                            const HashConfig& params,
                            ByteSource* out,
                            CryptoJobMode mode,
                            CryptoErrorStore* errors) {
  auto ctx = EVPMDCtxPointer::New();

  if (!ctx.digestInit(params.digest) || !ctx.digestUpdate(params.in))
      [[unlikely]] {
    return false;
  }

  if (params.length > 0) [[likely]] {
    auto data = ctx.digestFinal(params.length);
    if (!data) [[unlikely]]
      return false;

    DCHECK(!data.isSecure());
    *out = ByteSource::Allocated(data.release());
  }

  return true;
}

#if OPENSSL_WITH_EVP_MAC
namespace {

static constexpr std::array<unsigned char, 1> kEmptyString = {};
static constexpr size_t kKeccakKmac128Rate = 168;
static constexpr size_t kKeccakKmac256Rate = 136;
static constexpr size_t kMaxCShakeCustomizationSize = 512;

struct EncodedLength {
  std::array<unsigned char, sizeof(size_t) + 1> data;
  size_t size;
};

struct EncodedStringInput {
  const void* data;
  size_t byte_length;
  size_t bit_length;
};

struct KeccakKmacXof {
  ncrypto::EVPMDCtxPointer ctx;
  size_t rate;
};

size_t EncodedLengthSize(size_t value) {
  size_t size = 1;
  size_t remaining = value;
  while (remaining >>= CHAR_BIT) size++;
  return size + 1;
}

bool AddSize(size_t a, size_t b, size_t* out) {
  if (a > std::numeric_limits<size_t>::max() - b) return false;
  *out = a + b;
  return true;
}

EncodedLength EncodeLength(size_t value, bool left) {
  const size_t value_size = EncodedLengthSize(value) - 1;
  EncodedLength encoded = {{}, value_size + 1};

  if (left) encoded.data[0] = static_cast<unsigned char>(value_size);
  for (size_t n = 0; n < value_size; n++) {
    const size_t shift = CHAR_BIT * (value_size - n - 1);
    encoded.data[(left ? 1 : 0) + n] =
        static_cast<unsigned char>(value >> shift);
  }
  if (!left) encoded.data[value_size] = static_cast<unsigned char>(value_size);

  return encoded;
}

bool DigestUpdate(ncrypto::EVPMDCtxPointer* ctx,
                  const void* data,
                  size_t size) {
  if (size == 0) return true;
  return ctx->digestUpdate(ncrypto::Buffer<const void>{
      .data = data,
      .len = size,
  });
}

bool DigestUpdateZeros(ncrypto::EVPMDCtxPointer* ctx, size_t size) {
  static constexpr std::array<unsigned char, 168> zeros = {};
  while (size > 0) {
    const size_t chunk = std::min(size, zeros.size());
    if (!DigestUpdate(ctx, zeros.data(), chunk)) return false;
    size -= chunk;
  }
  return true;
}

bool EncodedStringSize(size_t byte_length, size_t bit_length, size_t* size) {
  return AddSize(EncodedLengthSize(bit_length), byte_length, size);
}

bool ByteLengthToBitLength(size_t byte_length, size_t* bit_length) {
  if (byte_length > std::numeric_limits<size_t>::max() / CHAR_BIT) {
    return false;
  }
  *bit_length = byte_length * CHAR_BIT;
  return true;
}

KeccakKmacXof NewKeccakKmacXof(bool use_128_bits) {
  // OpenSSL 3.x exposes the cSHAKE/KMAC suffix primitive as KECCAK-KMAC-*.
  const char* digest_name = use_128_bits ? OSSL_DIGEST_NAME_KECCAK_KMAC128
                                         : OSSL_DIGEST_NAME_KECCAK_KMAC256;
  auto digest = std::unique_ptr<EVP_MD, decltype(&EVP_MD_free)>{
      EVP_MD_fetch(nullptr, digest_name, nullptr), EVP_MD_free};
  if (!digest) return {};

  auto ctx = ncrypto::EVPMDCtxPointer::New();
  if (!ctx.digestInit(digest.get())) return {};

  return {
      .ctx = std::move(ctx),
      .rate = use_128_bits ? kKeccakKmac128Rate : kKeccakKmac256Rate,
  };
}

bool ToEncodedStringInput(const void* data,
                          size_t byte_length,
                          EncodedStringInput* input) {
  if (byte_length > 0 && data == nullptr) return false;

  size_t bit_length;
  if (!ByteLengthToBitLength(byte_length, &bit_length)) return false;

  *input = {
      .data = byte_length == 0 ? kEmptyString.data() : data,
      .byte_length = byte_length,
      .bit_length = bit_length,
  };
  return true;
}

bool DigestUpdateEncodedLength(ncrypto::EVPMDCtxPointer* ctx,
                               size_t value,
                               bool left) {
  const EncodedLength encoded = EncodeLength(value, left);
  return DigestUpdate(ctx, encoded.data.data(), encoded.size);
}

bool DigestUpdateEncodedString(ncrypto::EVPMDCtxPointer* ctx,
                               const void* data,
                               size_t byte_length,
                               size_t bit_length) {
  return DigestUpdateEncodedLength(ctx, bit_length, true) &&
         DigestUpdate(ctx, data, byte_length);
}

bool DigestUpdateBytepad(ncrypto::EVPMDCtxPointer* ctx,
                         size_t width,
                         const void* data,
                         size_t byte_length,
                         size_t bit_length,
                         const void* data2 = nullptr,
                         size_t byte_length2 = 0,
                         size_t bit_length2 = 0) {
  if (width == 0) return false;

  size_t encoded_size;
  size_t written = EncodedLengthSize(width);
  if (!EncodedStringSize(byte_length, bit_length, &encoded_size) ||
      !AddSize(written, encoded_size, &written)) {
    return false;
  }
  if (data2 != nullptr) {
    if (!EncodedStringSize(byte_length2, bit_length2, &encoded_size) ||
        !AddSize(written, encoded_size, &written)) {
      return false;
    }
  }

  size_t padded_size;
  if (!AddSize(written, width - 1, &padded_size)) return false;
  padded_size = padded_size / width * width;
  DCHECK_GE(padded_size, written);
  const size_t padding = padded_size - written;

  return DigestUpdateEncodedLength(ctx, width, true) &&
         DigestUpdateEncodedString(ctx, data, byte_length, bit_length) &&
         (data2 == nullptr ||
          DigestUpdateEncodedString(ctx, data2, byte_length2, bit_length2)) &&
         DigestUpdateZeros(ctx, padding);
}

}  // namespace

CShakeConfig::CShakeConfig(CShakeConfig&& other) noexcept
    : mode(other.mode),
      in(std::move(other.in)),
      function_name(std::move(other.function_name)),
      customization(std::move(other.customization)),
      variant(other.variant),
      length(other.length) {}

CShakeConfig& CShakeConfig::operator=(CShakeConfig&& other) noexcept {
  if (&other == this) return *this;
  this->~CShakeConfig();
  return *new (this) CShakeConfig(std::move(other));
}

void CShakeConfig::MemoryInfo(MemoryTracker* tracker) const {
  // If the Job is sync, then the CShakeConfig does not own the data.
  if (IsCryptoJobAsync(mode)) {
    tracker->TrackFieldWithSize("in", in.size());
    tracker->TrackFieldWithSize("function_name", function_name.size());
    tracker->TrackFieldWithSize("customization", customization.size());
  }
}

MaybeLocal<Value> CShakeTraits::EncodeOutput(Environment* env,
                                             const CShakeConfig& params,
                                             ByteSource* out) {
  return out->ToArrayBuffer(env);
}

Maybe<void> CShakeTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    CShakeConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  params->mode = mode;

  CHECK(args[offset]->IsString());  // Algorithm name
  Utf8Value algorithm_name(env->isolate(), args[offset]);
  std::string_view algorithm_str = algorithm_name.ToStringView();

  if (algorithm_str == "cSHAKE128") {
    params->variant = CShakeVariant::CSHAKE128;
  } else if (algorithm_str == "cSHAKE256") {
    params->variant = CShakeVariant::CSHAKE256;
  } else {
    UNREACHABLE();
  }

  ArrayBufferOrViewContents<char> data(args[offset + 1]);
  if (!data.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "data is too big");
    return Nothing<void>();
  }
  params->in = IsCryptoJobAsync(mode) ? data.ToCopy() : data.ToByteSource();

  if (!args[offset + 2]->IsUndefined()) {
    ArrayBufferOrViewContents<char> function_name(args[offset + 2]);
    if (!function_name.CheckSizeInt32()) [[unlikely]] {
      THROW_ERR_OUT_OF_RANGE(env, "functionName is too big");
      return Nothing<void>();
    }
    params->function_name = IsCryptoJobAsync(mode)
                                ? function_name.ToCopy()
                                : function_name.ToByteSource();
  }

  if (!args[offset + 3]->IsUndefined()) {
    ArrayBufferOrViewContents<char> customization(args[offset + 3]);
    if (!customization.CheckSizeInt32()) [[unlikely]] {
      THROW_ERR_OUT_OF_RANGE(env, "customization is too big");
      return Nothing<void>();
    }
    params->customization = IsCryptoJobAsync(mode)
                                ? customization.ToCopy()
                                : customization.ToByteSource();
  }

  CHECK(args[offset + 4]->IsUint32());  // Length
  params->length = args[offset + 4].As<Uint32>()->Value();

  return JustVoid();
}

bool CShakeTraits::DeriveBits(Environment* env,
                              const CShakeConfig& params,
                              ByteSource* out,
                              CryptoJobMode mode,
                              CryptoErrorStore*) {
  CShakeParams cshake_params = {
      .variant = params.variant,
      .function_name_data = params.function_name.data(),
      .function_name_size = params.function_name.size(),
      .customization_data = params.customization.data(),
      .customization_size = params.customization.size(),
      .bytepad_input = nullptr,
      .input_data = params.in.data(),
      .input_size = params.in.size(),
      .append_output_length = false,
      .length = params.length,
  };
  return DeriveCShakeBits(cshake_params, out);
}

bool DeriveCShakeBits(const CShakeParams& params, ByteSource* out) {
  if (params.customization_size > kMaxCShakeCustomizationSize) {
    return false;
  }

  if (params.length == 0) {
    *out = ByteSource();
    return true;
  }

  auto xof = NewKeccakKmacXof(params.variant == CShakeVariant::CSHAKE128);
  if (!xof.ctx) return false;
  auto ctx = std::move(xof.ctx);

  EncodedStringInput function_name;
  EncodedStringInput customization;
  if (!ToEncodedStringInput(params.function_name_data,
                            params.function_name_size,
                            &function_name) ||
      !ToEncodedStringInput(params.customization_data,
                            params.customization_size,
                            &customization)) {
    return false;
  }

  if (!DigestUpdateBytepad(&ctx,
                           xof.rate,
                           function_name.data,
                           function_name.byte_length,
                           function_name.bit_length,
                           customization.data,
                           customization.byte_length,
                           customization.bit_length)) {
    return false;
  }

  if (params.bytepad_input != nullptr &&
      !DigestUpdateBytepad(&ctx,
                           xof.rate,
                           params.bytepad_input->data,
                           params.bytepad_input->byte_length,
                           params.bytepad_input->bit_length)) {
    return false;
  }

  if (!DigestUpdate(&ctx, params.input_data, params.input_size)) {
    return false;
  }

  if (params.append_output_length &&
      !DigestUpdateEncodedLength(&ctx, params.length, false)) {
    return false;
  }

  const size_t length_bytes =
      NumBitsToBytes(static_cast<size_t>(params.length));
  auto data = ctx.digestFinal(length_bytes);
  if (!data) [[unlikely]]
    return false;

  DCHECK(!data.isSecure());
  *out = ByteSource::Allocated(data.release());
  if (params.length % CHAR_BIT != 0) TruncateToBitLength(params.length, out);
  return true;
}
#endif  // OPENSSL_WITH_EVP_MAC

}  // namespace crypto
}  // namespace node
