#include "crypto/crypto_kdf.h"
#include "ncrypto.h"
#include "v8.h"

#include <openssl/core_names.h>     /* OSSL_KDF_* */

namespace node {
namespace crypto {

v8::MaybeLocal<v8::Value> KDFTraits::EncodeOutput(
    Environment* env,
    const KDFConfig& params,
    ByteSource* out) {
  return out->ToArrayBuffer(env);
}

v8::Maybe<void> KDFTraits::AdditionalConfig(
    CryptoJobMode mode,
    const v8::FunctionCallbackInfo<v8::Value>& args,
    unsigned int offset,
    KDFConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  params->mode = mode;

  CHECK(args[offset]->IsString());  // Algorithm
  CHECK(args[offset + 1]->IsObject());  // Params
  CHECK(args[offset + 2]->IsUint32());  // Length

  Utf8Value algorithm(env->isolate(), args[offset]);
  params->kdfCtx = ncrypto::KdfCtxPointer::FromName(algorithm.ToStringView());

  // if (!pass.CheckSizeInt32()) [[unlikely]] {
  //   THROW_ERR_OUT_OF_RANGE(env, "pass is too large");
  //   return v8::Nothing<void>();
  // }
  // if (!salt.CheckSizeInt32()) [[unlikely]] {
  //   THROW_ERR_OUT_OF_RANGE(env, "salt is too large");
  //   return v8::Nothing<void>();
  // }

  auto paramsObj = args[offset + 1].As<v8::Object>();
  auto propertyNames = paramsObj->GetOwnPropertyNames(env->context()).ToLocalChecked();

  for (uint32_t i = 0; i < propertyNames->Length(); ++i) {
    auto key = propertyNames->Get(env->context(), i).ToLocalChecked();
    if (!key->IsString()) {
      continue;
    }

    auto name = Utf8Value(env->isolate(), key).ToString();
    auto value = paramsObj->Get(env->context(), key).ToLocalChecked();

    if (value->IsUint32()) {
      params->kdfParams[name] = value.As<v8::Uint32>()->Value();
    } else if (value->IsNumber()) {
      params->kdfParams[name] = value.As<v8::Number>()->Value();
    } else if (value->IsString()) {
      params->kdfParams[name] = Utf8Value(env->isolate(), value).ToString();
    } else if (IsAnyBufferSource(value)) {
      ArrayBufferOrViewContents<char> abv(value);
      params->kdfParams[name] = std::vector<char>(abv.data(), abv.data() + abv.size());
    }
  }

  if (!params->kdfCtx.setParams(params->kdfParams)) {
    if (uint32_t err = ERR_peek_last_error(); err != 0) {
      THROW_ERR_CRYPTO_INVALID_KDF_PARAMS(
          env, "Invalid kdf params: %s", ERR_error_string(err, nullptr));
    } else {
      THROW_ERR_CRYPTO_INVALID_KDF_PARAMS(env);
    }
    return v8::Nothing<void>();
  }
  
  params->length = args[offset + 2].As<v8::Uint32>()->Value();
  if (auto kdfSize = params->kdfCtx.getSize(); kdfSize == 0) {
    if (uint32_t err = ERR_peek_last_error(); err != 0) {
      THROW_ERR_CRYPTO_INVALID_KDF_PARAMS(
          env, "Invalid kdf params: %s", ERR_error_string(err, nullptr));
    } else {
      THROW_ERR_CRYPTO_INVALID_KDF_PARAMS(env);
    }
    return v8::Nothing<void>();
  } else if (kdfSize != SIZE_MAX && params->length != kdfSize) {
    THROW_ERR_CRYPTO_INVALID_KEYLEN(env);
    return v8::Nothing<void>();
  }

  return v8::JustVoid();
}

bool KDFTraits::DeriveBits(
    Environment* env,
    const KDFConfig& params,
    ByteSource* out) {
  // If the params.length is zero-length, just return an empty buffer.
  // It's useless, yes, but allowed via the API.
  if (params.length == 0) {
    *out = {};
    return true;
  }

  auto dp = params.kdfCtx.derive(params.length);

  if (!dp) return false;
  DCHECK(!dp.isSecure());
  *out = ByteSource::Allocated(dp.release());
  return true;
}

// helper type for the visitor
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

void KDFConfig::MemoryInfo(MemoryTracker* tracker) const {
  // If the job is sync, then the KDFConfig does not own the data
  if (mode == kCryptoJobAsync) {
    for (auto&& [key, value] : kdfParams) {
      tracker->TrackFieldWithSize(key.data(), std::visit(overloads{
        [](uint32_t value) { return sizeof(value); },
        [](double value) { return sizeof(value); },
        [](const std::string& value) { return value.size(); },
        [](const std::vector<char>& value) { return value.size(); },
      }, value));
    }
  }
}

}  // namespace crypto
}  // namespace node
