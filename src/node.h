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

#define NODE_MAKE_VERSION(major, minor, patch)                                \
  ((major) * 0x1000 + (minor) * 0x100 + (patch))

#ifdef __clang__
# define NODE_CLANG_AT_LEAST(major, minor, patch)                             \
  (NODE_MAKE_VERSION(major, minor, patch) <=                                  \
      NODE_MAKE_VERSION(__clang_major__, __clang_minor__, __clang_patchlevel__))
#else
# define NODE_CLANG_AT_LEAST(major, minor, patch) (0)
#endif

#ifdef __GNUC__
# define NODE_GNUC_AT_LEAST(major, minor, patch)                              \
  (NODE_MAKE_VERSION(major, minor, patch) <=                                  \
      NODE_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__))
#else
# define NODE_GNUC_AT_LEAST(major, minor, patch) (0)
#endif

#if NODE_CLANG_AT_LEAST(2, 9, 0) || NODE_GNUC_AT_LEAST(4, 5, 0)
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

NODE_DEPRECATED("Use ErrnoException(isolate, ...)",
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

NODE_EXTERN v8::Local<v8::Value> MakeCallback(
    v8::Isolate* isolate,
    v8::Local<v8::Object> recv,
    const char* method,
    int argc,
    v8::Local<v8::Value>* argv);
NODE_EXTERN v8::Local<v8::Value> MakeCallback(
    v8::Isolate* isolate,
    v8::Local<v8::Object> recv,
    v8::Local<v8::String> symbol,
    int argc,
    v8::Local<v8::Value>* argv);
NODE_EXTERN v8::Local<v8::Value> MakeCallback(
    v8::Isolate* isolate,
    v8::Local<v8::Object> recv,
    v8::Local<v8::Function> callback,
    int argc,
    v8::Local<v8::Value>* argv);

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
#if HAVE_OPENSSL && NODE_FIPS_MODE
NODE_EXTERN extern bool enable_fips_crypto;
NODE_EXTERN extern bool force_fips_crypto;
#endif

NODE_EXTERN int Start(int argc, char *argv[]);
NODE_EXTERN void Init(int* argc,
                      const char** argv,
                      int* exec_argc,
                      const char*** exec_argv);

class IsolateData;
class Environment;

NODE_EXTERN IsolateData* CreateIsolateData(v8::Isolate* isolate,
                                           struct uv_loop_s* loop);
NODE_EXTERN void FreeIsolateData(IsolateData* isolate_data);

NODE_EXTERN Environment* CreateEnvironment(IsolateData* isolate_data,
                                           v8::Local<v8::Context> context,
                                           int argc,
                                           const char* const* argv,
                                           int exec_argc,
                                           const char* const* exec_argv);

NODE_EXTERN void LoadEnvironment(Environment* env);
NODE_EXTERN void FreeEnvironment(Environment* env);

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
    v8::Local<v8::Context> context = isolate->GetCurrentContext();            \
    v8::Local<v8::String> constant_name =                                     \
        v8::String::NewFromUtf8(isolate, #constant);                          \
    v8::Local<v8::Number> constant_value =                                    \
        v8::Number::New(isolate, static_cast<double>(constant));              \
    v8::PropertyAttribute constant_attributes =                               \
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);    \
    (target)->DefineOwnProperty(context,                                      \
                                constant_name,                                \
                                constant_value,                               \
                                constant_attributes).FromJust();              \
  }                                                                           \
  while (0)

// Used to be a macro, hence the uppercase name.
inline void NODE_SET_METHOD(v8::Local<v8::Template> recv,
                            const char* name,
                            v8::FunctionCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(isolate,
                                                                callback);
  v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name);
  t->SetClassName(fn_name);
  recv->Set(fn_name, t);
}

// Used to be a macro, hence the uppercase name.
inline void NODE_SET_METHOD(v8::Local<v8::Object> recv,
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
inline void NODE_SET_PROTOTYPE_METHOD(v8::Local<v8::FunctionTemplate> recv,
                                      const char* name,
                                      v8::FunctionCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Signature> s = v8::Signature::New(isolate, recv);
  v8::Local<v8::FunctionTemplate> t =
      v8::FunctionTemplate::New(isolate, callback, v8::Local<v8::Value>(), s);
  v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name);
  t->SetClassName(fn_name);
  recv->PrototypeTemplate()->Set(v8::String::NewFromUtf8(isolate, name), t);
}
#define NODE_SET_PROTOTYPE_METHOD node::NODE_SET_PROTOTYPE_METHOD

// BINARY is a deprecated alias of LATIN1.
enum encoding {ASCII, UTF8, BASE64, UCS2, BINARY, HEX, BUFFER, LATIN1 = BINARY};

NODE_EXTERN enum encoding ParseEncoding(
    v8::Isolate* isolate,
    v8::Local<v8::Value> encoding_v,
    enum encoding default_encoding = LATIN1);
NODE_DEPRECATED("Use ParseEncoding(isolate, ...)",
                inline enum encoding ParseEncoding(
      v8::Local<v8::Value> encoding_v,
      enum encoding default_encoding = LATIN1) {
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
                                        enum encoding encoding = LATIN1);

// The input buffer should be in host endianness.
NODE_EXTERN v8::Local<v8::Value> Encode(v8::Isolate* isolate,
                                        const uint16_t* buf,
                                        size_t len);

NODE_DEPRECATED("Use Encode(isolate, ...)",
                inline v8::Local<v8::Value> Encode(
    const void* buf,
    size_t len,
    enum encoding encoding = LATIN1) {
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
                                v8::Local<v8::Value>,
                                enum encoding encoding = LATIN1);
NODE_DEPRECATED("Use DecodeBytes(isolate, ...)",
                inline ssize_t DecodeBytes(
    v8::Local<v8::Value> val,
    enum encoding encoding = LATIN1) {
  return DecodeBytes(v8::Isolate::GetCurrent(), val, encoding);
})

// returns bytes written.
NODE_EXTERN ssize_t DecodeWrite(v8::Isolate* isolate,
                                char* buf,
                                size_t buflen,
                                v8::Local<v8::Value>,
                                enum encoding encoding = LATIN1);
NODE_DEPRECATED("Use DecodeWrite(isolate, ...)",
                inline ssize_t DecodeWrite(char* buf,
                                           size_t buflen,
                                           v8::Local<v8::Value> val,
                                           enum encoding encoding = LATIN1) {
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


typedef void (*addon_register_func)(
    v8::Local<v8::Object> exports,
    v8::Local<v8::Value> module,
    void* priv);

typedef void (*addon_context_register_func)(
    v8::Local<v8::Object> exports,
    v8::Local<v8::Value> module,
    v8::Local<v8::Context> context,
    void* priv);

#define NM_F_BUILTIN 0x01
#define NM_F_LINKED  0x02

struct node_module {
  int nm_version;
  unsigned int nm_flags;
  void* nm_dso_handle;
  const char* nm_filename;
  node::addon_register_func nm_register_func;
  node::addon_context_register_func nm_context_register_func;
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

#ifdef NODE_SHARED_MODE
# define NODE_CTOR_PREFIX
#else
# define NODE_CTOR_PREFIX static
#endif

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define NODE_C_CTOR(fn)                                               \
  NODE_CTOR_PREFIX void __cdecl fn(void);                             \
  __declspec(dllexport, allocate(".CRT$XCU"))                         \
      void (__cdecl*fn ## _)(void) = fn;                              \
  NODE_CTOR_PREFIX void __cdecl fn(void)
#else
#define NODE_C_CTOR(fn)                                               \
  NODE_CTOR_PREFIX void fn(void) __attribute__((constructor));        \
  NODE_CTOR_PREFIX void fn(void)
#endif

#define NODE_MODULE_X(modname, regfunc, priv, flags)                  \
  extern "C" {                                                        \
    static node::node_module _module =                                \
    {                                                                 \
      NODE_MODULE_VERSION,                                            \
      flags,                                                          \
      NULL,                                                           \
      __FILE__,                                                       \
      (node::addon_register_func) (regfunc),                          \
      NULL,                                                           \
      NODE_STRINGIFY(modname),                                        \
      priv,                                                           \
      NULL                                                            \
    };                                                                \
    NODE_C_CTOR(_register_ ## modname) {                              \
      node_module_register(&_module);                                 \
    }                                                                 \
  }

#define NODE_MODULE_CONTEXT_AWARE_X(modname, regfunc, priv, flags)    \
  extern "C" {                                                        \
    static node::node_module _module =                                \
    {                                                                 \
      NODE_MODULE_VERSION,                                            \
      flags,                                                          \
      NULL,                                                           \
      __FILE__,                                                       \
      NULL,                                                           \
      (node::addon_context_register_func) (regfunc),                  \
      NODE_STRINGIFY(modname),                                        \
      priv,                                                           \
      NULL                                                            \
    };                                                                \
    NODE_C_CTOR(_register_ ## modname) {                              \
      node_module_register(&_module);                                 \
    }                                                                 \
  }

#define NODE_MODULE(modname, regfunc)                                 \
  NODE_MODULE_X(modname, regfunc, NULL, 0)

#define NODE_MODULE_CONTEXT_AWARE(modname, regfunc)                   \
  NODE_MODULE_CONTEXT_AWARE_X(modname, regfunc, NULL, 0)

#define NODE_MODULE_CONTEXT_AWARE_BUILTIN(modname, regfunc)           \
  NODE_MODULE_CONTEXT_AWARE_X(modname, regfunc, NULL, NM_F_BUILTIN)   \

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
