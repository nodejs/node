#include "node.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

namespace node {
namespace util {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

static void IsMapIterator(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsMapIterator());
}


static void IsSetIterator(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsSetIterator());
}

static void IsPromise(const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(1, args.Length());
  args.GetReturnValue().Set(args[0]->IsPromise());
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "isMapIterator", IsMapIterator);
  env->SetMethod(target, "isSetIterator", IsSetIterator);
  env->SetMethod(target, "isPromise", IsPromise);
}

}  // namespace util
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(util, node::util::Initialize)
