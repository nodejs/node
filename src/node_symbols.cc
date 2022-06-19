#include "env-inl.h"
#include "node_binding.h"
#include "util.h"

namespace node {

using v8::Context;
using v8::Local;
using v8::Object;
using v8::Value;

namespace symbols {

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
#define V(PropertyName, StringValue)                                           \
  target                                                                       \
      ->Set(env->context(),                                                    \
            env->PropertyName()->Description(env->isolate()),                  \
            env->PropertyName())                                               \
      .Check();
  PER_ISOLATE_SYMBOL_PROPERTIES(V)
#undef V
}

}  // namespace symbols
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(symbols, node::symbols::Initialize)
