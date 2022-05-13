#include "crypto/crypto_keygen.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "threadpoolwork-inl.h"
#include "v8.h"

#include <cmath>

namespace node {

using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Nothing;
using v8::Object;
using v8::Uint32;
using v8::Value;

namespace crypto {
// NidKeyPairGenJob input arguments:
//   1. CryptoJobMode
//   2. NID
//   3. Public Format
//   4. Public Type
//   5. Private Format
//   6. Private Type
//   7. Cipher
//   8. Passphrase
Maybe<bool> NidKeyPairGenTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int* offset,
    NidKeyPairGenConfig* params) {
  CHECK(args[*offset]->IsInt32());
  params->params.id = args[*offset].As<Int32>()->Value();

  *offset += 1;

  return Just(true);
}

EVPKeyCtxPointer NidKeyPairGenTraits::Setup(NidKeyPairGenConfig* params) {
  EVPKeyCtxPointer ctx =
      EVPKeyCtxPointer(EVP_PKEY_CTX_new_id(params->params.id, nullptr));
  if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0)
    return EVPKeyCtxPointer();

  return ctx;
}

void SecretKeyGenConfig::MemoryInfo(MemoryTracker* tracker) const {
  if (out != nullptr)
    tracker->TrackFieldWithSize("out", length);
}

Maybe<bool> SecretKeyGenTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int* offset,
    SecretKeyGenConfig* params) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[*offset]->IsUint32());
  params->length = static_cast<size_t>(
      std::trunc(args[*offset].As<Uint32>()->Value() / CHAR_BIT));
  if (params->length > INT_MAX) {
    const std::string msg{
      SPrintF("length must be less than or equal to %s bits",
              static_cast<uint64_t>(INT_MAX) * CHAR_BIT)
    };
    THROW_ERR_OUT_OF_RANGE(env, msg.c_str());
    return Nothing<bool>();
  }
  *offset += 1;
  return Just(true);
}

KeyGenJobStatus SecretKeyGenTraits::DoKeyGen(
    Environment* env,
    SecretKeyGenConfig* params) {
  CHECK_LE(params->length, INT_MAX);
  params->out = MallocOpenSSL<char>(params->length);
  EntropySource(reinterpret_cast<unsigned char*>(params->out), params->length);
  return KeyGenJobStatus::OK;
}

Maybe<bool> SecretKeyGenTraits::EncodeKey(
    Environment* env,
    SecretKeyGenConfig* params,
    Local<Value>* result) {
  ByteSource out = ByteSource::Allocated(params->out, params->length);
  std::shared_ptr<KeyObjectData> data =
      KeyObjectData::CreateSecret(std::move(out));
  return Just(KeyObjectHandle::Create(env, data).ToLocal(result));
}

namespace Keygen {
void Initialize(Environment* env, Local<Object> target) {
  NidKeyPairGenJob::Initialize(env, target);
  SecretKeyGenJob::Initialize(env, target);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  NidKeyPairGenJob::RegisterExternalReferences(registry);
  SecretKeyGenJob::RegisterExternalReferences(registry);
}

}  // namespace Keygen
}  // namespace crypto
}  // namespace node
