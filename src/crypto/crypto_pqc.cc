#include "crypto/crypto_pqc.h"
#include "ncrypto.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

using v8::Value;

namespace crypto {

void GetPqcKeyTypes(const v8::FunctionCallbackInfo<Value>& args) {
  v8::LocalVector<Value> names(args.GetIsolate());
  ncrypto::KeyAlgorithm::ForEachPqc(
      [&](const ncrypto::KeyAlgorithm& algorithm) {
        names.push_back(OneByteString(args.GetIsolate(), algorithm.name()));
      });
  args.GetReturnValue().Set(
      v8::Array::New(args.GetIsolate(), names.data(), names.size()));
}
}  // namespace crypto
}  // namespace node
