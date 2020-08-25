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
  SET_NO_MEMORY_INFO();
  SET_MEMORY_INFO_NAME(RandomBytesConfig);
  SET_SELF_SIZE(RandomBytesConfig);
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

namespace Random {
void Initialize(Environment* env, v8::Local<v8::Object> target);
}  // namespace Random
}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_RANDOM_H_
