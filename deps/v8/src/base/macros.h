// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_MACROS_H_
#define V8_BASE_MACROS_H_

#include <limits>
#include <type_traits>

#include "include/v8config.h"
#include "src/base/compiler-specific.h"
#include "src/base/logging.h"

// No-op macro which is used to work around MSVC's funky VA_ARGS support.
#define EXPAND(X) X

// This macro does nothing. That's all.
#define NOTHING(...)

#define CONCAT_(a, ...) a##__VA_ARGS__
#define CONCAT(a, ...) CONCAT_(a, __VA_ARGS__)
// Creates an unique identifier. Useful for scopes to avoid shadowing names.
#define UNIQUE_IDENTIFIER(base) CONCAT(base, __COUNTER__)

// COUNT_MACRO_ARGS(...) returns the number of arguments passed. Currently, up
// to 8 arguments are supported.
#define COUNT_MACRO_ARGS(...) \
  EXPAND(COUNT_MACRO_ARGS_IMPL(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0))
#define COUNT_MACRO_ARGS_IMPL(_8, _7, _6, _5, _4, _3, _2, _1, N, ...) N
// GET_NTH_ARG(N, ...) returns the Nth argument in the list of arguments
// following. Currently, up to N=8 is supported.
#define GET_NTH_ARG(N, ...) CONCAT(GET_NTH_ARG_IMPL_, N)(__VA_ARGS__)
#define GET_NTH_ARG_IMPL_0(_0, ...) _0
#define GET_NTH_ARG_IMPL_1(_0, _1, ...) _1
#define GET_NTH_ARG_IMPL_2(_0, _1, _2, ...) _2
#define GET_NTH_ARG_IMPL_3(_0, _1, _2, _3, ...) _3
#define GET_NTH_ARG_IMPL_4(_0, _1, _2, _3, _4, ...) _4
#define GET_NTH_ARG_IMPL_5(_0, _1, _2, _3, _4, _5, ...) _5
#define GET_NTH_ARG_IMPL_6(_0, _1, _2, _3, _4, _5, _6, ...) _6
#define GET_NTH_ARG_IMPL_7(_0, _1, _2, _3, _4, _5, _6, _7, ...) _7

// UNPAREN(x) removes a layer of nested parentheses on x, if any. This means
// that both UNPAREN(x) and UNPAREN((x)) expand to x. This is helpful for macros
// that want to support multi argument templates with commas, e.g.
//
//   #define FOO(Type, Name) UNPAREN(Type) Name;
//
// will work with both
//
//   FOO(int, x);
//   FOO((Foo<int, double, float>), x);
#define UNPAREN(X) CONCAT(DROP_, UNPAREN_ X)
#define UNPAREN_(...) UNPAREN_ __VA_ARGS__
#define DROP_UNPAREN_

#define OFFSET_OF(type, field) offsetof(type, field)

// A comma, to be used in macro arguments where it would otherwise be
// interpreted as separator of arguments.
#define LITERAL_COMMA ,

// The arraysize(arr) macro returns the # of elements in an array arr.
// The expression is a compile-time constant, and therefore can be
// used in defining new arrays, for example.  If you use arraysize on
// a pointer by mistake, you will get a compile-time error.
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

#if !V8_CC_MSVC
// That gcc wants both of these prototypes seems mysterious. VC, for
// its part, can't decide which to use (another mystery). Matching of
// template overloads: the final frontier.
template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
#endif

// This is an equivalent to C++20's std::bit_cast<>(), but with additional
// warnings. It morally does what `*reinterpret_cast<Dest*>(&source)` does, but
// the cast/deref pair is undefined behavior, while bit_cast<>() isn't.
//
// This is not a magic "get out of UB free" card. This must only be used on
// values, not on references or pointers. For pointers, use
// reinterpret_cast<>(), or static_cast<>() when casting between void* and other
// pointers, and then look at https://eel.is/c++draft/basic.lval#11 as that's
// probably UB also.
namespace v8::base {

template <class Dest, class Source>
V8_INLINE Dest bit_cast(Source const& source) {
  static_assert(!std::is_pointer_v<Source>,
                "bit_cast must not be used on pointer types");
  static_assert(!std::is_pointer_v<Dest>,
                "bit_cast must not be used on pointer types");
  static_assert(!std::is_reference_v<Dest>,
                "bit_cast must not be used on reference types");
  static_assert(
      sizeof(Dest) == sizeof(Source),
      "bit_cast requires source and destination types to be the same size");
  static_assert(std::is_trivially_copyable_v<Source>,
                "bit_cast requires the source type to be trivially copyable");
  static_assert(
      std::is_trivially_copyable_v<Dest>,
      "bit_cast requires the destination type to be trivially copyable");

#if V8_HAS_BUILTIN_BIT_CAST
  return __builtin_bit_cast(Dest, source);
#else
  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
#endif
}

}  // namespace v8::base

// Explicitly declare the assignment operator as deleted.
// Note: This macro is deprecated and will be removed soon. Please explicitly
// delete the assignment operator instead.
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete

// Explicitly declare all implicit constructors as deleted, namely the
// default constructor, copy constructor and operator= functions.
// This is especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  TypeName(const TypeName&) = delete;            \
  DISALLOW_ASSIGN(TypeName)

// Disallow copying a type, but provide default construction, move construction
// and move assignment. Especially useful for move-only structs.
#define MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(TypeName) \
  TypeName() = default;                               \
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(TypeName)

// Disallow copying a type, and only provide move construction and move
// assignment. Especially useful for move-only structs.
#define MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(TypeName)       \
  TypeName(TypeName&&) V8_NOEXCEPT = default;            \
  TypeName& operator=(TypeName&&) V8_NOEXCEPT = default; \
  TypeName(const TypeName&) = delete;                    \
  DISALLOW_ASSIGN(TypeName)

// A macro to disallow the dynamic allocation.
// This should be used in the private: declarations for a class
// Declaring operator new and delete as deleted is not spec compliant.
// Extract from 3.2.2 of C++11 spec:
//  [...] A non-placement deallocation function for a class is
//  odr-used by the definition of the destructor of that class, [...]
#define DISALLOW_NEW_AND_DELETE()                                \
  void* operator new(size_t) { v8::base::OS::Abort(); }          \
  void* operator new[](size_t) { v8::base::OS::Abort(); }        \
  void operator delete(void*, size_t) { v8::base::OS::Abort(); } \
  void operator delete[](void*, size_t) { v8::base::OS::Abort(); }

// Define V8_USE_ADDRESS_SANITIZER macro.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define V8_USE_ADDRESS_SANITIZER 1
#endif
#endif

// Define V8_USE_HWADDRESS_SANITIZER macro.
#if defined(__has_feature)
#if __has_feature(hwaddress_sanitizer)
#define V8_USE_HWADDRESS_SANITIZER 1
#endif
#endif

// Define V8_USE_MEMORY_SANITIZER macro.
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define V8_USE_MEMORY_SANITIZER 1
#endif
#endif

// Define V8_USE_UNDEFINED_BEHAVIOR_SANITIZER macro.
#if defined(__has_feature)
#if __has_feature(undefined_behavior_sanitizer)
#define V8_USE_UNDEFINED_BEHAVIOR_SANITIZER 1
#endif
#endif

// Define V8_USE_SAFE_STACK macro.
#if defined(__has_feature)
#if __has_feature(safe_stack)
#define V8_USE_SAFE_STACK 1
#endif  // __has_feature(safe_stack)
#endif  // defined(__has_feature)

// DISABLE_CFI_PERF -- Disable Control Flow Integrity checks for Perf reasons.
#define DISABLE_CFI_PERF V8_CLANG_NO_SANITIZE("cfi")

// DISABLE_CFI_ICALL -- Disable Control Flow Integrity indirect call checks,
// useful because calls into JITed code can not be CFI verified. Same for
// UBSan's function pointer type checks.
#ifdef V8_OS_WIN
// On Windows, also needs __declspec(guard(nocf)) for CFG.
#define DISABLE_CFI_ICALL           \
  V8_CLANG_NO_SANITIZE("cfi-icall") \
  V8_CLANG_NO_SANITIZE("function")  \
  __declspec(guard(nocf))
#else
#define DISABLE_CFI_ICALL           \
  V8_CLANG_NO_SANITIZE("cfi-icall") \
  V8_CLANG_NO_SANITIZE("function")
#endif

// V8_PRETTY_FUNCTION_VALUE_OR(ELSE) emits a pretty function value, if
// available for this compiler, otherwise it emits ELSE.
#if defined(V8_CC_GNU)
#define V8_PRETTY_FUNCTION_VALUE_OR(ELSE) __PRETTY_FUNCTION__
#elif defined(V8_CC_MSVC)
#define V8_PRETTY_FUNCTION_VALUE_OR(ELSE) __FUNCSIG__
#else
#define V8_PRETTY_FUNCTION_VALUE_OR(ELSE) ELSE
#endif

namespace v8 {
namespace base {

// Note that some implementations of std::is_trivially_copyable mandate that at
// least one of the copy constructor, move constructor, copy assignment or move
// assignment is non-deleted, while others do not. Be aware that also
// base::is_trivially_copyable will differ for these cases.
template <typename T>
struct is_trivially_copyable {
#if V8_CC_MSVC || (__GNUC__ == 12 && __GNUC_MINOR__ <= 2)
  // Unfortunately, MSVC 2015 is broken in that std::is_trivially_copyable can
  // be false even though it should be true according to the standard.
  // (status at 2018-02-26, observed on the msvc waterfall bot).
  // Interestingly, the lower-level primitives used below are working as
  // intended, so we reimplement this according to the standard.
  // See also https://developercommunity.visualstudio.com/content/problem/
  //          170883/msvc-type-traits-stdis-trivial-is-bugged.html.
  //
  // GCC 12.1 and 12.2 are broken too, they are shipped by some stable Linux
  // distributions, so the same polyfill is also used.
  // See
  // https://gcc.gnu.org/git/?p=gcc.git;a=commitdiff;h=aeba3e009b0abfccaf01797556445dbf891cc8dc
  static constexpr bool value =
      // Copy constructor is trivial or deleted.
      (std::is_trivially_copy_constructible<T>::value ||
       !std::is_copy_constructible<T>::value) &&
      // Copy assignment operator is trivial or deleted.
      (std::is_trivially_copy_assignable<T>::value ||
       !std::is_copy_assignable<T>::value) &&
      // Move constructor is trivial or deleted.
      (std::is_trivially_move_constructible<T>::value ||
       !std::is_move_constructible<T>::value) &&
      // Move assignment operator is trivial or deleted.
      (std::is_trivially_move_assignable<T>::value ||
       !std::is_move_assignable<T>::value) &&
      // (Some implementations mandate that one of the above is non-deleted, but
      // the standard does not, so let's skip this check.)
      // Trivial non-deleted destructor.
      std::is_trivially_destructible<T>::value;
#else
  static constexpr bool value = std::is_trivially_copyable<T>::value;
#endif
};
#define ASSERT_TRIVIALLY_COPYABLE(T)                         \
  static_assert(::v8::base::is_trivially_copyable<T>::value, \
                #T " should be trivially copyable")
#define ASSERT_NOT_TRIVIALLY_COPYABLE(T)                      \
  static_assert(!::v8::base::is_trivially_copyable<T>::value, \
                #T " should not be trivially copyable")

// The USE(x, ...) template is used to silence C++ compiler warnings
// issued for (yet) unused variables (typically parameters).
// The arguments are guaranteed to be evaluated from left to right.
struct Use {
  template <typename T>
  constexpr Use(T&&) {}  // NOLINT(runtime/explicit)
};
#define USE(...)                                                   \
  do {                                                             \
    ::v8::base::Use unused_tmp_array_for_use_macro[]{__VA_ARGS__}; \
    (void)unused_tmp_array_for_use_macro;                          \
  } while (false)

}  // namespace base
}  // namespace v8

// implicit_cast<A>(x) triggers an implicit cast from {x} to type {A}. This is
// useful in situations where static_cast<A>(x) would do too much.
// Only use this for cheap-to-copy types, or use move semantics explicitly.
template <class A>
V8_INLINE A implicit_cast(A x) {
  return x;
}

// Define our own macros for writing 64-bit constants.  This is less fragile
// than defining __STDC_CONSTANT_MACROS before including <stdint.h>, and it
// works on compilers that don't have it (like MSVC).
#if V8_CC_MSVC
# if V8_HOST_ARCH_64_BIT
#  define V8_PTR_PREFIX   "ll"
# else
#  define V8_PTR_PREFIX   ""
# endif  // V8_HOST_ARCH_64_BIT
#elif V8_CC_MINGW64
# define V8_PTR_PREFIX    "I64"
#elif V8_HOST_ARCH_64_BIT
# define V8_PTR_PREFIX    "l"
#else
#if V8_OS_AIX
#define V8_PTR_PREFIX "l"
#else
# define V8_PTR_PREFIX    ""
#endif
#endif

#define V8PRIxPTR V8_PTR_PREFIX "x"
#define V8PRIdPTR V8_PTR_PREFIX "d"
#define V8PRIuPTR V8_PTR_PREFIX "u"

#if V8_TARGET_ARCH_64_BIT
#define V8_PTR_HEX_DIGITS 12
#define V8PRIxPTR_FMT "0x%012" V8PRIxPTR
#else
#define V8_PTR_HEX_DIGITS 8
#define V8PRIxPTR_FMT "0x%08" V8PRIxPTR
#endif

// ptrdiff_t is 't' according to the standard, but MSVC uses 'I'.
#if V8_CC_MSVC
#define V8PRIxPTRDIFF "Ix"
#define V8PRIdPTRDIFF "Id"
#define V8PRIuPTRDIFF "Iu"
#else
#define V8PRIxPTRDIFF "tx"
#define V8PRIdPTRDIFF "td"
#define V8PRIuPTRDIFF "tu"
#endif

// Fix for Mac OS X defining uintptr_t as "unsigned long":
#if V8_OS_DARWIN
#undef V8PRIxPTR
#define V8PRIxPTR "lx"
#undef V8PRIdPTR
#define V8PRIdPTR "ld"
#undef V8PRIuPTR
#define V8PRIuPTR "lxu"
#endif

// Make a uint64 from two uint32_t halves.
inline uint64_t make_uint64(uint32_t high, uint32_t low) {
  return (uint64_t{high} << 32) + low;
}

// Return the largest multiple of m which is <= x.
template <typename T>
constexpr T RoundDown(T x, intptr_t m) {
  static_assert(std::is_integral<T>::value);
  // m must be a power of two.
  DCHECK(m != 0 && ((m & (m - 1)) == 0));
  return x & static_cast<T>(-m);
}
template <intptr_t m, typename T>
constexpr T RoundDown(T x) {
  static_assert(std::is_integral<T>::value);
  // m must be a power of two.
  static_assert(m != 0 && ((m & (m - 1)) == 0));
  return x & static_cast<T>(-m);
}

// Return the smallest multiple of m which is >= x.
template <typename T>
constexpr T RoundUp(T x, intptr_t m) {
  static_assert(std::is_integral<T>::value);
  DCHECK_GE(x, 0);
  DCHECK_GE(std::numeric_limits<T>::max() - x, m - 1);  // Overflow check.
  return RoundDown<T>(static_cast<T>(x + (m - 1)), m);
}

template <intptr_t m, typename T>
constexpr T RoundUp(T x) {
  static_assert(std::is_integral<T>::value);
  DCHECK_GE(x, 0);
  DCHECK_GE(std::numeric_limits<T>::max() - x, m - 1);  // Overflow check.
  return RoundDown<m, T>(static_cast<T>(x + (m - 1)));
}

template <typename T, typename U>
constexpr inline bool IsAligned(T value, U alignment) {
  return (value & (alignment - 1)) == 0;
}

inline void* AlignedAddress(void* address, size_t alignment) {
  return reinterpret_cast<void*>(
      RoundDown(reinterpret_cast<uintptr_t>(address), alignment));
}

inline void* RoundUpAddress(void* address, size_t alignment) {
  return reinterpret_cast<void*>(
      RoundUp(reinterpret_cast<uintptr_t>(address), alignment));
}

// Bounds checks for float to integer conversions, which does truncation. Hence,
// the range of legal values is (min - 1, max + 1).
template <typename int_t, typename float_t, typename biggest_int_t = int64_t>
bool is_inbounds(float_t v) {
  static_assert(sizeof(int_t) < sizeof(biggest_int_t),
                "int_t can't be bounds checked by the compiler");
  constexpr float_t kLowerBound =
      static_cast<float_t>(std::numeric_limits<int_t>::min()) - 1;
  constexpr float_t kUpperBound =
      static_cast<float_t>(std::numeric_limits<int_t>::max()) + 1;
  constexpr bool kLowerBoundIsMin =
      static_cast<biggest_int_t>(kLowerBound) ==
      static_cast<biggest_int_t>(std::numeric_limits<int_t>::min());
  constexpr bool kUpperBoundIsMax =
      static_cast<biggest_int_t>(kUpperBound) ==
      static_cast<biggest_int_t>(std::numeric_limits<int_t>::max());
  // Using USE(var) is only a workaround for a GCC 8.1 bug.
  USE(kLowerBoundIsMin);
  USE(kUpperBoundIsMax);
  return (kLowerBoundIsMin ? (kLowerBound <= v) : (kLowerBound < v)) &&
         (kUpperBoundIsMax ? (v <= kUpperBound) : (v < kUpperBound));
}

#ifdef V8_OS_WIN

// Setup for Windows shared library export.
#define V8_EXPORT_ENUM
#ifdef BUILDING_V8_SHARED_PRIVATE
#define V8_EXPORT_PRIVATE __declspec(dllexport)
#elif USING_V8_SHARED_PRIVATE
#define V8_EXPORT_PRIVATE __declspec(dllimport)
#else
#define V8_EXPORT_PRIVATE
#endif  // BUILDING_V8_SHARED

#else  // V8_OS_WIN

// Setup for Linux shared library export.
#if V8_HAS_ATTRIBUTE_VISIBILITY
#ifdef BUILDING_V8_SHARED_PRIVATE
#define V8_EXPORT_PRIVATE __attribute__((visibility("default")))
#define V8_EXPORT_ENUM V8_EXPORT_PRIVATE
#else
#define V8_EXPORT_PRIVATE
#define V8_EXPORT_ENUM
#endif
#else
#define V8_EXPORT_PRIVATE
#define V8_EXPORT_ENUM
#endif

#endif  // V8_OS_WIN

// Defines IF_WASM, to be used in macro lists for elements that should only be
// there if WebAssembly is enabled.
#if V8_ENABLE_WEBASSEMBLY
// EXPAND is needed to work around MSVC's broken __VA_ARGS__ expansion.
#define IF_WASM(V, ...) EXPAND(V(__VA_ARGS__))
#else
#define IF_WASM(V, ...)
#endif  // V8_ENABLE_WEBASSEMBLY

#ifdef V8_ENABLE_DRUMBRAKE
#define IF_WASM_DRUMBRAKE(V, ...) EXPAND(V(__VA_ARGS__))
#else
#define IF_WASM_DRUMBRAKE(V, ...)
#endif  // V8_ENABLE_DRUMBRAKE

#if defined(V8_ENABLE_DRUMBRAKE) && !defined(V8_DRUMBRAKE_BOUNDS_CHECKS)
#define IF_WASM_DRUMBRAKE_INSTR_HANDLER(V, ...) EXPAND(V(__VA_ARGS__))
#else
#define IF_WASM_DRUMBRAKE_INSTR_HANDLER(V, ...)
#endif  // V8_ENABLE_DRUMBRAKE && !V8_DRUMBRAKE_BOUNDS_CHECKS

// Defines IF_TSAN, to be used in macro lists for elements that should only be
// there if TSAN is enabled.
#ifdef V8_IS_TSAN
// EXPAND is needed to work around MSVC's broken __VA_ARGS__ expansion.
#define IF_TSAN(V, ...) EXPAND(V(__VA_ARGS__))
#else
#define IF_TSAN(V, ...)
#endif  // V8_IS_TSAN

// Defines IF_INTL, to be used in macro lists for elements that should only be
// there if INTL is enabled.
#ifdef V8_INTL_SUPPORT
// EXPAND is needed to work around MSVC's broken __VA_ARGS__ expansion.
#define IF_INTL(V, ...) EXPAND(V(__VA_ARGS__))
#else
#define IF_INTL(V, ...)
#endif  // V8_INTL_SUPPORT

// Defines IF_SHADOW_STACK, to be used in macro lists for elements that should
// only be there if CET shadow stack is enabled.
#ifdef V8_ENABLE_CET_SHADOW_STACK
// EXPAND is needed to work around MSVC's broken __VA_ARGS__ expansion.
#define IF_SHADOW_STACK(V, ...) EXPAND(V(__VA_ARGS__))
#else
#define IF_SHADOW_STACK(V, ...)
#endif  // V8_ENABLE_CET_SHADOW_STACK

// Defines IF_TARGET_ARCH_64_BIT, to be used in macro lists for elements that
// should only be there if the target architecture is a 64-bit one.
#if V8_TARGET_ARCH_64_BIT
// EXPAND is needed to work around MSVC's broken __VA_ARGS__ expansion.
#define IF_TARGET_ARCH_64_BIT(V, ...) EXPAND(V(__VA_ARGS__))
#else
#define IF_TARGET_ARCH_64_BIT(V, ...)
#endif  // V8_TARGET_ARCH_64_BIT

// Defines IF_V8_WASM_RANDOM_FUZZERS and IF_NO_V8_WASM_RANDOM_FUZZERS, to be
// used in macro lists for elements that should only be there/absent when
// building the Wasm fuzzers.
#ifdef V8_WASM_RANDOM_FUZZERS
// EXPAND is needed to work around MSVC's broken __VA_ARGS__ expansion.
#define IF_V8_WASM_RANDOM_FUZZERS(V, ...) EXPAND(V(__VA_ARGS__))
#define IF_NO_V8_WASM_RANDOM_FUZZERS(V, ...)
#else
#define IF_V8_WASM_RANDOM_FUZZERS(V, ...)
#define IF_NO_V8_WASM_RANDOM_FUZZERS(V, ...) EXPAND(V(__VA_ARGS__))
#endif  // V8_WASM_RANDOM_FUZZERS

#ifdef GOOGLE3
// Disable FRIEND_TEST macro in Google3.
#define FRIEND_TEST(test_case_name, test_name)
#endif

#endif  // V8_BASE_MACROS_H_
