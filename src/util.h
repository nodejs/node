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

#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_persistent.h"
#include "v8.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>  // std::function
#include <set>
#include <string>
#include <array>
#include <unordered_map>

namespace node {

// These should be used in our code as opposed to the native
// versions as they abstract out some platform and or
// compiler version specific functionality
// malloc(0) and realloc(ptr, 0) have implementation-defined behavior in
// that the standard allows them to either return a unique pointer or a
// nullptr for zero-sized allocation requests.  Normalize by always using
// a nullptr.
template <typename T>
inline T* UncheckedRealloc(T* pointer, size_t n);
template <typename T>
inline T* UncheckedMalloc(size_t n);
template <typename T>
inline T* UncheckedCalloc(size_t n);

// Same things, but aborts immediately instead of returning nullptr when
// no memory is available.
template <typename T>
inline T* Realloc(T* pointer, size_t n);
template <typename T>
inline T* Malloc(size_t n);
template <typename T>
inline T* Calloc(size_t n);

inline char* Malloc(size_t n);
inline char* Calloc(size_t n);
inline char* UncheckedMalloc(size_t n);
inline char* UncheckedCalloc(size_t n);

template <typename T>
inline T MultiplyWithOverflowCheck(T a, T b);

// Used by the allocation functions when allocation fails.
// Thin wrapper around v8::Isolate::LowMemoryNotification() that checks
// whether V8 is initialized.
void LowMemoryNotification();

// The slightly odd function signature for Assert() is to ease
// instruction cache pressure in calls from CHECK.
[[noreturn]] void Abort();
[[noreturn]] void Assert(const char* const (*args)[4]);
void DumpBacktrace(FILE* fp);

#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                    \
  TypeName(const TypeName&) = delete;                                         \
  TypeName& operator=(const TypeName&) = delete

// Windows 8+ does not like abort() in Release mode
#ifdef _WIN32
#define ABORT_NO_BACKTRACE() _exit(134)
#else
#define ABORT_NO_BACKTRACE() abort()
#endif

#define ABORT() node::Abort()

#ifdef __GNUC__
#define LIKELY(expr) __builtin_expect(!!(expr), 1)
#define UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__
#else
#define LIKELY(expr) expr
#define UNLIKELY(expr) expr
#define PRETTY_FUNCTION_NAME ""
#endif

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (UNLIKELY(!(expr))) {                                                  \
      static const char* const args[] = { __FILE__, STRINGIFY(__LINE__),      \
                                          #expr, PRETTY_FUNCTION_NAME };      \
      node::Assert(&args);                                                    \
    }                                                                         \
  } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_NULL(val) CHECK((val) == nullptr)
#define CHECK_NOT_NULL(val) CHECK((val) != nullptr)
#define CHECK_IMPLIES(a, b) CHECK(!(a) || (b))

#define UNREACHABLE() ABORT()

// TAILQ-style intrusive list node.
template <typename T>
class ListNode;

// TAILQ-style intrusive list head.
template <typename T, ListNode<T> (T::*M)>
class ListHead;

template <typename T>
class ListNode {
 public:
  inline ListNode();
  inline ~ListNode();
  inline void Remove();
  inline bool IsEmpty() const;

 private:
  template <typename U, ListNode<U> (U::*M)> friend class ListHead;
  friend int GenDebugSymbols();
  ListNode* prev_;
  ListNode* next_;
  DISALLOW_COPY_AND_ASSIGN(ListNode);
};

template <typename T, ListNode<T> (T::*M)>
class ListHead {
 public:
  class Iterator {
   public:
    inline T* operator*() const;
    inline const Iterator& operator++();
    inline bool operator!=(const Iterator& that) const;

   private:
    friend class ListHead;
    inline explicit Iterator(ListNode<T>* node);
    ListNode<T>* node_;
  };

  inline ListHead() = default;
  inline ~ListHead();
  inline void PushBack(T* element);
  inline void PushFront(T* element);
  inline bool IsEmpty() const;
  inline T* PopFront();
  inline Iterator begin() const;
  inline Iterator end() const;

 private:
  friend int GenDebugSymbols();
  ListNode<T> head_;
  DISALLOW_COPY_AND_ASSIGN(ListHead);
};

// The helper is for doing safe downcasts from base types to derived types.
template <typename Inner, typename Outer>
class ContainerOfHelper {
 public:
  inline ContainerOfHelper(Inner Outer::*field, Inner* pointer);
  template <typename TypeName>
  inline operator TypeName*() const;
 private:
  Outer* const pointer_;
};

// Calculate the address of the outer (i.e. embedding) struct from
// the interior pointer to a data member.
template <typename Inner, typename Outer>
inline ContainerOfHelper<Inner, Outer> ContainerOf(Inner Outer::*field,
                                                   Inner* pointer);

// If persistent.IsWeak() == false, then do not call persistent.Reset()
// while the returned Local<T> is still in scope, it will destroy the
// reference to the object.
template <class TypeName>
inline v8::Local<TypeName> PersistentToLocal(
    v8::Isolate* isolate,
    const Persistent<TypeName>& persistent);

// Unchecked conversion from a non-weak Persistent<T> to Local<T>,
// use with care!
//
// Do not call persistent.Reset() while the returned Local<T> is still in
// scope, it will destroy the reference to the object.
template <class TypeName>
inline v8::Local<TypeName> StrongPersistentToLocal(
    const Persistent<TypeName>& persistent);

template <class TypeName>
inline v8::Local<TypeName> WeakPersistentToLocal(
    v8::Isolate* isolate,
    const Persistent<TypeName>& persistent);

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

// Used to be a macro, hence the uppercase name.
template <int N>
inline v8::Local<v8::String> FIXED_ONE_BYTE_STRING(
    v8::Isolate* isolate,
    const char(&data)[N]) {
  return OneByteString(isolate, data, N - 1);
}

template <std::size_t N>
inline v8::Local<v8::String> FIXED_ONE_BYTE_STRING(
    v8::Isolate* isolate,
    const std::array<char, N>& arr) {
  return OneByteString(isolate, arr.data(), N - 1);
}



// Swaps bytes in place. nbytes is the number of bytes to swap and must be a
// multiple of the word size (checked by function).
inline void SwapBytes16(char* data, size_t nbytes);
inline void SwapBytes32(char* data, size_t nbytes);
inline void SwapBytes64(char* data, size_t nbytes);

// tolower() is locale-sensitive.  Use ToLower() instead.
inline char ToLower(char c);
inline std::string ToLower(const std::string& in);

// strcasecmp() is locale-sensitive.  Use StringEqualNoCase() instead.
inline bool StringEqualNoCase(const char* a, const char* b);

// strncasecmp() is locale-sensitive.  Use StringEqualNoCaseN() instead.
inline bool StringEqualNoCaseN(const char* a, const char* b, size_t length);

// Allocates an array of member type T. For up to kStackStorageSize items,
// the stack is used, otherwise malloc().
template <typename T, size_t kStackStorageSize = 1024>
class MaybeStackBuffer {
 public:
  const T* out() const {
    return buf_;
  }

  T* out() {
    return buf_;
  }

  // operator* for compatibility with `v8::String::(Utf8)Value`
  T* operator*() {
    return buf_;
  }

  const T* operator*() const {
    return buf_;
  }

  T& operator[](size_t index) {
    CHECK_LT(index, length());
    return buf_[index];
  }

  const T& operator[](size_t index) const {
    CHECK_LT(index, length());
    return buf_[index];
  }

  size_t length() const {
    return length_;
  }

  // Current maximum capacity of the buffer with which SetLength() can be used
  // without first calling AllocateSufficientStorage().
  size_t capacity() const {
    return IsAllocated() ? capacity_ :
                           IsInvalidated() ? 0 : kStackStorageSize;
  }

  // Make sure enough space for `storage` entries is available.
  // This method can be called multiple times throughout the lifetime of the
  // buffer, but once this has been called Invalidate() cannot be used.
  // Content of the buffer in the range [0, length()) is preserved.
  void AllocateSufficientStorage(size_t storage) {
    CHECK(!IsInvalidated());
    if (storage > capacity()) {
      bool was_allocated = IsAllocated();
      T* allocated_ptr = was_allocated ? buf_ : nullptr;
      buf_ = Realloc(allocated_ptr, storage);
      capacity_ = storage;
      if (!was_allocated && length_ > 0)
        memcpy(buf_, buf_st_, length_ * sizeof(buf_[0]));
    }

    length_ = storage;
  }

  void SetLength(size_t length) {
    // capacity() returns how much memory is actually available.
    CHECK_LE(length, capacity());
    length_ = length;
  }

  void SetLengthAndZeroTerminate(size_t length) {
    // capacity() returns how much memory is actually available.
    CHECK_LE(length + 1, capacity());
    SetLength(length);

    // T() is 0 for integer types, nullptr for pointers, etc.
    buf_[length] = T();
  }

  // Make derefencing this object return nullptr.
  // This method can be called multiple times throughout the lifetime of the
  // buffer, but once this has been called AllocateSufficientStorage() cannot
  // be used.
  void Invalidate() {
    CHECK(!IsAllocated());
    length_ = 0;
    buf_ = nullptr;
  }

  // If the buffer is stored in the heap rather than on the stack.
  bool IsAllocated() const {
    return !IsInvalidated() && buf_ != buf_st_;
  }

  // If Invalidate() has been called.
  bool IsInvalidated() const {
    return buf_ == nullptr;
  }

  // Release ownership of the malloc'd buffer.
  // Note: This does not free the buffer.
  void Release() {
    CHECK(IsAllocated());
    buf_ = buf_st_;
    length_ = 0;
    capacity_ = 0;
  }

  MaybeStackBuffer() : length_(0), capacity_(0), buf_(buf_st_) {
    // Default to a zero-length, null-terminated buffer.
    buf_[0] = T();
  }

  explicit MaybeStackBuffer(size_t storage) : MaybeStackBuffer() {
    AllocateSufficientStorage(storage);
  }

  ~MaybeStackBuffer() {
    if (IsAllocated())
      free(buf_);
  }

 private:
  size_t length_;
  // capacity of the malloc'ed buf_
  size_t capacity_;
  T* buf_;
  T buf_st_[kStackStorageSize];
};

class Utf8Value : public MaybeStackBuffer<char> {
 public:
  explicit Utf8Value(v8::Isolate* isolate, v8::Local<v8::Value> value);
};

class TwoByteValue : public MaybeStackBuffer<uint16_t> {
 public:
  explicit TwoByteValue(v8::Isolate* isolate, v8::Local<v8::Value> value);
};

class BufferValue : public MaybeStackBuffer<char> {
 public:
  explicit BufferValue(v8::Isolate* isolate, v8::Local<v8::Value> value);
};

#define SPREAD_BUFFER_ARG(val, name)                                          \
  CHECK((val)->IsArrayBufferView());                                          \
  v8::Local<v8::ArrayBufferView> name = (val).As<v8::ArrayBufferView>();      \
  v8::ArrayBuffer::Contents name##_c = name->Buffer()->GetContents();         \
  const size_t name##_offset = name->ByteOffset();                            \
  const size_t name##_length = name->ByteLength();                            \
  char* const name##_data =                                                   \
      static_cast<char*>(name##_c.Data()) + name##_offset;                    \
  if (name##_length > 0)                                                      \
    CHECK_NE(name##_data, nullptr);

// Use this when a variable or parameter is unused in order to explicitly
// silence a compiler warning about that.
template <typename T> inline void USE(T&&) {}

// Run a function when exiting the current scope.
struct OnScopeLeave {
  std::function<void()> fn_;

  explicit OnScopeLeave(std::function<void()> fn) : fn_(fn) {}
  ~OnScopeLeave() { fn_(); }
};

// Simple RAII wrapper for contiguous data that uses malloc()/free().
template <typename T>
struct MallocedBuffer {
  T* data;
  size_t size;

  T* release() {
    T* ret = data;
    data = nullptr;
    return ret;
  }

  void Truncate(size_t new_size) {
    CHECK(new_size <= size);
    size = new_size;
  }

  inline bool is_empty() const { return data == nullptr; }

  MallocedBuffer() : data(nullptr), size(0) {}
  explicit MallocedBuffer(size_t size) : data(Malloc<T>(size)), size(size) {}
  MallocedBuffer(T* data, size_t size) : data(data), size(size) {}
  MallocedBuffer(MallocedBuffer&& other) : data(other.data), size(other.size) {
    other.data = nullptr;
  }
  MallocedBuffer& operator=(MallocedBuffer&& other) {
    this->~MallocedBuffer();
    return *new(this) MallocedBuffer(std::move(other));
  }
  ~MallocedBuffer() {
    free(data);
  }
  MallocedBuffer(const MallocedBuffer&) = delete;
  MallocedBuffer& operator=(const MallocedBuffer&) = delete;
};

// Test whether some value can be called with ().
template <typename T, typename = void>
struct is_callable : std::is_function<T> { };

template <typename T>
struct is_callable<T, typename std::enable_if<
    std::is_same<decltype(void(&T::operator())), void>::value
    >::type> : std::true_type { };

template <typename T, void (*function)(T*)>
struct FunctionDeleter {
  void operator()(T* pointer) const { function(pointer); }
  typedef std::unique_ptr<T, FunctionDeleter> Pointer;
};

template <typename T, void (*function)(T*)>
using DeleteFnPtr = typename FunctionDeleter<T, function>::Pointer;

std::set<std::string> ParseCommaSeparatedSet(const std::string& in);

inline v8::MaybeLocal<v8::Value> ToV8Value(v8::Local<v8::Context> context,
                                           const std::string& str);
template <typename T>
inline v8::MaybeLocal<v8::Value> ToV8Value(v8::Local<v8::Context> context,
                                           const std::vector<T>& vec);
template <typename T, typename U>
inline v8::MaybeLocal<v8::Value> ToV8Value(v8::Local<v8::Context> context,
                                           const std::unordered_map<T, U>& map);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_UTIL_H_
