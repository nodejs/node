#include "node.h"
#include "node_i18n.h"
#include "env-inl.h"
#include "util-inl.h"

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
  // TODO(addaleax): Use options parser variable instead.
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

#if !defined(NODE_WITHOUT_NODE_OPTIONS)
  READONLY_BOOLEAN_PROPERTY("hasNodeOptions");
#endif

  // TODO(addaleax): This seems to be an unused, private API. Remove it?
  READONLY_STRING_PROPERTY(target, "icuDataDir",
      per_process_opts->icu_data_dir);

#endif  // NODE_HAVE_I18N_SUPPORT

  if (env->options()->preserve_symlinks)
    READONLY_BOOLEAN_PROPERTY("preserveSymlinks");
  if (env->options()->preserve_symlinks_main)
    READONLY_BOOLEAN_PROPERTY("preserveSymlinksMain");

  if (env->options()->experimental_modules) {
    READONLY_BOOLEAN_PROPERTY("experimentalModules");
    const std::string& userland_loader = env->options()->userland_loader;
    if (!userland_loader.empty()) {
      READONLY_STRING_PROPERTY(target, "userLoader",  userland_loader);
    }
  }

  if (env->options()->experimental_vm_modules)
    READONLY_BOOLEAN_PROPERTY("experimentalVMModules");

  if (env->options()->experimental_worker)
    READONLY_BOOLEAN_PROPERTY("experimentalWorker");

  if (env->options()->experimental_repl_await)
    READONLY_BOOLEAN_PROPERTY("experimentalREPLAwait");

  if (env->options()->pending_deprecation)
    READONLY_BOOLEAN_PROPERTY("pendingDeprecation");

  if (env->options()->expose_internals)
    READONLY_BOOLEAN_PROPERTY("exposeInternals");

  if (env->abort_on_uncaught_exception())
    READONLY_BOOLEAN_PROPERTY("shouldAbortOnUncaughtException");

  READONLY_PROPERTY(target,
                    "bits",
                    Number::New(env->isolate(), 8 * sizeof(intptr_t)));

  const std::string& warning_file = env->options()->redirect_warnings;
  if (!warning_file.empty()) {
    READONLY_STRING_PROPERTY(target, "warningFile", warning_file);
  }

  std::shared_ptr<DebugOptions> debug_options = env->options()->debug_options;
  Local<Object> debug_options_obj = Object::New(isolate);
  READONLY_PROPERTY(target, "debugOptions", debug_options_obj);

  READONLY_STRING_PROPERTY(debug_options_obj, "host",
                           debug_options->host());

  READONLY_PROPERTY(debug_options_obj,
                    "port",
                    Integer::New(isolate, debug_options->port()));

  READONLY_PROPERTY(debug_options_obj,
                    "inspectorEnabled",
                    Boolean::New(isolate, debug_options->inspector_enabled));
}  // InitConfig

}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(config, node::Initialize)
