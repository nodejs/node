#ifndef SRC_CRYPTO_CRYPTO_RANDOM_H_
#define SRC_CRYPTO_CRYPTO_RANDOM_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_util.h"
#include "base_object.h"
#include "allocated_buffer.h"
#include "env.h"
#include "memory_tracker.h"
#include "node_internals.h"
#include "v8.h"

namespace node {
namespace crypto {
struct RandomBytesConfig final : public MemoryRetainer {
  unsigned char* buffer;
  size_t size;
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(RandomBytesConfig)
  SET_SELF_SIZE(RandomBytesConfig)
};

struct RandomBytesTraits final {
  using AdditionalParameters = RandomBytesConfig;
  static constexpr const char* JobName = "RandomBytesJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_RANDOMBYTESREQUEST;

  static v8::Maybe<bool> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      RandomBytesConfig* params);

  static bool DeriveBits(
      Environment* env,
      const RandomBytesConfig& params,
      ByteSource* out_);

  static v8::Maybe<bool> EncodeOutput(
      Environment* env,
      const RandomBytesConfig& params,
      ByteSource* unused,
      v8::Local<v8::Value>* result);
};

using RandomBytesJob = DeriveBitsJob<RandomBytesTraits>;

struct RandomPrimeConfig final : public MemoryRetainer {
  BignumPointer prime;
  BignumPointer rem;
  BignumPointer add;
  int bits;
  bool safe;
  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(RandomPrimeConfig)
  SET_SELF_SIZE(RandomPrimeConfig)
};

struct RandomPrimeTraits final {
  using AdditionalParameters = RandomPrimeConfig;
  static constexpr const char* JobName = "RandomPrimeJob";
  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_RANDOMPRIMEREQUEST;

  static v8::Maybe<bool> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      RandomPrimeConfig* params);

  static bool DeriveBits(
      Environment* env,
      const RandomPrimeConfig& params,
      ByteSource* out_);

  static v8::Maybe<bool> EncodeOutput(
      Environment* env,
      const RandomPrimeConfig& params,
      ByteSource* unused,
      v8::Local<v8::Value>* result);
};

using RandomPrimeJob = DeriveBitsJob<RandomPrimeTraits>;

struct CheckPrimeConfig final : public MemoryRetainer {
  BignumPointer candidate;
  int checks = 1;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(CheckPrimeConfig)
  SET_SELF_SIZE(CheckPrimeConfig)
};

struct CheckPrimeTraits final {
  using AdditionalParameters = CheckPrimeConfig;
  static constexpr const char* JobName = "CheckPrimeJob";

  static constexpr AsyncWrap::ProviderType Provider =
      AsyncWrap::PROVIDER_CHECKPRIMEREQUEST;

  static v8::Maybe<bool> AdditionalConfig(
      CryptoJobMode mode,
      const v8::FunctionCallbackInfo<v8::Value>& args,
      unsigned int offset,
      CheckPrimeConfig* params);

  static bool DeriveBits(
      Environment* env,
      const CheckPrimeConfig& params,
      ByteSource* out);

  static v8::Maybe<bool> EncodeOutput(
      Environment* env,
      const CheckPrimeConfig& params,
      ByteSource* out,
      v8::Local<v8::Value>* result);
};

using CheckPrimeJob = DeriveBitsJob<CheckPrimeTraits>;

namespace Random {
void Initialize(Environment* env, v8::Local<v8::Object> target);
}  // namespace Random
}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_RANDOM_H_
