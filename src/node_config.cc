#include "node.h"
#include "node_i18n.h"
#include "node_options-inl.h"
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
using v8::Value;

// The config binding is used to provide an internal view of compile or runtime
// config options that are required internally by lib/*.js code. This is an
// alternative to dropping additional properties onto the process object as
// has been the practice previously in node.cc.

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

#if defined(DEBUG) && DEBUG
  READONLY_TRUE_PROPERTY(target, "isDebugBuild");
#else
  READONLY_FALSE_PROPERTY(target, "isDebugBuild");
#endif  // defined(DEBUG) && DEBUG

#if HAVE_OPENSSL
  READONLY_TRUE_PROPERTY(target, "hasOpenSSL");
#else
  READONLY_FALSE_PROPERTY(target, "hasOpenSSL");
#endif  // HAVE_OPENSSL

#ifdef NODE_FIPS_MODE
  READONLY_TRUE_PROPERTY(target, "fipsMode");
  // TODO(addaleax): Use options parser variable instead.
  if (per_process_opts->force_fips_crypto)
    READONLY_TRUE_PROPERTY(target, "fipsForced");
#endif

#ifdef NODE_HAVE_I18N_SUPPORT

  READONLY_TRUE_PROPERTY(target, "hasIntl");

#ifdef NODE_HAVE_SMALL_ICU
  READONLY_TRUE_PROPERTY(target, "hasSmallICU");
#endif  // NODE_HAVE_SMALL_ICU

#if NODE_USE_V8_PLATFORM
  READONLY_TRUE_PROPERTY(target, "hasTracing");
#endif

#if HAVE_INSPECTOR
  READONLY_TRUE_PROPERTY(target, "hasInspector");
#else
  READONLY_FALSE_PROPERTY(target, "hasInspector");
#endif

#if !defined(NODE_WITHOUT_NODE_OPTIONS)
  READONLY_TRUE_PROPERTY(target, "hasNodeOptions");
#endif

#endif  // NODE_HAVE_I18N_SUPPORT

  if (env->options()->preserve_symlinks)
    READONLY_TRUE_PROPERTY(target, "preserveSymlinks");
  if (env->options()->preserve_symlinks_main)
    READONLY_TRUE_PROPERTY(target, "preserveSymlinksMain");

  if (env->options()->experimental_modules) {
    READONLY_TRUE_PROPERTY(target, "experimentalModules");
    const std::string& userland_loader = env->options()->userland_loader;
    if (!userland_loader.empty()) {
      READONLY_STRING_PROPERTY(target, "userLoader",  userland_loader);
    }
  }

  if (env->options()->experimental_vm_modules)
    READONLY_TRUE_PROPERTY(target, "experimentalVMModules");

  if (env->options()->experimental_worker)
    READONLY_TRUE_PROPERTY(target, "experimentalWorker");

  if (env->options()->experimental_repl_await)
    READONLY_TRUE_PROPERTY(target, "experimentalREPLAwait");

  if (env->options()->expose_internals)
    READONLY_TRUE_PROPERTY(target, "exposeInternals");

  if (env->abort_on_uncaught_exception())
    READONLY_TRUE_PROPERTY(target, "shouldAbortOnUncaughtException");

  READONLY_PROPERTY(target,
                    "bits",
                    Number::New(env->isolate(), 8 * sizeof(intptr_t)));

  Local<Object> debug_options_obj = Object::New(isolate);
  READONLY_PROPERTY(target, "debugOptions", debug_options_obj);

  const DebugOptions& debug_options = env->options()->debug_options();
  READONLY_PROPERTY(debug_options_obj,
                    "inspectorEnabled",
                    Boolean::New(isolate, debug_options.inspector_enabled));
  READONLY_STRING_PROPERTY(
      debug_options_obj, "host", debug_options.host_port.host());
  READONLY_PROPERTY(debug_options_obj,
                    "port",
                    Integer::New(isolate, debug_options.host_port.port()));
}  // InitConfig

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(config, node::Initialize)
