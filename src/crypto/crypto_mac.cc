#include "crypto/crypto_mac.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "string_bytes.h"
#include "v8.h"

#if OPENSSL_WITH_EVP_MAC
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#endif

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::Name;
using v8::Null;
using v8::Object;
using v8::Uint32;
using v8::Value;

namespace crypto {

#if OPENSSL_WITH_EVP_MAC
namespace {

struct InitializedMac final {
  ncrypto::EVPMacCtxPointer context;
  size_t output_size = 0;
  bool has_output_length = false;
};

struct MaybeCachedMac final {
  EVP_MAC* cached_mac = nullptr;
  ncrypto::EVPMacPointer mac;
  int32_t cache_id = -1;
  ncrypto::MacKind kind = ncrypto::MacKind::kOther;
};

bool MaybeThrowCryptoError(Environment* env, const char* message) {
  const unsigned long error = ERR_get_error();  // NOLINT(runtime/int)
  if (error == 0) return false;
  ThrowCryptoError(env, error, message);
  return true;
}

void ThrowMacError(Environment* env, const char* message) {
  if (!MaybeThrowCryptoError(env, message)) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, message);
  }
}

void ResetMacCache(Environment* env,
                   uint64_t generation,
                   Local<Object> algorithm_cache = Local<Object>()) {
  ncrypto::MacCache* cache = env->provider_mac_cache.get();
  CHECK_NOT_NULL(cache);
  if (!algorithm_cache.IsEmpty()) {
    Isolate* isolate = env->isolate();
    Local<Context> context = env->context();
    for (const auto& entry : cache->aliases()) {
      if (algorithm_cache
              ->Set(context,
                    OneByteString(isolate, entry.first),
                    Int32::New(isolate, -1))
              .IsNothing()) {
        return;
      }
    }
  }
  cache->reset(generation);
  env->supported_mac_algorithms.clear();
  env->supported_mac_algorithms_initialized = false;
  env->mac_cache_generation = generation;
}

bool SynchronizeMacCache(Environment* env,
                         Local<Object> algorithm_cache = Local<Object>()) {
  const uint64_t generation = ncrypto::getFipsStateGeneration();
  if (env->mac_cache_generation == generation) return false;
  ResetMacCache(env, generation, algorithm_cache);
  return true;
}

ncrypto::MacCache::Result GetCachedMacByID(
    Environment* env,
    int32_t id,
    Local<Object> algorithm_cache = Local<Object>()) {
  if (SynchronizeMacCache(env, algorithm_cache) ||
      env->provider_mac_cache == nullptr) {
    return {};
  }
  return env->provider_mac_cache->lookup(id, env->mac_cache_generation);
}

MaybeCachedMac FetchAndMaybeCacheMac(
    Environment* env,
    const char* name,
    Local<Object> algorithm_cache = Local<Object>()) {
  SynchronizeMacCache(env, algorithm_cache);
  if (env->isolate()->HasPendingException()) return {};
  ncrypto::MacCache* cache = env->provider_mac_cache.get();
  CHECK_NOT_NULL(cache);
  const uint64_t generation = env->mac_cache_generation;

  if (auto cached = cache->lookup(name, generation); cached.mac != nullptr) {
    return {cached.mac, {}, cached.id, cached.kind};
  }

  ncrypto::EVPMacPointer mac;
  {
    ncrypto::MarkPopErrorOnReturn mark_pop_error_on_return;
    mac = ncrypto::EVPMacPointer::Fetch(name);
  }
  if (!mac) return {};

  if (generation == ncrypto::getFipsStateGeneration()) {
    auto cached = cache->insert(name, std::move(mac), generation);
    if (cached.mac != nullptr) {
      return {cached.mac, {}, cached.id, cached.kind};
    }
  }

  const ncrypto::MacKind kind = ncrypto::MacCache::GetKind(mac.get());
  return {nullptr, std::move(mac), -1, kind};
}

EVP_MAC* GetMacImplementation(Environment* env,
                              Local<Value> algorithm,
                              Local<Value> cache_id_value,
                              Local<Value> algorithm_cache,
                              ncrypto::EVPMacPointer* mac_owner,
                              ncrypto::MacKind* kind) {
  CHECK(algorithm->IsString());
  CHECK(cache_id_value->IsInt32());
  CHECK(algorithm_cache->IsObject());
  CHECK_NOT_NULL(mac_owner);
  CHECK_NOT_NULL(kind);

  Local<Object> cache = algorithm_cache.As<Object>();
  const int32_t cache_id = cache_id_value.As<Int32>()->Value();
  if (cache_id != -1) {
    auto cached = GetCachedMacByID(env, cache_id, cache);
    if (cached.mac != nullptr) {
      *kind = cached.kind;
      return cached.mac;
    }
    if (env->isolate()->HasPendingException()) return nullptr;
  }

  Isolate* isolate = env->isolate();
  Utf8Value utf8(isolate, algorithm);
  MaybeCachedMac result = FetchAndMaybeCacheMac(env, *utf8, cache);
  if (env->isolate()->HasPendingException()) return nullptr;
  if (result.cache_id != -1) {
    if (cache
            ->Set(
                env->context(), algorithm, Int32::New(isolate, result.cache_id))
            .IsNothing()) {
      return nullptr;
    }
  }

  if (result.cached_mac != nullptr) {
    *kind = result.kind;
    return result.cached_mac;
  }
  if (result.mac) {
    *mac_owner = std::move(result.mac);
    *kind = result.kind;
    return mac_owner->get();
  }
  return nullptr;
}

bool IsSettableParameter(const ncrypto::EVPMacCtxPointer& context,
                         const char* name,
                         unsigned int type) {
  const OSSL_PARAM* settable = context.getSettableParams();
  const OSSL_PARAM* descriptor =
      settable == nullptr ? nullptr : OSSL_PARAM_locate_const(settable, name);
  return descriptor != nullptr && descriptor->data_type == type;
}

bool RequireSettableParameter(Environment* env,
                              const ncrypto::EVPMacCtxPointer& context,
                              Local<Value> algorithm,
                              const char* option,
                              const char* parameter,
                              unsigned int type) {
  if (IsSettableParameter(context, parameter, type)) return true;
  Utf8Value name(env->isolate(), algorithm);
  THROW_ERR_INVALID_ARG_VALUE(
      env,
      "The property 'options.%s' is not supported by MAC %s",
      option,
      *name);
  return false;
}

bool InitializeMacContext(Environment* env,
                          const FunctionCallbackInfo<Value>& args,
                          InitializedMac* output) {
  CHECK(args.Length() == 5 || args.Length() == 10);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsObject());

  Isolate* isolate = env->isolate();
  ByteSource key = ByteSource::FromSecretKeyBytes(env, args[3]);

  std::optional<Utf8Value> digest;
  if (!args[4]->IsUndefined()) {
    CHECK(args[4]->IsString());
    digest.emplace(isolate, args[4]);
  }
  std::optional<Utf8Value> cipher;
  if (!args[5]->IsUndefined()) {
    CHECK(args[5]->IsString());
    cipher.emplace(isolate, args[5]);
  }

  const bool has_iv = !args[6]->IsUndefined();
  ByteSource iv;
  if (has_iv) iv = ByteSource::FromBuffer(args[6]);
  const bool has_customization = !args[7]->IsUndefined();
  ByteSource customization;
  if (has_customization) customization = ByteSource::FromBuffer(args[7]);
  const bool has_salt = !args[8]->IsUndefined();
  ByteSource salt;
  if (has_salt) salt = ByteSource::FromBuffer(args[8]);

  const bool has_output_length = !args[9]->IsUndefined();
  size_t output_length = 0;
  if (has_output_length) {
    CHECK(args[9]->IsUint32());
    output_length = args[9].As<Uint32>()->Value();
  }

  ncrypto::EVPMacPointer mac_owner;
  ncrypto::MacKind kind = ncrypto::MacKind::kOther;
  EVP_MAC* mac =
      GetMacImplementation(env, args[0], args[1], args[2], &mac_owner, &kind);
  if (mac == nullptr) {
    if (env->isolate()->IsExecutionTerminating() ||
        env->isolate()->HasPendingException()) {
      return false;
    }
    Utf8Value name(env->isolate(), args[0]);
    THROW_ERR_CRYPTO_INVALID_MAC(env, "Invalid MAC: %s", *name);
    return false;
  }

  if (kind == ncrypto::MacKind::kHmac && !digest.has_value()) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "The property 'options.digest' is required for HMAC");
    return false;
  }
  if (kind == ncrypto::MacKind::kCmac && !cipher.has_value()) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "The property 'options.cipher' is required for CMAC");
    return false;
  }
  if (kind == ncrypto::MacKind::kGmac) {
    if (!cipher.has_value()) {
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The property 'options.cipher' is required for GMAC");
      return false;
    }
    if (!has_iv || iv.empty()) {
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The property 'options.iv' must be non-empty for GMAC");
      return false;
    }
  }

  ncrypto::EVPMacCtxPointer context = ncrypto::EVPMacCtxPointer::New(mac);
  if (!context) {
    ThrowMacError(env, "Failed to create MAC context");
    return false;
  }

  std::array<OSSL_PARAM, 7> params;
  size_t count = 0;
  if (digest.has_value()) {
    if (!RequireSettableParameter(env,
                                  context,
                                  args[0],
                                  "digest",
                                  OSSL_MAC_PARAM_DIGEST,
                                  OSSL_PARAM_UTF8_STRING)) {
      return false;
    }
    params[count++] = OSSL_PARAM_construct_utf8_string(
        OSSL_MAC_PARAM_DIGEST, const_cast<char*>(digest->out()), 0);
  }
  if (cipher.has_value()) {
    if (!RequireSettableParameter(env,
                                  context,
                                  args[0],
                                  "cipher",
                                  OSSL_MAC_PARAM_CIPHER,
                                  OSSL_PARAM_UTF8_STRING)) {
      return false;
    }
    params[count++] = OSSL_PARAM_construct_utf8_string(
        OSSL_MAC_PARAM_CIPHER, const_cast<char*>(cipher->out()), 0);
  }

  unsigned char empty_parameter = 0;
  auto add_bytes = [&](bool present,
                       const ByteSource& value,
                       const char* option,
                       const char* parameter) {
    if (!present) return true;
    if (!RequireSettableParameter(env,
                                  context,
                                  args[0],
                                  option,
                                  parameter,
                                  OSSL_PARAM_OCTET_STRING)) {
      return false;
    }
    void* data = value.empty() ? static_cast<void*>(&empty_parameter)
                               : const_cast<void*>(value.data());
    params[count++] =
        OSSL_PARAM_construct_octet_string(parameter, data, value.size());
    return true;
  };

  if (!add_bytes(has_iv, iv, "iv", OSSL_MAC_PARAM_IV) ||
      !add_bytes(has_customization,
                 customization,
                 "customization",
                 OSSL_MAC_PARAM_CUSTOM) ||
      !add_bytes(has_salt, salt, "salt", OSSL_MAC_PARAM_SALT)) {
    return false;
  }

  if (has_output_length) {
    if (output_length > Buffer::kMaxLength) {
      env->isolate()->ThrowException(ERR_BUFFER_TOO_LARGE(env->isolate()));
      return false;
    }
    if (!RequireSettableParameter(env,
                                  context,
                                  args[0],
                                  "outputLength",
                                  OSSL_MAC_PARAM_SIZE,
                                  OSSL_PARAM_UNSIGNED_INTEGER)) {
      return false;
    }
    params[count++] =
        OSSL_PARAM_construct_size_t(OSSL_MAC_PARAM_SIZE, &output_length);
  }
  params[count] = OSSL_PARAM_construct_end();

  if (!context.init(key, params.data())) {
    ThrowMacError(env, "Failed to initialize MAC");
    return false;
  }

  const size_t output_size = context.getSize();
  if (output_size > Buffer::kMaxLength) {
    ERR_clear_error();
    env->isolate()->ThrowException(ERR_BUFFER_TOO_LARGE(env->isolate()));
    return false;
  }
  if (has_output_length && output_size != output_length) {
    ERR_clear_error();
    Utf8Value name(env->isolate(), args[0]);
    THROW_ERR_INVALID_ARG_VALUE(
        env,
        "The property 'options.outputLength' was not honored by MAC %s",
        *name);
    return false;
  }
  if (!has_output_length && output_size == 0) {
    ERR_clear_error();
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "MAC did not report an output size");
    return false;
  }

  output->context = std::move(context);
  output->output_size = output_size;
  output->has_output_length = has_output_length;
  return true;
}

v8::MaybeLocal<Value> FinalizeMac(Environment* env,
                                  ncrypto::EVPMacCtxPointer* context,
                                  size_t output_size,
                                  bool has_output_length,
                                  enum encoding encoding) {
  ncrypto::DataPointer result = context->final(output_size);
  if (!result) {
    ThrowMacError(env, "Failed to finalize MAC");
    context->reset();
    return {};
  }
  context->reset();
  if (has_output_length && result.size() != output_size) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "MAC returned an unexpected output length");
    return {};
  }
  if (encoding != BUFFER) {
    return StringBytes::Encode(env->isolate(),
                               static_cast<const char*>(result.get()),
                               result.size(),
                               encoding);
  }
  if (result.size() == 0) return Buffer::New(env, 0);

  ByteSource bytes = ByteSource::Allocated(result.release());
  return bytes.ToBuffer(env);
}

void SaveMacName(const char* name, void* arg) {
  if (name == nullptr) return;
  const std::string_view view(name);
  const bool is_dotted_decimal =
      view.find('.') != std::string_view::npos &&
      std::all_of(view.begin(), view.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || c == '.';
      });
  if (is_dotted_decimal) return;

  std::string normalized(view);
  std::transform(normalized.begin(),
                 normalized.end(),
                 normalized.begin(),
                 [](unsigned char c) {
                   if (c >= 'A' && c <= 'Z') {
                     return static_cast<char>(c + ('a' - 'A'));
                   }
                   return static_cast<char>(c);
                 });
  static_cast<Environment*>(arg)->supported_mac_algorithms.push_back(
      std::move(normalized));
}

void SaveSupportedProviderMac(EVP_MAC* enumerated, void* arg) {
  Environment* env = static_cast<Environment*>(arg);
  const char* name = EVP_MAC_get0_name(enumerated);
  if (name == nullptr) return;

  MaybeCachedMac result = FetchAndMaybeCacheMac(env, name);
  EVP_MAC* fetched =
      result.cached_mac != nullptr ? result.cached_mac : result.mac.get();
  if (fetched == nullptr) return;
  EVP_MAC_names_do_all(fetched, SaveMacName, env);
}

const std::vector<std::string>& GetSupportedMacAlgorithms(Environment* env) {
  while (true) {
    SynchronizeMacCache(env);
    const uint64_t generation = env->mac_cache_generation;
    if (!env->supported_mac_algorithms_initialized) {
      ncrypto::MarkPopErrorOnReturn mark_pop_error_on_return;
      EVP_MAC_do_all_provided(nullptr, SaveSupportedProviderMac, env);
      std::sort(env->supported_mac_algorithms.begin(),
                env->supported_mac_algorithms.end());
      env->supported_mac_algorithms.erase(
          std::unique(env->supported_mac_algorithms.begin(),
                      env->supported_mac_algorithms.end()),
          env->supported_mac_algorithms.end());
      env->supported_mac_algorithms_initialized = true;
    }

    const uint64_t current_generation = ncrypto::getFipsStateGeneration();
    if (generation == current_generation) {
      return env->supported_mac_algorithms;
    }
    ResetMacCache(env, current_generation);
  }
}

}  // namespace
#endif  // OPENSSL_WITH_EVP_MAC

#if OPENSSL_WITH_EVP_MAC
bool Mac::MacUpdate(const char* data, size_t length) {
  if (!context_) return false;
  if (length == 0) return true;
  if (EVP_MAC_update(context_.get(),
                     reinterpret_cast<const unsigned char*>(data),
                     length) == 1) {
    return true;
  }
  MaybeThrowCryptoError(env(), "Failed to update MAC");
  context_.reset();
  return false;
}

Mac::Mac(Environment* env,
         Local<Object> wrap,
         ncrypto::EVPMacCtxPointer&& context,
         size_t output_size,
         bool has_output_length)
    : BaseObject(env, wrap),
      context_(std::move(context)),
      output_size_(output_size),
      has_output_length_(has_output_length) {
  MakeWeak();
}
#else
Mac::Mac(Environment* env, Local<Object> wrap) : BaseObject(env, wrap) {
  MakeWeak();
}
#endif

void Mac::MemoryInfo(MemoryTracker* tracker) const {
#if OPENSSL_WITH_EVP_MAC
  tracker->TrackFieldWithSize("context", context_ ? kSizeOf_EVP_MAC_CTX : 0);
#else
  static_cast<void>(tracker);
#endif
}

void Mac::Initialize(Environment* env, Local<Object> target) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<FunctionTemplate> t = NewFunctionTemplate(isolate, New);
  t->InstanceTemplate()->SetInternalFieldCount(Mac::kInternalFieldCount);
  SetProtoMethod(isolate, t, "update", MacUpdate);
  SetProtoMethod(isolate, t, "final", MacFinal);
  SetConstructorFunction(context, target, "Mac", t);

  SetMethodNoSideEffect(context, target, "getMacs", GetMacs);
  SetMethodNoSideEffect(
      context, target, "getCachedMacAliases", GetCachedAliases);
}

void Mac::RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(MacUpdate);
  registry->Register(MacFinal);
  registry->Register(GetMacs);
  registry->Register(GetCachedAliases);
}

void Mac::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
#if OPENSSL_WITH_EVP_MAC
  CHECK(args.Length() == 5 || args.Length() == 10);
  Environment* env = Environment::GetCurrent(args);
  InitializedMac initialized;
  if (!InitializeMacContext(env, args, &initialized)) return;
  new Mac(env,
          args.This(),
          std::move(initialized.context),
          initialized.output_size,
          initialized.has_output_length);
#else
  THROW_ERR_CRYPTO_MAC_NOT_SUPPORTED(Environment::GetCurrent(args),
                                     "MAC is not supported");
#endif
}

void Mac::MacUpdate(const FunctionCallbackInfo<Value>& args) {
#if OPENSSL_WITH_EVP_MAC
  Decode<Mac>(args,
              [](Mac* mac,
                 const FunctionCallbackInfo<Value>& args,
                 const char* data,
                 size_t length) {
                args.GetReturnValue().Set(mac->MacUpdate(data, length));
              });
#else
  THROW_ERR_CRYPTO_MAC_NOT_SUPPORTED(Environment::GetCurrent(args),
                                     "MAC is not supported");
#endif
}

void Mac::MacFinal(const FunctionCallbackInfo<Value>& args) {
#if OPENSSL_WITH_EVP_MAC
  Mac* mac;
  ASSIGN_OR_RETURN_UNWRAP(&mac, args.This());
  Environment* env = mac->env();
  if (!mac->context_) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "MAC context is not initialized");
    return;
  }
  enum encoding encoding = BUFFER;
  if (args.Length() >= 1) {
    encoding = ParseEncoding(env->isolate(), args[0], BUFFER);
  }
  Local<Value> result;
  if (FinalizeMac(env,
                  &mac->context_,
                  mac->output_size_,
                  mac->has_output_length_,
                  encoding)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
#else
  THROW_ERR_CRYPTO_MAC_NOT_SUPPORTED(Environment::GetCurrent(args),
                                     "MAC is not supported");
#endif
}

void Mac::GetMacs(const FunctionCallbackInfo<Value>& args) {
#if OPENSSL_WITH_EVP_MAC
  Local<Context> context = args.GetIsolate()->GetCurrentContext();
  Environment* env = Environment::GetCurrent(context);
  Local<Value> result;
  if (ToV8Value(context, GetSupportedMacAlgorithms(env)).ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
#else
  args.GetReturnValue().Set(Array::New(args.GetIsolate(), 0));
#endif
}

void Mac::GetCachedAliases(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  LocalVector<Name> names(isolate);
  LocalVector<Value> values(isolate);
#if OPENSSL_WITH_EVP_MAC
  Environment* env = Environment::GetCurrent(args);
  SynchronizeMacCache(env);
  const auto& aliases = env->provider_mac_cache->aliases();
  names.reserve(aliases.size());
  values.reserve(aliases.size());
  for (const auto& [alias, id] : aliases) {
    names.push_back(OneByteString(isolate, alias));
    values.push_back(Int32::New(isolate, id));
  }
#endif
  Local<Object> result = Object::New(
      isolate, Null(isolate), names.data(), values.data(), names.size());
  args.GetReturnValue().Set(result);
}

}  // namespace crypto
}  // namespace node
