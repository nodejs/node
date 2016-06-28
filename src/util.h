#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "v8.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <tr1/type_traits>  // NOLINT(build/c++tr1)
#else
#include <type_traits>  // std::remove_reference
#endif

namespace node {

#define FIXED_ONE_BYTE_STRING(isolate, string)                                \
  (node::OneByteString((isolate), (string), sizeof(string) - 1))

#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                    \
  void operator=(const TypeName&) = delete;                                   \
  void operator=(TypeName&&) = delete;                                        \
  TypeName(const TypeName&) = delete;                                         \
  TypeName(TypeName&&) = delete

// Windows 8+ does not like abort() in Release mode
#ifdef _WIN32
#define ABORT() raise(SIGABRT)
#else
#define ABORT() abort()
#endif

#if defined(NDEBUG)
# define ASSERT(expression)
# define CHECK(expression)                                                    \
  do {                                                                        \
    if (!(expression)) ABORT();                                               \
  } while (0)
#else
# define ASSERT(expression)  assert(expression)
# define CHECK(expression)   assert(expression)
#endif

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_GE(a, b) ASSERT((a) >= (b))
#define ASSERT_GT(a, b) ASSERT((a) > (b))
#define ASSERT_LE(a, b) ASSERT((a) <= (b))
#define ASSERT_LT(a, b) ASSERT((a) < (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_NE(a, b) CHECK((a) != (b))

#define UNREACHABLE() ABORT()

// TAILQ-style intrusive list node.
template <typename T>
class ListNode;

template <typename T>
using ListNodeMember = ListNode<T> T::*;

// VS 2013 doesn't understand dependent templates.
#ifdef _MSC_VER
#define ListNodeMember(T) ListNodeMember
#else
#define ListNodeMember(T) ListNodeMember<T>
#endif

// TAILQ-style intrusive list head.
template <typename T, ListNodeMember(T) M>
class ListHead;

template <typename T>
class ListNode {
 public:
  inline ListNode();
  inline ~ListNode();
  inline void Remove();
  inline bool IsEmpty() const;

 private:
  template <typename U, ListNodeMember(U) M> friend class ListHead;
  ListNode* prev_;
  ListNode* next_;
  DISALLOW_COPY_AND_ASSIGN(ListNode);
};

template <typename T, ListNodeMember(T) M>
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
  inline void MoveBack(ListHead* that);
  inline void PushBack(T* element);
  inline void PushFront(T* element);
  inline bool IsEmpty() const;
  inline T* PopFront();
  inline Iterator begin() const;
  inline Iterator end() const;

 private:
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
    const v8::Persistent<TypeName>& persistent);

// Unchecked conversion from a non-weak Persistent<T> to Local<TLocal<T>,
// use with care!
//
// Do not call persistent.Reset() while the returned Local<T> is still in
// scope, it will destroy the reference to the object.
template <class TypeName>
inline v8::Local<TypeName> StrongPersistentToLocal(
    const v8::Persistent<TypeName>& persistent);

template <class TypeName>
inline v8::Local<TypeName> WeakPersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent);

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

inline void Wrap(v8::Local<v8::Object> object, void* pointer);

inline void ClearWrap(v8::Local<v8::Object> object);

template <typename TypeName>
inline TypeName* Unwrap(v8::Local<v8::Object> object);

inline void SwapBytes(uint16_t* dst, const uint16_t* src, size_t buflen);

// tolower() is locale-sensitive.  Use ToLower() instead.
inline char ToLower(char c);

// strcasecmp() is locale-sensitive.  Use StringEqualNoCase() instead.
inline bool StringEqualNoCase(const char* a, const char* b);

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

    size_t length() const {
      return length_;
    }

    // Call to make sure enough space for `storage` entries is available.
    // There can only be 1 call to AllocateSufficientStorage or Invalidate
    // per instance.
    void AllocateSufficientStorage(size_t storage) {
      if (storage <= kStackStorageSize) {
        buf_ = buf_st_;
      } else {
        // Guard against overflow.
        CHECK_LE(storage, sizeof(T) * storage);

        buf_ = static_cast<T*>(malloc(sizeof(T) * storage));
        CHECK_NE(buf_, nullptr);
      }

      // Remember how much was allocated to check against that in SetLength().
      length_ = storage;
    }

    void SetLength(size_t length) {
      // length_ stores how much memory was allocated.
      CHECK_LE(length, length_);
      length_ = length;
    }

    void SetLengthAndZeroTerminate(size_t length) {
      // length_ stores how much memory was allocated.
      CHECK_LE(length + 1, length_);
      SetLength(length);

      // T() is 0 for integer types, nullptr for pointers, etc.
      buf_[length] = T();
    }

    // Make derefencing this object return nullptr.
    // Calling this is mutually exclusive with calling
    // AllocateSufficientStorage.
    void Invalidate() {
      CHECK_EQ(buf_, buf_st_);
      length_ = 0;
      buf_ = nullptr;
    }

    MaybeStackBuffer() : length_(0), buf_(buf_st_) {
      // Default to a zero-length, null-terminated buffer.
      buf_[0] = T();
    }

    ~MaybeStackBuffer() {
      if (buf_ != buf_st_)
        free(buf_);
    }

  private:
    size_t length_;
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

}  // namespace node

#endif  // SRC_UTIL_H_
