#ifndef SRC_CRYPTO_CRYPTO_MAC_H_
#define SRC_CRYPTO_CRYPTO_MAC_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "memory_tracker.h"
#include "v8.h"

namespace node {
namespace crypto {

class Mac final : public BaseObject {
 public:
  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Mac)
  SET_SELF_SIZE(Mac)

 private:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void MacUpdate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void MacFinal(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetMacs(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCachedAliases(const v8::FunctionCallbackInfo<v8::Value>& args);

#if OPENSSL_WITH_EVP_MAC
  Mac(Environment* env,
      v8::Local<v8::Object> wrap,
      ncrypto::EVPMacCtxPointer&& context,
      size_t output_size,
      bool has_output_length);
  bool MacUpdate(const char* data, size_t length);

  ncrypto::EVPMacCtxPointer context_;
  size_t output_size_ = 0;
  bool has_output_length_ = false;
#else
  Mac(Environment* env, v8::Local<v8::Object> wrap);
#endif
};

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_MAC_H_
