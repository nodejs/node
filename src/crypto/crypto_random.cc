#include "crypto/crypto_random.h"
#include "crypto/crypto_util.h"
#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

namespace node {

using v8::FunctionCallbackInfo;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;
using v8::Uint32;
using v8::Value;

namespace crypto {
Maybe<bool> RandomBytesTraits::EncodeOutput(
    Environment* env,
    const RandomBytesConfig& params,
    ByteSource* unused,
    v8::Local<v8::Value>* result) {
  *result = v8::Undefined(env->isolate());
  return Just(!result->IsEmpty());
}

Maybe<bool> RandomBytesTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    RandomBytesConfig* params) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(IsAnyByteSource(args[offset]));  // Buffer to fill
  CHECK(args[offset + 1]->IsUint32());  // Offset
  CHECK(args[offset + 2]->IsUint32());  // Size

  ArrayBufferOrViewContents<unsigned char> in(args[offset]);

  const uint32_t byte_offset = args[offset + 1].As<Uint32>()->Value();
  const uint32_t size = args[offset + 2].As<Uint32>()->Value();
  CHECK_GE(byte_offset + size, byte_offset);  // Overflow check.
  CHECK_LE(byte_offset + size, in.size());  // Bounds check.

  if (UNLIKELY(size > INT_MAX)) {
    THROW_ERR_OUT_OF_RANGE(env, "buffer is too large");
    return Nothing<bool>();
  }

  params->buffer = in.data() + byte_offset;
  params->size = size;

  return Just(true);
}

bool RandomBytesTraits::DeriveBits(
    Environment* env,
    const RandomBytesConfig& params,
    ByteSource* unused) {
  CheckEntropy();  // Ensure that OpenSSL's PRNG is properly seeded.
  return RAND_bytes(params.buffer, params.size) != 0;
}

namespace Random {
void Initialize(Environment* env, Local<Object> target) {
  RandomBytesJob::Initialize(env, target);
}
}  // namespace Random
}  // namespace crypto
}  // namespace node
