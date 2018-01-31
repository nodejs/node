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

#define READONLY_PROPERTY(obj, name, value)                                   \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           FIXED_ONE_BYTE_STRING(isolate, name),              \
                           value, ReadOnly).FromJust();                       \
  } while (0)

static void InitConfig(Local<Object> target,
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

  target->DefineOwnProperty(
      context,
      FIXED_ONE_BYTE_STRING(isolate, "icuDataDir"),
      String::NewFromUtf8(isolate,
                          icu_data_dir.data(),
                          v8::NewStringType::kNormal).ToLocalChecked(),
      ReadOnly).FromJust();

#endif  // NODE_HAVE_I18N_SUPPORT

  if (config_preserve_symlinks)
    READONLY_BOOLEAN_PROPERTY("preserveSymlinks");

  if (config_experimental_modules) {
    READONLY_BOOLEAN_PROPERTY("experimentalModules");
    if (!config_userland_loader.empty()) {
      target->DefineOwnProperty(
          context,
          FIXED_ONE_BYTE_STRING(isolate, "userLoader"),
          String::NewFromUtf8(isolate,
                              config_userland_loader.data(),
                              v8::NewStringType::kNormal).ToLocalChecked(),
          ReadOnly).FromJust();
    }
  }

  if (config_experimental_vm_modules)
    READONLY_BOOLEAN_PROPERTY("experimentalVMModules");

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
    target->DefineOwnProperty(
        context,
        FIXED_ONE_BYTE_STRING(isolate, "warningFile"),
        String::NewFromUtf8(isolate,
                            config_warning_file.data(),
                            v8::NewStringType::kNormal).ToLocalChecked(),
        ReadOnly).FromJust();
  }

  Local<Object> debugOptions = Object::New(isolate);

  target->DefineOwnProperty(
      context,
      FIXED_ONE_BYTE_STRING(isolate, "debugOptions"),
      debugOptions, ReadOnly).FromJust();

  debugOptions->DefineOwnProperty(
      context,
      FIXED_ONE_BYTE_STRING(isolate, "host"),
      String::NewFromUtf8(isolate,
                          debug_options.host_name().c_str(),
                          v8::NewStringType::kNormal).ToLocalChecked(),
      ReadOnly).FromJust();

  debugOptions->DefineOwnProperty(
      context,
      env->port_string(),
      Integer::New(isolate, debug_options.port()),
      ReadOnly).FromJust();

  debugOptions->DefineOwnProperty(
      context,
      FIXED_ONE_BYTE_STRING(isolate, "inspectorEnabled"),
      Boolean::New(isolate, debug_options.inspector_enabled()), ReadOnly)
          .FromJust();
}  // InitConfig

}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(config, node::InitConfig)
