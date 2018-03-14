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

#ifndef SRC_NODE_INTERNALS_H_
#define SRC_NODE_INTERNALS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_persistent.h"
#include "util-inl.h"
#include "env-inl.h"
#include "uv.h"
#include "v8.h"
#include "tracing/trace_event.h"
#include "node_perf_common.h"
#include "node_debug_options.h"

#include <stdint.h>
#include <stdlib.h>

#include <string>

// Custom constants used by both node_constants.cc and node_zlib.cc
#define Z_MIN_WINDOWBITS 8
#define Z_MAX_WINDOWBITS 15
#define Z_DEFAULT_WINDOWBITS 15
// Fewer than 64 bytes per chunk is not recommended.
// Technically it could work with as few as 8, but even 64 bytes
// is low.  Usually a MB or more is best.
#define Z_MIN_CHUNK 64
#define Z_MAX_CHUNK std::numeric_limits<double>::infinity()
#define Z_DEFAULT_CHUNK (16 * 1024)
#define Z_MIN_MEMLEVEL 1
#define Z_MAX_MEMLEVEL 9
#define Z_DEFAULT_MEMLEVEL 8
#define Z_MIN_LEVEL -1
#define Z_MAX_LEVEL 9
#define Z_DEFAULT_LEVEL Z_DEFAULT_COMPRESSION

enum {
  NM_F_BUILTIN  = 1 << 0,
  NM_F_LINKED   = 1 << 1,
  NM_F_INTERNAL = 1 << 2,
};

struct sockaddr;

// Variation on NODE_DEFINE_CONSTANT that sets a String value.
#define NODE_DEFINE_STRING_CONSTANT(target, name, constant)                   \
  do {                                                                        \
    v8::Isolate* isolate = target->GetIsolate();                              \
    v8::Local<v8::String> constant_name =                                     \
        v8::String::NewFromUtf8(isolate, name);                               \
    v8::Local<v8::String> constant_value =                                    \
        v8::String::NewFromUtf8(isolate, constant);                           \
    v8::PropertyAttribute constant_attributes =                               \
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete);    \
    target->DefineOwnProperty(isolate->GetCurrentContext(),                   \
                              constant_name,                                  \
                              constant_value,                                 \
                              constant_attributes).FromJust();                \
  } while (0)


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

// A list of built-in modules. In order to do module registration
// in node::Init(), need to add built-in modules in the following list.
// Then in node::RegisterBuiltinModules(), it calls modules' registration
// function. This helps the built-in modules are loaded properly when
// node is built as static library. No need to depends on the
// __attribute__((constructor)) like mechanism in GCC.
#define NODE_BUILTIN_STANDARD_MODULES(V)                                      \
    V(async_wrap)                                                             \
    V(buffer)                                                                 \
    V(cares_wrap)                                                             \
    V(config)                                                                 \
    V(contextify)                                                             \
    V(domain)                                                                 \
    V(fs)                                                                     \
    V(fs_event_wrap)                                                          \
    V(http2)                                                                  \
    V(http_parser)                                                            \
    V(inspector)                                                              \
    V(js_stream)                                                              \
    V(module_wrap)                                                            \
    V(os)                                                                     \
    V(performance)                                                            \
    V(pipe_wrap)                                                              \
    V(process_wrap)                                                           \
    V(serdes)                                                                 \
    V(signal_wrap)                                                            \
    V(spawn_sync)                                                             \
    V(stream_wrap)                                                            \
    V(tcp_wrap)                                                               \
    V(timer_wrap)                                                             \
    V(trace_events)                                                           \
    V(tty_wrap)                                                               \
    V(udp_wrap)                                                               \
    V(url)                                                                    \
    V(util)                                                                   \
    V(uv)                                                                     \
    V(v8)                                                                     \
    V(zlib)

#define NODE_BUILTIN_MODULES(V)                                               \
  NODE_BUILTIN_STANDARD_MODULES(V)                                            \
  NODE_BUILTIN_OPENSSL_MODULES(V)                                             \
  NODE_BUILTIN_ICU_MODULES(V)

#define NODE_MODULE_CONTEXT_AWARE_CPP(modname, regfunc, priv, flags)          \
  static node::node_module _module = {                                        \
    NODE_MODULE_VERSION,                                                      \
    flags,                                                                    \
    nullptr,                                                                  \
    __FILE__,                                                                 \
    nullptr,                                                                  \
    (node::addon_context_register_func) (regfunc),                            \
    NODE_STRINGIFY(modname),                                                  \
    priv,                                                                     \
    nullptr                                                                   \
  };                                                                          \
  void _register_ ## modname() {                                              \
    node_module_register(&_module);                                           \
  }


#define NODE_BUILTIN_MODULE_CONTEXT_AWARE(modname, regfunc)                   \
  NODE_MODULE_CONTEXT_AWARE_CPP(modname, regfunc, nullptr, NM_F_BUILTIN)

namespace node {

// Set in node.cc by ParseArgs with the value of --openssl-config.
// Used in node_crypto.cc when initializing OpenSSL.
extern std::string openssl_config;

// Set in node.cc by ParseArgs when --preserve-symlinks is used.
// Used in node_config.cc to set a constant on process.binding('config')
// that is used by lib/module.js
extern bool config_preserve_symlinks;

// Set in node.cc by ParseArgs when --experimental-modules is used.
// Used in node_config.cc to set a constant on process.binding('config')
// that is used by lib/module.js
extern bool config_experimental_modules;

// Set in node.cc by ParseArgs when --experimental-vm-modules is used.
// Used in node_config.cc to set a constant on process.binding('config')
// that is used by lib/vm.js
extern bool config_experimental_vm_modules;

// Set in node.cc by ParseArgs when --loader is used.
// Used in node_config.cc to set a constant on process.binding('config')
// that is used by lib/internal/bootstrap_node.js
extern std::string config_userland_loader;

// Set in node.cc by ParseArgs when --expose-internals or --expose_internals is
// used.
// Used in node_config.cc to set a constant on process.binding('config')
// that is used by lib/internal/bootstrap_node.js
extern bool config_expose_internals;

// Set in node.cc by ParseArgs when --redirect-warnings= is used.
// Used to redirect warning output to a file rather than sending
// it to stderr.
extern std::string config_warning_file;  // NOLINT(runtime/string)

// Set in node.cc by ParseArgs when --pending-deprecation or
// NODE_PENDING_DEPRECATION is used
extern bool config_pending_deprecation;

// Tells whether it is safe to call v8::Isolate::GetCurrent().
extern bool v8_initialized;

// Contains initial debug options.
// Set in node.cc.
// Used in node_config.cc.
extern node::DebugOptions debug_options;

// Forward declaration
class Environment;

// If persistent.IsWeak() == false, then do not call persistent.Reset()
// while the returned Local<T> is still in scope, it will destroy the
// reference to the object.
template <class TypeName>
inline v8::Local<TypeName> PersistentToLocal(
    v8::Isolate* isolate,
    const Persistent<TypeName>& persistent);

// Creates a new context with Node.js-specific tweaks.  Currently, it removes
// the `v8BreakIterator` property from the global `Intl` object if present.
// See https://github.com/nodejs/node/issues/14909 for more info.
v8::Local<v8::Context> NewContext(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template =
        v8::Local<v8::ObjectTemplate>());

// Convert a struct sockaddr to a { address: '1.2.3.4', port: 1234 } JS object.
// Sets address and port properties on the info object and returns it.
// If |info| is omitted, a new object is returned.
v8::Local<v8::Object> AddressToJS(
    Environment* env,
    const sockaddr* addr,
    v8::Local<v8::Object> info = v8::Local<v8::Object>());

template <typename T, int (*F)(const typename T::HandleType*, sockaddr*, int*)>
void GetSockOrPeerName(const v8::FunctionCallbackInfo<v8::Value>& args) {
  T* const wrap = Unwrap<T>(args.Holder());
  if (wrap == nullptr)
    return args.GetReturnValue().Set(UV_EBADF);
  CHECK(args[0]->IsObject());
  sockaddr_storage storage;
  int addrlen = sizeof(storage);
  sockaddr* const addr = reinterpret_cast<sockaddr*>(&storage);
  const int err = F(&wrap->handle_, addr, &addrlen);
  if (err == 0)
    AddressToJS(wrap->env(), addr, args[0].As<v8::Object>());
  args.GetReturnValue().Set(err);
}

void FatalException(v8::Isolate* isolate,
                    v8::Local<v8::Value> error,
                    v8::Local<v8::Message> message);


void SignalExit(int signo);
#ifdef __POSIX__
void RegisterSignalHandler(int signal,
                           void (*handler)(int signal),
                           bool reset_handler = false);
#endif

bool SafeGetenv(const char* key, std::string* text);

std::string GetHumanReadableProcessName();
void GetHumanReadableProcessName(char (*name)[1024]);

template <typename T, size_t N>
constexpr size_t arraysize(const T(&)[N]) { return N; }

#ifndef ROUND_UP
# define ROUND_UP(a, b) ((a) % (b) ? ((a) + (b)) - ((a) % (b)) : (a))
#endif

#ifdef __GNUC__
# define MUST_USE_RESULT __attribute__((warn_unused_result))
#else
# define MUST_USE_RESULT
#endif

bool IsExceptionDecorated(Environment* env, v8::Local<v8::Value> er);

enum ErrorHandlingMode { CONTEXTIFY_ERROR, FATAL_ERROR, MODULE_ERROR };
void AppendExceptionLine(Environment* env,
                         v8::Local<v8::Value> er,
                         v8::Local<v8::Message> message,
                         enum ErrorHandlingMode mode);

NO_RETURN void FatalError(const char* location, const char* message);

// Like a `TryCatch` but exits the process if an exception was caught.
class FatalTryCatch : public v8::TryCatch {
 public:
  explicit FatalTryCatch(Environment* env)
      : TryCatch(env->isolate()), env_(env) {}
  ~FatalTryCatch();

 private:
  Environment* env_;
};

v8::Maybe<bool> ProcessEmitWarning(Environment* env, const char* fmt, ...);
v8::Maybe<bool> ProcessEmitDeprecationWarning(Environment* env,
                                              const char* warning,
                                              const char* deprecation_code);

void FillStatsArray(double* fields, const uv_stat_t* s);

void SetupProcessObject(Environment* env,
                        int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv);

// Call _register<module_name> functions for all of
// the built-in modules. Because built-in modules don't
// use the __attribute__((constructor)). Need to
// explicitly call the _register* functions.
void RegisterBuiltinModules();

enum Endianness {
  kLittleEndian,  // _Not_ LITTLE_ENDIAN, clashes with endian.h.
  kBigEndian
};

inline enum Endianness GetEndianness() {
  // Constant-folded by the compiler.
  const union {
    uint8_t u8[2];
    uint16_t u16;
  } u = {
    { 1, 0 }
  };
  return u.u16 == 1 ? kLittleEndian : kBigEndian;
}

inline bool IsLittleEndian() {
  return GetEndianness() == kLittleEndian;
}

inline bool IsBigEndian() {
  return GetEndianness() == kBigEndian;
}

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  inline uint32_t* zero_fill_field() { return &zero_fill_field_; }

  virtual void* Allocate(size_t size);  // Defined in src/node.cc
  virtual void* AllocateUninitialized(size_t size)
    { return node::UncheckedMalloc(size); }
  virtual void Free(void* data, size_t) { free(data); }

 private:
  uint32_t zero_fill_field_ = 1;  // Boolean but exposed as uint32 to JS land.
};

namespace Buffer {
v8::MaybeLocal<v8::Object> Copy(Environment* env, const char* data, size_t len);
v8::MaybeLocal<v8::Object> New(Environment* env, size_t size);
// Takes ownership of |data|.
v8::MaybeLocal<v8::Object> New(Environment* env,
                               char* data,
                               size_t length,
                               void (*callback)(char* data, void* hint),
                               void* hint);
// Takes ownership of |data|.  Must allocate |data| with malloc() or realloc()
// because ArrayBufferAllocator::Free() deallocates it again with free().
// Mixing operator new and free() is undefined behavior so don't do that.
v8::MaybeLocal<v8::Object> New(Environment* env, char* data, size_t length);

inline
v8::MaybeLocal<v8::Uint8Array> New(Environment* env,
                                   v8::Local<v8::ArrayBuffer> ab,
                                   size_t byte_offset,
                                   size_t length) {
  v8::Local<v8::Uint8Array> ui = v8::Uint8Array::New(ab, byte_offset, length);
  v8::Maybe<bool> mb =
      ui->SetPrototype(env->context(), env->buffer_prototype_object());
  if (mb.IsNothing())
    return v8::MaybeLocal<v8::Uint8Array>();
  return ui;
}

// Construct a Buffer from a MaybeStackBuffer (and also its subclasses like
// Utf8Value and TwoByteValue).
// If |buf| is invalidated, an empty MaybeLocal is returned, and nothing is
// changed.
// If |buf| contains actual data, this method takes ownership of |buf|'s
// underlying buffer. However, |buf| itself can be reused even after this call,
// but its capacity, if increased through AllocateSufficientStorage, is not
// guaranteed to stay the same.
template <typename T>
static v8::MaybeLocal<v8::Object> New(Environment* env,
                                      MaybeStackBuffer<T>* buf) {
  v8::MaybeLocal<v8::Object> ret;
  char* src = reinterpret_cast<char*>(buf->out());
  const size_t len_in_bytes = buf->length() * sizeof(buf->out()[0]);

  if (buf->IsAllocated())
    ret = New(env, src, len_in_bytes);
  else if (!buf->IsInvalidated())
    ret = Copy(env, src, len_in_bytes);

  if (ret.IsEmpty())
    return ret;

  if (buf->IsAllocated())
    buf->Release();

  return ret;
}
}  // namespace Buffer

v8::MaybeLocal<v8::Value> InternalMakeCallback(
    Environment* env,
    v8::Local<v8::Object> recv,
    const v8::Local<v8::Function> callback,
    int argc,
    v8::Local<v8::Value> argv[],
    async_context asyncContext);

class InternalCallbackScope {
 public:
  // Tell the constructor whether its `object` parameter may be empty or not.
  enum ResourceExpectation { kRequireResource, kAllowEmptyResource };
  InternalCallbackScope(Environment* env,
                        v8::Local<v8::Object> object,
                        const async_context& asyncContext,
                        ResourceExpectation expect = kRequireResource);
  // Utility that can be used by AsyncWrap classes.
  explicit InternalCallbackScope(AsyncWrap* async_wrap);
  ~InternalCallbackScope();
  void Close();

  inline bool Failed() const { return failed_; }
  inline void MarkAsFailed() { failed_ = true; }
  inline bool IsInnerMakeCallback() const {
    return callback_scope_.in_makecallback();
  }

 private:
  Environment* env_;
  async_context async_context_;
  v8::Local<v8::Object> object_;
  Environment::AsyncCallbackScope callback_scope_;
  bool failed_ = false;
  bool pushed_ids_ = false;
  bool closed_ = false;
};

#define NODE_MODULE_CONTEXT_AWARE_INTERNAL(modname, regfunc)                  \
  NODE_MODULE_CONTEXT_AWARE_CPP(modname, regfunc, nullptr, NM_F_INTERNAL)

}  // namespace node


#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_INTERNALS_H_
