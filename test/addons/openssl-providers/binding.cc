#include <assert.h>
#include <node.h>

#include <openssl/opensslv.h>
#if OPENSSL_VERSION_MAJOR >= 3
#include <openssl/provider.h>
#endif

namespace {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

#if OPENSSL_VERSION_MAJOR >= 3
int collectProviders(OSSL_PROVIDER* provider, void* cbdata) {
  static_cast<std::vector<OSSL_PROVIDER*>*>(cbdata)->push_back(provider);
  return 1;
}
#endif

inline void GetProviders(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  std::vector<Local<Value>> arr = {};
#if OPENSSL_VERSION_MAJOR >= 3
  std::vector<OSSL_PROVIDER*> providers;
  OSSL_PROVIDER_do_all(nullptr, &collectProviders, &providers);
  for (auto provider : providers) {
    arr.push_back(
        String::NewFromUtf8(isolate, OSSL_PROVIDER_get0_name(provider))
            .ToLocalChecked());
  }
#endif
  args.GetReturnValue().Set(Array::New(isolate, arr.data(), arr.size()));
}

inline void Initialize(Local<Object> exports,
                       Local<Value> module,
                       Local<Context> context) {
  NODE_SET_METHOD(exports, "getProviders", GetProviders);
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
