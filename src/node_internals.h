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

#include <assert.h>
#include <stdlib.h>

#include "v8.h"

namespace node {

// Defined in node.cc
extern v8::Isolate* node_isolate;

// Defined in node.cc at startup.
extern v8::Persistent<v8::Object> process_p;

template <typename TypeName>
class CachedBase {
public:
  CachedBase();
  operator v8::Handle<TypeName>() const;
  void operator=(v8::Handle<TypeName> that);  // Can only assign once.
  bool IsEmpty() const;
private:
  CachedBase(const CachedBase&);
  void operator=(const CachedBase&);
  v8::Persistent<TypeName> handle_;
};

template <typename TypeName>
class Cached : public CachedBase<TypeName> {
public:
  operator v8::Handle<v8::Value>() const;
  void operator=(v8::Handle<TypeName> that);
};

template <>
class Cached<v8::Value> : public CachedBase<v8::Value> {
public:
  operator v8::Handle<v8::Value>() const;
  void operator=(v8::Handle<v8::Value> that);
};

template <typename TypeName>
v8::Handle<v8::Value> MakeCallback(
    const v8::Persistent<v8::Object>& recv,
    const TypeName method,
    int argc,
    v8::Handle<v8::Value>* argv);

template <typename TypeName>
v8::Handle<v8::Value> MakeCallback(
    const v8::Persistent<v8::Object>& recv,
    const Cached<TypeName>& method,
    int argc,
    v8::Handle<v8::Value>* argv);

inline bool HasInstance(v8::Persistent<v8::FunctionTemplate>& function_template,
                        v8::Handle<v8::Value> value);

inline v8::Local<v8::Object> NewInstance(v8::Persistent<v8::Function>& ctor,
                                          int argc = 0,
                                          v8::Handle<v8::Value>* argv = NULL);

// TODO(bnoordhuis) Move to src/node_buffer.h once it's been established
// that the current approach to dealing with Persistent is working out.
namespace Buffer {

template <typename TypeName>
inline char* Data(v8::Persistent<TypeName>& val);

template <typename TypeName>
inline size_t Length(v8::Persistent<TypeName>& val);

} // namespace Buffer

#ifdef _WIN32
// emulate snprintf() on windows, _snprintf() doesn't zero-terminate the buffer
// on overflow...
#include <stdarg.h>
inline static int snprintf(char* buf, unsigned int len, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = _vsprintf_p(buf, len, fmt, ap);
  if (len) buf[len - 1] = '\0';
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
  ((intptr_t) ((char *) &(((type *) 8)->member) - 8))
#endif

#ifndef container_of
# define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - offset_of(type, member)))
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
    v8::ThrowException(fun(v8::String::New(errmsg)));                         \
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
  NODE_EXTERN v8::Local<v8::Value> ErrnoException(int errorno,
                                                  const char* syscall = NULL,
                                                  const char* message = NULL,
                                                  const char* path = NULL);
  v8::ThrowException(ErrnoException(errorno, syscall, message, path));
}

inline static void ThrowUVException(int errorno,
                                    const char* syscall = NULL,
                                    const char* message = NULL,
                                    const char* path = NULL) {
  NODE_EXTERN v8::Local<v8::Value> UVException(int errorno,
                                               const char* syscall = NULL,
                                               const char* message = NULL,
                                               const char* path = NULL);
  v8::ThrowException(UVException(errorno, syscall, message, path));
}

NO_RETURN void FatalError(const char* location, const char* message);

#define UNWRAP(type)                                                        \
  assert(!args.This().IsEmpty());                                           \
  assert(args.This()->InternalFieldCount() > 0);                            \
  type* wrap = static_cast<type*>(                                          \
      args.This()->GetAlignedPointerFromInternalField(0));                  \
  if (!wrap) {                                                              \
    fprintf(stderr, #type ": Aborting due to unwrap failure at %s:%d\n",    \
            __FILE__, __LINE__);                                            \
    abort();                                                                \
  }

// allow for quick domain check
extern bool using_domains;

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

template <typename TypeName>
CachedBase<TypeName>::CachedBase() {
}

template <typename TypeName>
CachedBase<TypeName>::operator v8::Handle<TypeName>() const {
  return v8::Local<TypeName>::New(node_isolate, handle_);
}

template <typename TypeName>
void CachedBase<TypeName>::operator=(v8::Handle<TypeName> that) {
  assert(handle_.IsEmpty() == true);  // Can only assign once.
  handle_.Reset(node_isolate, that);
}

template <typename TypeName>
bool CachedBase<TypeName>::IsEmpty() const {
  return handle_.IsEmpty();
}

template <typename TypeName>
Cached<TypeName>::operator v8::Handle<v8::Value>() const {
  return CachedBase<TypeName>::operator v8::Handle<TypeName>();
}

template <typename TypeName>
void Cached<TypeName>::operator=(v8::Handle<TypeName> that) {
  CachedBase<TypeName>::operator=(that);
}

inline Cached<v8::Value>::operator v8::Handle<v8::Value>() const {
  return CachedBase<v8::Value>::operator v8::Handle<v8::Value>();
}

inline void Cached<v8::Value>::operator=(v8::Handle<v8::Value> that) {
  CachedBase<v8::Value>::operator=(that);
}

// Forward declarations, see node.h
NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    const v8::Handle<v8::Object> recv,
    const char* method,
    int argc,
    v8::Handle<v8::Value>* argv);
NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    const v8::Handle<v8::Object> object,
    const v8::Handle<v8::String> symbol,
    int argc,
    v8::Handle<v8::Value>* argv);
NODE_EXTERN v8::Handle<v8::Value> MakeCallback(
    const v8::Handle<v8::Object> object,
    const v8::Handle<v8::Function> callback,
    int argc,
    v8::Handle<v8::Value>* argv);

template <typename TypeName>
v8::Handle<v8::Value> MakeCallback(
    const v8::Persistent<v8::Object>& recv,
    const TypeName method,
    int argc,
    v8::Handle<v8::Value>* argv) {
  v8::Local<v8::Object> recv_obj =
      v8::Local<v8::Object>::New(node_isolate, recv);
  return MakeCallback(recv_obj, method, argc, argv);
}

template <typename TypeName>
v8::Handle<v8::Value> MakeCallback(
    const v8::Persistent<v8::Object>& recv,
    const Cached<TypeName>& method,
    int argc,
    v8::Handle<v8::Value>* argv) {
  const v8::Handle<TypeName> handle = method;
  return MakeCallback(recv, handle, argc, argv);
}

inline bool HasInstance(v8::Persistent<v8::FunctionTemplate>& function_template,
                        v8::Handle<v8::Value> value) {
  v8::Local<v8::FunctionTemplate> function_template_handle =
      v8::Local<v8::FunctionTemplate>::New(node_isolate, function_template);
  return function_template_handle->HasInstance(value);
}

inline v8::Local<v8::Object> NewInstance(v8::Persistent<v8::Function>& ctor,
                                          int argc,
                                          v8::Handle<v8::Value>* argv) {
  v8::Local<v8::Function> constructor_handle =
      v8::Local<v8::Function>::New(node_isolate, ctor);
  return constructor_handle->NewInstance(argc, argv);
}

namespace Buffer {

template <typename TypeName>
inline char* Data(v8::Persistent<TypeName>& val) {
  NODE_EXTERN char* Data(v8::Handle<v8::Value>);
  NODE_EXTERN char* Data(v8::Handle<v8::Object>);
  return Data(v8::Local<TypeName>::New(node_isolate, val));
}

template <typename TypeName>
inline size_t Length(v8::Persistent<TypeName>& val) {
  NODE_EXTERN size_t Length(v8::Handle<v8::Value>);
  NODE_EXTERN size_t Length(v8::Handle<v8::Object>);
  return Length(v8::Local<TypeName>::New(node_isolate, val));
}

} // namespace Buffer

} // namespace node

#endif // SRC_NODE_INTERNALS_H_
