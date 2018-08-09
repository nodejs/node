#include "node.h"
#include "node_i18n.h"
#include "env-inl.h"
#include "util-inl.h"
#include "node_debug_options.h"

namespace node {

using v8::Boolean;
using v8::Context;
using v8::Integer;
using v8::Isolate;
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
    target->DefineOwnProperty(context,                                        \
                              FIXED_ONE_BYTE_STRING(isolate, str),            \
                              True(isolate), ReadOnly).FromJust();            \
  } while (0)

#define READONLY_STRING_PROPERTY(obj, str, val)                                \
  do {                                                                         \
    (obj)->DefineOwnProperty(context,                                          \
                             FIXED_ONE_BYTE_STRING(isolate, str),              \
                             String::NewFromUtf8(                              \
                                 isolate,                                      \
                                 val.data(),                                   \
                                 v8::NewStringType::kNormal).ToLocalChecked(), \
                             ReadOnly).FromJust();                             \
  } while (0)


#define READONLY_PROPERTY(obj, name, value)                                   \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           FIXED_ONE_BYTE_STRING(isolate, name),              \
                           value, ReadOnly).FromJust();                       \
  } while (0)

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

#ifdef NODE_FIPS_MODE
  READONLY_BOOLEAN_PROPERTY("fipsMode");
  if (force_fips_crypto)
    READONLY_BOOLEAN_PROPERTY("fipsForced");
#endif

#ifdef NODE_HAVE_I18N_SUPPORT

  READONLY_BOOLEAN_PROPERTY("hasIntl");

#ifdef NODE_HAVE_SMALL_ICU
  READONLY_BOOLEAN_PROPERTY("hasSmallICU");
#endif  // NODE_HAVE_SMALL_ICU

#if NODE_USE_V8_PLATFORM
  READONLY_BOOLEAN_PROPERTY("hasTracing");
#endif

  READONLY_STRING_PROPERTY(target, "icuDataDir", icu_data_dir);

#endif  // NODE_HAVE_I18N_SUPPORT

  if (config_preserve_symlinks)
    READONLY_BOOLEAN_PROPERTY("preserveSymlinks");
  if (config_preserve_symlinks_main)
    READONLY_BOOLEAN_PROPERTY("preserveSymlinksMain");

  if (config_experimental_modules) {
    READONLY_BOOLEAN_PROPERTY("experimentalModules");
    if (!config_userland_loader.empty()) {
      READONLY_STRING_PROPERTY(target, "userLoader",  config_userland_loader);
    }
  }

  if (config_experimental_vm_modules)
    READONLY_BOOLEAN_PROPERTY("experimentalVMModules");

  if (config_experimental_worker)
    READONLY_BOOLEAN_PROPERTY("experimentalWorker");

  if (config_experimental_repl_await)
    READONLY_BOOLEAN_PROPERTY("experimentalREPLAwait");

  if (config_pending_deprecation)
    READONLY_BOOLEAN_PROPERTY("pendingDeprecation");

  if (config_expose_internals)
    READONLY_BOOLEAN_PROPERTY("exposeInternals");

  if (env->abort_on_uncaught_exception())
    READONLY_BOOLEAN_PROPERTY("shouldAbortOnUncaughtException");

  READONLY_PROPERTY(target,
                    "bits",
                    Number::New(env->isolate(), 8 * sizeof(intptr_t)));

  if (!config_warning_file.empty()) {
    READONLY_STRING_PROPERTY(target, "warningFile", config_warning_file);
  }

  Local<Object> debugOptions = Object::New(isolate);
  READONLY_PROPERTY(target, "debugOptions", debugOptions);

  READONLY_STRING_PROPERTY(debugOptions, "host", debug_options.host_name());

  READONLY_PROPERTY(debugOptions,
                    "port",
                    Integer::New(isolate, debug_options.port()));

  READONLY_PROPERTY(debugOptions,
                    "inspectorEnabled",
                    Boolean::New(isolate, debug_options.inspector_enabled()));
}  // InitConfig

}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(config, node::Initialize)
