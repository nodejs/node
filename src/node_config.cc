#include "node.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"


namespace node {

using v8::Context;
using v8::Local;
using v8::Object;
using v8::Value;
using v8::ReadOnly;

// The config binding is used to provide an internal view of compile or runtime
// config options that are required internally by lib/*.js code. This is an
// alternative to dropping additional properties onto the process object as
// has been the practice previously in node.cc.

#define READONLY_BOOLEAN_PROPERTY(str)                                        \
  do {                                                                        \
    target->DefineOwnProperty(env->context(),                                 \
                              OneByteString(env->isolate(), str),             \
                              True(env->isolate()), ReadOnly).FromJust();     \
  } while (0)

void InitConfig(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
#ifdef NODE_FIPS_MODE
  Environment* env = Environment::GetCurrent(context);
  READONLY_BOOLEAN_PROPERTY("fipsMode");
#endif
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(config, node::InitConfig)
