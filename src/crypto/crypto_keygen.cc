#include "crypto/crypto_keygen.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "ncrypto.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#include <cmath>

namespace node {

using ncrypto::DataPointer;
using ncrypto::EVPKeyCtxPointer;
using v8::FunctionCallbackInfo;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Object;
using v8::Uint32;
using v8::Value;

namespace crypto {
// NamedKeyPairGenJob input arguments:
//   1. CryptoJobMode
//   2. Algorithm name
//   3. Public Format
//   4. Public Type
//   5. Private Format
//   6. Private Type
//   7. Cipher
//   8. Passphrase
Maybe<void> NamedKeyPairGenTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int* offset,
    NamedKeyPairGenConfig* params) {
  CHECK(args[*offset]->IsString());
  Utf8Value name(args.GetIsolate(), args[*offset]);
  params->params.algorithm = ncrypto::KeyAlgorithm::FromName(*name);
  CHECK_NOT_NULL(params->params.algorithm);

  *offset += 1;

  return JustVoid();
}

EVPKeyCtxPointer NamedKeyPairGenTraits::Setup(NamedKeyPairGenConfig* params) {
  auto ctx = EVPKeyCtxPointer::NewFromAlgorithm(*params->params.algorithm);
  if (!ctx || !ctx.initForKeygen()) return {};
  return ctx;
}

void SecretKeyGenConfig::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TraitTrackInline(out, "out");
}

Maybe<void> SecretKeyGenTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int* offset,
    SecretKeyGenConfig* params) {
  CHECK(args[*offset]->IsUint32());
  uint32_t bits = args[*offset].As<Uint32>()->Value();
  params->length_bits = bits;
  if (mode == kCryptoJobWebCrypto) {
    params->length = NumBitsToBytes(static_cast<size_t>(bits));
    params->truncate_to_bit_length = bits % CHAR_BIT != 0;
  } else {
    params->length = bits / CHAR_BIT;
  }
  *offset += 1;
  return JustVoid();
}

KeyGenJobStatus SecretKeyGenTraits::DoKeyGen(Environment* env,
                                             SecretKeyGenConfig* params) {
  auto bytes = DataPointer::Alloc(params->length);
  if (!ncrypto::CSPRNG(static_cast<unsigned char*>(bytes.get()),
                       params->length)) {
    return KeyGenJobStatus::FAILED;
  }
  params->out = ByteSource::Allocated(bytes.release());
  if (params->truncate_to_bit_length)
    TruncateToBitLength(params->length_bits, &params->out);
  return KeyGenJobStatus::OK;
}

MaybeLocal<Value> SecretKeyGenTraits::EncodeKey(Environment* env,
                                                SecretKeyGenConfig* params) {
  auto data = KeyObjectData::CreateSecret(std::move(params->out));
  return KeyObjectHandle::Create(env, data).FromMaybe(Local<Value>());
}

namespace Keygen {
void Initialize(Environment* env, Local<Object> target) {
  NamedKeyPairGenJob::Initialize(env, target);
  SecretKeyGenJob::Initialize(env, target);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  NamedKeyPairGenJob::RegisterExternalReferences(registry);
  SecretKeyGenJob::RegisterExternalReferences(registry);
}
}  // namespace Keygen
}  // namespace crypto
}  // namespace node
