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

#include "v8.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define FIXED_ONE_BYTE_STRING(isolate, string)                                \
  (node::OneByteString((isolate), (string), sizeof(string) - 1))

struct sockaddr;

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

// If persistent.IsWeak() == false, then do not call persistent.Dispose()
// while the returned Local<T> is still in scope, it will destroy the
// reference to the object.
template <class TypeName>
inline v8::Local<TypeName> PersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent);

v8::Handle<v8::Value> MakeCallback(
    const v8::Handle<v8::Object> recv,
    uint32_t index,
    int argc,
    v8::Handle<v8::Value>* argv);

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

inline bool HasInstance(
    const v8::Persistent<v8::FunctionTemplate>& function_template,
    v8::Handle<v8::Value> value);

inline v8::Local<v8::Object> NewInstance(
    const v8::Persistent<v8::Function>& ctor,
    int argc = 0,
    v8::Handle<v8::Value>* argv = NULL);

// Convenience wrapper around v8::String::NewFromOneByte().
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const char* data,
                                           int length = -1);

// For the people that compile with -funsigned-char.
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const signed char* data,
                                           int length = -1);

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const unsigned char* data,
                                           int length = -1);

// Convert a struct sockaddr to a { address: '1.2.3.4', port: 1234 } JS object.
// Sets address and port properties on the info object and returns it.
// If |info| is omitted, a new object is returned.
v8::Local<v8::Object> AddressToJS(
    const sockaddr* addr,
    v8::Handle<v8::Object> info = v8::Handle<v8::Object>());

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

#define NODE_WRAP(Object, Pointer)                                             \
  do {                                                                         \
    assert(!Object.IsEmpty());                                                 \
    assert(Object->InternalFieldCount() > 0);                                  \
    Object->SetAlignedPointerInInternalField(0, Pointer);                      \
  }                                                                            \
  while (0)

#define NODE_UNWRAP(Object, TypeName, Var)                                     \
  do {                                                                         \
    assert(!Object.IsEmpty());                                                 \
    assert(Object->InternalFieldCount() > 0);                                  \
    Var = static_cast<TypeName*>(                                              \
        Object->GetAlignedPointerFromInternalField(0));                        \
    if (!Var) {                                                                \
      fprintf(stderr, #TypeName ": Aborting due to unwrap failure at %s:%d\n", \
              __FILE__, __LINE__);                                             \
      abort();                                                                 \
    }                                                                          \
  }                                                                            \
  while (0)

#define NODE_UNWRAP_NO_ABORT(Object, TypeName, Var)                            \
  do {                                                                         \
    assert(!Object.IsEmpty());                                                 \
    assert(Object->InternalFieldCount() > 0);                                  \
    Var = static_cast<TypeName*>(                                              \
        Object->GetAlignedPointerFromInternalField(0));                        \
  }                                                                            \
  while (0)

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

template <class TypeName>
inline v8::Local<TypeName> PersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent) {
  if (persistent.IsWeak()) {
    return v8::Local<TypeName>::New(isolate, persistent);
  } else {
    return *reinterpret_cast<v8::Local<TypeName>*>(
        const_cast<v8::Persistent<TypeName>*>(&persistent));
  }
}

template <typename TypeName>
CachedBase<TypeName>::CachedBase() {
}

template <typename TypeName>
CachedBase<TypeName>::operator v8::Handle<TypeName>() const {
  return PersistentToLocal(node_isolate, handle_);
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

template <typename TypeName>
v8::Handle<v8::Value> MakeCallback(
    const v8::Persistent<v8::Object>& recv,
    const TypeName method,
    int argc,
    v8::Handle<v8::Value>* argv) {
  v8::Local<v8::Object> recv_obj = PersistentToLocal(node_isolate, recv);
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

inline bool HasInstance(
    const v8::Persistent<v8::FunctionTemplate>& function_template,
    v8::Handle<v8::Value> value) {
  if (function_template.IsEmpty()) return false;
  v8::Local<v8::FunctionTemplate> function_template_handle =
      PersistentToLocal(node_isolate, function_template);
  return function_template_handle->HasInstance(value);
}

inline v8::Local<v8::Object> NewInstance(
    const v8::Persistent<v8::Function>& ctor,
    int argc,
    v8::Handle<v8::Value>* argv) {
  v8::Local<v8::Function> constructor_handle =
      PersistentToLocal(node_isolate, ctor);
  return constructor_handle->NewInstance(argc, argv);
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::String::kNormalString,
                                    length);
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const signed char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::String::kNormalString,
                                    length);
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const unsigned char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::String::kNormalString,
                                    length);
}

bool InDomain();

v8::Handle<v8::Value> GetDomain();

}  // namespace node

#endif  // SRC_NODE_INTERNALS_H_
