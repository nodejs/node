#include "crypto/crypto_timing.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "node_errors.h"
#include "v8.h"
#include "node.h"

#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace node {

using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

namespace crypto {
namespace Timing {
void TimingSafeEqual(const FunctionCallbackInfo<Value>& args) {
  // Moving the type checking into JS leads to test failures, most likely due
  // to V8 inlining certain parts of the wrapper. Therefore, keep them in C++.
  // Refs: https://github.com/nodejs/node/issues/34073.
  Environment* env = Environment::GetCurrent(args);
  if (!IsAnyByteSource(args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(
      env, "The \"buf1\" argument must be an instance of "
      "ArrayBuffer, Buffer, TypedArray, or DataView.");
    return;
  }
  if (!IsAnyByteSource(args[1])) {
    THROW_ERR_INVALID_ARG_TYPE(
      env, "The \"buf2\" argument must be an instance of "
      "ArrayBuffer, Buffer, TypedArray, or DataView.");
    return;
  }

  ArrayBufferOrViewContents<char> buf1(args[0]);
  ArrayBufferOrViewContents<char> buf2(args[1]);

  if (buf1.size() != buf2.size()) {
    THROW_ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH(env);
    return;
  }

  uint16_t bufKey[8];
  CHECK(crypto::EntropySource(reinterpret_cast<unsigned char*>(bufKey),
                              sizeof(bufKey)));
  char key[kKeySize];
  snprintf(key,
           sizeof(key),
           "%04x%04x%04x%04x%04x%04x%04x%04x",
           bufKey[0],
           bufKey[1],
           bufKey[2],
           bufKey[3],
           bufKey[4],
           bufKey[5],
           bufKey[6],
           bufKey[7]);

  std::array<unsigned char, EVP_MAX_MD_SIZE> hash1;
  std::array<unsigned char, EVP_MAX_MD_SIZE> hash2;
  unsigned int hash1Len;
  unsigned int hash2Len;

  HMAC(EVP_sha256(),
       key,
       kKeySize,
       reinterpret_cast<unsigned char const*>(buf1.data()),
       static_cast<int>(buf1.size()),
       hash1.data(),
       &hash1Len);

  HMAC(EVP_sha256(),
       key,
       kKeySize,
       reinterpret_cast<unsigned char const*>(buf2.data()),
       static_cast<int>(buf2.size()),
       hash2.data(),
       &hash2Len);

  assert(hash1Len == hash2Len);

  return args.GetReturnValue().Set(
      CRYPTO_memcmp(hash1.data(), hash2.data(), hash1Len) == 0);
}

void Initialize(Environment* env, Local<Object> target) {
  env->SetMethodNoSideEffect(target, "timingSafeEqual", TimingSafeEqual);
}
void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(TimingSafeEqual);
}
}  // namespace Timing

}  // namespace crypto
}  // namespace node
