#include "crypto/crypto_random.h"
#include "crypto/crypto_util.h"
#include "allocated_buffer-inl.h"
#include "async_wrap-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#include <openssl/bn.h>
#include <openssl/rand.h>

namespace node {

using v8::ArrayBuffer;
using v8::BackingStore;
using v8::False;
using v8::FunctionCallbackInfo;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;
using v8::True;
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

void RandomPrimeConfig::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("prime", prime ? bits * 8 : 0);
}

Maybe<bool> RandomPrimeTraits::EncodeOutput(
    Environment* env,
    const RandomPrimeConfig& params,
    ByteSource* unused,
    v8::Local<v8::Value>* result) {
  size_t size = BN_num_bytes(params.prime.get());
  std::shared_ptr<BackingStore> store =
      ArrayBuffer::NewBackingStore(env->isolate(), size);
  BN_bn2binpad(
      params.prime.get(),
      reinterpret_cast<unsigned char*>(store->Data()),
      size);
  *result = ArrayBuffer::New(env->isolate(), store);
  return Just(true);
}

Maybe<bool> RandomPrimeTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    RandomPrimeConfig* params) {
  ClearErrorOnReturn clear_error;
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[offset]->IsUint32());  // Size
  CHECK(args[offset + 1]->IsBoolean());  // Safe

  const uint32_t size = args[offset].As<Uint32>()->Value();
  bool safe = args[offset + 1]->IsTrue();

  if (!args[offset + 2]->IsUndefined()) {
    ArrayBufferOrViewContents<unsigned char> add(args[offset + 2]);
    params->add.reset(BN_bin2bn(add.data(), add.size(), nullptr));
    if (!params->add) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env, "could not generate prime");
      return Nothing<bool>();
    }
  }

  if (!args[offset + 3]->IsUndefined()) {
    ArrayBufferOrViewContents<unsigned char> rem(args[offset + 3]);
    params->rem.reset(BN_bin2bn(rem.data(), rem.size(), nullptr));
    if (!params->rem) {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env, "could not generate prime");
      return Nothing<bool>();
    }
  }

  int bits = static_cast<int>(size);
  if (bits < 0) {
    THROW_ERR_OUT_OF_RANGE(env, "invalid size");
    return Nothing<bool>();
  }

  if (params->add) {
    if (BN_num_bits(params->add.get()) > bits) {
      // If we allowed this, the best case would be returning a static prime
      // that wasn't generated randomly. The worst case would be an infinite
      // loop within OpenSSL, blocking the main thread or one of the threads
      // in the thread pool.
      THROW_ERR_OUT_OF_RANGE(env, "invalid options.add");
      return Nothing<bool>();
    }

    if (params->rem) {
      if (BN_cmp(params->add.get(), params->rem.get()) != 1) {
        // This would definitely lead to an infinite loop if allowed since
        // OpenSSL does not check this condition.
        THROW_ERR_OUT_OF_RANGE(env, "invalid options.rem");
        return Nothing<bool>();
      }
    }
  }

  params->bits = bits;
  params->safe = safe;
  params->prime.reset(BN_secure_new());
  if (!params->prime) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "could not generate prime");
    return Nothing<bool>();
  }

  return Just(true);
}

bool RandomPrimeTraits::DeriveBits(
    Environment* env,
    const RandomPrimeConfig& params,
    ByteSource* unused) {

  CheckEntropy();

  if (BN_generate_prime_ex(
          params.prime.get(),
          params.bits,
          params.safe ? 1 : 0,
          params.add.get(),
          params.rem.get(),
          nullptr) == 0) {
    return false;
  }

  return true;
}

void CheckPrimeConfig::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize(
      "prime", candidate ? BN_num_bytes(candidate.get()) : 0);
}

Maybe<bool> CheckPrimeTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    CheckPrimeConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  ArrayBufferOrViewContents<unsigned char> candidate(args[offset]);

  params->candidate =
      BignumPointer(BN_bin2bn(
          candidate.data(),
          candidate.size(),
          nullptr));

  CHECK(args[offset + 1]->IsUint32());  // Checks

  const int checks = static_cast<int>(args[offset + 1].As<Uint32>()->Value());
  if (checks < 0) {
    THROW_ERR_OUT_OF_RANGE(env, "invalid options.checks");
    return Nothing<bool>();
  }

  params->checks = checks;

  return Just(true);
}

bool CheckPrimeTraits::DeriveBits(
    Environment* env,
    const CheckPrimeConfig& params,
    ByteSource* out) {

  BignumCtxPointer ctx(BN_CTX_new());

  int ret = BN_is_prime_ex(
            params.candidate.get(),
            params.checks,
            ctx.get(),
            nullptr);
  if (ret < 0) return false;
  char* data = MallocOpenSSL<char>(1);
  data[0] = ret;
  *out = ByteSource::Allocated(data, 1);
  return true;
}

Maybe<bool> CheckPrimeTraits::EncodeOutput(
    Environment* env,
    const CheckPrimeConfig& params,
    ByteSource* out,
    v8::Local<v8::Value>* result) {
  *result = out->get()[0] ? True(env->isolate()) : False(env->isolate());
  return Just(true);
}

namespace Random {
void Initialize(Environment* env, Local<Object> target) {
  RandomBytesJob::Initialize(env, target);
  RandomPrimeJob::Initialize(env, target);
  CheckPrimeJob::Initialize(env, target);
}
}  // namespace Random
}  // namespace crypto
}  // namespace node
