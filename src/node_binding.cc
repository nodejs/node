#include "node_binding.h"
#include "env-inl.h"
#include "node_native_module.h"
#include "util.h"

#if HAVE_OPENSSL
#define NODE_BUILTIN_OPENSSL_MODULES(V) V(crypto) V(tls_wrap)
#else
#define NODE_BUILTIN_OPENSSL_MODULES(V)
#endif

#if NODE_HAVE_I18N_SUPPORT
#define NODE_BUILTIN_ICU_MODULES(V) V(icu)
#else
#define NODE_BUILTIN_ICU_MODULES(V)
#endif

#if NODE_REPORT
#define NODE_BUILTIN_REPORT_MODULES(V) V(report)
#else
#define NODE_BUILTIN_REPORT_MODULES(V)
#endif

// A list of built-in modules. In order to do module registration
// in node::Init(), need to add built-in modules in the following list.
// Then in binding::RegisterBuiltinModules(), it calls modules' registration
// function. This helps the built-in modules are loaded properly when
// node is built as static library. No need to depend on the
// __attribute__((constructor)) like mechanism in GCC.
#define NODE_BUILTIN_STANDARD_MODULES(V)                                       \
  V(async_wrap)                                                                \
  V(buffer)                                                                    \
  V(cares_wrap)                                                                \
  V(config)                                                                    \
  V(contextify)                                                                \
  V(credentials)                                                               \
  V(domain)                                                                    \
  V(fs)                                                                        \
  V(fs_event_wrap)                                                             \
  V(heap_utils)                                                                \
  V(http2)                                                                     \
  V(http_parser)                                                               \
  V(http_parser_llhttp)                                                        \
  V(inspector)                                                                 \
  V(js_stream)                                                                 \
  V(messaging)                                                                 \
  V(module_wrap)                                                               \
  V(native_module)                                                             \
  V(options)                                                                   \
  V(os)                                                                        \
  V(performance)                                                               \
  V(pipe_wrap)                                                                 \
  V(process_wrap)                                                              \
  V(process_methods)                                                           \
  V(serdes)                                                                    \
  V(signal_wrap)                                                               \
  V(spawn_sync)                                                                \
  V(stream_pipe)                                                               \
  V(stream_wrap)                                                               \
  V(string_decoder)                                                            \
  V(symbols)                                                                   \
  V(task_queue)                                                                \
  V(tcp_wrap)                                                                  \
  V(timers)                                                                    \
  V(trace_events)                                                              \
  V(tty_wrap)                                                                  \
  V(types)                                                                     \
  V(udp_wrap)                                                                  \
  V(url)                                                                       \
  V(util)                                                                      \
  V(uv)                                                                        \
  V(v8)                                                                        \
  V(worker)                                                                    \
  V(zlib)

#define NODE_BUILTIN_MODULES(V)                                                \
  NODE_BUILTIN_STANDARD_MODULES(V)                                             \
  NODE_BUILTIN_OPENSSL_MODULES(V)                                              \
  NODE_BUILTIN_ICU_MODULES(V)                                                  \
  NODE_BUILTIN_REPORT_MODULES(V)

// This is used to load built-in modules. Instead of using
// __attribute__((constructor)), we call the _register_<modname>
// function for each built-in modules explicitly in
// binding::RegisterBuiltinModules(). This is only forward declaration.
// The definitions are in each module's implementation when calling
// the NODE_MODULE_CONTEXT_AWARE_INTERNAL.
#define V(modname) void _register_##modname();
NODE_BUILTIN_MODULES(V)
#undef V

namespace node {

using v8::Context;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Value;

// Globals per process
static node_module* modlist_internal;
static node_module* modlist_linked;
static node_module* modlist_addon;
static uv_once_t init_modpending_once = UV_ONCE_INIT;
static uv_key_t thread_local_modpending;

// This is set by node::Init() which is used by embedders
bool node_is_initialized = false;

extern "C" void node_module_register(void* m) {
  struct node_module* mp = reinterpret_cast<struct node_module*>(m);

  if (mp->nm_flags & NM_F_INTERNAL) {
    mp->nm_link = modlist_internal;
    modlist_internal = mp;
  } else if (!node_is_initialized) {
    // "Linked" modules are included as part of the node project.
    // Like builtins they are registered *before* node::Init runs.
    mp->nm_flags = NM_F_LINKED;
    mp->nm_link = modlist_linked;
    modlist_linked = mp;
  } else {
    uv_key_set(&thread_local_modpending, mp);
  }
}

namespace binding {

DLib::DLib(const char* filename, int flags)
    : filename_(filename), flags_(flags), handle_(nullptr) {}

#ifdef __POSIX__
bool DLib::Open() {
  handle_ = dlopen(filename_.c_str(), flags_);
  if (handle_ != nullptr) return true;
  errmsg_ = dlerror();
  return false;
}

void DLib::Close() {
  if (handle_ == nullptr) return;
  dlclose(handle_);
  handle_ = nullptr;
}

void* DLib::GetSymbolAddress(const char* name) {
  return dlsym(handle_, name);
}
#else   // !__POSIX__
bool DLib::Open() {
  int ret = uv_dlopen(filename_.c_str(), &lib_);
  if (ret == 0) {
    handle_ = static_cast<void*>(lib_.handle);
    return true;
  }
  errmsg_ = uv_dlerror(&lib_);
  uv_dlclose(&lib_);
  return false;
}

void DLib::Close() {
  if (handle_ == nullptr) return;
  uv_dlclose(&lib_);
  handle_ = nullptr;
}

void* DLib::GetSymbolAddress(const char* name) {
  void* address;
  if (0 == uv_dlsym(&lib_, name, &address)) return address;
  return nullptr;
}
#endif  // !__POSIX__

using InitializerCallback = void (*)(Local<Object> exports,
                                     Local<Value> module,
                                     Local<Context> context);

inline InitializerCallback GetInitializerCallback(DLib* dlib) {
  const char* name = "node_register_module_v" STRINGIFY(NODE_MODULE_VERSION);
  return reinterpret_cast<InitializerCallback>(dlib->GetSymbolAddress(name));
}

inline napi_addon_register_func GetNapiInitializerCallback(DLib* dlib) {
  const char* name =
      STRINGIFY(NAPI_MODULE_INITIALIZER_BASE) STRINGIFY(NAPI_MODULE_VERSION);
  return reinterpret_cast<napi_addon_register_func>(
      dlib->GetSymbolAddress(name));
}

void InitModpendingOnce() {
  CHECK_EQ(0, uv_key_create(&thread_local_modpending));
}

// DLOpen is process.dlopen(module, filename, flags).
// Used to load 'module.node' dynamically shared objects.
//
// FIXME(bnoordhuis) Not multi-context ready. TBD how to resolve the conflict
// when two contexts try to load the same shared object. Maybe have a shadow
// cache that's a plain C list or hash table that's shared across contexts?
void DLOpen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  auto context = env->context();

  uv_once(&init_modpending_once, InitModpendingOnce);
  CHECK_NULL(uv_key_get(&thread_local_modpending));

  if (args.Length() < 2) {
    env->ThrowError("process.dlopen needs at least 2 arguments.");
    return;
  }

  int32_t flags = DLib::kDefaultFlags;
  if (args.Length() > 2 && !args[2]->Int32Value(context).To(&flags)) {
    return env->ThrowTypeError("flag argument must be an integer.");
  }

  Local<Object> module;
  Local<Object> exports;
  Local<Value> exports_v;
  if (!args[0]->ToObject(context).ToLocal(&module) ||
      !module->Get(context, env->exports_string()).ToLocal(&exports_v) ||
      !exports_v->ToObject(context).ToLocal(&exports)) {
    return;  // Exception pending.
  }

  node::Utf8Value filename(env->isolate(), args[1]);  // Cast
  env->TryLoadAddon(*filename, flags, [&](DLib* dlib) {
    const bool is_opened = dlib->Open();

    // Objects containing v14 or later modules will have registered themselves
    // on the pending list.  Activate all of them now.  At present, only one
    // module per object is supported.
    node_module* const mp =
        static_cast<node_module*>(uv_key_get(&thread_local_modpending));
    uv_key_set(&thread_local_modpending, nullptr);

    if (!is_opened) {
      Local<String> errmsg =
          OneByteString(env->isolate(), dlib->errmsg_.c_str());
      dlib->Close();
#ifdef _WIN32
      // Windows needs to add the filename into the error message
      errmsg = String::Concat(
          env->isolate(), errmsg, args[1]->ToString(context).ToLocalChecked());
#endif  // _WIN32
      env->isolate()->ThrowException(Exception::Error(errmsg));
      return false;
    }

    if (mp == nullptr) {
      if (auto callback = GetInitializerCallback(dlib)) {
        callback(exports, module, context);
      } else if (auto napi_callback = GetNapiInitializerCallback(dlib)) {
        napi_module_register_by_symbol(exports, module, context, napi_callback);
      } else {
        dlib->Close();
        env->ThrowError("Module did not self-register.");
        return false;
      }
      return true;
    }

    // -1 is used for N-API modules
    if ((mp->nm_version != -1) && (mp->nm_version != NODE_MODULE_VERSION)) {
      // Even if the module did self-register, it may have done so with the
      // wrong version. We must only give up after having checked to see if it
      // has an appropriate initializer callback.
      if (auto callback = GetInitializerCallback(dlib)) {
        callback(exports, module, context);
        return true;
      }
      char errmsg[1024];
      snprintf(errmsg,
               sizeof(errmsg),
               "The module '%s'"
               "\nwas compiled against a different Node.js version using"
               "\nNODE_MODULE_VERSION %d. This version of Node.js requires"
               "\nNODE_MODULE_VERSION %d. Please try re-compiling or "
               "re-installing\nthe module (for instance, using `npm rebuild` "
               "or `npm install`).",
               *filename,
               mp->nm_version,
               NODE_MODULE_VERSION);

      // NOTE: `mp` is allocated inside of the shared library's memory, calling
      // `dlclose` will deallocate it
      dlib->Close();
      env->ThrowError(errmsg);
      return false;
    }
    CHECK_EQ(mp->nm_flags & NM_F_BUILTIN, 0);

    mp->nm_dso_handle = dlib->handle_;
    mp->nm_link = modlist_addon;
    modlist_addon = mp;

    if (mp->nm_context_register_func != nullptr) {
      mp->nm_context_register_func(exports, module, context, mp->nm_priv);
    } else if (mp->nm_register_func != nullptr) {
      mp->nm_register_func(exports, module, mp->nm_priv);
    } else {
      dlib->Close();
      env->ThrowError("Module has no declared entry point.");
      return false;
    }

    return true;
  });

  // Tell coverity that 'handle' should not be freed when we return.
  // coverity[leaked_storage]
}

inline struct node_module* FindModule(struct node_module* list,
                                      const char* name,
                                      int flag) {
  struct node_module* mp;

  for (mp = list; mp != nullptr; mp = mp->nm_link) {
    if (strcmp(mp->nm_modname, name) == 0) break;
  }

  CHECK(mp == nullptr || (mp->nm_flags & flag) != 0);
  return mp;
}

node_module* get_internal_module(const char* name) {
  return FindModule(modlist_internal, name, NM_F_INTERNAL);
}
node_module* get_linked_module(const char* name) {
  return FindModule(modlist_linked, name, NM_F_LINKED);
}

static Local<Object> InitModule(Environment* env,
                                node_module* mod,
                                Local<String> module) {
  Local<Object> exports = Object::New(env->isolate());
  // Internal bindings don't have a "module" object, only exports.
  CHECK_NULL(mod->nm_register_func);
  CHECK_NOT_NULL(mod->nm_context_register_func);
  Local<Value> unused = Undefined(env->isolate());
  mod->nm_context_register_func(exports, unused, env->context(), mod->nm_priv);
  return exports;
}

static void ThrowIfNoSuchModule(Environment* env, const char* module_v) {
  char errmsg[1024];
  snprintf(errmsg, sizeof(errmsg), "No such module: %s", module_v);
  env->ThrowError(errmsg);
}

void GetInternalBinding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());

  Local<String> module = args[0].As<String>();
  node::Utf8Value module_v(env->isolate(), module);
  Local<Object> exports;

  node_module* mod = get_internal_module(*module_v);
  if (mod != nullptr) {
    exports = InitModule(env, mod, module);
  } else if (!strcmp(*module_v, "constants")) {
    exports = Object::New(env->isolate());
    CHECK(
        exports->SetPrototype(env->context(), Null(env->isolate())).FromJust());
    DefineConstants(env->isolate(), exports);
  } else if (!strcmp(*module_v, "natives")) {
    exports = per_process::native_module_loader.GetSourceObject(env->context());
    // Legacy feature: process.binding('natives').config contains stringified
    // config.gypi
    CHECK(exports
              ->Set(env->context(),
                    env->config_string(),
                    per_process::native_module_loader.GetConfigString(
                        env->isolate()))
              .FromJust());
  } else {
    return ThrowIfNoSuchModule(env, *module_v);
  }

  args.GetReturnValue().Set(exports);
}

void GetLinkedBinding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());

  Local<String> module_name = args[0].As<String>();

  node::Utf8Value module_name_v(env->isolate(), module_name);
  node_module* mod = get_linked_module(*module_name_v);

  if (mod == nullptr) {
    char errmsg[1024];
    snprintf(errmsg,
             sizeof(errmsg),
             "No such module was linked: %s",
             *module_name_v);
    return env->ThrowError(errmsg);
  }

  Local<Object> module = Object::New(env->isolate());
  Local<Object> exports = Object::New(env->isolate());
  Local<String> exports_prop =
      String::NewFromUtf8(env->isolate(), "exports", NewStringType::kNormal)
          .ToLocalChecked();
  module->Set(env->context(), exports_prop, exports).FromJust();

  if (mod->nm_context_register_func != nullptr) {
    mod->nm_context_register_func(
        exports, module, env->context(), mod->nm_priv);
  } else if (mod->nm_register_func != nullptr) {
    mod->nm_register_func(exports, module, mod->nm_priv);
  } else {
    return env->ThrowError("Linked module has no declared entry point.");
  }

  auto effective_exports =
      module->Get(env->context(), exports_prop).ToLocalChecked();

  args.GetReturnValue().Set(effective_exports);
}

// Call built-in modules' _register_<module name> function to
// do module registration explicitly.
void RegisterBuiltinModules() {
#define V(modname) _register_##modname();
  NODE_BUILTIN_MODULES(V)
#undef V
}

}  // namespace binding
}  // namespace node
