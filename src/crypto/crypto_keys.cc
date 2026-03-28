#include "crypto/crypto_keys.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "crypto/crypto_common.h"
#include "crypto/crypto_dh.h"
#include "crypto/crypto_dsa.h"
#include "crypto/crypto_ec.h"
#include "crypto/crypto_ml_dsa.h"
#include "crypto/crypto_rsa.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_buffer.h"
#include "string_bytes.h"
#include "threadpoolwork-inl.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using ncrypto::BignumPointer;
using ncrypto::BIOPointer;
using ncrypto::ECKeyPointer;
using ncrypto::ECPointPointer;
using ncrypto::EVPKeyCtxPointer;
using ncrypto::EVPKeyPointer;
using ncrypto::MarkPopErrorOnReturn;
using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Int32;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Nothing;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

namespace crypto {
namespace {
Maybe<EVPKeyPointer::AsymmetricKeyEncodingConfig> GetKeyFormatAndTypeFromJs(
    const FunctionCallbackInfo<Value>& args,
    unsigned int* offset,
    KeyEncodingContext context) {
  EVPKeyPointer::AsymmetricKeyEncodingConfig config;
  // During key pair generation, it is possible not to specify a key encoding,
  // which will lead to a key object being returned.
  if (args[*offset]->IsUndefined()) {
    CHECK_EQ(context, kKeyContextGenerate);
    CHECK(args[*offset + 1]->IsUndefined());
    config.output_key_object = true;
  } else {
    config.output_key_object = false;

    CHECK(args[*offset]->IsInt32());
    config.format = static_cast<EVPKeyPointer::PKFormatType>(
        args[*offset].As<Int32>()->Value());

    if (config.format == EVPKeyPointer::PKFormatType::RAW_PUBLIC ||
        config.format == EVPKeyPointer::PKFormatType::RAW_PRIVATE ||
        config.format == EVPKeyPointer::PKFormatType::RAW_SEED) {
      // Raw formats use the type slot for ec_point_form (int) or null.
      if (args[*offset + 1]->IsInt32()) {
        config.ec_point_form = args[*offset + 1].As<Int32>()->Value();
      } else {
        CHECK(args[*offset + 1]->IsNullOrUndefined());
      }
    } else if (args[*offset + 1]->IsInt32()) {
      config.type = static_cast<EVPKeyPointer::PKEncodingType>(
          args[*offset + 1].As<Int32>()->Value());
    } else {
      CHECK((context == kKeyContextInput &&
             config.format == EVPKeyPointer::PKFormatType::PEM) ||
            (context == kKeyContextGenerate &&
             config.format == EVPKeyPointer::PKFormatType::JWK));
      CHECK(args[*offset + 1]->IsNullOrUndefined());
      config.type = EVPKeyPointer::PKEncodingType::PKCS1;
    }
  }

  *offset += 2;
  return Just(config);
}

MaybeLocal<Value> ToV8Value(
    Environment* env,
    const BIOPointer& bio,
    const EVPKeyPointer::AsymmetricKeyEncodingConfig& config) {
  if (!bio) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Invalid BIO pointer");
    return {};
  }
  BUF_MEM* bptr = bio;
  if (!bptr) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Unable to create BUF_MEM pointer");
    return {};
  }
  if (config.format == EVPKeyPointer::PKFormatType::PEM) {
    // PEM is an ASCII format, so we will return it as a string.
    return String::NewFromUtf8(
               env->isolate(), bptr->data, NewStringType::kNormal, bptr->length)
        .FromMaybe(Local<Value>());
  }

  CHECK_EQ(config.format, EVPKeyPointer::PKFormatType::DER);
  // DER is binary, return it as a buffer.
  return Buffer::Copy(env, bptr->data, bptr->length).FromMaybe(Local<Value>());
}

MaybeLocal<Value> WritePrivateKey(
    Environment* env,
    const EVPKeyPointer& pkey,
    const EVPKeyPointer::PrivateKeyEncodingConfig& config) {
  if (!pkey) return {};
  auto res = pkey.writePrivateKey(config);
  if (res) return ToV8Value(env, std::move(res.value), config);

  ThrowCryptoError(
      env, res.openssl_error.value_or(0), "Failed to encode private key");
  return MaybeLocal<Value>();
}

MaybeLocal<Value> WritePublicKey(
    Environment* env,
    const EVPKeyPointer& pkey,
    const EVPKeyPointer::PublicKeyEncodingConfig& config) {
  if (!pkey) return {};
  auto res = pkey.writePublicKey(config);
  if (res) return ToV8Value(env, res.value, config);

  ThrowCryptoError(
      env, res.openssl_error.value_or(0), "Failed to encode public key");
  return MaybeLocal<Value>();
}

bool ExportJWKSecretKey(Environment* env,
                        const KeyObjectData& key,
                        Local<Object> target) {
  CHECK_EQ(key.GetKeyType(), kKeyTypeSecret);

  Local<Value> raw;
  return StringBytes::Encode(env->isolate(),
                             key.GetSymmetricKey(),
                             key.GetSymmetricKeySize(),
                             BASE64URL)
             .ToLocal(&raw) &&
         target
             ->Set(env->context(), env->jwk_kty_string(), env->jwk_oct_string())
             .IsJust() &&
         target->Set(env->context(), env->jwk_k_string(), raw).IsJust();
}

KeyObjectData ImportJWKSecretKey(Environment* env, Local<Object> jwk) {
  Local<Value> key;
  if (!jwk->Get(env->context(), env->jwk_k_string()).ToLocal(&key) ||
      !key->IsString()) {
    THROW_ERR_CRYPTO_INVALID_JWK(env, "Invalid JWK secret key format");
    return {};
  }

  static_assert(String::kMaxLength <= INT_MAX);
  return KeyObjectData::CreateSecret(
      ByteSource::FromEncodedString(env, key.As<String>()));
}

bool ExportJWKAsymmetricKey(Environment* env,
                            const KeyObjectData& key,
                            Local<Object> target,
                            bool handleRsaPss) {
  switch (key.GetAsymmetricKey().id()) {
    case EVP_PKEY_RSA_PSS: {
      if (handleRsaPss) return ExportJWKRsaKey(env, key, target);
      break;
    }
    case EVP_PKEY_RSA:
      return ExportJWKRsaKey(env, key, target);
    case EVP_PKEY_EC:
      return ExportJWKEcKey(env, key, target);
    case EVP_PKEY_ED25519:
      // Fall through
    case EVP_PKEY_ED448:
      // Fall through
    case EVP_PKEY_X25519:
      // Fall through
    case EVP_PKEY_X448:
      return ExportJWKEdKey(env, key, target);
#if OPENSSL_WITH_PQC
    case EVP_PKEY_ML_DSA_44:
      // Fall through
    case EVP_PKEY_ML_DSA_65:
      // Fall through
    case EVP_PKEY_ML_DSA_87:
      return ExportJwkMlDsaKey(env, key, target);
#endif
  }
  THROW_ERR_CRYPTO_JWK_UNSUPPORTED_KEY_TYPE(env);
  return false;
}

KeyObjectData ImportJWKAsymmetricKey(Environment* env,
                                     Local<Object> jwk,
                                     std::string_view kty,
                                     const FunctionCallbackInfo<Value>& args,
                                     unsigned int offset) {
  if (kty == "RSA") {
    return ImportJWKRsaKey(env, jwk, args, offset);
  } else if (kty == "EC") {
    return ImportJWKEcKey(env, jwk, args, offset);
  }

  THROW_ERR_CRYPTO_INVALID_JWK(
      env, "%s is not a supported JWK key type", kty.data());
  return {};
}

bool GetSecretKeyDetail(Environment* env,
                        const KeyObjectData& key,
                        Local<Object> target) {
  // For the secret key detail, all we care about is the length,
  // converted to bits.
  return target
      ->Set(env->context(),
            env->length_string(),
            Number::New(
                env->isolate(),
                static_cast<double>(key.GetSymmetricKeySize() * CHAR_BIT)))
      .IsJust();
}

bool GetAsymmetricKeyDetail(Environment* env,
                            const KeyObjectData& key,
                            Local<Object> target) {
  if (!key) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env);
    return false;
  }
  switch (key.GetAsymmetricKey().id()) {
    case EVP_PKEY_RSA:
      // Fall through
    case EVP_PKEY_RSA2:
      // Fall through
    case EVP_PKEY_RSA_PSS: return GetRsaKeyDetail(env, key, target);
    case EVP_PKEY_DSA: return GetDsaKeyDetail(env, key, target);
    case EVP_PKEY_EC: return GetEcKeyDetail(env, key, target);
    case EVP_PKEY_DH: return GetDhKeyDetail(env, key, target);
  }
  THROW_ERR_CRYPTO_INVALID_KEYTYPE(env);
  return false;
}

KeyObjectData TryParsePrivateKey(
    Environment* env,
    const EVPKeyPointer::PrivateKeyEncodingConfig& config,
    const ncrypto::Buffer<const unsigned char>& buffer) {
  auto res = EVPKeyPointer::TryParsePrivateKey(config, buffer);
  if (res) {
    return KeyObjectData::CreateAsymmetric(KeyType::kKeyTypePrivate,
                                           std::move(res.value));
  }

  if (res.error.value() == EVPKeyPointer::PKParseError::NEED_PASSPHRASE) {
    THROW_ERR_MISSING_PASSPHRASE(env, "Passphrase required for encrypted key");
  } else {
    ThrowCryptoError(
        env, res.openssl_error.value_or(0), "Failed to read private key");
  }
  return {};
}

bool ExportJWKInner(Environment* env,
                    const KeyObjectData& key,
                    Local<Value> result,
                    bool handleRsaPss) {
  return key.GetKeyType() == kKeyTypeSecret
             ? ExportJWKSecretKey(env, key, result.As<Object>())
             : ExportJWKAsymmetricKey(
                   env, key, result.As<Object>(), handleRsaPss);
}

int GetNidFromName(const char* name) {
  static constexpr struct {
    const char* name;
    int nid;
  } kNameToNid[] = {
    {"Ed25519", EVP_PKEY_ED25519},
    {"Ed448", EVP_PKEY_ED448},
    {"X25519", EVP_PKEY_X25519},
    {"X448", EVP_PKEY_X448},
#if OPENSSL_WITH_PQC
    {"ML-DSA-44", EVP_PKEY_ML_DSA_44},
    {"ML-DSA-65", EVP_PKEY_ML_DSA_65},
    {"ML-DSA-87", EVP_PKEY_ML_DSA_87},
    {"ML-KEM-512", EVP_PKEY_ML_KEM_512},
    {"ML-KEM-768", EVP_PKEY_ML_KEM_768},
    {"ML-KEM-1024", EVP_PKEY_ML_KEM_1024},
    {"SLH-DSA-SHA2-128f", EVP_PKEY_SLH_DSA_SHA2_128F},
    {"SLH-DSA-SHA2-128s", EVP_PKEY_SLH_DSA_SHA2_128S},
    {"SLH-DSA-SHA2-192f", EVP_PKEY_SLH_DSA_SHA2_192F},
    {"SLH-DSA-SHA2-192s", EVP_PKEY_SLH_DSA_SHA2_192S},
    {"SLH-DSA-SHA2-256f", EVP_PKEY_SLH_DSA_SHA2_256F},
    {"SLH-DSA-SHA2-256s", EVP_PKEY_SLH_DSA_SHA2_256S},
    {"SLH-DSA-SHAKE-128f", EVP_PKEY_SLH_DSA_SHAKE_128F},
    {"SLH-DSA-SHAKE-128s", EVP_PKEY_SLH_DSA_SHAKE_128S},
    {"SLH-DSA-SHAKE-192f", EVP_PKEY_SLH_DSA_SHAKE_192F},
    {"SLH-DSA-SHAKE-192s", EVP_PKEY_SLH_DSA_SHAKE_192S},
    {"SLH-DSA-SHAKE-256f", EVP_PKEY_SLH_DSA_SHAKE_256F},
    {"SLH-DSA-SHAKE-256s", EVP_PKEY_SLH_DSA_SHAKE_256S},
#endif
  };
  for (const auto& entry : kNameToNid) {
    if (StringEqualNoCase(name, entry.name)) return entry.nid;
  }
  return NID_undef;
}
}  // namespace

bool KeyObjectData::ToEncodedPublicKey(
    Environment* env,
    const EVPKeyPointer::PublicKeyEncodingConfig& config,
    Local<Value>* out) {
  CHECK(key_type_ != KeyType::kKeyTypeSecret);
  if (config.output_key_object) {
    // Note that this has the downside of containing sensitive data of the
    // private key.
    return KeyObjectHandle::Create(env, addRefWithType(KeyType::kKeyTypePublic))
        .ToLocal(out);
  } else if (config.format == EVPKeyPointer::PKFormatType::JWK) {
    *out = Object::New(env->isolate());
    return ExportJWKInner(
        env, addRefWithType(KeyType::kKeyTypePublic), *out, false);
  } else if (config.format == EVPKeyPointer::PKFormatType::RAW_PUBLIC) {
    Mutex::ScopedLock lock(mutex());
    const auto& pkey = GetAsymmetricKey();
    if (pkey.id() == EVP_PKEY_EC) {
      const EC_KEY* ec_key = pkey;
      CHECK_NOT_NULL(ec_key);
      auto form = static_cast<point_conversion_form_t>(config.ec_point_form);
      const auto group = ECKeyPointer::GetGroup(ec_key);
      const auto point = ECKeyPointer::GetPublicKey(ec_key);
      return ECPointToBuffer(env, group, point, form).ToLocal(out);
    }
    switch (pkey.id()) {
      case EVP_PKEY_ED25519:
      case EVP_PKEY_ED448:
      case EVP_PKEY_X25519:
      case EVP_PKEY_X448:
#if OPENSSL_WITH_PQC
      case EVP_PKEY_ML_DSA_44:
      case EVP_PKEY_ML_DSA_65:
      case EVP_PKEY_ML_DSA_87:
      case EVP_PKEY_ML_KEM_512:
      case EVP_PKEY_ML_KEM_768:
      case EVP_PKEY_ML_KEM_1024:
      case EVP_PKEY_SLH_DSA_SHA2_128F:
      case EVP_PKEY_SLH_DSA_SHA2_128S:
      case EVP_PKEY_SLH_DSA_SHA2_192F:
      case EVP_PKEY_SLH_DSA_SHA2_192S:
      case EVP_PKEY_SLH_DSA_SHA2_256F:
      case EVP_PKEY_SLH_DSA_SHA2_256S:
      case EVP_PKEY_SLH_DSA_SHAKE_128F:
      case EVP_PKEY_SLH_DSA_SHAKE_128S:
      case EVP_PKEY_SLH_DSA_SHAKE_192F:
      case EVP_PKEY_SLH_DSA_SHAKE_192S:
      case EVP_PKEY_SLH_DSA_SHAKE_256F:
      case EVP_PKEY_SLH_DSA_SHAKE_256S:
#endif
        break;
      default:
        THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
        return false;
    }
    auto raw_data = pkey.rawPublicKey();
    if (!raw_data) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to get raw public key");
      return false;
    }
    return Buffer::Copy(env, raw_data.get<const char>(), raw_data.size())
        .ToLocal(out);
  }

  return WritePublicKey(env, GetAsymmetricKey(), config).ToLocal(out);
}

bool KeyObjectData::ToEncodedPrivateKey(
    Environment* env,
    const EVPKeyPointer::PrivateKeyEncodingConfig& config,
    Local<Value>* out) {
  CHECK(key_type_ != KeyType::kKeyTypeSecret);
  if (config.output_key_object) {
    return KeyObjectHandle::Create(env,
                                   addRefWithType(KeyType::kKeyTypePrivate))
        .ToLocal(out);
  } else if (config.format == EVPKeyPointer::PKFormatType::JWK) {
    *out = Object::New(env->isolate());
    return ExportJWKInner(
        env, addRefWithType(KeyType::kKeyTypePrivate), *out, false);
  } else if (config.format == EVPKeyPointer::PKFormatType::RAW_PRIVATE) {
    Mutex::ScopedLock lock(mutex());
    const auto& pkey = GetAsymmetricKey();
    if (pkey.id() == EVP_PKEY_EC) {
      const EC_KEY* ec_key = pkey;
      CHECK_NOT_NULL(ec_key);
      const BIGNUM* private_key = ECKeyPointer::GetPrivateKey(ec_key);
      CHECK_NOT_NULL(private_key);
      const auto group = ECKeyPointer::GetGroup(ec_key);
      auto order = BignumPointer::New();
      CHECK(order);
      CHECK(EC_GROUP_get_order(group, order.get(), nullptr));
      auto buf = BignumPointer::EncodePadded(private_key, order.byteLength());
      if (!buf) {
        THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                          "Failed to export EC private key");
        return false;
      }
      return Buffer::Copy(env, buf.get<const char>(), buf.size()).ToLocal(out);
    }
    switch (pkey.id()) {
      case EVP_PKEY_ED25519:
      case EVP_PKEY_ED448:
      case EVP_PKEY_X25519:
      case EVP_PKEY_X448:
#if OPENSSL_WITH_PQC
      case EVP_PKEY_SLH_DSA_SHA2_128F:
      case EVP_PKEY_SLH_DSA_SHA2_128S:
      case EVP_PKEY_SLH_DSA_SHA2_192F:
      case EVP_PKEY_SLH_DSA_SHA2_192S:
      case EVP_PKEY_SLH_DSA_SHA2_256F:
      case EVP_PKEY_SLH_DSA_SHA2_256S:
      case EVP_PKEY_SLH_DSA_SHAKE_128F:
      case EVP_PKEY_SLH_DSA_SHAKE_128S:
      case EVP_PKEY_SLH_DSA_SHAKE_192F:
      case EVP_PKEY_SLH_DSA_SHAKE_192S:
      case EVP_PKEY_SLH_DSA_SHAKE_256F:
      case EVP_PKEY_SLH_DSA_SHAKE_256S:
#endif
        break;
      default:
        THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
        return false;
    }
    auto raw_data = pkey.rawPrivateKey();
    if (!raw_data) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to get raw private key");
      return false;
    }
    return Buffer::Copy(env, raw_data.get<const char>(), raw_data.size())
        .ToLocal(out);
  } else if (config.format == EVPKeyPointer::PKFormatType::RAW_SEED) {
    Mutex::ScopedLock lock(mutex());
    const auto& pkey = GetAsymmetricKey();
    switch (pkey.id()) {
#if OPENSSL_WITH_PQC
      case EVP_PKEY_ML_DSA_44:
      case EVP_PKEY_ML_DSA_65:
      case EVP_PKEY_ML_DSA_87:
      case EVP_PKEY_ML_KEM_512:
      case EVP_PKEY_ML_KEM_768:
      case EVP_PKEY_ML_KEM_1024:
        break;
#endif
      default:
        THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
        return false;
    }
#if OPENSSL_WITH_PQC
    auto raw_data = pkey.rawSeed();
    if (!raw_data) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to get raw seed");
      return false;
    }
    return Buffer::Copy(env, raw_data.get<const char>(), raw_data.size())
        .ToLocal(out);
#else
    THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
    return false;
#endif
  }

  return WritePrivateKey(env, GetAsymmetricKey(), config).ToLocal(out);
}

Maybe<EVPKeyPointer::PrivateKeyEncodingConfig>
KeyObjectData::GetPrivateKeyEncodingFromJs(
    const FunctionCallbackInfo<Value>& args,
    unsigned int* offset,
    KeyEncodingContext context) {
  Environment* env = Environment::GetCurrent(args);

  EVPKeyPointer::PrivateKeyEncodingConfig config;
  if (!GetKeyFormatAndTypeFromJs(args, offset, context).To(&config)) {
    return Nothing<EVPKeyPointer::PrivateKeyEncodingConfig>();
  }

  if (config.output_key_object) {
    if (context != kKeyContextInput)
      (*offset)++;
  } else if (config.format == EVPKeyPointer::PKFormatType::RAW_PRIVATE ||
             config.format == EVPKeyPointer::PKFormatType::RAW_SEED) {
    // Raw formats don't support encryption. Still consume the arg offsets.
    if (context != kKeyContextInput) {
      CHECK(args[*offset]->IsNullOrUndefined());
      (*offset)++;
    }
    CHECK(args[*offset]->IsNullOrUndefined());
  } else {
    bool needs_passphrase = false;
    if (context != kKeyContextInput) {
      if (args[*offset]->IsString()) {
        Utf8Value cipher_name(env->isolate(), args[*offset]);
        config.cipher = ncrypto::getCipherByName(*cipher_name);
        if (config.cipher == nullptr) {
          THROW_ERR_CRYPTO_UNKNOWN_CIPHER(env);
          return Nothing<EVPKeyPointer::PrivateKeyEncodingConfig>();
        }
        needs_passphrase = true;
      } else {
        CHECK(args[*offset]->IsNullOrUndefined());
        config.cipher = nullptr;
      }
      (*offset)++;
    }

    if (IsAnyBufferSource(args[*offset])) {
      CHECK_IMPLIES(context != kKeyContextInput, config.cipher != nullptr);
      ArrayBufferOrViewContents<char> passphrase(args[*offset]);
      if (!passphrase.CheckSizeInt32()) [[unlikely]] {
        THROW_ERR_OUT_OF_RANGE(env, "passphrase is too big");
        return Nothing<EVPKeyPointer::PrivateKeyEncodingConfig>();
      }
      config.passphrase = passphrase.ToDataPointer();
    } else {
      CHECK(args[*offset]->IsNullOrUndefined() && !needs_passphrase);
    }
  }

  (*offset)++;
  return Just<EVPKeyPointer::PrivateKeyEncodingConfig>(std::move(config));
}

Maybe<EVPKeyPointer::PublicKeyEncodingConfig>
KeyObjectData::GetPublicKeyEncodingFromJs(
    const FunctionCallbackInfo<Value>& args,
    unsigned int* offset,
    KeyEncodingContext context) {
  return GetKeyFormatAndTypeFromJs(args, offset, context);
}

KeyObjectData KeyObjectData::GetPrivateKeyFromJs(
    const v8::FunctionCallbackInfo<v8::Value>& args,
    unsigned int* offset,
    bool allow_key_object) {
  if (args[*offset]->IsString() || IsAnyBufferSource(args[*offset])) {
    Environment* env = Environment::GetCurrent(args);
    auto key = ByteSource::FromStringOrBuffer(env, args[(*offset)++]);

    EVPKeyPointer::PrivateKeyEncodingConfig config;
    if (!GetPrivateKeyEncodingFromJs(args, offset, kKeyContextInput)
             .To(&config)) {
      return {};
    }

    return TryParsePrivateKey(
        env,
        config,
        ncrypto::Buffer<const unsigned char>{
            .data = reinterpret_cast<const unsigned char*>(key.data()),
            .len = key.size(),
        });
  }

  CHECK(args[*offset]->IsObject() && allow_key_object);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args[*offset].As<Object>(), KeyObjectData());
  CHECK_EQ(key->Data().GetKeyType(), kKeyTypePrivate);
  (*offset) += 4;
  return key->Data().addRef();
}

KeyObjectData KeyObjectData::GetPublicOrPrivateKeyFromJs(
    const FunctionCallbackInfo<Value>& args, unsigned int* offset) {
  if (IsAnyBufferSource(args[*offset])) {
    Environment* env = Environment::GetCurrent(args);
    ArrayBufferOrViewContents<char> data(args[(*offset)++]);
    if (!data.CheckSizeInt32()) [[unlikely]] {
      THROW_ERR_OUT_OF_RANGE(env, "keyData is too big");
      return {};
    }

    EVPKeyPointer::PrivateKeyEncodingConfig config;
    if (!KeyObjectData::GetPrivateKeyEncodingFromJs(
             args, offset, kKeyContextInput)
             .To(&config)) {
      return {};
    }

    ncrypto::Buffer<const unsigned char> buffer = {
        .data = reinterpret_cast<const unsigned char*>(data.data()),
        .len = data.size(),
    };

    if (config.format == EVPKeyPointer::PKFormatType::PEM) {
      // For PEM, we can easily determine whether it is a public or private key
      // by looking for the respective PEM tags.
      auto res = EVPKeyPointer::TryParsePublicKeyPEM(buffer);
      if (res) {
        return CreateAsymmetric(kKeyTypePublic, std::move(res.value));
      }

      if (res.error.value() == EVPKeyPointer::PKParseError::NOT_RECOGNIZED) {
        return TryParsePrivateKey(env, config, buffer);
      }
      ThrowCryptoError(
          env, res.openssl_error.value_or(0), "Failed to read asymmetric key");
      return {};
    }

    // For DER, the type determines how to parse it. SPKI, PKCS#8 and SEC1 are
    // easy, but PKCS#1 can be a public key or a private key.
    static const auto is_public = [](const auto& config,
                                     const auto& buffer) -> bool {
      switch (config.type) {
        case EVPKeyPointer::PKEncodingType::PKCS1:
          return !EVPKeyPointer::IsRSAPrivateKey(buffer);
        case EVPKeyPointer::PKEncodingType::SPKI:
          return true;
        case EVPKeyPointer::PKEncodingType::PKCS8:
          return false;
        case EVPKeyPointer::PKEncodingType::SEC1:
          return false;
        default:
          UNREACHABLE("Invalid key encoding type");
      }
    };

    if (is_public(config, buffer)) {
      auto res = EVPKeyPointer::TryParsePublicKey(config, buffer);
      if (res) {
        return CreateAsymmetric(KeyType::kKeyTypePublic, std::move(res.value));
      }

      ThrowCryptoError(
          env, res.openssl_error.value_or(0), "Failed to read asymmetric key");
      return {};
    }

    return TryParsePrivateKey(env, config, buffer);
  }

  CHECK(args[*offset]->IsObject());
  KeyObjectHandle* key =
      BaseObject::Unwrap<KeyObjectHandle>(args[*offset].As<Object>());
  CHECK_NOT_NULL(key);
  CHECK_NE(key->Data().GetKeyType(), kKeyTypeSecret);
  (*offset) += 4;
  return key->Data().addRef();
}

KeyObjectData KeyObjectData::GetParsedKey(KeyType type,
                                          Environment* env,
                                          EVPKeyPointer&& pkey,
                                          ParseKeyResult ret,
                                          const char* default_msg) {
  MarkPopErrorOnReturn mark_pop_error_on_return;
  switch (ret) {
    case ParseKeyResult::kParseKeyOk: {
      return CreateAsymmetric(type, std::move(pkey));
    }
    case ParseKeyResult::kParseKeyNeedPassphrase: {
      THROW_ERR_MISSING_PASSPHRASE(env,
                                   "Passphrase required for encrypted key");
      return {};
    }
    default: {
      ThrowCryptoError(env, mark_pop_error_on_return.peekError(), default_msg);
      return {};
    }
  }
}

KeyObjectData::KeyObjectData(std::nullptr_t)
    : key_type_(KeyType::kKeyTypeSecret) {}

KeyObjectData::KeyObjectData(ByteSource symmetric_key)
    : key_type_(KeyType::kKeyTypeSecret),
      data_(std::make_shared<Data>(std::move(symmetric_key))) {}

KeyObjectData::KeyObjectData(KeyType type, EVPKeyPointer&& pkey)
    : key_type_(type), data_(std::make_shared<Data>(std::move(pkey))) {}

void KeyObjectData::MemoryInfo(MemoryTracker* tracker) const {
  if (!*this) return;
  switch (GetKeyType()) {
    case kKeyTypeSecret: {
      if (data_->symmetric_key) {
        tracker->TrackFieldWithSize("symmetric_key",
                                    data_->symmetric_key.size());
      }
      break;
    }
    case kKeyTypePrivate:
      // Fall through
    case kKeyTypePublic: {
      if (data_->asymmetric_key) {
        tracker->TrackFieldWithSize(
            "key",
            kSizeOf_EVP_PKEY + data_->asymmetric_key.rawPublicKeySize() +
                data_->asymmetric_key.rawPrivateKeySize());
      }
      break;
    }
    default:
      UNREACHABLE();
  }
}

Mutex& KeyObjectData::mutex() const {
  if (!mutex_) mutex_ = std::make_shared<Mutex>();
  return *mutex_.get();
}

KeyObjectData KeyObjectData::CreateSecret(ByteSource key) {
  return KeyObjectData(std::move(key));
}

KeyObjectData KeyObjectData::CreateAsymmetric(KeyType key_type,
                                              EVPKeyPointer&& pkey) {
  CHECK(pkey);
  return KeyObjectData(key_type, std::move(pkey));
}

KeyType KeyObjectData::GetKeyType() const {
  CHECK(data_);
  return key_type_;
}

const EVPKeyPointer& KeyObjectData::GetAsymmetricKey() const {
  CHECK_NE(key_type_, kKeyTypeSecret);
  CHECK(data_);
  return data_->asymmetric_key;
}

const char* KeyObjectData::GetSymmetricKey() const {
  CHECK_EQ(key_type_, kKeyTypeSecret);
  CHECK(data_);
  return data_->symmetric_key.data<char>();
}

size_t KeyObjectData::GetSymmetricKeySize() const {
  CHECK_EQ(key_type_, kKeyTypeSecret);
  CHECK(data_);
  return data_->symmetric_key.size();
}

bool KeyObjectHandle::HasInstance(Environment* env, Local<Value> value) {
  auto t = env->crypto_key_object_handle_constructor();
  return !t.IsEmpty() && t->HasInstance(value);
}

Local<Function> KeyObjectHandle::Initialize(Environment* env) {
  auto templ = env->crypto_key_object_handle_constructor();
  if (templ.IsEmpty()) {
    Isolate* isolate = env->isolate();
    templ = NewFunctionTemplate(isolate, New);
    templ->InstanceTemplate()->SetInternalFieldCount(
        KeyObjectHandle::kInternalFieldCount);

    SetProtoMethod(isolate, templ, "init", Init);
    SetProtoMethodNoSideEffect(
        isolate, templ, "getSymmetricKeySize", GetSymmetricKeySize);
    SetProtoMethodNoSideEffect(
        isolate, templ, "getAsymmetricKeyType", GetAsymmetricKeyType);
    SetProtoMethodNoSideEffect(
        isolate, templ, "checkEcKeyData", CheckEcKeyData);
    SetProtoMethod(isolate, templ, "export", Export);
    SetProtoMethod(isolate, templ, "exportJwk", ExportJWK);
    SetProtoMethod(isolate, templ, "initECRaw", InitECRaw);
    SetProtoMethod(isolate, templ, "initEDRaw", InitEDRaw);
    SetProtoMethodNoSideEffect(isolate, templ, "rawPublicKey", RawPublicKey);
    SetProtoMethodNoSideEffect(isolate, templ, "rawPrivateKey", RawPrivateKey);
    SetProtoMethod(isolate, templ, "initPqcRaw", InitPqcRaw);
    SetProtoMethodNoSideEffect(isolate, templ, "rawSeed", RawSeed);
    SetProtoMethod(isolate, templ, "initECPrivateRaw", InitECPrivateRaw);
    SetProtoMethodNoSideEffect(
        isolate, templ, "exportECPublicRaw", ExportECPublicRaw);
    SetProtoMethodNoSideEffect(
        isolate, templ, "exportECPrivateRaw", ExportECPrivateRaw);
    SetProtoMethod(isolate, templ, "initJwk", InitJWK);
    SetProtoMethod(isolate, templ, "keyDetail", GetKeyDetail);
    SetProtoMethod(isolate, templ, "equals", Equals);

    env->set_crypto_key_object_handle_constructor(templ);
  }
  return templ->GetFunction(env->context()).ToLocalChecked();
}

void KeyObjectHandle::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Init);
  registry->Register(GetSymmetricKeySize);
  registry->Register(GetAsymmetricKeyType);
  registry->Register(CheckEcKeyData);
  registry->Register(Export);
  registry->Register(ExportJWK);
  registry->Register(InitECRaw);
  registry->Register(InitEDRaw);
  registry->Register(RawPublicKey);
  registry->Register(RawPrivateKey);
  registry->Register(InitPqcRaw);
  registry->Register(RawSeed);
  registry->Register(InitECPrivateRaw);
  registry->Register(ExportECPublicRaw);
  registry->Register(ExportECPrivateRaw);
  registry->Register(InitJWK);
  registry->Register(GetKeyDetail);
  registry->Register(Equals);
}

MaybeLocal<Object> KeyObjectHandle::Create(Environment* env,
                                           const KeyObjectData& data) {
  Local<Object> obj;
  Local<Function> ctor = KeyObjectHandle::Initialize(env);
  CHECK(!env->crypto_key_object_handle_constructor().IsEmpty());
  if (!ctor->NewInstance(env->context(), 0, nullptr).ToLocal(&obj)) {
    return {};
  }

  KeyObjectHandle* key = Unwrap<KeyObjectHandle>(obj);
  CHECK_NOT_NULL(key);
  key->data_ = data.addRef();
  return obj;
}

const KeyObjectData& KeyObjectHandle::Data() {
  return data_;
}

void KeyObjectHandle::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new KeyObjectHandle(env, args.This());
}

KeyObjectHandle::KeyObjectHandle(Environment* env,
                                 Local<Object> wrap)
    : BaseObject(env, wrap) {
  MakeWeak();
}

void KeyObjectHandle::Init(const FunctionCallbackInfo<Value>& args) {
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());
  MarkPopErrorOnReturn mark_pop_error_on_return;

  CHECK(args[0]->IsInt32());
  KeyType type = static_cast<KeyType>(args[0].As<Uint32>()->Value());

  unsigned int offset;

  switch (type) {
  case kKeyTypeSecret: {
    CHECK_EQ(args.Length(), 2);
    ArrayBufferOrViewContents<char> buf(args[1]);
    key->data_ = KeyObjectData::CreateSecret(buf.ToCopy());
    break;
  }
  case kKeyTypePublic: {
    CHECK_EQ(args.Length(), 5);

    offset = 1;
    auto data = KeyObjectData::GetPublicOrPrivateKeyFromJs(args, &offset);
    if (!data) return;
    key->data_ = data.addRefWithType(kKeyTypePublic);
    break;
  }
  case kKeyTypePrivate: {
    CHECK_EQ(args.Length(), 5);
    offset = 1;
    if (auto data = KeyObjectData::GetPrivateKeyFromJs(args, &offset, false)) {
      key->data_ = std::move(data);
    }
    break;
  }
  default:
    UNREACHABLE();
  }
}

void KeyObjectHandle::InitJWK(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());
  MarkPopErrorOnReturn mark_pop_error_on_return;

  // The argument must be a JavaScript object that we will inspect
  // to get the JWK properties from.
  CHECK(args[0]->IsObject());

  // Step one, Secret key or not?
  Local<Object> input = args[0].As<Object>();

  Local<Value> kty;
  if (!input->Get(env->context(), env->jwk_kty_string()).ToLocal(&kty) ||
      !kty->IsString()) {
    return THROW_ERR_CRYPTO_INVALID_JWK(env);
  }

  Utf8Value kty_string(env->isolate(), kty);

  if (kty_string == "oct") {
    // Secret key
    key->data_ = ImportJWKSecretKey(env, input);
    if (!key->data_) {
      // ImportJWKSecretKey is responsible for throwing an appropriate error
      return;
    }
  } else {
    key->data_ = ImportJWKAsymmetricKey(env, input, *kty_string, args, 1);
    if (!key->data_) {
      // ImportJWKAsymmetricKey is responsible for throwing an appropriate error
      return;
    }
  }

  args.GetReturnValue().Set(key->data_.GetKeyType());
}

void KeyObjectHandle::InitECRaw(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  CHECK(args[0]->IsString());
  Utf8Value name(env->isolate(), args[0]);

  MarkPopErrorOnReturn mark_pop_error_on_return;

  int id = ncrypto::Ec::GetCurveIdFromName(*name);
  if (id == NID_undef) return THROW_ERR_CRYPTO_INVALID_CURVE(env);

  auto eckey = ECKeyPointer::NewByCurveName(id);
  if (!eckey)
    return args.GetReturnValue().Set(false);

  const auto group = eckey.getGroup();
  auto pub = ECDH::BufferToPoint(env, group, args[1]);

  if (!pub || !eckey || !eckey.setPublicKey(pub)) {
    return args.GetReturnValue().Set(false);
  }

  auto pkey = EVPKeyPointer::New();
  if (!pkey.assign(eckey)) {
    args.GetReturnValue().Set(false);
  }

  eckey.release();  // Release ownership of the key

  key->data_ = KeyObjectData::CreateAsymmetric(kKeyTypePublic, std::move(pkey));

  args.GetReturnValue().Set(true);
}

void KeyObjectHandle::InitEDRaw(const FunctionCallbackInfo<Value>& args) {
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  CHECK(args[0]->IsString());
  Utf8Value name(args.GetIsolate(), args[0]);

  ArrayBufferOrViewContents<unsigned char> key_data(args[1]);
  KeyType type = FromV8Value<KeyType>(args[2]);

  MarkPopErrorOnReturn mark_pop_error_on_return;

  typedef EVPKeyPointer (*new_key_fn)(
      int, const ncrypto::Buffer<const unsigned char>&);
  new_key_fn fn = type == kKeyTypePrivate ? EVPKeyPointer::NewRawPrivate
                                          : EVPKeyPointer::NewRawPublic;

  int id = GetNidFromName(*name);

  switch (id) {
    case EVP_PKEY_X25519:
    case EVP_PKEY_X448:
    case EVP_PKEY_ED25519:
    case EVP_PKEY_ED448: {
      auto pkey = fn(id,
                     ncrypto::Buffer<const unsigned char>{
                         .data = key_data.data(),
                         .len = key_data.size(),
                     });
      if (!pkey) {
        return args.GetReturnValue().Set(false);
      }
      key->data_ = KeyObjectData::CreateAsymmetric(type, std::move(pkey));
      CHECK(key->data_);
      break;
    }
    default:
      return args.GetReturnValue().Set(false);
  }

  args.GetReturnValue().Set(true);
}

void KeyObjectHandle::InitPqcRaw(const FunctionCallbackInfo<Value>& args) {
#if OPENSSL_WITH_PQC
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  CHECK(args[0]->IsString());
  Utf8Value name(args.GetIsolate(), args[0]);

  ArrayBufferOrViewContents<unsigned char> key_data(args[1]);
  KeyType type = FromV8Value<KeyType>(args[2]);

  MarkPopErrorOnReturn mark_pop_error_on_return;

  int id = GetNidFromName(*name);

  typedef EVPKeyPointer (*new_key_fn)(
      int, const ncrypto::Buffer<const unsigned char>&);
  new_key_fn fn;

  switch (id) {
    case EVP_PKEY_ML_DSA_44:
    case EVP_PKEY_ML_DSA_65:
    case EVP_PKEY_ML_DSA_87:
    case EVP_PKEY_ML_KEM_512:
    case EVP_PKEY_ML_KEM_768:
    case EVP_PKEY_ML_KEM_1024:
      fn = type == kKeyTypePrivate ? EVPKeyPointer::NewRawSeed
                                   : EVPKeyPointer::NewRawPublic;
      break;
    case EVP_PKEY_SLH_DSA_SHA2_128F:
    case EVP_PKEY_SLH_DSA_SHA2_128S:
    case EVP_PKEY_SLH_DSA_SHA2_192F:
    case EVP_PKEY_SLH_DSA_SHA2_192S:
    case EVP_PKEY_SLH_DSA_SHA2_256F:
    case EVP_PKEY_SLH_DSA_SHA2_256S:
    case EVP_PKEY_SLH_DSA_SHAKE_128F:
    case EVP_PKEY_SLH_DSA_SHAKE_128S:
    case EVP_PKEY_SLH_DSA_SHAKE_192F:
    case EVP_PKEY_SLH_DSA_SHAKE_192S:
    case EVP_PKEY_SLH_DSA_SHAKE_256F:
    case EVP_PKEY_SLH_DSA_SHAKE_256S:
      fn = type == kKeyTypePrivate ? EVPKeyPointer::NewRawPrivate
                                   : EVPKeyPointer::NewRawPublic;
      break;
    default:
      return args.GetReturnValue().Set(false);
  }

  auto pkey = fn(id,
                 ncrypto::Buffer<const unsigned char>{
                     .data = key_data.data(),
                     .len = key_data.size(),
                 });
  if (!pkey) {
    return args.GetReturnValue().Set(false);
  }
  key->data_ = KeyObjectData::CreateAsymmetric(type, std::move(pkey));
  CHECK(key->data_);

  args.GetReturnValue().Set(true);
#else
  Environment* env = Environment::GetCurrent(args);
  THROW_ERR_INVALID_ARG_VALUE(env, "Unsupported key type");
#endif
}

void KeyObjectHandle::Equals(const FunctionCallbackInfo<Value>& args) {
  KeyObjectHandle* self_handle;
  KeyObjectHandle* arg_handle;
  ASSIGN_OR_RETURN_UNWRAP(&self_handle, args.This());
  ASSIGN_OR_RETURN_UNWRAP(&arg_handle, args[0].As<Object>());
  const auto& key = self_handle->Data();
  const auto& key2 = arg_handle->Data();

  KeyType key_type = key.GetKeyType();
  CHECK_EQ(key_type, key2.GetKeyType());

  bool ret;
  switch (key_type) {
    case kKeyTypeSecret: {
      size_t size = key.GetSymmetricKeySize();
      if (size == key2.GetSymmetricKeySize()) {
        ret = CRYPTO_memcmp(
                  key.GetSymmetricKey(), key2.GetSymmetricKey(), size) == 0;
      } else {
        ret = false;
      }
      break;
    }
    case kKeyTypePublic:
      // Fall through
    case kKeyTypePrivate: {
      EVP_PKEY* pkey = key.GetAsymmetricKey().get();
      EVP_PKEY* pkey2 = key2.GetAsymmetricKey().get();
#if OPENSSL_VERSION_MAJOR >= 3
      int ok = EVP_PKEY_eq(pkey, pkey2);
#else
      int ok = EVP_PKEY_cmp(pkey, pkey2);
#endif
      if (ok == -2) {
        Environment* env = Environment::GetCurrent(args);
        return THROW_ERR_CRYPTO_UNSUPPORTED_OPERATION(env);
      }
      ret = ok == 1;
      break;
    }
    default:
      UNREACHABLE("unsupported key type");
  }

  args.GetReturnValue().Set(ret);
}

void KeyObjectHandle::GetKeyDetail(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  CHECK(args[0]->IsObject());

  const auto& data = key->Data();

  if (data.GetKeyType() == kKeyTypeSecret) {
    if (GetSecretKeyDetail(env, data, args[0].As<Object>())) [[likely]] {
      args.GetReturnValue().Set(args[0]);
    }
    return;
  }

  if (GetAsymmetricKeyDetail(env, data, args[0].As<Object>())) [[likely]] {
    args.GetReturnValue().Set(args[0]);
  }
}

Local<Value> KeyObjectHandle::GetAsymmetricKeyType() const {
  switch (data_.GetAsymmetricKey().id()) {
    case EVP_PKEY_RSA:
      return env()->crypto_rsa_string();
    case EVP_PKEY_RSA_PSS:
      return env()->crypto_rsa_pss_string();
    case EVP_PKEY_DSA:
      return env()->crypto_dsa_string();
    case EVP_PKEY_DH:
      return env()->crypto_dh_string();
    case EVP_PKEY_EC:
      return env()->crypto_ec_string();
    case EVP_PKEY_ED25519:
      return env()->crypto_ed25519_string();
    case EVP_PKEY_ED448:
      return env()->crypto_ed448_string();
    case EVP_PKEY_X25519:
      return env()->crypto_x25519_string();
    case EVP_PKEY_X448:
      return env()->crypto_x448_string();
#if OPENSSL_WITH_PQC
    case EVP_PKEY_ML_DSA_44:
      return env()->crypto_ml_dsa_44_string();
    case EVP_PKEY_ML_DSA_65:
      return env()->crypto_ml_dsa_65_string();
    case EVP_PKEY_ML_DSA_87:
      return env()->crypto_ml_dsa_87_string();
    case EVP_PKEY_ML_KEM_512:
      return env()->crypto_ml_kem_512_string();
    case EVP_PKEY_ML_KEM_768:
      return env()->crypto_ml_kem_768_string();
    case EVP_PKEY_ML_KEM_1024:
      return env()->crypto_ml_kem_1024_string();
    case EVP_PKEY_SLH_DSA_SHA2_128F:
      return env()->crypto_slh_dsa_sha2_128f_string();
    case EVP_PKEY_SLH_DSA_SHA2_128S:
      return env()->crypto_slh_dsa_sha2_128s_string();
    case EVP_PKEY_SLH_DSA_SHA2_192F:
      return env()->crypto_slh_dsa_sha2_192f_string();
    case EVP_PKEY_SLH_DSA_SHA2_192S:
      return env()->crypto_slh_dsa_sha2_192s_string();
    case EVP_PKEY_SLH_DSA_SHA2_256F:
      return env()->crypto_slh_dsa_sha2_256f_string();
    case EVP_PKEY_SLH_DSA_SHA2_256S:
      return env()->crypto_slh_dsa_sha2_256s_string();
    case EVP_PKEY_SLH_DSA_SHAKE_128F:
      return env()->crypto_slh_dsa_shake_128f_string();
    case EVP_PKEY_SLH_DSA_SHAKE_128S:
      return env()->crypto_slh_dsa_shake_128s_string();
    case EVP_PKEY_SLH_DSA_SHAKE_192F:
      return env()->crypto_slh_dsa_shake_192f_string();
    case EVP_PKEY_SLH_DSA_SHAKE_192S:
      return env()->crypto_slh_dsa_shake_192s_string();
    case EVP_PKEY_SLH_DSA_SHAKE_256F:
      return env()->crypto_slh_dsa_shake_256f_string();
    case EVP_PKEY_SLH_DSA_SHAKE_256S:
      return env()->crypto_slh_dsa_shake_256s_string();
#endif
    default:
      return Undefined(env()->isolate());
  }
}

void KeyObjectHandle::GetAsymmetricKeyType(
    const FunctionCallbackInfo<Value>& args) {
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  args.GetReturnValue().Set(key->GetAsymmetricKeyType());
}

bool KeyObjectHandle::CheckEcKeyData() const {
  MarkPopErrorOnReturn mark_pop_error_on_return;

  const auto& key = data_.GetAsymmetricKey();
  EVPKeyCtxPointer ctx = key.newCtx();
  CHECK(ctx);
  CHECK_EQ(key.id(), EVP_PKEY_EC);

  return data_.GetKeyType() == kKeyTypePrivate ? ctx.privateCheck()
                                               : ctx.publicCheck();
}

void KeyObjectHandle::CheckEcKeyData(const FunctionCallbackInfo<Value>& args) {
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  args.GetReturnValue().Set(key->CheckEcKeyData());
}

void KeyObjectHandle::GetSymmetricKeySize(
    const FunctionCallbackInfo<Value>& args) {
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());
  args.GetReturnValue().Set(
      static_cast<uint32_t>(key->Data().GetSymmetricKeySize()));
}

void KeyObjectHandle::Export(const FunctionCallbackInfo<Value>& args) {
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  KeyType type = key->Data().GetKeyType();
  unsigned int offset = 0;

  Local<Value> result;
  if (type == kKeyTypeSecret) {
    if (key->ExportSecretKey().ToLocal(&result)) [[likely]] {
      args.GetReturnValue().Set(result);
    }
    return;
  }

  if (type == kKeyTypePublic) {
    EVPKeyPointer::PublicKeyEncodingConfig config;
    if (!KeyObjectData::GetPublicKeyEncodingFromJs(
             args, &offset, kKeyContextExport)
             .To(&config)) {
      return;
    }
    CHECK_EQ(offset, static_cast<unsigned int>(args.Length()));
    if (key->ExportPublicKey(config).ToLocal(&result)) [[likely]] {
      args.GetReturnValue().Set(result);
    }
    return;
  }

  CHECK_EQ(type, kKeyTypePrivate);
  EVPKeyPointer::PrivateKeyEncodingConfig config;
  if (!KeyObjectData::GetPrivateKeyEncodingFromJs(
           args, &offset, kKeyContextExport)
           .To(&config)) {
    return;
  }
  CHECK_EQ(offset, static_cast<unsigned int>(args.Length()));
  if (key->ExportPrivateKey(config).ToLocal(&result)) [[likely]] {
    args.GetReturnValue().Set(result);
  }
}

MaybeLocal<Value> KeyObjectHandle::ExportSecretKey() const {
  return Buffer::Copy(
             env(), data_.GetSymmetricKey(), data_.GetSymmetricKeySize())
      .FromMaybe(Local<Value>());
}

MaybeLocal<Value> KeyObjectHandle::ExportPublicKey(
    const EVPKeyPointer::PublicKeyEncodingConfig& config) const {
  return WritePublicKey(env(), data_.GetAsymmetricKey(), config);
}

MaybeLocal<Value> KeyObjectHandle::ExportPrivateKey(
    const EVPKeyPointer::PrivateKeyEncodingConfig& config) const {
  return WritePrivateKey(env(), data_.GetAsymmetricKey(), config);
}

void KeyObjectHandle::RawPublicKey(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  const KeyObjectData& data = key->Data();
  CHECK_NE(data.GetKeyType(), kKeyTypeSecret);

  Mutex::ScopedLock lock(data.mutex());
  const auto& pkey = data.GetAsymmetricKey();

  switch (pkey.id()) {
    case EVP_PKEY_ED25519:
    case EVP_PKEY_ED448:
    case EVP_PKEY_X25519:
    case EVP_PKEY_X448:
#if OPENSSL_WITH_PQC
    case EVP_PKEY_ML_DSA_44:
    case EVP_PKEY_ML_DSA_65:
    case EVP_PKEY_ML_DSA_87:
    case EVP_PKEY_ML_KEM_512:
    case EVP_PKEY_ML_KEM_768:
    case EVP_PKEY_ML_KEM_1024:
    case EVP_PKEY_SLH_DSA_SHA2_128F:
    case EVP_PKEY_SLH_DSA_SHA2_128S:
    case EVP_PKEY_SLH_DSA_SHA2_192F:
    case EVP_PKEY_SLH_DSA_SHA2_192S:
    case EVP_PKEY_SLH_DSA_SHA2_256F:
    case EVP_PKEY_SLH_DSA_SHA2_256S:
    case EVP_PKEY_SLH_DSA_SHAKE_128F:
    case EVP_PKEY_SLH_DSA_SHAKE_128S:
    case EVP_PKEY_SLH_DSA_SHAKE_192F:
    case EVP_PKEY_SLH_DSA_SHAKE_192S:
    case EVP_PKEY_SLH_DSA_SHAKE_256F:
    case EVP_PKEY_SLH_DSA_SHAKE_256S:
#endif
      break;
    default:
      return THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
  }

  auto raw_data = pkey.rawPublicKey();
  if (!raw_data) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                             "Failed to get raw public key");
  }

  args.GetReturnValue().Set(
      Buffer::Copy(env, raw_data.get<const char>(), raw_data.size())
          .FromMaybe(Local<Value>()));
}

void KeyObjectHandle::RawPrivateKey(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  const KeyObjectData& data = key->Data();
  CHECK_EQ(data.GetKeyType(), kKeyTypePrivate);

  Mutex::ScopedLock lock(data.mutex());
  const auto& pkey = data.GetAsymmetricKey();

  switch (pkey.id()) {
    case EVP_PKEY_ED25519:
    case EVP_PKEY_ED448:
    case EVP_PKEY_X25519:
    case EVP_PKEY_X448:
#if OPENSSL_WITH_PQC
    case EVP_PKEY_SLH_DSA_SHA2_128F:
    case EVP_PKEY_SLH_DSA_SHA2_128S:
    case EVP_PKEY_SLH_DSA_SHA2_192F:
    case EVP_PKEY_SLH_DSA_SHA2_192S:
    case EVP_PKEY_SLH_DSA_SHA2_256F:
    case EVP_PKEY_SLH_DSA_SHA2_256S:
    case EVP_PKEY_SLH_DSA_SHAKE_128F:
    case EVP_PKEY_SLH_DSA_SHAKE_128S:
    case EVP_PKEY_SLH_DSA_SHAKE_192F:
    case EVP_PKEY_SLH_DSA_SHAKE_192S:
    case EVP_PKEY_SLH_DSA_SHAKE_256F:
    case EVP_PKEY_SLH_DSA_SHAKE_256S:
#endif
      break;
    default:
      return THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
  }

  auto raw_data = pkey.rawPrivateKey();
  if (!raw_data) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                             "Failed to get raw private key");
  }

  args.GetReturnValue().Set(
      Buffer::Copy(env, raw_data.get<const char>(), raw_data.size())
          .FromMaybe(Local<Value>()));
}

void KeyObjectHandle::ExportECPublicRaw(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  const KeyObjectData& data = key->Data();
  CHECK_NE(data.GetKeyType(), kKeyTypeSecret);

  Mutex::ScopedLock lock(data.mutex());
  const auto& m_pkey = data.GetAsymmetricKey();
  if (m_pkey.id() != EVP_PKEY_EC) {
    return THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
  }

  const EC_KEY* ec_key = m_pkey;
  CHECK_NOT_NULL(ec_key);

  CHECK(args[0]->IsInt32());
  auto form =
      static_cast<point_conversion_form_t>(args[0].As<Int32>()->Value());

  const auto group = ECKeyPointer::GetGroup(ec_key);
  const auto point = ECKeyPointer::GetPublicKey(ec_key);

  Local<Object> buf;
  if (!ECPointToBuffer(env, group, point, form).ToLocal(&buf)) return;

  args.GetReturnValue().Set(buf);
}

void KeyObjectHandle::ExportECPrivateRaw(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  const KeyObjectData& data = key->Data();
  CHECK_EQ(data.GetKeyType(), kKeyTypePrivate);

  Mutex::ScopedLock lock(data.mutex());
  const auto& m_pkey = data.GetAsymmetricKey();
  if (m_pkey.id() != EVP_PKEY_EC) {
    return THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
  }

  const EC_KEY* ec_key = m_pkey;
  CHECK_NOT_NULL(ec_key);

  const BIGNUM* private_key = ECKeyPointer::GetPrivateKey(ec_key);
  CHECK_NOT_NULL(private_key);

  const auto group = ECKeyPointer::GetGroup(ec_key);
  auto order = BignumPointer::New();
  CHECK(order);
  CHECK(EC_GROUP_get_order(group, order.get(), nullptr));

  auto buf = BignumPointer::EncodePadded(private_key, order.byteLength());
  if (!buf) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                             "Failed to export EC private key");
  }

  args.GetReturnValue().Set(Buffer::Copy(env, buf.get<const char>(), buf.size())
                                .FromMaybe(Local<Value>()));
}

void KeyObjectHandle::InitECPrivateRaw(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  CHECK(args[0]->IsString());
  Utf8Value name(env->isolate(), args[0]);

  ArrayBufferOrViewContents<unsigned char> key_data(args[1]);

  MarkPopErrorOnReturn mark_pop_error_on_return;

  int nid = ncrypto::Ec::GetCurveIdFromName(*name);
  if (nid == NID_undef) return THROW_ERR_CRYPTO_INVALID_CURVE(env);

  auto eckey = ECKeyPointer::NewByCurveName(nid);
  if (!eckey) return args.GetReturnValue().Set(false);

  // Validate key data size matches the curve's expected private key length
  const auto group = eckey.getGroup();
  auto order = BignumPointer::New();
  CHECK(order);
  CHECK(EC_GROUP_get_order(group, order.get(), nullptr));
  if (key_data.size() != order.byteLength())
    return args.GetReturnValue().Set(false);

  BignumPointer priv_bn(key_data.data(), key_data.size());
  if (!priv_bn) return args.GetReturnValue().Set(false);

  if (!eckey.setPrivateKey(priv_bn)) return args.GetReturnValue().Set(false);

  // Compute public key from private key
  auto pub_point = ECPointPointer::New(group);
  if (!pub_point || !pub_point.mul(group, priv_bn.get())) {
    return args.GetReturnValue().Set(false);
  }

  if (!eckey.setPublicKey(pub_point)) return args.GetReturnValue().Set(false);

  auto pkey = EVPKeyPointer::New();
  if (!pkey.assign(eckey)) {
    return args.GetReturnValue().Set(false);
  }

  eckey.release();

  key->data_ =
      KeyObjectData::CreateAsymmetric(kKeyTypePrivate, std::move(pkey));

  args.GetReturnValue().Set(true);
}

void KeyObjectHandle::RawSeed(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  const KeyObjectData& data = key->Data();
  CHECK_EQ(data.GetKeyType(), kKeyTypePrivate);

  Mutex::ScopedLock lock(data.mutex());
  const auto& pkey = data.GetAsymmetricKey();

  switch (pkey.id()) {
#if OPENSSL_WITH_PQC
    case EVP_PKEY_ML_DSA_44:
    case EVP_PKEY_ML_DSA_65:
    case EVP_PKEY_ML_DSA_87:
    case EVP_PKEY_ML_KEM_512:
    case EVP_PKEY_ML_KEM_768:
    case EVP_PKEY_ML_KEM_1024:
      break;
#endif
    default:
      return THROW_ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS(env);
  }

#if OPENSSL_WITH_PQC
  auto raw_data = pkey.rawSeed();
  if (!raw_data) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to get raw seed");
  }

  args.GetReturnValue().Set(
      Buffer::Copy(env, raw_data.get<const char>(), raw_data.size())
          .FromMaybe(Local<Value>()));
#endif
}

void KeyObjectHandle::ExportJWK(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args.This());

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsBoolean());

  if (ExportJWKInner(env, key->Data(), args[0], args[1]->IsTrue())) {
    args.GetReturnValue().Set(args[0]);
  }
}

void NativeKeyObject::Initialize(Environment* env, Local<Object> target) {
  SetMethod(env->context(),
            target,
            "createNativeKeyObjectClass",
            NativeKeyObject::CreateNativeKeyObjectClass);
}

void NativeKeyObject::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(NativeKeyObject::CreateNativeKeyObjectClass);
  registry->Register(NativeKeyObject::New);
}

void NativeKeyObject::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsObject());
  KeyObjectHandle* handle = Unwrap<KeyObjectHandle>(args[0].As<Object>());
  CHECK_NOT_NULL(handle);
  new NativeKeyObject(env, args.This(), handle->Data());
}

void NativeKeyObject::CreateNativeKeyObjectClass(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  CHECK_EQ(args.Length(), 1);
  Local<Value> callback = args[0];
  CHECK(callback->IsFunction());

  Local<FunctionTemplate> t =
      NewFunctionTemplate(isolate, NativeKeyObject::New);
  t->InstanceTemplate()->SetInternalFieldCount(
      NativeKeyObject::kInternalFieldCount);

  Local<Value> ctor;
  if (!t->GetFunction(env->context()).ToLocal(&ctor))
    return;

  Local<Value> recv = Undefined(env->isolate());
  Local<Value> ret_v;
  if (!callback.As<Function>()->Call(
          env->context(), recv, 1, &ctor).ToLocal(&ret_v)) {
    return;
  }
  Local<Array> ret = ret_v.As<Array>();
  if (!ret->Get(env->context(), 1).ToLocal(&ctor)) return;
  env->set_crypto_key_object_secret_constructor(ctor.As<Function>());
  if (!ret->Get(env->context(), 2).ToLocal(&ctor)) return;
  env->set_crypto_key_object_public_constructor(ctor.As<Function>());
  if (!ret->Get(env->context(), 3).ToLocal(&ctor)) return;
  env->set_crypto_key_object_private_constructor(ctor.As<Function>());
  args.GetReturnValue().Set(ret);
}

BaseObjectPtr<BaseObject> NativeKeyObject::KeyObjectTransferData::Deserialize(
        Environment* env,
        Local<Context> context,
        std::unique_ptr<worker::TransferData> self) {
  if (context != env->context()) {
    THROW_ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE(env);
    return {};
  }

  Local<Value> handle;
  if (!KeyObjectHandle::Create(env, data_).ToLocal(&handle))
    return {};

  Local<Function> key_ctor;
  Local<Value> arg = FIXED_ONE_BYTE_STRING(env->isolate(),
                                           "internal/crypto/keys");
  if (env->builtin_module_require()
          ->Call(context, Null(env->isolate()), 1, &arg)
          .IsEmpty()) {
    return {};
  }
  switch (data_.GetKeyType()) {
    case kKeyTypeSecret:
      key_ctor = env->crypto_key_object_secret_constructor();
      break;
    case kKeyTypePublic:
      key_ctor = env->crypto_key_object_public_constructor();
      break;
    case kKeyTypePrivate:
      key_ctor = env->crypto_key_object_private_constructor();
      break;
    default:
      UNREACHABLE();
  }

  Local<Value> key;
  if (!key_ctor->NewInstance(context, 1, &handle).ToLocal(&key))
    return {};

  return BaseObjectPtr<BaseObject>(Unwrap<KeyObjectHandle>(key.As<Object>()));
}

BaseObject::TransferMode NativeKeyObject::GetTransferMode() const {
  return BaseObject::TransferMode::kCloneable;
}

std::unique_ptr<worker::TransferData> NativeKeyObject::CloneForMessaging()
    const {
  return std::make_unique<KeyObjectTransferData>(handle_data_);
}

namespace Keys {
void Initialize(Environment* env, Local<Object> target) {
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "KeyObjectHandle"),
              KeyObjectHandle::Initialize(env)).Check();

  constexpr int kKeyEncodingPKCS1 =
      static_cast<int>(EVPKeyPointer::PKEncodingType::PKCS1);
  constexpr int kKeyEncodingPKCS8 =
      static_cast<int>(EVPKeyPointer::PKEncodingType::PKCS8);
  constexpr int kKeyEncodingSPKI =
      static_cast<int>(EVPKeyPointer::PKEncodingType::SPKI);
  constexpr int kKeyEncodingSEC1 =
      static_cast<int>(EVPKeyPointer::PKEncodingType::SEC1);
  constexpr int kKeyFormatDER =
      static_cast<int>(EVPKeyPointer::PKFormatType::DER);
  constexpr int kKeyFormatPEM =
      static_cast<int>(EVPKeyPointer::PKFormatType::PEM);
  constexpr int kKeyFormatJWK =
      static_cast<int>(EVPKeyPointer::PKFormatType::JWK);
  constexpr int kKeyFormatRawPublic =
      static_cast<int>(EVPKeyPointer::PKFormatType::RAW_PUBLIC);
  constexpr int kKeyFormatRawPrivate =
      static_cast<int>(EVPKeyPointer::PKFormatType::RAW_PRIVATE);
  constexpr int kKeyFormatRawSeed =
      static_cast<int>(EVPKeyPointer::PKFormatType::RAW_SEED);

  constexpr auto kSigEncDER = DSASigEnc::DER;
  constexpr auto kSigEncP1363 = DSASigEnc::P1363;

  NODE_DEFINE_CONSTANT(target, kWebCryptoKeyFormatRaw);
  NODE_DEFINE_CONSTANT(target, kWebCryptoKeyFormatPKCS8);
  NODE_DEFINE_CONSTANT(target, kWebCryptoKeyFormatSPKI);
  NODE_DEFINE_CONSTANT(target, kWebCryptoKeyFormatJWK);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_ED25519);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_ED448);
#if OPENSSL_WITH_PQC
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_ML_DSA_44);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_ML_DSA_65);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_ML_DSA_87);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_ML_KEM_512);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_ML_KEM_768);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_ML_KEM_1024);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHA2_128F);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHA2_128S);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHA2_192F);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHA2_192S);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHA2_256F);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHA2_256S);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHAKE_128F);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHAKE_128S);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHAKE_192F);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHAKE_192S);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHAKE_256F);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_SLH_DSA_SHAKE_256S);
#endif
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_X25519);
  NODE_DEFINE_CONSTANT(target, EVP_PKEY_X448);
  NODE_DEFINE_CONSTANT(target, kKeyEncodingPKCS1);
  NODE_DEFINE_CONSTANT(target, kKeyEncodingPKCS8);
  NODE_DEFINE_CONSTANT(target, kKeyEncodingSPKI);
  NODE_DEFINE_CONSTANT(target, kKeyEncodingSEC1);
  NODE_DEFINE_CONSTANT(target, kKeyFormatDER);
  NODE_DEFINE_CONSTANT(target, kKeyFormatPEM);
  NODE_DEFINE_CONSTANT(target, kKeyFormatJWK);
  NODE_DEFINE_CONSTANT(target, kKeyFormatRawPublic);
  NODE_DEFINE_CONSTANT(target, kKeyFormatRawPrivate);
  NODE_DEFINE_CONSTANT(target, kKeyFormatRawSeed);
  NODE_DEFINE_CONSTANT(target, kKeyTypeSecret);
  NODE_DEFINE_CONSTANT(target, kKeyTypePublic);
  NODE_DEFINE_CONSTANT(target, kKeyTypePrivate);
  NODE_DEFINE_CONSTANT(target, kSigEncDER);
  NODE_DEFINE_CONSTANT(target, kSigEncP1363);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  KeyObjectHandle::RegisterExternalReferences(registry);
}
}  // namespace Keys

}  // namespace crypto
}  // namespace node
