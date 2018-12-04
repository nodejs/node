// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_MACROS_H_
#define V8_BASE_MACROS_H_

#include <limits>

#include "src/base/compiler-specific.h"
#include "src/base/format-macros.h"
#include "src/base/logging.h"

// No-op macro which is used to work around MSVC's funky VA_ARGS support.
#define EXPAND(x) x

// This macro does nothing. That's all.
#define NOTHING(...)

// TODO(all) Replace all uses of this macro with C++'s offsetof. To do that, we
// have to make sure that only standard-layout types and simple field
// designators are used.
#define OFFSET_OF(type, field) \
  (reinterpret_cast<intptr_t>(&(reinterpret_cast<type*>(16)->field)) - 16)


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

// bit_cast<Dest,Source> is a template function that implements the
// equivalent of "*reinterpret_cast<Dest*>(&source)".  We need this in
// very low-level functions like the protobuf library and fast math
// support.
//
//   float f = 3.14159265358979;
//   int i = bit_cast<int32>(f);
//   // i = 0x40490fdb
//
// The classical address-casting method is:
//
//   // WRONG
//   float f = 3.14159265358979;            // WRONG
//   int i = * reinterpret_cast<int*>(&f);  // WRONG
//
// The address-casting method actually produces undefined behavior
// according to ISO C++ specification section 3.10 -15 -.  Roughly, this
// section says: if an object in memory has one type, and a program
// accesses it with a different type, then the result is undefined
// behavior for most values of "different type".
//
// This is true for any cast syntax, either *(int*)&f or
// *reinterpret_cast<int*>(&f).  And it is particularly true for
// conversions between integral lvalues and floating-point lvalues.
//
// The purpose of 3.10 -15- is to allow optimizing compilers to assume
// that expressions with different types refer to different memory.  gcc
// 4.0.1 has an optimizer that takes advantage of this.  So a
// non-conforming program quietly produces wildly incorrect output.
//
// The problem is not the use of reinterpret_cast.  The problem is type
// punning: holding an object in memory of one type and reading its bits
// back using a different type.
//
// The C++ standard is more subtle and complex than this, but that
// is the basic idea.
//
// Anyways ...
//
// bit_cast<> calls memcpy() which is blessed by the standard,
// especially by the example in section 3.9 .  Also, of course,
// bit_cast<> wraps up the nasty logic in one place.
//
// Fortunately memcpy() is very fast.  In optimized mode, with a
// constant size, gcc 2.95.3, gcc 4.0.1, and msvc 7.1 produce inline
// code with the minimal amount of data movement.  On a 32-bit system,
// memcpy(d,s,4) compiles to one load and one store, and memcpy(d,s,8)
// compiles to two loads and two stores.
//
// I tested this code with gcc 2.95.3, gcc 4.0.1, icc 8.1, and msvc 7.1.
//
// WARNING: if Dest or Source is a non-POD type, the result of the memcpy
// is likely to surprise you.
template <class Dest, class Source>
V8_INLINE Dest bit_cast(Source const& source) {
  static_assert(sizeof(Dest) == sizeof(Source),
                "source and dest must be same size");
  Dest dest;
  memcpy(&dest, &source, sizeof(dest));
  return dest;
}

// Explicitly declare the assignment operator as deleted.
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete;

// Explicitly declare the copy constructor and assignment operator as deleted.
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  DISALLOW_ASSIGN(TypeName)

// Explicitly declare all copy/move constructors and assignments as deleted.
#define DISALLOW_COPY_AND_MOVE_AND_ASSIGN(TypeName) \
  TypeName(TypeName&&) = delete;                    \
  TypeName& operator=(TypeName&&) = delete;         \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

// Explicitly declare all implicit constructors as deleted, namely the
// default constructor, copy constructor and operator= functions.
// This is especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

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
  DISALLOW_COPY_AND_ASSIGN(TypeName)

// A macro to disallow the dynamic allocation.
// This should be used in the private: declarations for a class
// Declaring operator new and delete as deleted is not spec compliant.
// Extract from 3.2.2 of C++11 spec:
//  [...] A non-placement deallocation function for a class is
//  odr-used by the definition of the destructor of that class, [...]
#define DISALLOW_NEW_AND_DELETE()                            \
  void* operator new(size_t) { base::OS::Abort(); }          \
  void* operator new[](size_t) { base::OS::Abort(); };       \
  void operator delete(void*, size_t) { base::OS::Abort(); } \
  void operator delete[](void*, size_t) { base::OS::Abort(); }

// Define V8_USE_ADDRESS_SANITIZER macro.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define V8_USE_ADDRESS_SANITIZER 1
#endif
#endif

// Define DISABLE_ASAN macro.
#ifdef V8_USE_ADDRESS_SANITIZER
#define DISABLE_ASAN __attribute__((no_sanitize_address))
#else
#define DISABLE_ASAN
#endif

// Define V8_USE_MEMORY_SANITIZER macro.
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define V8_USE_MEMORY_SANITIZER 1
#endif
#endif

// Helper macro to define no_sanitize attributes only with clang.
#if defined(__clang__) && defined(__has_attribute)
#if __has_attribute(no_sanitize)
#define CLANG_NO_SANITIZE(what) __attribute__((no_sanitize(what)))
#endif
#endif
#if !defined(CLANG_NO_SANITIZE)
#define CLANG_NO_SANITIZE(what)
#endif

// DISABLE_CFI_PERF -- Disable Control Flow Integrity checks for Perf reasons.
#define DISABLE_CFI_PERF CLANG_NO_SANITIZE("cfi")

// DISABLE_CFI_ICALL -- Disable Control Flow Integrity indirect call checks,
// useful because calls into JITed code can not be CFI verified.
#define DISABLE_CFI_ICALL CLANG_NO_SANITIZE("cfi-icall")

#if V8_CC_GNU
#define V8_IMMEDIATE_CRASH() __builtin_trap()
#else
#define V8_IMMEDIATE_CRASH() ((void(*)())0)()
#endif

// A convenience wrapper around static_assert without a string message argument.
// Once C++17 becomes the default, this macro can be removed in favor of the
// new static_assert(condition) overload.
#define STATIC_ASSERT(test) static_assert(test, #test)

namespace v8 {
namespace base {

// Note that some implementations of std::is_trivially_copyable mandate that at
// least one of the copy constructor, move constructor, copy assignment or move
// assignment is non-deleted, while others do not. Be aware that also
// base::is_trivially_copyable will differ for these cases.
template <typename T>
struct is_trivially_copyable {
#if V8_CC_MSVC
  // Unfortunately, MSVC 2015 is broken in that std::is_trivially_copyable can
  // be false even though it should be true according to the standard.
  // (status at 2018-02-26, observed on the msvc waterfall bot).
  // Interestingly, the lower-level primitives used below are working as
  // intended, so we reimplement this according to the standard.
  // See also https://developercommunity.visualstudio.com/content/problem/
  //          170883/msvc-type-traits-stdis-trivial-is-bugged.html.
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

#elif defined(__GNUC__) && __GNUC__ < 5
  // WARNING:
  // On older libstdc++ versions, there is no way to correctly implement
  // is_trivially_copyable. The workaround below is an approximation (neither
  // over- nor underapproximation). E.g. it wrongly returns true if the move
  // constructor is non-trivial, and it wrongly returns false if the copy
  // constructor is deleted, but copy assignment is trivial.
  // TODO(rongjie) Remove this workaround once we require gcc >= 5.0
  static constexpr bool value =
      __has_trivial_copy(T) && __has_trivial_destructor(T);

#else
  static constexpr bool value = std::is_trivially_copyable<T>::value;
#endif
};
#if defined(__GNUC__) && __GNUC__ < 5
// On older libstdc++ versions, base::is_trivially_copyable<T>::value is only an
// approximation (see above), so make ASSERT_{NOT_,}TRIVIALLY_COPYABLE a noop.
#define ASSERT_TRIVIALLY_COPYABLE(T) static_assert(true, "check disabled")
#define ASSERT_NOT_TRIVIALLY_COPYABLE(T) static_assert(true, "check disabled")
#else
#define ASSERT_TRIVIALLY_COPYABLE(T)                         \
  static_assert(::v8::base::is_trivially_copyable<T>::value, \
                #T " should be trivially copyable")
#define ASSERT_NOT_TRIVIALLY_COPYABLE(T)                      \
  static_assert(!::v8::base::is_trivially_copyable<T>::value, \
                #T " should not be trivially copyable")
#endif

// The USE(x, ...) template is used to silence C++ compiler warnings
// issued for (yet) unused variables (typically parameters).
// The arguments are guaranteed to be evaluated from left to right.
struct Use {
  template <typename T>
  Use(T&&) {}  // NOLINT(runtime/explicit)
};
#define USE(...)                                                   \
  do {                                                             \
    ::v8::base::Use unused_tmp_array_for_use_macro[]{__VA_ARGS__}; \
    (void)unused_tmp_array_for_use_macro;                          \
  } while (false)

// Evaluate the instantiations of an expression with parameter packs.
// Since USE has left-to-right evaluation order of it's arguments,
// the parameter pack is iterated from left to right and side effects
// have defined behavior.
#define ITERATE_PACK(...) USE(0, ((__VA_ARGS__), 0)...)

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

#ifdef V8_TARGET_ARCH_64_BIT
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
#if V8_OS_MACOSX
#undef V8PRIxPTR
#define V8PRIxPTR "lx"
#undef V8PRIdPTR
#define V8PRIdPTR "ld"
#undef V8PRIuPTR
#define V8PRIuPTR "lxu"
#endif

// The following macro works on both 32 and 64-bit platforms.
// Usage: instead of writing 0x1234567890123456
//      write V8_2PART_UINT64_C(0x12345678,90123456);
#define V8_2PART_UINT64_C(a, b) (((static_cast<uint64_t>(a) << 32) + 0x##b##u))

// Return the largest multiple of m which is <= x.
template <typename T>
inline T RoundDown(T x, intptr_t m) {
  STATIC_ASSERT(std::is_integral<T>::value);
  // m must be a power of two.
  DCHECK(m != 0 && ((m & (m - 1)) == 0));
  return x & -m;
}
template <intptr_t m, typename T>
constexpr inline T RoundDown(T x) {
  STATIC_ASSERT(std::is_integral<T>::value);
  // m must be a power of two.
  STATIC_ASSERT(m != 0 && ((m & (m - 1)) == 0));
  return x & -m;
}

// Return the smallest multiple of m which is >= x.
template <typename T>
inline T RoundUp(T x, intptr_t m) {
  STATIC_ASSERT(std::is_integral<T>::value);
  return RoundDown<T>(static_cast<T>(x + m - 1), m);
}
template <intptr_t m, typename T>
constexpr inline T RoundUp(T x) {
  STATIC_ASSERT(std::is_integral<T>::value);
  return RoundDown<m, T>(static_cast<T>(x + (m - 1)));
}

template <typename T, typename U>
inline bool IsAligned(T value, U alignment) {
  return (value & (alignment - 1)) == 0;
}

inline void* AlignedAddress(void* address, size_t alignment) {
  // The alignment must be a power of two.
  DCHECK_EQ(alignment & (alignment - 1), 0u);
  return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address) &
                                 ~static_cast<uintptr_t>(alignment - 1));
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
  return (kLowerBoundIsMin ? (kLowerBound <= v) : (kLowerBound < v)) &&
         (kUpperBoundIsMax ? (v <= kUpperBound) : (v < kUpperBound));
}

#ifdef V8_OS_WIN

// Setup for Windows shared library export.
#ifdef BUILDING_V8_SHARED
#define V8_EXPORT_PRIVATE __declspec(dllexport)
#elif USING_V8_SHARED
#define V8_EXPORT_PRIVATE __declspec(dllimport)
#else
#define V8_EXPORT_PRIVATE
#endif  // BUILDING_V8_SHARED

#else  // V8_OS_WIN

// Setup for Linux shared library export.
#if V8_HAS_ATTRIBUTE_VISIBILITY
#ifdef BUILDING_V8_SHARED
#define V8_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define V8_EXPORT_PRIVATE
#endif
#else
#define V8_EXPORT_PRIVATE
#endif

#endif  // V8_OS_WIN

#endif  // V8_BASE_MACROS_H_
