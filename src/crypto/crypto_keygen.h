#ifndef SRC_CRYPTO_CRYPTO_KEYGEN_H_
#define SRC_CRYPTO_CRYPTO_KEYGEN_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "base_object.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "memory_tracker.h"
#include "v8.h"

namespace node::crypto {
namespace Keygen {
void Initialize(Environment* env, v8::Local<v8::Object> target);
void RegisterExternalReferences(ExternalReferenceRegistry* registry);
}  // namespace Keygen

enum class KeyGenJobStatus {
  OK,
  FAILED
};

struct WebCryptoKeyGenConfig final {
  v8::Global<v8::Value> algorithm;
  uint32_t usages_mask = 0;
  uint32_t public_usages_mask = 0;
  uint32_t private_usages_mask = 0;
  bool extractable = false;

  WebCryptoKeyGenConfig() = default;
  WebCryptoKeyGenConfig(WebCryptoKeyGenConfig&&) = default;
  WebCryptoKeyGenConfig& operator=(WebCryptoKeyGenConfig&&) = default;
  WebCryptoKeyGenConfig(const WebCryptoKeyGenConfig&) = delete;
  WebCryptoKeyGenConfig& operator=(const WebCryptoKeyGenConfig&) = delete;
};

// A Base CryptoJob for generating secret keys or key pairs.
// The KeyGenTraits is largely responsible for the details of
// the implementation, while KeyGenJob handles the common
// mechanisms.
template <typename KeyGenTraits>
class KeyGenJob final : public CryptoJob<KeyGenTraits> {
 public:
  using AdditionalParams = typename KeyGenTraits::AdditionalParameters;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    CHECK(args.IsConstructCall());

    CryptoJobMode mode = GetCryptoJobMode(args[0]);

    unsigned int offset = 1;

    AdditionalParams params;
    if (KeyGenTraits::AdditionalConfig(mode, args, &offset, &params)
            .IsNothing()) {
      // The KeyGenTraits::AdditionalConfig is responsible for
      // calling an appropriate THROW_CRYPTO_* variant reporting
      // whatever error caused initialization to fail.
      return;
    }

    WebCryptoKeyGenConfig config;
    if (mode == kCryptoJobWebCrypto) {
      if constexpr (KeyGenTraits::kWebCryptoKeyPair) {
        CHECK(args[offset]->IsObject());
        CHECK(args[offset + 1]->IsUint32());
        CHECK(args[offset + 2]->IsUint32());
        CHECK(args[offset + 3]->IsBoolean());
        config.algorithm.Reset(env->isolate(), args[offset]);
        config.public_usages_mask = args[offset + 1].As<v8::Uint32>()->Value();
        config.private_usages_mask = args[offset + 2].As<v8::Uint32>()->Value();
        config.extractable = args[offset + 3]->IsTrue();
      } else {
        CHECK(args[offset]->IsObject());
        CHECK(args[offset + 1]->IsUint32());
        CHECK(args[offset + 2]->IsBoolean());
        config.algorithm.Reset(env->isolate(), args[offset]);
        config.usages_mask = args[offset + 1].As<v8::Uint32>()->Value();
        config.extractable = args[offset + 2]->IsTrue();
      }
    }

    new KeyGenJob<KeyGenTraits>(
        env, args.This(), mode, std::move(params), std::move(config));
  }

  static void Initialize(
      Environment* env,
      v8::Local<v8::Object> target) {
    CryptoJob<KeyGenTraits>::Initialize(New, env, target);
  }

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
    CryptoJob<KeyGenTraits>::RegisterExternalReferences(New, registry);
  }

  KeyGenJob(Environment* env,
            v8::Local<v8::Object> object,
            CryptoJobMode mode,
            AdditionalParams&& params,
            WebCryptoKeyGenConfig&& config)
      : CryptoJob<KeyGenTraits>(
            env, object, KeyGenTraits::Provider, mode, std::move(params)),
        webcrypto_config_(std::move(config)) {}

  void DoThreadPoolWork() override {
    AdditionalParams* params = CryptoJob<KeyGenTraits>::params();

    switch (KeyGenTraits::DoKeyGen(AsyncWrap::env(), params)) {
      case KeyGenJobStatus::OK:
        status_ = KeyGenJobStatus::OK;
        // Success!
        break;
      case KeyGenJobStatus::FAILED: {
        CryptoErrorStore* errors = CryptoJob<KeyGenTraits>::errors();
        errors->Capture();
        if (errors->Empty())
          errors->Insert(NodeCryptoError::KEY_GENERATION_JOB_FAILED);
      }
    }
  }

  v8::Maybe<void> ToResult(v8::Local<v8::Value>* err,
                           v8::Local<v8::Value>* result) override {
    Environment* env = AsyncWrap::env();
    CryptoErrorStore* errors = CryptoJob<KeyGenTraits>::errors();
    AdditionalParams* params = CryptoJob<KeyGenTraits>::params();

    if (status_ == KeyGenJobStatus::OK) {
      v8::TryCatch try_catch(env->isolate());
      v8::MaybeLocal<v8::Value> encoded =
          CryptoJob<KeyGenTraits>::mode() == kCryptoJobWebCrypto
              ? EncodeWebCryptoKey(env, params)
              : KeyGenTraits::EncodeKey(env, params);
      if (encoded.ToLocal(result)) {
        *err = Undefined(env->isolate());
      } else {
        CHECK(try_catch.HasCaught());
        CHECK(try_catch.CanContinue());
        *result = Undefined(env->isolate());
        *err = try_catch.Exception();
      }
    } else {
      if (errors->Empty()) errors->Capture();
      CHECK(!errors->Empty());
      *result = Undefined(env->isolate());
      if (!errors->ToException(env).ToLocal(err)) {
        return v8::Nothing<void>();
      }
    }
    CHECK(!result->IsEmpty());
    CHECK(!err->IsEmpty());
    return v8::JustVoid();
  }

  SET_SELF_SIZE(KeyGenJob)

 private:
  v8::MaybeLocal<v8::Value> EncodeWebCryptoKey(Environment* env,
                                               AdditionalParams* params) {
    v8::Isolate* isolate = env->isolate();
    v8::Local<v8::Value> algorithm =
        v8::Local<v8::Value>::New(isolate, webcrypto_config_.algorithm);

    if constexpr (KeyGenTraits::kWebCryptoKeyPair) {
      v8::Local<v8::Value> public_key;
      v8::Local<v8::Value> private_key;
      if (!NativeCryptoKey::Create(env,
                                   params->key.addRefWithType(kKeyTypePublic),
                                   algorithm,
                                   webcrypto_config_.public_usages_mask,
                                   true)
               .ToLocal(&public_key) ||
          !NativeCryptoKey::Create(env,
                                   params->key.addRefWithType(kKeyTypePrivate),
                                   algorithm,
                                   webcrypto_config_.private_usages_mask,
                                   webcrypto_config_.extractable)
               .ToLocal(&private_key)) {
        return {};
      }

      v8::Local<v8::Object> ret = v8::Object::New(isolate);
      if (!ret->DefineOwnProperty(env->context(),
                                  OneByteString(isolate, "publicKey"),
                                  public_key)
               .FromMaybe(false) ||
          !ret->DefineOwnProperty(env->context(),
                                  OneByteString(isolate, "privateKey"),
                                  private_key)
               .FromMaybe(false)) {
        return {};
      }
      return ret;
    } else {
      auto data = KeyObjectData::CreateSecret(std::move(params->out));
      return NativeCryptoKey::Create(env,
                                     data,
                                     algorithm,
                                     webcrypto_config_.usages_mask,
                                     webcrypto_config_.extractable);
    }
  }

  WebCryptoKeyGenConfig webcrypto_config_;
  KeyGenJobStatus status_ = KeyGenJobStatus::FAILED;
};

// A Base KeyGenTraits for Key Pair generation algorithms.
template <typename KeyPairAlgorithmTraits>
struct KeyPairGenTraits final {
  using AdditionalParameters =
      typename KeyPairAlgorithmTraits::AdditionalParameters;
  static constexpr bool kWebCryptoKeyPair = true;

  static const AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_KEYPAIRGENREQUEST;
  static constexpr const char* JobName = KeyPairAlgorithmTraits::JobName;

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int* offset,
      AdditionalParameters* params) {
    // Notice that offset is a pointer. Each of the AdditionalConfig,
    // GetPublicKeyEncodingFromJs, and GetPrivateKeyEncodingFromJs
    // functions will update the value of the offset as they successfully
    // process input parameters. This allows each job to have a variable
    // number of input parameters specific to each job type.
    if (KeyPairAlgorithmTraits::AdditionalConfig(mode, args, offset, params)
            .IsNothing()) {
      return v8::Nothing<void>();
    }

    if (mode == kCryptoJobWebCrypto) return v8::JustVoid();

    if (!KeyObjectData::GetPublicKeyEncodingFromJs(
             args, offset, kKeyContextGenerate)
             .To(&params->public_key_encoding) ||
        !KeyObjectData::GetPrivateKeyEncodingFromJs(
             args, offset, kKeyContextGenerate)
             .To(&params->private_key_encoding)) {
      return v8::Nothing<void>();
    }

    return v8::JustVoid();
  }

  static KeyGenJobStatus DoKeyGen(
      Environment* env,
      AdditionalParameters* params) {
    ncrypto::EVPKeyCtxPointer ctx = KeyPairAlgorithmTraits::Setup(params);

    if (!ctx)
      return KeyGenJobStatus::FAILED;

    // Generate the key
    EVP_PKEY* pkey = nullptr;
    if (!EVP_PKEY_keygen(ctx.get(), &pkey))
      return KeyGenJobStatus::FAILED;

    auto data = KeyObjectData::CreateAsymmetric(KeyType::kKeyTypePrivate,
                                                ncrypto::EVPKeyPointer(pkey));
    if (!data) [[unlikely]]
      return KeyGenJobStatus::FAILED;
    params->key = std::move(data);
    return KeyGenJobStatus::OK;
  }

  static v8::MaybeLocal<v8::Value> EncodeKey(Environment* env,
                                             AdditionalParameters* params) {
    v8::Local<v8::Value> keys[2];
    if (!params->key.ToEncodedPublicKey(
            env, params->public_key_encoding, &keys[0]) ||
        !params->key.ToEncodedPrivateKey(
            env, params->private_key_encoding, &keys[1])) {
      return {};
    }
    return v8::Array::New(env->isolate(), keys, arraysize(keys));
  }
};

struct SecretKeyGenConfig final : public MemoryRetainer {
  size_t length;        // In bytes.
  ByteSource out;       // Placeholder for the generated key bytes.

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SecretKeyGenConfig)
  SET_SELF_SIZE(SecretKeyGenConfig)
};

struct SecretKeyGenTraits final {
  using AdditionalParameters = SecretKeyGenConfig;
  static constexpr bool kWebCryptoKeyPair = false;
  static const AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_KEYGENREQUEST;
  static constexpr const char* JobName = "SecretKeyGenJob";

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int* offset,
      SecretKeyGenConfig* params);

  static KeyGenJobStatus DoKeyGen(
      Environment* env,
      SecretKeyGenConfig* params);

  static v8::MaybeLocal<v8::Value> EncodeKey(Environment* env,
                                             SecretKeyGenConfig* params);
};

template <typename AlgorithmParams>
struct KeyPairGenConfig final : public MemoryRetainer {
  ncrypto::EVPKeyPointer::PublicKeyEncodingConfig public_key_encoding;
  ncrypto::EVPKeyPointer::PrivateKeyEncodingConfig private_key_encoding;
  KeyObjectData key;
  AlgorithmParams params;

  KeyPairGenConfig() = default;

  explicit KeyPairGenConfig(KeyPairGenConfig&& other) noexcept
      : public_key_encoding(other.public_key_encoding),
        private_key_encoding(
            std::forward<ncrypto::EVPKeyPointer::PrivateKeyEncodingConfig>(
                other.private_key_encoding)),
        key(std::move(other.key)),
        params(std::move(other.params)) {}

  KeyPairGenConfig& operator=(KeyPairGenConfig&& other) noexcept {
    if (&other == this) return *this;
    this->~KeyPairGenConfig();
    return *new (this) KeyPairGenConfig(std::move(other));
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("key", key);
    if (private_key_encoding.passphrase.has_value()) {
      auto& passphrase = private_key_encoding.passphrase.value();
      tracker->TrackFieldWithSize("private_key_encoding.passphrase",
                                  passphrase.size());
    }
    tracker->TrackField("params", params);
  }

  SET_MEMORY_INFO_NAME(KeyPairGenConfig)
  SET_SELF_SIZE(KeyPairGenConfig)
};

struct NidKeyPairParams final : public MemoryRetainer {
  int id;
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(NidKeyPairParams)
  SET_SELF_SIZE(NidKeyPairParams)
};

using NidKeyPairGenConfig = KeyPairGenConfig<NidKeyPairParams>;

struct NidKeyPairGenTraits final {
  using AdditionalParameters = NidKeyPairGenConfig;
  static constexpr const char* JobName = "NidKeyPairGenJob";

  static ncrypto::EVPKeyCtxPointer Setup(NidKeyPairGenConfig* params);

  static v8::Maybe<void> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int* offset,
      NidKeyPairGenConfig* params);
};

using NidKeyPairGenJob = KeyGenJob<KeyPairGenTraits<NidKeyPairGenTraits>>;
using SecretKeyGenJob = KeyGenJob<SecretKeyGenTraits>;
}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_KEYGEN_H_
