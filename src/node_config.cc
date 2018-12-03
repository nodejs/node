#include "node.h"
#include "node_i18n.h"
#include "env-inl.h"
#include "util-inl.h"

namespace node {

using v8::Context;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::ReadOnly;
using v8::String;
using v8::Value;

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

#define READONLY_PROPERTY(obj, name, value)                                   \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           FIXED_ONE_BYTE_STRING(env->isolate(), name),       \
                           value, ReadOnly).FromJust();                       \
  } while (0)

void InitConfig(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
#ifdef NODE_HAVE_I18N_SUPPORT

  READONLY_BOOLEAN_PROPERTY("hasIntl");

#ifdef NODE_HAVE_SMALL_ICU
  READONLY_BOOLEAN_PROPERTY("hasSmallICU");
#endif  // NODE_HAVE_SMALL_ICU

  if (flag_icu_data_dir)
    READONLY_BOOLEAN_PROPERTY("usingICUDataDir");
#endif  // NODE_HAVE_I18N_SUPPORT

  if (config_preserve_symlinks)
    READONLY_BOOLEAN_PROPERTY("preserveSymlinks");

  if (config_expose_internals)
    READONLY_BOOLEAN_PROPERTY("exposeInternals");


  READONLY_PROPERTY(target,
                    "maxHTTPHeaderSize",
                    Number::New(env->isolate(), max_http_header_size));

  if (!config_warning_file.empty()) {
    Local<String> name = OneByteString(env->isolate(), "warningFile");
    Local<String> value = String::NewFromUtf8(env->isolate(),
                                              config_warning_file.data(),
                                              v8::NewStringType::kNormal,
                                              config_warning_file.size())
                                                .ToLocalChecked();
    target->DefineOwnProperty(env->context(), name, value).FromJust();
  }
}  // InitConfig

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(config, node::InitConfig)
