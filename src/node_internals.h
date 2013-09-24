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

#include "env.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

struct sockaddr;

namespace node {

// Defined in node.cc
extern v8::Isolate* node_isolate;

// If persistent.IsWeak() == false, then do not call persistent.Dispose()
// while the returned Local<T> is still in scope, it will destroy the
// reference to the object.
template <class TypeName>
inline v8::Local<TypeName> PersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent);

// Call with valid HandleScope and while inside Context scope.
v8::Handle<v8::Value> MakeCallback(Environment* env,
                                   v8::Handle<v8::Object> object,
                                   const char* method,
                                   int argc = 0,
                                   v8::Handle<v8::Value>* argv = NULL);

// Call with valid HandleScope and while inside Context scope.
v8::Handle<v8::Value> MakeCallback(Environment* env,
                                   const v8::Handle<v8::Object> object,
                                   uint32_t index,
                                   int argc = 0,
                                   v8::Handle<v8::Value>* argv = NULL);

// Call with valid HandleScope and while inside Context scope.
v8::Handle<v8::Value> MakeCallback(Environment* env,
                                   const v8::Handle<v8::Object> object,
                                   const v8::Handle<v8::String> symbol,
                                   int argc = 0,
                                   v8::Handle<v8::Value>* argv = NULL);

// Call with valid HandleScope and while inside Context scope.
v8::Handle<v8::Value> MakeCallback(Environment* env,
                                   const v8::Handle<v8::Object> object,
                                   const v8::Handle<v8::Function> callback,
                                   int argc = 0,
                                   v8::Handle<v8::Value>* argv = NULL);

// Convert a struct sockaddr to a { address: '1.2.3.4', port: 1234 } JS object.
// Sets address and port properties on the info object and returns it.
// If |info| is omitted, a new object is returned.
v8::Local<v8::Object> AddressToJS(
    Environment* env,
    const sockaddr* addr,
    v8::Local<v8::Object> info = v8::Handle<v8::Object>());

#ifdef _WIN32
// emulate snprintf() on windows, _snprintf() doesn't zero-terminate the buffer
// on overflow...
#include <stdarg.h>
inline static int snprintf(char* buf, unsigned int len, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = _vsprintf_p(buf, len, fmt, ap);
  if (len)
    buf[len - 1] = '\0';
  va_end(ap);
  return n;
}
#endif

#if defined(__x86_64__)
# define BITS_PER_LONG 64
#else
# define BITS_PER_LONG 32
#endif

#ifndef offset_of
// g++ in strict mode complains loudly about the system offsetof() macro
// because it uses NULL as the base address.
# define offset_of(type, member) \
  (reinterpret_cast<intptr_t>( \
      reinterpret_cast<char*>(&(reinterpret_cast<type*>(8)->member)) - 8))
#endif

#ifndef container_of
# define container_of(ptr, type, member) \
  (reinterpret_cast<type*>(reinterpret_cast<char*>(ptr) - \
                           offset_of(type, member)))
#endif

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
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

// this would have been a template function were it not for the fact that g++
// sometimes fails to resolve it...
#define THROW_ERROR(fun)                                                      \
  do {                                                                        \
    v8::HandleScope scope(node_isolate);                                      \
    v8::ThrowException(fun(OneByteString(node_isolate, errmsg)));             \
  }                                                                           \
  while (0)

inline static void ThrowError(const char* errmsg) {
  THROW_ERROR(v8::Exception::Error);
}

inline static void ThrowTypeError(const char* errmsg) {
  THROW_ERROR(v8::Exception::TypeError);
}

inline static void ThrowRangeError(const char* errmsg) {
  THROW_ERROR(v8::Exception::RangeError);
}

inline static void ThrowErrnoException(int errorno,
                                       const char* syscall = NULL,
                                       const char* message = NULL,
                                       const char* path = NULL) {
  v8::ThrowException(ErrnoException(errorno, syscall, message, path));
}

inline static void ThrowUVException(int errorno,
                                    const char* syscall = NULL,
                                    const char* message = NULL,
                                    const char* path = NULL) {
  v8::ThrowException(UVException(errorno, syscall, message, path));
}

NO_RETURN void FatalError(const char* location, const char* message);

v8::Local<v8::Object> BuildStatsObject(Environment* env, const uv_stat_t* s);

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
inline MUST_USE_RESULT bool ParseArrayIndex(v8::Handle<v8::Value> arg,
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

}  // namespace node

#endif  // SRC_NODE_INTERNALS_H_
