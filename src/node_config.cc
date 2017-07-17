#include "node.h"
#include "node_i18n.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "node_debug_options.h"


namespace node {

using v8::Boolean;
using v8::Context;
using v8::Integer;
using v8::Local;
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

static void InitConfig(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
#ifdef NODE_HAVE_I18N_SUPPORT

  READONLY_BOOLEAN_PROPERTY("hasIntl");

#ifdef NODE_HAVE_SMALL_ICU
  READONLY_BOOLEAN_PROPERTY("hasSmallICU");
#endif  // NODE_HAVE_SMALL_ICU

  target->DefineOwnProperty(env->context(),
                            OneByteString(env->isolate(), "icuDataDir"),
                            OneByteString(env->isolate(), icu_data_dir.data()))
      .FromJust();

#endif  // NODE_HAVE_I18N_SUPPORT

  if (config_preserve_symlinks)
    READONLY_BOOLEAN_PROPERTY("preserveSymlinks");

  if (config_pending_deprecation)
    READONLY_BOOLEAN_PROPERTY("pendingDeprecation");

  if (!config_warning_file.empty()) {
    Local<String> name = OneByteString(env->isolate(), "warningFile");
    Local<String> value = String::NewFromUtf8(env->isolate(),
                                              config_warning_file.data(),
                                              v8::NewStringType::kNormal,
                                              config_warning_file.size())
                                                .ToLocalChecked();
    target->DefineOwnProperty(env->context(), name, value).FromJust();
  }

  Local<Object> debugOptions = Object::New(env->isolate());

  target->DefineOwnProperty(env->context(),
                            OneByteString(env->isolate(), "debugOptions"),
                            debugOptions).FromJust();

  debugOptions->DefineOwnProperty(env->context(),
                            OneByteString(env->isolate(), "host"),
                            String::NewFromUtf8(env->isolate(),
                            debug_options.host_name().c_str())).FromJust();

  debugOptions->DefineOwnProperty(env->context(),
                            OneByteString(env->isolate(), "port"),
                            Integer::New(env->isolate(),
                            debug_options.port())).FromJust();

  debugOptions->DefineOwnProperty(env->context(),
                            OneByteString(env->isolate(), "inspectorEnabled"),
                            Boolean::New(env->isolate(),
                            debug_options.inspector_enabled())).FromJust();

  if (config_expose_internals)
    READONLY_BOOLEAN_PROPERTY("exposeInternals");

  if (config_expose_http2)
    READONLY_BOOLEAN_PROPERTY("exposeHTTP2");
}  // InitConfig

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(config, node::InitConfig)
