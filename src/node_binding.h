#ifndef SRC_NODE_BINDING_H_
#define SRC_NODE_BINDING_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if defined(__POSIX__)
#include <dlfcn.h>
#endif

#include <string>

#include "node.h"
#include "node_api.h"
#include "quic/guard.h"
#include "uv.h"

enum {
  NM_F_BUILTIN = 1 << 0,  // Unused.
  NM_F_LINKED = 1 << 1,
  NM_F_INTERNAL = 1 << 2,
  NM_F_DELETEME = 1 << 3,
};

// Make sure our internal values match the public API's values.
static_assert(static_cast<int>(NM_F_LINKED) ==
                  static_cast<int>(node::ModuleFlags::kLinked),
              "NM_F_LINKED != node::ModuleFlags::kLinked");

#if NODE_HAVE_I18N_SUPPORT
#define NODE_BUILTIN_ICU_BINDINGS(V) V(icu)
#else
#define NODE_BUILTIN_ICU_BINDINGS(V)
#endif

#if HAVE_OPENSSL && OPENSSL_NO_QUIC != 1
#define NODE_BUILTIN_QUIC_BINDINGS(V) V(quic)
#else
#define NODE_BUILTIN_QUIC_BINDINGS(V)
#endif

#if HAVE_OPENSSL && HAVE_DTLS
#define NODE_BUILTIN_DTLS_BINDINGS(V) V(dtls)
#else
#define NODE_BUILTIN_DTLS_BINDINGS(V)
#endif

#if HAVE_SQLITE
#define NODE_BUILTIN_SQLITE_BINDINGS(V)                                        \
  V(sqlite)                                                                    \
  V(webstorage)
#else
#define NODE_BUILTIN_SQLITE_BINDINGS(V)
#endif

#if HAVE_FFI
#define NODE_BUILTIN_FFI_BINDINGS(V) V(ffi)
#else
#define NODE_BUILTIN_FFI_BINDINGS(V)
#endif

#define NODE_BINDINGS_WITH_PER_ISOLATE_INIT(V)                                 \
  V(async_wrap)                                                                \
  V(blob)                                                                      \
  V(builtins)                                                                  \
  V(contextify)                                                                \
  V(diagnostics_channel)                                                       \
  V(encoding_binding)                                                          \
  V(fs)                                                                        \
  V(fs_dir)                                                                    \
  V(http_parser)                                                               \
  V(locks)                                                                     \
  V(messaging)                                                                 \
  V(mksnapshot)                                                                \
  V(modules)                                                                   \
  V(module_wrap)                                                               \
  V(performance)                                                               \
  V(process_methods)                                                           \
  V(timers)                                                                    \
  V(url)                                                                       \
  V(worker)                                                                    \
  NODE_BUILTIN_ICU_BINDINGS(V)                                                 \
  NODE_BUILTIN_QUIC_BINDINGS(V)                                                \
  NODE_BUILTIN_DTLS_BINDINGS(V)

#define NODE_BINDING_CONTEXT_AWARE_CPP(modname, regfunc, priv, flags)          \
  static node::node_module _module = {                                         \
      NODE_MODULE_VERSION,                                                     \
      flags,                                                                   \
      nullptr,                                                                 \
      __FILE__,                                                                \
      nullptr,                                                                 \
      (node::addon_context_register_func)(regfunc),                            \
      NODE_STRINGIFY(modname),                                                 \
      priv,                                                                    \
      nullptr};                                                                \
  void _register_##modname() { node_module_register(&_module); }

void napi_module_register_by_symbol(
    v8::Local<v8::Object> exports,
    v8::Local<v8::Value> module,
    v8::Local<v8::Context> context,
    napi_addon_register_func init,
    int32_t module_api_version = NODE_API_DEFAULT_MODULE_API_VERSION);

node::addon_context_register_func get_node_api_context_register_func(
    node::Environment* node_env,
    const char* module_name,
    int32_t module_api_version);

namespace node {

// Define a node internal binding that may be loaded in a context of
// a node::Environment.
// If an internal binding needs initializing per-isolate templates, define
// with NODE_BINDING_PER_ISOLATE_INIT too.
#define NODE_BINDING_CONTEXT_AWARE_INTERNAL(modname, regfunc)                  \
  NODE_BINDING_CONTEXT_AWARE_CPP(modname, regfunc, nullptr, NM_F_INTERNAL)

// Define a per-isolate initialization function for a node internal binding.
// The modname should be registered in the NODE_BINDINGS_WITH_PER_ISOLATE_INIT
// list.
#define NODE_BINDING_PER_ISOLATE_INIT(modname, per_isolate_func)               \
  void _register_isolate_##modname(node::IsolateData* isolate_data,            \
                                   v8::Local<v8::ObjectTemplate> target) {     \
    per_isolate_func(isolate_data, target);                                    \
  }

// Globals per process
// This is set by node::Init() which is used by embedders
extern bool node_is_initialized;

namespace binding {

class DLib {
 public:
#ifdef __POSIX__
  static const int kDefaultFlags = RTLD_LAZY;
#else
  static const int kDefaultFlags = 0;
#endif

  DLib(const char* filename, int flags);

  bool Open();
  void Close();
  void* GetSymbolAddress(const char* name);
  void SaveInGlobalHandleMap(node_module* mp);
  node_module* GetSavedModuleFromGlobalHandleMap();

  const std::string filename_;
  const int flags_;
  std::string errmsg_;
  void* handle_;
#ifndef __POSIX__
  uv_lib_t lib_;
#endif
  bool has_entry_in_global_handle_map_ = false;

  DLib(const DLib&) = delete;
  DLib& operator=(const DLib&) = delete;
};

// Call _register<module_name> functions for all of
// the built-in bindings. Because built-in bindings don't
// use the __attribute__((constructor)). Need to
// explicitly call the _register* functions.
void RegisterBuiltinBindings();
// Create per-isolate templates for the internal bindings.
void CreateInternalBindingTemplates(IsolateData* isolate_data);
void GetInternalBinding(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetLinkedBinding(const v8::FunctionCallbackInfo<v8::Value>& args);
void DLOpen(const v8::FunctionCallbackInfo<v8::Value>& args);
void DLOpenBinary(const v8::FunctionCallbackInfo<v8::Value>& args);

// Materializes the bytes of a dynamically shared object into a form
// dlopen()/LoadLibrary() can load, with the smallest, most private on-disk
// footprint each platform allows:
//   Linux:        an anonymous in-memory memfd, loaded via /proc/self/fd/N -
//                 the bytes never touch the filesystem.
//   other POSIX:  a 0700 mkdtemp() directory plus an O_EXCL|O_NOFOLLOW file,
//                 unlink()ed right after the load (the mapping keeps it alive).
//   Windows:      a temp file, written and closed before the load because the
//                 loader shares read alone. It cannot be unlinked while its
//                 image is mapped, so it is kept with the module it loaded as
//                 and both are released at process exit.
// Used for a native addon or an FFI library that lives somewhere the dynamic
// loader cannot open by path, such as a virtual file system. Call exactly one
// of Materialize()+AfterOpen() around the load; a destroyed image that never
// reached AfterOpen() cleans up after itself.
class AddonImage {
 public:
  AddonImage() = default;
  ~AddonImage();
  AddonImage(const AddonImage&) = delete;
  AddonImage& operator=(const AddonImage&) = delete;

  // The directory a temporary image would be written to, with a trailing
  // separator; empty when it cannot be determined. Names the resource for the
  // file-system permission check.
  static std::string TempDir();

  // On success sets path() to a real, loadable path for `data`.
  bool Materialize(const char* data, size_t len);
  const std::string& path() const { return path_; }
  const std::string& errmsg() const { return errmsg_; }

  // Call exactly once, right after the load; `opened` says whether the load
  // succeeded and `module` is the module handle it produced. Releases what is
  // no longer needed: on POSIX closes the memfd or unlinks the temp file, which
  // a successful load keeps alive through its own mapping. Windows cannot
  // unlink a mapped image, so there the file is removed at once only when the
  // load failed; otherwise it is kept, with `module`, until process exit, where
  // the module is unloaded and the file finally deleted.
  void AfterOpen(bool opened, void* module);

 private:
  std::string path_;
  std::string errmsg_;
  bool consumed_ = false;
#ifdef _WIN32
  std::wstring wpath_;  // the path of the image, to delete it again at exit
#else
  bool MaterializeTempFile(const char* data, size_t len);
  int fd_ = -1;
  std::string temp_dir_;
#endif
};

}  // namespace binding

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_BINDING_H_
