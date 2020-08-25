#ifndef SRC_CRYPTO_CRYPTO_KEYGEN_H_
#define SRC_CRYPTO_CRYPTO_KEYGEN_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "allocated_buffer.h"
#include "async_wrap.h"
#include "base_object.h"
#include "env.h"
#include "memory_tracker.h"
#include "v8.h"

namespace node {
namespace crypto {
namespace Keygen {
void Initialize(Environment* env, v8::Local<v8::Object> target);
}  // namespace Keygen

enum class KeyGenJobStatus {
  ERR_OK,
  ERR_FAILED
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

    new KeyGenJob<KeyGenTraits>(env, args.This(), mode, std::move(params));
  }

  static void Initialize(
      Environment* env,
      v8::Local<v8::Object> target) {
    CryptoJob<KeyGenTraits>::Initialize(New, env, target);
  }

  KeyGenJob(
      Environment* env,
      v8::Local<v8::Object> object,
      CryptoJobMode mode,
      AdditionalParams&& params)
      : CryptoJob<KeyGenTraits>(
            env,
            object,
            KeyGenTraits::Provider,
            mode,
            std::move(params)) {}

  void DoThreadPoolWork() override {
    // Make sure the the CSPRNG is properly seeded so the results are secure
    CheckEntropy();

    AdditionalParams* params = CryptoJob<KeyGenTraits>::params();

    switch (KeyGenTraits::DoKeyGen(AsyncWrap::env(), params)) {
      case KeyGenJobStatus::ERR_OK:
        status_ = KeyGenJobStatus::ERR_OK;
        // Success!
        break;
      case KeyGenJobStatus::ERR_FAILED: {
        CryptoErrorVector* errors = CryptoJob<KeyGenTraits>::errors();
        errors->Capture();
        if (errors->empty())
          errors->push_back(std::string("Key generation job failed"));
      }
    }
  }

  v8::Maybe<bool> ToResult(
      v8::Local<v8::Value>* err,
      v8::Local<v8::Value>* result) override {
    Environment* env = AsyncWrap::env();
    CryptoErrorVector* errors = CryptoJob<KeyGenTraits>::errors();
    AdditionalParams* params = CryptoJob<KeyGenTraits>::params();
    if (status_ == KeyGenJobStatus::ERR_OK &&
        LIKELY(!KeyGenTraits::EncodeKey(env, params, result).IsNothing())) {
      *err = Undefined(env->isolate());
      return v8::Just(true);
    }

    if (errors->empty())
      errors->Capture();
    CHECK(!errors->empty());
    *result = Undefined(env->isolate());
    return v8::Just(errors->ToException(env).ToLocal(err));
  }

  SET_SELF_SIZE(KeyGenJob);

 private:
  KeyGenJobStatus status_ = KeyGenJobStatus::ERR_FAILED;
};

// A Base KeyGenTraits for Key Pair generation algorithms.
template <typename KeyPairAlgorithmTraits>
struct KeyPairGenTraits final {
  using AdditionalParameters =
      typename KeyPairAlgorithmTraits::AdditionalParameters;

  static const AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_KEYPAIRGENREQUEST;
  static constexpr const char* JobName = KeyPairAlgorithmTraits::JobName;

  static v8::Maybe<bool> AdditionalConfig(
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
      return v8::Just(false);
    }

    params->public_key_encoding = ManagedEVPPKey::GetPublicKeyEncodingFromJs(
        args,
        offset,
        kKeyContextGenerate);

    auto private_key_encoding =
        ManagedEVPPKey::GetPrivateKeyEncodingFromJs(
            args,
            offset,
            kKeyContextGenerate);

    if (!private_key_encoding.IsEmpty())
      params->private_key_encoding = private_key_encoding.Release();

    return v8::Just(true);
  }

  static KeyGenJobStatus DoKeyGen(
      Environment* env,
      AdditionalParameters* params) {
    EVPKeyCtxPointer ctx = KeyPairAlgorithmTraits::Setup(params);
    if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0)
      return KeyGenJobStatus::ERR_FAILED;

    // Generate the key
    EVP_PKEY* pkey = nullptr;
    if (!EVP_PKEY_keygen(ctx.get(), &pkey))
      return KeyGenJobStatus::ERR_FAILED;

    params->key = ManagedEVPPKey(EVPKeyPointer(pkey));
    return KeyGenJobStatus::ERR_OK;
  }

  static v8::Maybe<bool> EncodeKey(
      Environment* env,
      AdditionalParameters* params,
      v8::Local<v8::Value>* result) {
    v8::Local<v8::Value> keys[2];
    if (ManagedEVPPKey::ToEncodedPublicKey(
            env,
            std::move(params->key),
            params->public_key_encoding,
            &keys[0]).IsNothing() ||
        ManagedEVPPKey::ToEncodedPrivateKey(
            env,
            std::move(params->key),
            params->private_key_encoding,
            &keys[1]).IsNothing()) {
      return v8::Nothing<bool>();
    }
    *result = v8::Array::New(env->isolate(), keys, arraysize(keys));
    return v8::Just(true);
  }
};

struct SecretKeyGenConfig final : public MemoryRetainer {
  size_t length;  // Expressed a a number of bits
  char* out = nullptr;  // Placeholder for the generated key bytes

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(SecretKeyGenConfig)
  SET_SELF_SIZE(SecretKeyGenConfig)
};

struct SecretKeyGenTraits final {
  using AdditionalParameters = SecretKeyGenConfig;
  static const AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_KEYGENREQUEST;
  static constexpr const char* JobName = "SecretKeyGenJob";

  static v8::Maybe<bool> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int* offset,
      SecretKeyGenConfig* params);

  static KeyGenJobStatus DoKeyGen(
      Environment* env,
      SecretKeyGenConfig* params);

  static v8::Maybe<bool> EncodeKey(
      Environment* env,
      SecretKeyGenConfig* params,
      v8::Local<v8::Value>* result);
};

template <typename AlgorithmParams>
struct KeyPairGenConfig final : public MemoryRetainer {
  PublicKeyEncodingConfig public_key_encoding;
  PrivateKeyEncodingConfig private_key_encoding;
  ManagedEVPPKey key;
  AlgorithmParams params;

  KeyPairGenConfig() = default;

  explicit KeyPairGenConfig(KeyPairGenConfig&& other) noexcept
      : public_key_encoding(other.public_key_encoding),
        private_key_encoding(
            std::forward<PrivateKeyEncodingConfig>(
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
    tracker->TrackFieldWithSize("private_key_encoding.passphrase",
                        private_key_encoding.passphrase_.size());
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

  static EVPKeyCtxPointer Setup(NidKeyPairGenConfig* params);

  static v8::Maybe<bool> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int* offset,
      NidKeyPairGenConfig* params);
};

using NidKeyPairGenJob = KeyGenJob<KeyPairGenTraits<NidKeyPairGenTraits>>;
using SecretKeyGenJob = KeyGenJob<SecretKeyGenTraits>;
}  // namespace crypto
}  // namespace node

#endif  // !defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_KEYGEN_H_

