#include "node.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"

namespace node {

using v8::Context;
using v8::Local;
using v8::Object;
using v8::String;
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
  Environment* env = Environment::GetCurrent(context);

#ifdef NODE_FIPS_MODE
  READONLY_BOOLEAN_PROPERTY("fipsMode");
#endif

  if (!config_warning_file.empty()) {
    Local<String> name = OneByteString(env->isolate(), "warningFile");
    Local<String> value = String::NewFromUtf8(env->isolate(),
                                              config_warning_file.data(),
                                              v8::NewStringType::kNormal,
                                              config_warning_file.size())
                                                .ToLocalChecked();
    target->DefineOwnProperty(env->context(), name, value).FromJust();
  }
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(config, node::InitConfig)
