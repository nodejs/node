// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
// See issue https://github.com/nodejs/node-v0.x-archive/issues/1236
#if defined(__MINGW32__) || defined(_MSC_VER)
#ifndef _WIN32_WINNT
# define _WIN32_WINNT   0x0600  // Windows Server 2008
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
#include "v8-platform.h"  // NOLINT(build/include_order)
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

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
# define NODE_DEPRECATED(message, declarator) declarator
#else  // NODE_WANT_INTERNALS
# if NODE_CLANG_AT_LEAST(2, 9, 0) || NODE_GNUC_AT_LEAST(4, 5, 0)
#  define NODE_DEPRECATED(message, declarator)                                 \
    __attribute__((deprecated(message))) declarator
# elif defined(_MSC_VER)
#  define NODE_DEPRECATED(message, declarator)                                 \
    __declspec(deprecated) declarator
# else
#  define NODE_DEPRECATED(message, declarator) declarator
# endif
#endif

// Forward-declare libuv loop
struct uv_loop_s;

// Forward-declare these functions now to stop MSVS from becoming
// terminally confused when it's done in node_internals.h
namespace node {

namespace tracing {

class TracingController;

}

NODE_EXTERN v8::Local<v8::Value> ErrnoException(v8::Isolate* isolate,
                                                int errorno,
                                                const char* syscall = nullptr,
                                                const char* message = nullptr,
                                                const char* path = nullptr);
NODE_EXTERN v8::Local<v8::Value> UVException(v8::Isolate* isolate,
                                             int errorno,
                                             const char* syscall = nullptr,
                                             const char* message = nullptr,
                                             const char* path = nullptr);
NODE_EXTERN v8::Local<v8::Value> UVException(v8::Isolate* isolate,
                                             int errorno,
                                             const char* syscall,
                                             const char* message,
                                             const char* path,
                                             const char* dest);

NODE_DEPRECATED("Use ErrnoException(isolate, ...)",
                inline v8::Local<v8::Value> ErrnoException(
      int errorno,
      const char* syscall = nullptr,
      const char* message = nullptr,
      const char* path = nullptr) {
  return ErrnoException(v8::Isolate::GetCurrent(),
                        errorno,
                        syscall,
                        message,
                        path);
})

NODE_DEPRECATED("Use UVException(isolate, ...)",
                inline v8::Local<v8::Value> UVException(int errorno,
                                        const char* syscall = nullptr,
                                        const char* message = nullptr,
                                        const char* path = nullptr) {
  return UVException(v8::Isolate::GetCurrent(),
                     errorno,
                     syscall,
                     message,
                     path);
})

/*
 * These methods need to be called in a HandleScope.
 *
 * It is preferred that you use the `MakeCallback` overloads taking
 * `async_context` arguments.
 */

NODE_DEPRECATED("Use MakeCallback(..., async_context)",
                NODE_EXTERN v8::Local<v8::Value> MakeCallback(
                    v8::Isolate* isolate,
                    v8::Local<v8::Object> recv,
                    const char* method,
                    int argc,
                    v8::Local<v8::Value>* argv));
NODE_DEPRECATED("Use MakeCallback(..., async_context)",
                NODE_EXTERN v8::Local<v8::Value> MakeCallback(
                    v8::Isolate* isolate,
                    v8::Local<v8::Object> recv,
                    v8::Local<v8::String> symbol,
                    int argc,
                    v8::Local<v8::Value>* argv));
NODE_DEPRECATED("Use MakeCallback(..., async_context)",
                NODE_EXTERN v8::Local<v8::Value> MakeCallback(
                    v8::Isolate* isolate,
                    v8::Local<v8::Object> recv,
                    v8::Local<v8::Function> callback,
                    int argc,
                    v8::Local<v8::Value>* argv));

}  // namespace node

#include <assert.h>
#include <stdint.h>

#ifndef NODE_STRINGIFY
#define NODE_STRINGIFY(n) NODE_STRINGIFY_HELPER(n)
#define NODE_STRINGIFY_HELPER(n) #n
#endif

#ifdef _WIN32
#if !defined(_SSIZE_T_) && !defined(_SSIZE_T_DEFINED)
typedef intptr_t ssize_t;
# define _SSIZE_T_
# define _SSIZE_T_DEFINED
#endif
#else  // !_WIN32
# include <sys/types.h>  // size_t, ssize_t
#endif  // _WIN32


namespace node {

// TODO(addaleax): Remove all of these.
NODE_DEPRECATED("use command-line flags",
                NODE_EXTERN extern bool no_deprecation);
#if HAVE_OPENSSL
NODE_DEPRECATED("use command-line flags",
                NODE_EXTERN extern bool ssl_openssl_cert_store);
# if NODE_FIPS_MODE
NODE_DEPRECATED("use command-line flags",
                NODE_EXTERN extern bool enable_fips_crypto);
NODE_DEPRECATED("user command-line flags",
                NODE_EXTERN extern bool force_fips_crypto);
# endif
#endif

// TODO(addaleax): Officially deprecate this and replace it with something
// better suited for a public embedder API.
NODE_EXTERN int Start(int argc, char* argv[]);

// TODO(addaleax): Officially deprecate this and replace it with something
// better suited for a public embedder API.
NODE_EXTERN void Init(int* argc,
                      const char** argv,
                      int* exec_argc,
                      const char*** exec_argv);

class ArrayBufferAllocator;

NODE_EXTERN ArrayBufferAllocator* CreateArrayBufferAllocator();
NODE_EXTERN void FreeArrayBufferAllocator(ArrayBufferAllocator* allocator);

class IsolateData;
class Environment;

class NODE_EXTERN MultiIsolatePlatform : public v8::Platform {
 public:
  virtual ~MultiIsolatePlatform() { }
  // Returns true if work was dispatched or executed. New tasks that are
  // posted during flushing of the queue are postponed until the next
  // flushing.
  virtual bool FlushForegroundTasks(v8::Isolate* isolate) = 0;
  virtual void DrainBackgroundTasks(v8::Isolate* isolate) = 0;
  virtual void CancelPendingDelayedTasks(v8::Isolate* isolate) = 0;

  // These will be called by the `IsolateData` creation/destruction functions.
  virtual void RegisterIsolate(IsolateData* isolate_data,
                               struct uv_loop_s* loop) = 0;
  virtual void UnregisterIsolate(IsolateData* isolate_data) = 0;
};

// Creates a new isolate with Node.js-specific settings.
NODE_EXTERN v8::Isolate* NewIsolate(ArrayBufferAllocator* allocator);

// Creates a new context with Node.js-specific tweaks.
NODE_EXTERN v8::Local<v8::Context> NewContext(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template =
        v8::Local<v8::ObjectTemplate>());

// If `platform` is passed, it will be used to register new Worker instances.
// It can be `nullptr`, in which case creating new Workers inside of
// Environments that use this `IsolateData` will not work.
// TODO(helloshuangzi): switch to default parameters.
NODE_EXTERN IsolateData* CreateIsolateData(
    v8::Isolate* isolate,
    struct uv_loop_s* loop);
NODE_EXTERN IsolateData* CreateIsolateData(
    v8::Isolate* isolate,
    struct uv_loop_s* loop,
    MultiIsolatePlatform* platform);
NODE_EXTERN IsolateData* CreateIsolateData(
    v8::Isolate* isolate,
    struct uv_loop_s* loop,
    MultiIsolatePlatform* platform,
    ArrayBufferAllocator* allocator);
NODE_EXTERN void FreeIsolateData(IsolateData* isolate_data);

// TODO(addaleax): Add an official variant using STL containers, and move
// per-Environment options parsing here.
NODE_EXTERN Environment* CreateEnvironment(IsolateData* isolate_data,
                                           v8::Local<v8::Context> context,
                                           int argc,
                                           const char* const* argv,
                                           int exec_argc,
                                           const char* const* exec_argv);

NODE_EXTERN void LoadEnvironment(Environment* env);
NODE_EXTERN void FreeEnvironment(Environment* env);

// This may return nullptr if context is not associated with a Node instance.
NODE_EXTERN Environment* GetCurrentEnvironment(v8::Local<v8::Context> context);

// This returns the MultiIsolatePlatform used in the main thread of Node.js.
// If NODE_USE_V8_PLATFORM haven't been defined when Node.js was built,
// it returns nullptr.
NODE_EXTERN MultiIsolatePlatform* GetMainThreadMultiIsolatePlatform();

NODE_EXTERN MultiIsolatePlatform* CreatePlatform(
    int thread_pool_size,
    node::tracing::TracingController* tracing_controller);
MultiIsolatePlatform* InitializeV8Platform(int thread_pool_size);
NODE_EXTERN void FreePlatform(MultiIsolatePlatform* platform);

NODE_EXTERN void EmitBeforeExit(Environment* env);
NODE_EXTERN int EmitExit(Environment* env);
NODE_EXTERN void RunAtExit(Environment* env);

// This may return nullptr if the current v8::Context is not associated
// with a Node instance.
NODE_EXTERN struct uv_loop_s* GetCurrentEventLoop(v8::Isolate* isolate);

/* Converts a unixtime to V8 Date */
NODE_DEPRECATED("Use v8::Date::New() directly",
                inline v8::Local<v8::Value> NODE_UNIXTIME_V8(double time) {
                  return v8::Date::New(
                             v8::Isolate::GetCurrent()->GetCurrentContext(),
                             1000 * time)
                      .ToLocalChecked();
                })
#define NODE_UNIXTIME_V8 node::NODE_UNIXTIME_V8
NODE_DEPRECATED("Use v8::Date::ValueOf() directly",
                inline double NODE_V8_UNIXTIME(v8::Local<v8::Date> date) {
  return date->ValueOf() / 1000;
})
#define NODE_V8_UNIXTIME node::NODE_V8_UNIXTIME

#define NODE_DEFINE_CONSTANT(target, constant)                                \
  do {                                                                        \
    v8::Isolate* isolate = target->GetIsolate();                              \
    v8::Local<v8::Context> context = isolate->GetCurrentContext();            \
    v8::Local<v8::String> constant_name =                                     \
        v8::String::NewFromUtf8(isolate, #constant,                           \
            v8::NewStringType::kInternalized).ToLocalChecked();               \
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

#define NODE_DEFINE_HIDDEN_CONSTANT(target, constant)                         \
  do {                                                                        \
    v8::Isolate* isolate = target->GetIsolate();                              \
    v8::Local<v8::Context> context = isolate->GetCurrentContext();            \
    v8::Local<v8::String> constant_name =                                     \
        v8::String::NewFromUtf8(isolate, #constant,                           \
                                v8::NewStringType::kInternalized)             \
                                  .ToLocalChecked();                          \
    v8::Local<v8::Number> constant_value =                                    \
        v8::Number::New(isolate, static_cast<double>(constant));              \
    v8::PropertyAttribute constant_attributes =                               \
        static_cast<v8::PropertyAttribute>(v8::ReadOnly |                     \
                                           v8::DontDelete |                   \
                                           v8::DontEnum);                     \
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
  v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name,
      v8::NewStringType::kInternalized).ToLocalChecked();
  t->SetClassName(fn_name);
  recv->Set(fn_name, t);
}

// Used to be a macro, hence the uppercase name.
inline void NODE_SET_METHOD(v8::Local<v8::Object> recv,
                            const char* name,
                            v8::FunctionCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(isolate,
                                                                callback);
  v8::Local<v8::Function> fn = t->GetFunction(context).ToLocalChecked();
  v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name,
      v8::NewStringType::kInternalized).ToLocalChecked();
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
  v8::Local<v8::String> fn_name = v8::String::NewFromUtf8(isolate, name,
      v8::NewStringType::kInternalized).ToLocalChecked();
  t->SetClassName(fn_name);
  recv->PrototypeTemplate()->Set(fn_name, t);
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

NODE_EXTERN v8::Local<v8::Value> Encode(v8::Isolate* isolate,
                                        const char* buf,
                                        size_t len,
                                        enum encoding encoding = LATIN1);

// Warning: This reverses endianness on Big Endian platforms, even though the
// signature using uint16_t implies that it should not.
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
    const char* syscall = nullptr,
    const char* msg = "",
    const char* path = nullptr);

NODE_DEPRECATED("Use WinapiErrnoException(isolate, ...)",
                inline v8::Local<v8::Value> WinapiErrnoException(int errorno,
    const char* syscall = nullptr,  const char* msg = "",
    const char* path = nullptr) {
  return WinapiErrnoException(v8::Isolate::GetCurrent(),
                              errorno,
                              syscall,
                              msg,
                              path);
})
#endif

const char* signo_string(int errorno);


typedef void (*addon_register_func)(
    v8::Local<v8::Object> exports,
    v8::Local<v8::Value> module,
    void* priv);

typedef void (*addon_context_register_func)(
    v8::Local<v8::Object> exports,
    v8::Local<v8::Value> module,
    v8::Local<v8::Context> context,
    void* priv);

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
      NULL,  /* NOLINT (readability/null_usage) */                    \
      __FILE__,                                                       \
      (node::addon_register_func) (regfunc),                          \
      NULL,  /* NOLINT (readability/null_usage) */                    \
      NODE_STRINGIFY(modname),                                        \
      priv,                                                           \
      NULL   /* NOLINT (readability/null_usage) */                    \
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
      NULL,  /* NOLINT (readability/null_usage) */                    \
      __FILE__,                                                       \
      NULL,  /* NOLINT (readability/null_usage) */                    \
      (node::addon_context_register_func) (regfunc),                  \
      NODE_STRINGIFY(modname),                                        \
      priv,                                                           \
      NULL  /* NOLINT (readability/null_usage) */                     \
    };                                                                \
    NODE_C_CTOR(_register_ ## modname) {                              \
      node_module_register(&_module);                                 \
    }                                                                 \
  }

// Usage: `NODE_MODULE(NODE_GYP_MODULE_NAME, InitializerFunction)`
// If no NODE_MODULE is declared, Node.js looks for the well-known
// symbol `node_register_module_v${NODE_MODULE_VERSION}`.
#define NODE_MODULE(modname, regfunc)                                 \
  NODE_MODULE_X(modname, regfunc, NULL, 0)  // NOLINT (readability/null_usage)

#define NODE_MODULE_CONTEXT_AWARE(modname, regfunc)                   \
  /* NOLINTNEXTLINE (readability/null_usage) */                       \
  NODE_MODULE_CONTEXT_AWARE_X(modname, regfunc, NULL, 0)

/*
 * For backward compatibility in add-on modules.
 */
#define NODE_MODULE_DECL /* nothing */

#define NODE_MODULE_INITIALIZER_BASE node_register_module_v

#define NODE_MODULE_INITIALIZER_X(base, version)                      \
    NODE_MODULE_INITIALIZER_X_HELPER(base, version)

#define NODE_MODULE_INITIALIZER_X_HELPER(base, version) base##version

#define NODE_MODULE_INITIALIZER                                       \
  NODE_MODULE_INITIALIZER_X(NODE_MODULE_INITIALIZER_BASE,             \
      NODE_MODULE_VERSION)

#define NODE_MODULE_INIT()                                            \
  extern "C" NODE_MODULE_EXPORT void                                  \
  NODE_MODULE_INITIALIZER(v8::Local<v8::Object> exports,              \
                          v8::Local<v8::Value> module,                \
                          v8::Local<v8::Context> context);            \
  NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME,                     \
                            NODE_MODULE_INITIALIZER)                  \
  void NODE_MODULE_INITIALIZER(v8::Local<v8::Object> exports,         \
                               v8::Local<v8::Value> module,           \
                               v8::Local<v8::Context> context)

/* Called after the event loop exits but before the VM is disposed.
 * Callbacks are run in reverse order of registration, i.e. newest first.
 */
NODE_EXTERN void AtExit(void (*cb)(void* arg), void* arg = 0);

/* Registers a callback with the passed-in Environment instance. The callback
 * is called after the event loop exits, but before the VM is disposed.
 * Callbacks are run in reverse order of registration, i.e. newest first.
 */
NODE_EXTERN void AtExit(Environment* env, void (*cb)(void* arg), void* arg = 0);

typedef void (*promise_hook_func) (v8::PromiseHookType type,
                                   v8::Local<v8::Promise> promise,
                                   v8::Local<v8::Value> parent,
                                   void* arg);

typedef double async_id;
struct async_context {
  ::node::async_id async_id;
  ::node::async_id trigger_async_id;
};

/* Registers an additional v8::PromiseHook wrapper. This API exists because V8
 * itself supports only a single PromiseHook. */
NODE_DEPRECATED("Use async_hooks directly instead",
                NODE_EXTERN void AddPromiseHook(v8::Isolate* isolate,
                                                promise_hook_func fn,
                                                void* arg));

/* This is a lot like node::AtExit, except that the hooks added via this
 * function are run before the AtExit ones and will always be registered
 * for the current Environment instance.
 * These functions are safe to use in an addon supporting multiple
 * threads/isolates. */
NODE_EXTERN void AddEnvironmentCleanupHook(v8::Isolate* isolate,
                                           void (*fun)(void* arg),
                                           void* arg);

NODE_EXTERN void RemoveEnvironmentCleanupHook(v8::Isolate* isolate,
                                              void (*fun)(void* arg),
                                              void* arg);

/* Returns the id of the current execution context. If the return value is
 * zero then no execution has been set. This will happen if the user handles
 * I/O from native code. */
NODE_EXTERN async_id AsyncHooksGetExecutionAsyncId(v8::Isolate* isolate);

/* Return same value as async_hooks.triggerAsyncId(); */
NODE_EXTERN async_id AsyncHooksGetTriggerAsyncId(v8::Isolate* isolate);

/* If the native API doesn't inherit from the helper class then the callbacks
 * must be triggered manually. This triggers the init() callback. The return
 * value is the async id assigned to the resource.
 *
 * The `trigger_async_id` parameter should correspond to the resource which is
 * creating the new resource, which will usually be the return value of
 * `AsyncHooksGetTriggerAsyncId()`. */
NODE_EXTERN async_context EmitAsyncInit(v8::Isolate* isolate,
                                        v8::Local<v8::Object> resource,
                                        const char* name,
                                        async_id trigger_async_id = -1);

NODE_EXTERN async_context EmitAsyncInit(v8::Isolate* isolate,
                                        v8::Local<v8::Object> resource,
                                        v8::Local<v8::String> name,
                                        async_id trigger_async_id = -1);

/* Emit the destroy() callback. */
NODE_EXTERN void EmitAsyncDestroy(v8::Isolate* isolate,
                                  async_context asyncContext);

class InternalCallbackScope;

/* This class works like `MakeCallback()` in that it sets up a specific
 * asyncContext as the current one and informs the async_hooks and domains
 * modules that this context is currently active.
 *
 * `MakeCallback()` is a wrapper around this class as well as
 * `Function::Call()`. Either one of these mechanisms needs to be used for
 * top-level calls into JavaScript (i.e. without any existing JS stack).
 *
 * This object should be stack-allocated to ensure that it is contained in a
 * valid HandleScope.
 *
 * Exceptions happening within this scope will be treated like uncaught
 * exceptions. If this behaviour is undesirable, a new `v8::TryCatch` scope
 * needs to be created inside of this scope.
 */
class NODE_EXTERN CallbackScope {
 public:
  CallbackScope(v8::Isolate* isolate,
                v8::Local<v8::Object> resource,
                async_context asyncContext);
  ~CallbackScope();

 private:
  InternalCallbackScope* private_;
  v8::TryCatch try_catch_;

  void operator=(const CallbackScope&) = delete;
  void operator=(CallbackScope&&) = delete;
  CallbackScope(const CallbackScope&) = delete;
  CallbackScope(CallbackScope&&) = delete;
};

/* An API specific to emit before/after callbacks is unnecessary because
 * MakeCallback will automatically call them for you.
 *
 * These methods may create handles on their own, so run them inside a
 * HandleScope.
 *
 * `asyncId` and `triggerAsyncId` should correspond to the values returned by
 * `EmitAsyncInit()` and `AsyncHooksGetTriggerAsyncId()`, respectively, when the
 * invoking resource was created. If these values are unknown, 0 can be passed.
 * */
NODE_EXTERN
v8::MaybeLocal<v8::Value> MakeCallback(v8::Isolate* isolate,
                                       v8::Local<v8::Object> recv,
                                       v8::Local<v8::Function> callback,
                                       int argc,
                                       v8::Local<v8::Value>* argv,
                                       async_context asyncContext);
NODE_EXTERN
v8::MaybeLocal<v8::Value> MakeCallback(v8::Isolate* isolate,
                                       v8::Local<v8::Object> recv,
                                       const char* method,
                                       int argc,
                                       v8::Local<v8::Value>* argv,
                                       async_context asyncContext);
NODE_EXTERN
v8::MaybeLocal<v8::Value> MakeCallback(v8::Isolate* isolate,
                                       v8::Local<v8::Object> recv,
                                       v8::Local<v8::String> symbol,
                                       int argc,
                                       v8::Local<v8::Value>* argv,
                                       async_context asyncContext);

/* Helper class users can optionally inherit from. If
 * `AsyncResource::MakeCallback()` is used, then all four callbacks will be
 * called automatically. */
class AsyncResource {
 public:
  AsyncResource(v8::Isolate* isolate,
                v8::Local<v8::Object> resource,
                const char* name,
                async_id trigger_async_id = -1)
      : isolate_(isolate),
        resource_(isolate, resource) {
    async_context_ = EmitAsyncInit(isolate, resource, name,
                                   trigger_async_id);
  }

  AsyncResource(v8::Isolate* isolate,
                v8::Local<v8::Object> resource,
                v8::Local<v8::String> name,
                async_id trigger_async_id = -1)
      : isolate_(isolate),
        resource_(isolate, resource) {
    async_context_ = EmitAsyncInit(isolate, resource, name,
                                   trigger_async_id);
  }

  virtual ~AsyncResource() {
    EmitAsyncDestroy(isolate_, async_context_);
    resource_.Reset();
  }

  v8::MaybeLocal<v8::Value> MakeCallback(
      v8::Local<v8::Function> callback,
      int argc,
      v8::Local<v8::Value>* argv) {
    return node::MakeCallback(isolate_, get_resource(),
                              callback, argc, argv,
                              async_context_);
  }

  v8::MaybeLocal<v8::Value> MakeCallback(
      const char* method,
      int argc,
      v8::Local<v8::Value>* argv) {
    return node::MakeCallback(isolate_, get_resource(),
                              method, argc, argv,
                              async_context_);
  }

  v8::MaybeLocal<v8::Value> MakeCallback(
      v8::Local<v8::String> symbol,
      int argc,
      v8::Local<v8::Value>* argv) {
    return node::MakeCallback(isolate_, get_resource(),
                              symbol, argc, argv,
                              async_context_);
  }

  v8::Local<v8::Object> get_resource() {
    return resource_.Get(isolate_);
  }

  async_id get_async_id() const {
    return async_context_.async_id;
  }

  async_id get_trigger_async_id() const {
    return async_context_.trigger_async_id;
  }

 protected:
  class CallbackScope : public node::CallbackScope {
   public:
    explicit CallbackScope(AsyncResource* res)
      : node::CallbackScope(res->isolate_,
                            res->resource_.Get(res->isolate_),
                            res->async_context_) {}
  };

 private:
  v8::Isolate* isolate_;
  v8::Persistent<v8::Object> resource_;
  async_context async_context_;
};

}  // namespace node

#endif  // SRC_NODE_H_
