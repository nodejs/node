#ifndef SRC_NODE_H_
#define SRC_NODE_H_

#ifdef _WIN32
# ifndef BUILDING_NODE_EXTENSION
#   define NODE_EXTERN __declspec(dllexport)
# else
#   define NODE_EXTERN __declspec(dllimport)
# endif
#else
# define NODE_EXTERN /* nothing */
#endif

#ifdef BUILDING_NODE_EXTENSION
# undef BUILDING_V8_SHARED
# undef BUILDING_UV_SHARED
# define USING_V8_SHARED 1
# define USING_UV_SHARED 1
#endif

// This should be defined in make system.
// See issue https://github.com/joyent/node/issues/1236
#if defined(__MINGW32__) || defined(_MSC_VER)
#ifndef _WIN32_WINNT
# define _WIN32_WINNT   0x0501
#endif

#ifndef NOMINMAX
# define NOMINMAX
#endif

#endif

#if defined(_MSC_VER)
#define PATH_MAX MAX_PATH
#endif

#ifdef _WIN32
# define SIGKILL         9
#endif

#include "v8.h"  // NOLINT(build/include_order)
#include "node_version.h"  // NODE_MODULE_VERSION

#define IOJS_MAKE_VERSION(major, minor, patch)                                \
  ((major) * 0x1000 + (minor) * 0x100 + (patch))

#ifdef __clang__
# define IOJS_CLANG_AT_LEAST(major, minor, patch)                             \
  (IOJS_MAKE_VERSION(major, minor, patch) <=                                  \
      IOJS_MAKE_VERSION(__clang_major__, __clang_minor__, __clang_patchlevel__))
#else
# define IOJS_CLANG_AT_LEAST(major, minor, patch) (0)
#endif

#ifdef __GNUC__
# define IOJS_GNUC_AT_LEAST(major, minor, patch)                              \
  (IOJS_MAKE_VERSION(major, minor, patch) <=                                  \
      IOJS_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__))
#else
# define IOJS_GNUC_AT_LEAST(major, minor, patch) (0)
#endif

#if IOJS_CLANG_AT_LEAST(2, 9, 0) || IOJS_GNUC_AT_LEAST(4, 5, 0)
# define NODE_DEPRECATED(message, declarator)                                 \
    __attribute__((deprecated(message))) declarator
#elif defined(_MSC_VER)
# define NODE_DEPRECATED(message, declarator)                                 \
    __declspec(deprecated) declarator
#else
# define NODE_DEPRECATED(message, declarator)                                 \
    declarator
#endif

// Forward-declare libuv loop
struct uv_loop_s;

// Forward-declare these functions now to stop MSVS from becoming
// terminally confused when it's done in node_internals.h
namespace node {

NODE_EXTERN v8::Local<v8::Value> ErrnoException(v8::Isolate* isolate,
                                                int errorno,
                                                const char* syscall = NULL,
                                                const char* message = NULL,
                                                const char* path = NULL);
NODE_EXTERN v8::Local<v8::Value> UVException(v8::Isolate* isolate,
                                             int errorno,
                                             const char* syscall = NULL,
                                             const char* message = NULL,
                                             const char* path = NULL);
NODE_EXTERN v8::Local<v8::Value> UVException(v8::Isolate* isolate,
                                             int errorno,
                                             const char* syscall,
                                             const char* message,
                                             const char* path,
                                             const char* dest);

NODE_DEPRECATED("Use UVException(isolate, ...)",
                inline v8::Local<v8::Value> ErrnoException(
      int errorno,
      const char* syscall = NULL,
      const char* message = NULL,
      const char* path = NULL) {
  return ErrnoException(v8::Isolate::GetCurrent(),
                        errorno,
                        syscall,
                        message,
                        path);
})

inline v8::Local<v8::Value> UVException(int errorno,
                                        const char* syscall = NULL,
                                        const char* message = NULL,
                                        const char* path = NULL) {
  return UVException(v8::Isolate::GetCurrent(),
                     errorno,
                     syscall,
                     message,
                     path);
}

/*
 * MakeCallback doesn't have a HandleScope. That means the callers scope
 * will retain ownership of created handles from MakeCallback and related.
 * There is by default a wrapping HandleScope before uv_run, if the caller
 * doesn't have a HandleScope on the stack the global will take ownership
 * which won't be reaped until the uv loop exits.
 *
 * If a uv callback is fired, and there is no enclosing HandleScope in the
 * cb, you will appear to leak 4-bytes for every invocation. Take heed.
 */

NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    v8::Isolate* isolate,
    v8::Handle<v8::Object> recv,
    const char* method,
    int argc,
    v8::Handle<v8::Value>* argv);
NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    v8::Isolate* isolate,
    v8::Handle<v8::Object> recv,
    v8::Handle<v8::String> symbol,
    int argc,
    v8::Handle<v8::Value>* argv);
NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    v8::Isolate* isolate,
    v8::Handle<v8::Object> recv,
    v8::Handle<v8::Function> callback,
    int argc,
    v8::Handle<v8::Value>* argv);

}  // namespace node

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#include "node_internals.h"
#endif

#include <assert.h>
#include <stdint.h>

#ifndef NODE_STRINGIFY
#define NODE_STRINGIFY(n) NODE_STRINGIFY_HELPER(n)
#define NODE_STRINGIFY_HELPER(n) #n
#endif

#ifdef _WIN32
// TODO(tjfontaine) consider changing the usage of ssize_t to ptrdiff_t
#if !defined(_SSIZE_T_) && !defined(_SSIZE_T_DEFINED)
typedef intptr_t ssize_t;
# define _SSIZE_T_
# define _SSIZE_T_DEFINED
#endif
#else  // !_WIN32
# include <sys/types.h>  // size_t, ssize_t
#endif  // _WIN32


namespace node {

NODE_EXTERN extern bool no_deprecation;

NODE_EXTERN int Start(int argc, char *argv[]);
NODE_EXTERN void Init(int* argc,
                      const char** argv,
                      int* exec_argc,
                      const char*** exec_argv);

class Environment;

NODE_EXTERN Environment* CreateEnvironment(v8::Isolate* isolate,
                                           struct uv_loop_s* loop,
                                           v8::Handle<v8::Context> context,
                                           int argc,
                                           const char* const* argv,
                                           int exec_argc,
                                           const char* const* exec_argv);
NODE_EXTERN void LoadEnvironment(Environment* env);

// NOTE: Calling this is the same as calling
// CreateEnvironment() + LoadEnvironment() from above.
// `uv_default_loop()` will be passed as `loop`.
NODE_EXTERN Environment* CreateEnvironment(v8::Isolate* isolate,
                                           v8::Handle<v8::Context> context,
                                           int argc,
                                           const char* const* argv,
                                           int exec_argc,
                                           const char* const* exec_argv);


NODE_EXTERN void EmitBeforeExit(Environment* env);
NODE_EXTERN int EmitExit(Environment* env);
NODE_EXTERN void RunAtExit(Environment* env);

/* Converts a unixtime to V8 Date */
#define NODE_UNIXTIME_V8(t) v8::Date::New(v8::Isolate::GetCurrent(),          \
    1000 * static_cast<double>(t))
#define NODE_V8_UNIXTIME(v) (static_cast<double>((v)->NumberValue())/1000.0);

// Used to be a macro, hence the uppercase name.
#define NODE_DEFINE_CONSTANT(target, constant)                                \
  do {                                                                        \
    v8::Isolate* isolate = target->GetIsolate();                              \
    v8::Local<v8::String> constant_name =                                     \
        v8::String::NewFromUtf8(isolate, #constant);                          \
    v8::Local<v8::Number> constant_value =                                    \
        v8::Number::New(isolate, static_cast<double>(constant));              \
    v8::PropertyAttribute constant_attributes =                               \
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);    \
    (target)->ForceSet(constant_name, constant_value, constant_attributes);   \
  }                                                                           \
  while (0)

// Used to be a macro, hence the uppercase name.
template <typename TypeName>
inline void NODE_SET_METHOD(const TypeName& recv,
                            const char* name,
                            v8::FunctionCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(isolate,
                                                                callback);
  v8::Local<v8::Function> fn = t->GetFunction();
  v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name);
  fn->SetName(fn_name);
  recv->Set(fn_name, fn);
}
#define NODE_SET_METHOD node::NODE_SET_METHOD

// Used to be a macro, hence the uppercase name.
// Not a template because it only makes sense for FunctionTemplates.
inline void NODE_SET_PROTOTYPE_METHOD(v8::Handle<v8::FunctionTemplate> recv,
                                      const char* name,
                                      v8::FunctionCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Signature> s = v8::Signature::New(isolate, recv);
  v8::Local<v8::FunctionTemplate> t =
      v8::FunctionTemplate::New(isolate, callback, v8::Handle<v8::Value>(), s);
  v8::Local<v8::Function> fn = t->GetFunction();
  recv->PrototypeTemplate()->Set(v8::String::NewFromUtf8(isolate, name), fn);
  v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name);
  fn->SetName(fn_name);
}
#define NODE_SET_PROTOTYPE_METHOD node::NODE_SET_PROTOTYPE_METHOD

enum encoding {ASCII, UTF8, BASE64, UCS2, BINARY, HEX, BUFFER};
NODE_EXTERN enum encoding ParseEncoding(
    v8::Isolate* isolate,
    v8::Handle<v8::Value> encoding_v,
    enum encoding default_encoding = BINARY);
NODE_DEPRECATED("Use ParseEncoding(isolate, ...)",
                inline enum encoding ParseEncoding(
      v8::Handle<v8::Value> encoding_v,
      enum encoding default_encoding = BINARY) {
  return ParseEncoding(v8::Isolate::GetCurrent(), encoding_v, default_encoding);
})

NODE_EXTERN void FatalException(v8::Isolate* isolate,
                                const v8::TryCatch& try_catch);

NODE_DEPRECATED("Use FatalException(isolate, ...)",
                inline void FatalException(const v8::TryCatch& try_catch) {
  return FatalException(v8::Isolate::GetCurrent(), try_catch);
})

// Don't call with encoding=UCS2.
NODE_EXTERN v8::Local<v8::Value> Encode(v8::Isolate* isolate,
                                        const char* buf,
                                        size_t len,
                                        enum encoding encoding = BINARY);

// The input buffer should be in host endianness.
NODE_EXTERN v8::Local<v8::Value> Encode(v8::Isolate* isolate,
                                        const uint16_t* buf,
                                        size_t len);

NODE_DEPRECATED("Use Encode(isolate, ...)",
                inline v8::Local<v8::Value> Encode(
    const void* buf,
    size_t len,
    enum encoding encoding = BINARY) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (encoding == UCS2) {
    assert(reinterpret_cast<uintptr_t>(buf) % sizeof(uint16_t) == 0 &&
           "UCS2 buffer must be aligned on two-byte boundary.");
    const uint16_t* that = static_cast<const uint16_t*>(buf);
    return Encode(isolate, that, len / sizeof(*that));
  }
  return Encode(isolate, static_cast<const char*>(buf), len, encoding);
})

// Returns -1 if the handle was not valid for decoding
NODE_EXTERN ssize_t DecodeBytes(v8::Isolate* isolate,
                                v8::Handle<v8::Value>,
                                enum encoding encoding = BINARY);
NODE_DEPRECATED("Use DecodeBytes(isolate, ...)",
                inline ssize_t DecodeBytes(
    v8::Handle<v8::Value> val,
    enum encoding encoding = BINARY) {
  return DecodeBytes(v8::Isolate::GetCurrent(), val, encoding);
})

// returns bytes written.
NODE_EXTERN ssize_t DecodeWrite(v8::Isolate* isolate,
                                char* buf,
                                size_t buflen,
                                v8::Handle<v8::Value>,
                                enum encoding encoding = BINARY);
NODE_DEPRECATED("Use DecodeWrite(isolate, ...)",
                inline ssize_t DecodeWrite(char* buf,
                                           size_t buflen,
                                           v8::Handle<v8::Value> val,
                                           enum encoding encoding = BINARY) {
  return DecodeWrite(v8::Isolate::GetCurrent(), buf, buflen, val, encoding);
})

#ifdef _WIN32
NODE_EXTERN v8::Local<v8::Value> WinapiErrnoException(
    v8::Isolate* isolate,
    int errorno,
    const char *syscall = NULL,
    const char *msg = "",
    const char *path = NULL);

NODE_DEPRECATED("Use WinapiErrnoException(isolate, ...)",
                inline v8::Local<v8::Value> WinapiErrnoException(int errorno,
    const char *syscall = NULL,  const char *msg = "",
    const char *path = NULL) {
  return WinapiErrnoException(v8::Isolate::GetCurrent(),
                              errorno,
                              syscall,
                              msg,
                              path);
})
#endif

const char *signo_string(int errorno);

//==============================================================================
// Init Function Argument Discomvobulator
//==============================================================================
namespace detail {

typedef void (*addon_register_func)(
    void * init_function,
    v8::Local<v8::Object> exports,
    v8::Local<v8::Object> module,
    v8::Local<v8::Context> context,
    void * priv);

// This template is used to select the optional arguments of the (addon) init
// function. Each specialization returns the corresponding argument.
template <typename T> struct OptionalInitArg;

template <>
struct OptionalInitArg<v8::Local<v8::Object> > {
  static inline
  v8::Local<v8::Object>
  pick(v8::Local<v8::Object> module, v8::Local<v8::Context>, void *) {
    return module;
  }
};

template <>
struct OptionalInitArg<v8::Local<v8::Context> > {
  static inline
  v8::Local<v8::Context>
  pick(v8::Local<v8::Object>, v8::Local<v8::Context> context, void *) {
    return context;
  }
};

template <>
struct OptionalInitArg<void*> {
  static inline
  void*
  pick(v8::Local<v8::Object>, v8::Local<v8::Context>, void * private_) {
    return private_;
  }
};

// Template that takes the type of the init function as an argument. It is used
// to inspect the arguments of the init function at compile time and provide a
// suitable adapter registerAddon(...). This adapter is always of type
// addon_register_func. It calls the actual init function with the "requested"
// arguments. To put it differently, the implementation of registerAddon(...)
// is selected (or generated, if you like) based on the functions signature.
template <typename F> struct AddonInitAdapter;

// Partial specialization: Allow only function pointers with the following
// properties:
//   - returns void
//   - takes a Local<Object> as first argument (exports)
//   - takes zero or more additional arguments of arbitrary type
//
// To further narrow it down it uses a little bit of SFINAE. It only matches
// if there is a suitable specialization of OptionalInitArg<> for each
// additional argument. See registerAddon(...) below. This limits the argument
// types to:
//   - Local<Object>   (the module)
//   - Local<Context>  (the context, duh)
//   - void*           (the private pointer)
//
// The interesting thing about this application of SFINAE is that we use it
// to trigger a compile-time error. Since the generic version of
// AddonInitAdapter<> is only declared but never defined, the compiler bails
// after the substitution failure.
template <typename... Args>
struct AddonInitAdapter<void (*)(v8::Local<v8::Object>, Args...)> {
  typedef void (*init_function)(v8::Local<v8::Object>, Args...);

  static
  void
  registerAddon(void * f,
                v8::Local<v8::Object> exports,
                v8::Local<v8::Object> module,
                v8::Local<v8::Context> context,
                void * priv) {
    // restore function pointer type
    init_function init(reinterpret_cast<init_function>(f));
    // call it
    init(exports, OptionalInitArg<Args>::pick(module, context, priv)...);
  }
};

// Main entry point into the init-fuction-argument-discomvobulator. It is
// called with an init function as argument and returns a suitable
// addon_register_func. Note how a template function is used to capture
// the type F. A user of this function (our NODE_MODULE_X(...) macro, below)
// does not have to provide a type. No function pointer types, not even angular
// brackets at the call site. (Remember this pattern. It's pretty powerful)
template <typename F>
addon_register_func
selectAddonRegisterFunction(F f) {
  return AddonInitAdapter<F>::registerAddon;
}

}  // end of namespace detail

enum node_module_flags {
  NM_F_BUILTIN                     = (1<<0),
  NM_F_LINKED                      = (1<<1),
  // Used to emit a deprecation warning. Remove once
  // NODE_MODULE_CONTEXT_AWARE is phased out.
  NM_F_NODE_MODULE_CONTEXT_AWARE_IS_DEPRECATED = (1<<31)
};

struct node_module {
  int nm_version;
  unsigned int nm_flags;
  void* nm_dso_handle;
  const char* nm_filename;
  node::detail::addon_register_func nm_register_func;
  void* nm_init;
  const char* nm_modname;
  void* nm_priv;
  struct node_module* nm_link;
};

node_module* get_builtin_module(const char *name);
node_module* get_linked_module(const char *name);

extern "C" NODE_EXTERN void node_module_register(void* mod);

#ifdef _WIN32
# define NODE_MODULE_EXPORT __declspec(dllexport)
#else
# define NODE_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define NODE_C_CTOR(fn)                                               \
  static void __cdecl fn(void);                                       \
  __declspec(dllexport, allocate(".CRT$XCU"))                         \
      void (__cdecl*fn ## _)(void) = fn;                              \
  static void __cdecl fn(void)
#else
#define NODE_C_CTOR(fn)                                               \
  static void fn(void) __attribute__((constructor));                  \
  static void fn(void)
#endif

#define NODE_MODULE_X(modname, initfunc, priv, flags)                 \
  extern "C" {                                                        \
    static node::node_module _module_ ## modname =                    \
    {                                                                 \
      NODE_MODULE_VERSION,                                            \
      flags,                                                          \
      NULL,                                                           \
      __FILE__,                                                       \
      node::detail::selectAddonRegisterFunction(initfunc),            \
      reinterpret_cast<void*>(initfunc),                              \
      NODE_STRINGIFY(modname),                                        \
      priv,                                                           \
      NULL                                                            \
    };                                                                \
    NODE_C_CTOR(_register_ ## modname) {                              \
      node_module_register(&_module_ ## modname);                     \
    }                                                                 \
  }

#define NODE_MODULE(modname, initfunc)                                \
  NODE_MODULE_X(modname, initfunc, NULL, 0)

// NOTE(agnat): Deprecated. Just use NODE_MODULE(...)
#define NODE_MODULE_CONTEXT_AWARE(modname, initfunc)                  \
  NODE_MODULE_X(modname, initfunc, NULL,                              \
      NM_F_NODE_MODULE_CONTEXT_AWARE_IS_DEPRECATED)

#define NODE_MODULE_BUILTIN(modname, initfunc)                        \
  NODE_MODULE_X(modname, initfunc, NULL, node::NM_F_BUILTIN)

/*
 * For backward compatibility in add-on modules.
 */
#define NODE_MODULE_DECL /* nothing */

/* Called after the event loop exits but before the VM is disposed.
 * Callbacks are run in reverse order of registration, i.e. newest first.
 */
NODE_EXTERN void AtExit(void (*cb)(void* arg), void* arg = 0);

}  // namespace node

#endif  // SRC_NODE_H_
