#include "v8.h"
#include "node_internals.h"

namespace node {
namespace domain {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;


void Enable(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsFunction());

  env->set_domain_callback(args[0].As<Function>());
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "enable", Enable);
}

}  // namespace domain
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(domain, node::domain::Initialize)
