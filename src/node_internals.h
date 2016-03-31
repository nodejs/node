#ifndef SRC_NODE_INTERNALS_H_
#define SRC_NODE_INTERNALS_H_

#include "node.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <stdint.h>
#include <stdlib.h>

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
    target->ForceSet(isolate->GetCurrentContext(),                            \
                     constant_name,                                           \
                     constant_value,                                          \
                     constant_attributes);                                    \
  } while (0)

namespace node {

// Forward declaration
class Environment;

// If persistent.IsWeak() == false, then do not call persistent.Reset()
// while the returned Local<T> is still in scope, it will destroy the
// reference to the object.
template <class TypeName>
inline v8::Local<TypeName> PersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent);

// Call with valid HandleScope and while inside Context scope.
v8::Local<v8::Value> MakeCallback(Environment* env,
                                   v8::Local<v8::Object> recv,
                                   const char* method,
                                   int argc = 0,
                                   v8::Local<v8::Value>* argv = nullptr);

// Call with valid HandleScope and while inside Context scope.
v8::Local<v8::Value> MakeCallback(Environment* env,
                                   v8::Local<v8::Object> recv,
                                   uint32_t index,
                                   int argc = 0,
                                   v8::Local<v8::Value>* argv = nullptr);

// Call with valid HandleScope and while inside Context scope.
v8::Local<v8::Value> MakeCallback(Environment* env,
                                   v8::Local<v8::Object> recv,
                                   v8::Local<v8::String> symbol,
                                   int argc = 0,
                                   v8::Local<v8::Value>* argv = nullptr);

// Call with valid HandleScope and while inside Context scope.
v8::Local<v8::Value> MakeCallback(Environment* env,
                                   v8::Local<v8::Value> recv,
                                   v8::Local<v8::Function> callback,
                                   int argc = 0,
                                   v8::Local<v8::Value>* argv = nullptr);

bool KickNextTick();

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
  CHECK(args[0]->IsObject());
  sockaddr_storage storage;
  int addrlen = sizeof(storage);
  sockaddr* const addr = reinterpret_cast<sockaddr*>(&storage);
  const int err = F(&wrap->handle_, addr, &addrlen);
  if (err == 0)
    AddressToJS(wrap->env(), addr, args[0].As<v8::Object>());
  args.GetReturnValue().Set(err);
}

#ifdef _WIN32
// emulate snprintf() on windows, _snprintf() doesn't zero-terminate the buffer
// on overflow...
// VS 2015 added a standard conform snprintf
#if defined( _MSC_VER ) && (_MSC_VER < 1900)
#include <stdarg.h>
inline static int snprintf(char *buffer, size_t n, const char *format, ...) {
  va_list argp;
  va_start(argp, format);
  int ret = _vscprintf(format, argp);
  vsnprintf_s(buffer, n, _TRUNCATE, format, argp);
  va_end(argp);
  return ret;
}
#endif
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
#define arraysize(a) (sizeof(a) / sizeof(*a))  // Workaround for VS 2013.
#else
template <typename T, size_t N>
constexpr size_t arraysize(const T(&)[N]) { return N; }
#endif

#ifndef ROUND_UP
# define ROUND_UP(a, b) ((a) % (b) ? ((a) + (b)) - ((a) % (b)) : (a))
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
# define MUST_USE_RESULT __attribute__((warn_unused_result))
# define NO_RETURN __attribute__((noreturn))
#else
# define MUST_USE_RESULT
# define NO_RETURN
#endif

void AppendExceptionLine(Environment* env,
                         v8::Local<v8::Value> er,
                         v8::Local<v8::Message> message);

NO_RETURN void FatalError(const char* location, const char* message);

v8::Local<v8::Value> BuildStatsObject(Environment* env, const uv_stat_t* s);

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

// parse index for external array data
inline MUST_USE_RESULT bool ParseArrayIndex(v8::Local<v8::Value> arg,
                                            size_t def,
                                            size_t* ret) {
  if (arg->IsUndefined()) {
    *ret = def;
    return true;
  }

  int32_t tmp_i = arg->Int32Value();

  if (tmp_i < 0)
    return false;

  *ret = static_cast<size_t>(tmp_i);
  return true;
}

void ThrowError(v8::Isolate* isolate, const char* errmsg);
void ThrowTypeError(v8::Isolate* isolate, const char* errmsg);
void ThrowRangeError(v8::Isolate* isolate, const char* errmsg);
void ThrowErrnoException(v8::Isolate* isolate,
                         int errorno,
                         const char* syscall = nullptr,
                         const char* message = nullptr,
                         const char* path = nullptr);
void ThrowUVException(v8::Isolate* isolate,
                      int errorno,
                      const char* syscall = nullptr,
                      const char* message = nullptr,
                      const char* path = nullptr,
                      const char* dest = nullptr);

NODE_DEPRECATED("Use ThrowError(isolate)",
                inline void ThrowError(const char* errmsg) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return ThrowError(isolate, errmsg);
})
NODE_DEPRECATED("Use ThrowTypeError(isolate)",
                inline void ThrowTypeError(const char* errmsg) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return ThrowTypeError(isolate, errmsg);
})
NODE_DEPRECATED("Use ThrowRangeError(isolate)",
                inline void ThrowRangeError(const char* errmsg) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return ThrowRangeError(isolate, errmsg);
})
NODE_DEPRECATED("Use ThrowErrnoException(isolate)",
                inline void ThrowErrnoException(int errorno,
                                                const char* syscall = nullptr,
                                                const char* message = nullptr,
                                                const char* path = nullptr) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return ThrowErrnoException(isolate, errorno, syscall, message, path);
})
NODE_DEPRECATED("Use ThrowUVException(isolate)",
                inline void ThrowUVException(int errorno,
                                             const char* syscall = nullptr,
                                             const char* message = nullptr,
                                             const char* path = nullptr) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return ThrowUVException(isolate, errorno, syscall, message, path);
})

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  ArrayBufferAllocator() : env_(nullptr) { }

  inline void set_env(Environment* env) { env_ = env; }

  virtual void* Allocate(size_t size);  // Defined in src/node.cc
  virtual void* AllocateUninitialized(size_t size) { return malloc(size); }
  virtual void Free(void* data, size_t) { free(data); }

 private:
  Environment* env_;
};

enum NodeInstanceType { MAIN, WORKER };

class NodeInstanceData {
  public:
    NodeInstanceData(NodeInstanceType node_instance_type,
                     uv_loop_t* event_loop,
                     int argc,
                     const char** argv,
                     int exec_argc,
                     const char** exec_argv,
                     bool use_debug_agent_flag)
        : node_instance_type_(node_instance_type),
          exit_code_(1),
          event_loop_(event_loop),
          argc_(argc),
          argv_(argv),
          exec_argc_(exec_argc),
          exec_argv_(exec_argv),
          use_debug_agent_flag_(use_debug_agent_flag) {
      CHECK_NE(event_loop_, nullptr);
    }

    uv_loop_t* event_loop() const {
      return event_loop_;
    }

    int exit_code() {
      CHECK(is_main());
      return exit_code_;
    }

    void set_exit_code(int exit_code) {
      CHECK(is_main());
      exit_code_ = exit_code;
    }

    bool is_main() {
      return node_instance_type_ == MAIN;
    }

    bool is_worker() {
      return node_instance_type_ == WORKER;
    }

    int argc() {
      return argc_;
    }

    const char** argv() {
      return argv_;
    }

    int exec_argc() {
      return exec_argc_;
    }

    const char** exec_argv() {
      return exec_argv_;
    }

    bool use_debug_agent() {
      return is_main() && use_debug_agent_flag_;
    }

  private:
    const NodeInstanceType node_instance_type_;
    int exit_code_;
    uv_loop_t* const event_loop_;
    const int argc_;
    const char** argv_;
    const int exec_argc_;
    const char** exec_argv_;
    const bool use_debug_agent_flag_;

    DISALLOW_COPY_AND_ASSIGN(NodeInstanceData);
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
}  // namespace Buffer

}  // namespace node

#endif  // SRC_NODE_INTERNALS_H_
