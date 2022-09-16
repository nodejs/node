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

#ifndef SRC_UTIL_INL_H_
#define SRC_UTIL_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cmath>
#include <cstring>
#include <locale>
#include "util.h"
#include <immintrin.h>

// These are defined by <sys/byteorder.h> or <netinet/in.h> on some systems.
// To avoid warnings, undefine them before redefining them.
#ifdef BSWAP_2
# undef BSWAP_2
#endif
#ifdef BSWAP_4
# undef BSWAP_4
#endif
#ifdef BSWAP_8
# undef BSWAP_8
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#define BSWAP_2(x) _byteswap_ushort(x)
#define BSWAP_4(x) _byteswap_ulong(x)
#define BSWAP_8(x) _byteswap_uint64(x)
#else
#define BSWAP_2(x) ((x) << 8) | ((x) >> 8)
#define BSWAP_4(x)                                                            \
  (((x) & 0xFF) << 24) |                                                      \
  (((x) & 0xFF00) << 8) |                                                     \
  (((x) >> 8) & 0xFF00) |                                                     \
  (((x) >> 24) & 0xFF)
#define BSWAP_8(x)                                                            \
  (((x) & 0xFF00000000000000ull) >> 56) |                                     \
  (((x) & 0x00FF000000000000ull) >> 40) |                                     \
  (((x) & 0x0000FF0000000000ull) >> 24) |                                     \
  (((x) & 0x000000FF00000000ull) >> 8) |                                      \
  (((x) & 0x00000000FF000000ull) << 8) |                                      \
  (((x) & 0x0000000000FF0000ull) << 24) |                                     \
  (((x) & 0x000000000000FF00ull) << 40) |                                     \
  (((x) & 0x00000000000000FFull) << 56)
#endif

#define CHAR_TEST(bits, name, expr)                                           \
  template <typename T>                                                       \
  bool name(const T ch) {                                                     \
    static_assert(sizeof(ch) >= (bits) / 8,                                   \
                  "Character must be wider than " #bits " bits");             \
    return (expr);                                                            \
  }

namespace node {

template <typename T>
ListNode<T>::ListNode() : prev_(this), next_(this) {}

template <typename T>
ListNode<T>::~ListNode() {
  Remove();
}

template <typename T>
void ListNode<T>::Remove() {
  prev_->next_ = next_;
  next_->prev_ = prev_;
  prev_ = this;
  next_ = this;
}

template <typename T>
bool ListNode<T>::IsEmpty() const {
  return prev_ == this;
}

template <typename T, ListNode<T> (T::*M)>
ListHead<T, M>::Iterator::Iterator(ListNode<T>* node) : node_(node) {}

template <typename T, ListNode<T> (T::*M)>
T* ListHead<T, M>::Iterator::operator*() const {
  return ContainerOf(M, node_);
}

template <typename T, ListNode<T> (T::*M)>
const typename ListHead<T, M>::Iterator&
ListHead<T, M>::Iterator::operator++() {
  node_ = node_->next_;
  return *this;
}

template <typename T, ListNode<T> (T::*M)>
bool ListHead<T, M>::Iterator::operator!=(const Iterator& that) const {
  return node_ != that.node_;
}

template <typename T, ListNode<T> (T::*M)>
ListHead<T, M>::~ListHead() {
  while (IsEmpty() == false)
    head_.next_->Remove();
}

template <typename T, ListNode<T> (T::*M)>
void ListHead<T, M>::PushBack(T* element) {
  ListNode<T>* that = &(element->*M);
  head_.prev_->next_ = that;
  that->prev_ = head_.prev_;
  that->next_ = &head_;
  head_.prev_ = that;
}

template <typename T, ListNode<T> (T::*M)>
void ListHead<T, M>::PushFront(T* element) {
  ListNode<T>* that = &(element->*M);
  head_.next_->prev_ = that;
  that->prev_ = &head_;
  that->next_ = head_.next_;
  head_.next_ = that;
}

template <typename T, ListNode<T> (T::*M)>
bool ListHead<T, M>::IsEmpty() const {
  return head_.IsEmpty();
}

template <typename T, ListNode<T> (T::*M)>
T* ListHead<T, M>::PopFront() {
  if (IsEmpty())
    return nullptr;
  ListNode<T>* node = head_.next_;
  node->Remove();
  return ContainerOf(M, node);
}

template <typename T, ListNode<T> (T::*M)>
typename ListHead<T, M>::Iterator ListHead<T, M>::begin() const {
  return Iterator(head_.next_);
}

template <typename T, ListNode<T> (T::*M)>
typename ListHead<T, M>::Iterator ListHead<T, M>::end() const {
  return Iterator(const_cast<ListNode<T>*>(&head_));
}

template <typename Inner, typename Outer>
constexpr uintptr_t OffsetOf(Inner Outer::*field) {
  return reinterpret_cast<uintptr_t>(&(static_cast<Outer*>(nullptr)->*field));
}

template <typename Inner, typename Outer>
ContainerOfHelper<Inner, Outer>::ContainerOfHelper(Inner Outer::*field,
                                                   Inner* pointer)
    : pointer_(
        reinterpret_cast<Outer*>(
            reinterpret_cast<uintptr_t>(pointer) - OffsetOf(field))) {}

template <typename Inner, typename Outer>
template <typename TypeName>
ContainerOfHelper<Inner, Outer>::operator TypeName*() const {
  return static_cast<TypeName*>(pointer_);
}

template <typename Inner, typename Outer>
constexpr ContainerOfHelper<Inner, Outer> ContainerOf(Inner Outer::*field,
                                                      Inner* pointer) {
  return ContainerOfHelper<Inner, Outer>(field, pointer);
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::NewStringType::kNormal,
                                    length).ToLocalChecked();
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const signed char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::NewStringType::kNormal,
                                    length).ToLocalChecked();
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const unsigned char* data,
                                           int length) {
  return v8::String::NewFromOneByte(
             isolate, data, v8::NewStringType::kNormal, length)
      .ToLocalChecked();
}
#if (__x86_64__ || __i386__ || _M_X86 || _M_X64)

#ifdef _MSC_VER
  #include <intrin.h>
  #define __cpuid_count(__level, __count, __eax, __ebx, __ecx, __edx) \
  {						\
    int info[4];				\
    __cpuidex(info, __level, __count);	\
    __eax = info[0];			\
    __ebx = info[1];			\
    __ecx = info[2];			\
    __edx = info[3];			\
  }
#else
  #include <cpuid.h>
#endif

#define bit_XSAVE_XRSTORE (1 << 27)
#define bit_AVX512VBMI (1 << 1)
#define bit_SSSE3 (1 << 9)
#define bit_SSE41 (1 << 19)
#define bit_SSE42 (1 << 20)
#define _XCR_XFEATURE_ENABLED_MASK 0
#define _XCR_XMM_AND_YMM_STATE_ENABLED_BY_OS 0x6

// This static variable is initialized once when the library is first
// used, and not changed in the remaining lifetime of the program.
inline static int simd_level = 0;

__attribute__((target("avx512vbmi")))
inline static void set_simd_level() {
  //fast return if simd_level already judged
  if (simd_level != 0) 
    return;
  else {
    unsigned int eax, ebx = 0, ecx = 0, edx;
    unsigned int max_level;

    #ifdef _MSC_VER
      int info[4];
      __cpuidex(info, 0, 0);
      max_level = info[0];
    #else
      max_level = __get_cpuid_max(0, NULL);
    #endif

    // Try to find AVX512vbmi as the fasted path:
    // 1) CPUID indicates that the OS uses XSAVE and XRSTORE instructions
    //    (allowing saving YMM registers on context switch)
    // 2) CPUID indicates support for AVX512VBMI
    // 3) XGETBV indicates the AVX registers will be saved and restored on
	  //    context switch
    if (max_level >= 1) {
      __cpuid_count(1, 0, eax, ebx, ecx, edx);
      if (ecx & bit_XSAVE_XRSTORE) {
        uint64_t xcr_mask;
        xcr_mask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
        if (xcr_mask & _XCR_XMM_AND_YMM_STATE_ENABLED_BY_OS) {
          if (max_level >= 7) {
            __cpuid_count(7, 0, eax, ebx, ecx, edx);
            if (ecx & bit_AVX512VBMI) {
              simd_level = 1;
              return;
            }
          }
        }
      }
    }

    // Fall into SSE path, expected supported by almost all systems
    if (max_level >= 1) {
      __cpuid(1, eax, ebx, ecx, edx);
      if ((ecx & bit_SSSE3) | (ecx & bit_SSE42) | (ecx & bit_SSE42)) {
        simd_level = 2;
        return;
		  }
	  }
  }
  
  // Fall into legacy bit operations which is slow
  simd_level = 3;
  return;
}

__attribute__((target("avx512vbmi")))
inline static void
swap16_avx ( char* data, size_t* nbytes)
{
    __m512i shuffle_input = _mm512_set_epi64(0x3e3f3c3d3a3b3839,
                                            0x3637343532333031,
                                            0x2e2f2c2d2a2b2829,
                                            0x2627242522232021,
                                            0x1e1f1c1d1a1b1819,
                                            0x1617141512131011,
                                            0x0e0f0c0d0a0b0809,
                                            0x0607040502030001);
    while(*nbytes >= 64){
        __m512i v = _mm512_loadu_si512(data);
        __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);
        _mm512_storeu_si512(data, in);
        data += 64;
        *nbytes -= 64;
    }
}

__attribute__((target("avx512vbmi")))
inline static void
swap32_avx ( char* data, size_t* nbytes)
{
    __m512i shuffle_input = _mm512_set_epi64(0x3c3d3e3f38393a3b,
                                            0x3435363730313233,
                                            0x2c2d2e2f28292a2b,
                                            0x2425262720212223,
                                            0x1c1d1e1f18191a1b,
                                            0x1415161710111213,
                                            0x0c0d0e0f08090a0b,
                                            0x0405060700010203);
    while(*nbytes >= 64){
        __m512i v = _mm512_loadu_si512(data);
        __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);
        _mm512_storeu_si512(data, in);
        data += 64;
        *nbytes -= 64;
    }
}

__attribute__((target("avx512vbmi")))
inline static void
swap64_avx ( char* data, size_t* nbytes)
{
    __m512i shuffle_input = _mm512_set_epi64(0x38393a3b3c3d3e3f,
                                            0x3031323334353637,
                                            0x28292a2b2c2d2e2f,
                                            0x2021222324252627,
                                            0x18191a1b1c1d1e1f,
                                            0x1011121314151617,
                                            0x08090a0b0c0d0e0f,
                                            0x0001020304050607);
    while(*nbytes >= 64){
        __m512i v = _mm512_loadu_si512(data);
        __m512i in = _mm512_permutexvar_epi8(shuffle_input, v);
        _mm512_storeu_si512(data, in);
        data += 64;
        *nbytes -= 64;
    }
}

__attribute__((target("ssse3")))
inline static void
swap16_sse ( char* data, size_t* nbytes)
{
    __m128i shuffle_input = _mm_set_epi64x(0x0e0f0c0d0a0b0809,
                                          0x0607040502030001);
    while(*nbytes >= 16){
        __m128i v = _mm_loadu_si128((__m128i *)data);
        __m128i in = _mm_shuffle_epi8(v, shuffle_input);
        _mm_storeu_si128((__m128i *)data, in);
        data += 16;
        *nbytes -= 16;
    }
}

__attribute__((target("ssse3")))
inline static void
swap32_sse ( char* data, size_t* nbytes)
{
    __m128i shuffle_input = _mm_set_epi64x(0x0c0d0e0f08090a0b,
                                          0x0405060700010203);
    while(*nbytes >= 16){
        __m128i v = _mm_loadu_si128((__m128i *)data);
        __m128i in = _mm_shuffle_epi8(v, shuffle_input);
        _mm_storeu_si128((__m128i *)data, in);
        data += 16;
        *nbytes -= 16;
    }
}

__attribute__((target("ssse3")))
inline static void
swap64_sse ( char* data, size_t* nbytes)
{
    __m128i shuffle_input = _mm_set_epi64x(0x08090a0b0c0d0e0f,
                                          0x0001020304050607);
    while(*nbytes >= 16){
        __m128i v = _mm_loadu_si128((__m128i *)data);
        __m128i in = _mm_shuffle_epi8(v, shuffle_input);
        _mm_storeu_si128((__m128i *)data, in);
        data += 16;
        *nbytes -= 16;
    }
}

__attribute__((target("avx512vbmi")))
inline static void swap_simd (char* data, size_t* nbytes, size_t size) {
  //early return if level equals to 3 means no simd support
  set_simd_level();
  if(simd_level == 1) {
    switch(size) {
      case 16:
        swap16_avx(data, nbytes);
        break;
      case 32:
        swap32_avx(data, nbytes);
        break;
      case 64:
        swap64_avx(data, nbytes);
        break;
    }
  }
  else if(simd_level == 2) {
    switch(size) {
      case 16:
        swap16_sse(data, nbytes);
        break;
      case 32:
        swap32_sse(data, nbytes);
        break;
      case 64:
        swap64_sse(data, nbytes);
        break;
    }
  }
}

#else
inline static void swap_simd (char* data, size_t* nbytes, size_t size) {
  return;
}
#endif

__attribute__((target("avx512vbmi")))
void SwapBytes16(char* data, size_t nbytes) {
  CHECK_EQ(nbytes % 2, 0);
  //process n*16 bits data in batch using simd swap
  swap_simd(data, &nbytes, 16);

#if defined(_MSC_VER)
  if (AlignUp(data, sizeof(uint16_t)) == data) {
    // MSVC has no strict aliasing, and is able to highly optimize this case.
    uint16_t* data16 = reinterpret_cast<uint16_t*>(data);
    size_t len16 = nbytes / sizeof(*data16);
    for (size_t i = 0; i < len16; i++) {
      data16[i] = BSWAP_2(data16[i]);
    }
    return;
  }
#endif

  uint16_t temp;
  for (size_t i = 0; i < nbytes; i += sizeof(temp)) {
    memcpy(&temp, &data[i], sizeof(temp));
    temp = BSWAP_2(temp);
    memcpy(&data[i], &temp, sizeof(temp));
  }
}
__attribute__((target("avx512vbmi")))
void SwapBytes32(char* data, size_t nbytes) {
  CHECK_EQ(nbytes % 4, 0);
  //process n*32 bits data in batch using simd swap
  swap_simd(data, &nbytes, 32);

#if defined(_MSC_VER)
  // MSVC has no strict aliasing, and is able to highly optimize this case.
  if (AlignUp(data, sizeof(uint32_t)) == data) {
    uint32_t* data32 = reinterpret_cast<uint32_t*>(data);
    size_t len32 = nbytes / sizeof(*data32);
    for (size_t i = 0; i < len32; i++) {
      data32[i] = BSWAP_4(data32[i]);
    }
    return;
  }
#endif

  uint32_t temp;
  for (size_t i = 0; i < nbytes; i += sizeof(temp)) {
    memcpy(&temp, &data[i], sizeof(temp));
    temp = BSWAP_4(temp);
    memcpy(&data[i], &temp, sizeof(temp));
  }
}
__attribute__((target("avx512vbmi")))
void SwapBytes64(char* data, size_t nbytes) {
  CHECK_EQ(nbytes % 8, 0);
  //process n*64 bits data in batch using simd swap
  swap_simd(data, &nbytes, 64);

#if defined(_MSC_VER)
  if (AlignUp(data, sizeof(uint64_t)) == data) {
    // MSVC has no strict aliasing, and is able to highly optimize this case.
    uint64_t* data64 = reinterpret_cast<uint64_t*>(data);
    size_t len64 = nbytes / sizeof(*data64);
    for (size_t i = 0; i < len64; i++) {
      data64[i] = BSWAP_8(data64[i]);
    }
    return;
  }
#endif

  uint64_t temp;
  for (size_t i = 0; i < nbytes; i += sizeof(temp)) {
    memcpy(&temp, &data[i], sizeof(temp));
    temp = BSWAP_8(temp);
    memcpy(&data[i], &temp, sizeof(temp));
  }
}

char ToLower(char c) {
  return std::tolower(c, std::locale::classic());
}

std::string ToLower(const std::string& in) {
  std::string out(in.size(), 0);
  for (size_t i = 0; i < in.size(); ++i)
    out[i] = ToLower(in[i]);
  return out;
}

char ToUpper(char c) {
  return std::toupper(c, std::locale::classic());
}

std::string ToUpper(const std::string& in) {
  std::string out(in.size(), 0);
  for (size_t i = 0; i < in.size(); ++i)
    out[i] = ToUpper(in[i]);
  return out;
}

bool StringEqualNoCase(const char* a, const char* b) {
  while (ToLower(*a) == ToLower(*b++)) {
    if (*a++ == '\0')
      return true;
  }
  return false;
}

bool StringEqualNoCaseN(const char* a, const char* b, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (ToLower(a[i]) != ToLower(b[i]))
      return false;
    if (a[i] == '\0')
      return true;
  }
  return true;
}

template <typename T>
inline T MultiplyWithOverflowCheck(T a, T b) {
  auto ret = a * b;
  if (a != 0)
    CHECK_EQ(b, ret / a);

  return ret;
}

// These should be used in our code as opposed to the native
// versions as they abstract out some platform and or
// compiler version specific functionality.
// malloc(0) and realloc(ptr, 0) have implementation-defined behavior in
// that the standard allows them to either return a unique pointer or a
// nullptr for zero-sized allocation requests.  Normalize by always using
// a nullptr.
template <typename T>
T* UncheckedRealloc(T* pointer, size_t n) {
  size_t full_size = MultiplyWithOverflowCheck(sizeof(T), n);

  if (full_size == 0) {
    free(pointer);
    return nullptr;
  }

  void* allocated = realloc(pointer, full_size);

  if (UNLIKELY(allocated == nullptr)) {
    // Tell V8 that memory is low and retry.
    LowMemoryNotification();
    allocated = realloc(pointer, full_size);
  }

  return static_cast<T*>(allocated);
}

// As per spec realloc behaves like malloc if passed nullptr.
template <typename T>
inline T* UncheckedMalloc(size_t n) {
  if (n == 0) n = 1;
  return UncheckedRealloc<T>(nullptr, n);
}

template <typename T>
inline T* UncheckedCalloc(size_t n) {
  if (n == 0) n = 1;
  MultiplyWithOverflowCheck(sizeof(T), n);
  return static_cast<T*>(calloc(n, sizeof(T)));
}

template <typename T>
inline T* Realloc(T* pointer, size_t n) {
  T* ret = UncheckedRealloc(pointer, n);
  CHECK_IMPLIES(n > 0, ret != nullptr);
  return ret;
}

template <typename T>
inline T* Malloc(size_t n) {
  T* ret = UncheckedMalloc<T>(n);
  CHECK_IMPLIES(n > 0, ret != nullptr);
  return ret;
}

template <typename T>
inline T* Calloc(size_t n) {
  T* ret = UncheckedCalloc<T>(n);
  CHECK_IMPLIES(n > 0, ret != nullptr);
  return ret;
}

// Shortcuts for char*.
inline char* Malloc(size_t n) { return Malloc<char>(n); }
inline char* Calloc(size_t n) { return Calloc<char>(n); }
inline char* UncheckedMalloc(size_t n) { return UncheckedMalloc<char>(n); }
inline char* UncheckedCalloc(size_t n) { return UncheckedCalloc<char>(n); }

// This is a helper in the .cc file so including util-inl.h doesn't include more
// headers than we really need to.
void ThrowErrStringTooLong(v8::Isolate* isolate);

v8::MaybeLocal<v8::Value> ToV8Value(v8::Local<v8::Context> context,
                                    std::string_view str,
                                    v8::Isolate* isolate) {
  if (isolate == nullptr) isolate = context->GetIsolate();
  if (UNLIKELY(str.size() >= static_cast<size_t>(v8::String::kMaxLength))) {
    // V8 only has a TODO comment about adding an exception when the maximum
    // string size is exceeded.
    ThrowErrStringTooLong(isolate);
    return v8::MaybeLocal<v8::Value>();
  }

  return v8::String::NewFromUtf8(
             isolate, str.data(), v8::NewStringType::kNormal, str.size())
      .FromMaybe(v8::Local<v8::String>());
}

template <typename T>
v8::MaybeLocal<v8::Value> ToV8Value(v8::Local<v8::Context> context,
                                    const std::vector<T>& vec,
                                    v8::Isolate* isolate) {
  if (isolate == nullptr) isolate = context->GetIsolate();
  v8::EscapableHandleScope handle_scope(isolate);

  MaybeStackBuffer<v8::Local<v8::Value>, 128> arr(vec.size());
  arr.SetLength(vec.size());
  for (size_t i = 0; i < vec.size(); ++i) {
    if (!ToV8Value(context, vec[i], isolate).ToLocal(&arr[i]))
      return v8::MaybeLocal<v8::Value>();
  }

  return handle_scope.Escape(v8::Array::New(isolate, arr.out(), arr.length()));
}

template <typename T>
v8::MaybeLocal<v8::Value> ToV8Value(v8::Local<v8::Context> context,
                                    const std::set<T>& set,
                                    v8::Isolate* isolate) {
  if (isolate == nullptr) isolate = context->GetIsolate();
  v8::Local<v8::Set> set_js = v8::Set::New(isolate);
  v8::HandleScope handle_scope(isolate);

  for (const T& entry : set) {
    v8::Local<v8::Value> value;
    if (!ToV8Value(context, entry, isolate).ToLocal(&value))
      return {};
    if (set_js->Add(context, value).IsEmpty())
      return {};
  }

  return set_js;
}

template <typename T, typename U>
v8::MaybeLocal<v8::Value> ToV8Value(v8::Local<v8::Context> context,
                                    const std::unordered_map<T, U>& map,
                                    v8::Isolate* isolate) {
  if (isolate == nullptr) isolate = context->GetIsolate();
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::Map> ret = v8::Map::New(isolate);
  for (const auto& item : map) {
    v8::Local<v8::Value> first, second;
    if (!ToV8Value(context, item.first, isolate).ToLocal(&first) ||
        !ToV8Value(context, item.second, isolate).ToLocal(&second) ||
        ret->Set(context, first, second).IsEmpty()) {
      return v8::MaybeLocal<v8::Value>();
    }
  }

  return handle_scope.Escape(ret);
}

template <typename T, typename >
v8::MaybeLocal<v8::Value> ToV8Value(v8::Local<v8::Context> context,
                                    const T& number,
                                    v8::Isolate* isolate) {
  if (isolate == nullptr) isolate = context->GetIsolate();

  using Limits = std::numeric_limits<T>;
  // Choose Uint32, Int32, or Double depending on range checks.
  // These checks should all collapse at compile time.
  if (static_cast<uint32_t>(Limits::max()) <=
          std::numeric_limits<uint32_t>::max() &&
      static_cast<uint32_t>(Limits::min()) >=
          std::numeric_limits<uint32_t>::min() && Limits::is_exact) {
    return v8::Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(number));
  }

  if (static_cast<int32_t>(Limits::max()) <=
          std::numeric_limits<int32_t>::max() &&
      static_cast<int32_t>(Limits::min()) >=
          std::numeric_limits<int32_t>::min() && Limits::is_exact) {
    return v8::Integer::New(isolate, static_cast<int32_t>(number));
  }

  return v8::Number::New(isolate, static_cast<double>(number));
}

SlicedArguments::SlicedArguments(
    const v8::FunctionCallbackInfo<v8::Value>& args, size_t start) {
  const size_t length = static_cast<size_t>(args.Length());
  if (start >= length) return;
  const size_t size = length - start;

  AllocateSufficientStorage(size);
  for (size_t i = 0; i < size; ++i)
    (*this)[i] = args[i + start];
}

template <typename T, size_t S>
ArrayBufferViewContents<T, S>::ArrayBufferViewContents(
    v8::Local<v8::Value> value) {
  CHECK(value->IsArrayBufferView());
  Read(value.As<v8::ArrayBufferView>());
}

template <typename T, size_t S>
ArrayBufferViewContents<T, S>::ArrayBufferViewContents(
    v8::Local<v8::Object> value) {
  CHECK(value->IsArrayBufferView());
  Read(value.As<v8::ArrayBufferView>());
}

template <typename T, size_t S>
ArrayBufferViewContents<T, S>::ArrayBufferViewContents(
    v8::Local<v8::ArrayBufferView> abv) {
  Read(abv);
}

template <typename T, size_t S>
void ArrayBufferViewContents<T, S>::Read(v8::Local<v8::ArrayBufferView> abv) {
  static_assert(sizeof(T) == 1, "Only supports one-byte data at the moment");
  length_ = abv->ByteLength();
  if (length_ > sizeof(stack_storage_) || abv->HasBuffer()) {
    data_ = static_cast<T*>(abv->Buffer()->Data()) + abv->ByteOffset();
  } else {
    abv->CopyContents(stack_storage_, sizeof(stack_storage_));
    data_ = stack_storage_;
  }
}

// ECMA262 20.1.2.5
inline bool IsSafeJsInt(v8::Local<v8::Value> v) {
  if (!v->IsNumber()) return false;
  double v_d = v.As<v8::Number>()->Value();
  if (std::isnan(v_d)) return false;
  if (std::isinf(v_d)) return false;
  if (std::trunc(v_d) != v_d) return false;  // not int
  if (std::abs(v_d) <= static_cast<double>(kMaxSafeJsInteger)) return true;
  return false;
}

constexpr size_t FastStringKey::HashImpl(const char* str) {
  // Low-quality hash (djb2), but just fine for current use cases.
  size_t h = 5381;
  while (*str != '\0') {
    h = h * 33 + *(str++);  // NOLINT(readability/pointer_notation)
  }
  return h;
}

constexpr size_t FastStringKey::Hash::operator()(
    const FastStringKey& key) const {
  return key.cached_hash_;
}

constexpr bool FastStringKey::operator==(const FastStringKey& other) const {
  const char* p1 = name_;
  const char* p2 = other.name_;
  if (p1 == p2) return true;
  do {
    if (*(p1++) != *(p2++)) return false;
  } while (*p1 != '\0');
  return *p2 == '\0';
}

constexpr FastStringKey::FastStringKey(const char* name)
  : name_(name), cached_hash_(HashImpl(name)) {}

constexpr const char* FastStringKey::c_str() const {
  return name_;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_UTIL_INL_H_
