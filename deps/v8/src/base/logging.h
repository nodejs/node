// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_LOGGING_H_
#define V8_BASE_LOGGING_H_

#include <cstring>
#include <sstream>
#include <string>

#include "src/base/base-export.h"
#include "src/base/build_config.h"
#include "src/base/compiler-specific.h"

extern "C" PRINTF_FORMAT(3, 4) V8_NORETURN V8_BASE_EXPORT
    void V8_Fatal(const char* file, int line, const char* format, ...);

// The FATAL, UNREACHABLE and UNIMPLEMENTED macros are useful during
// development, but they should not be relied on in the final product.
#ifdef DEBUG
#define FATAL(msg)                              \
  V8_Fatal(__FILE__, __LINE__, "%s", (msg))
#define UNIMPLEMENTED()                         \
  V8_Fatal(__FILE__, __LINE__, "unimplemented code")
#define UNREACHABLE()                           \
  V8_Fatal(__FILE__, __LINE__, "unreachable code")
#else
#define FATAL(msg)                              \
  V8_Fatal("", 0, "%s", (msg))
#define UNIMPLEMENTED()                         \
  V8_Fatal("", 0, "unimplemented code")
#define UNREACHABLE() V8_Fatal("", 0, "unreachable code")
#endif


namespace v8 {
namespace base {

// Overwrite the default function that prints a stack trace.
V8_BASE_EXPORT void SetPrintStackTrace(void (*print_stack_trace_)());

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by DEBUG, so the check will be executed regardless of
// compilation mode.
//
// We make sure CHECK et al. always evaluates their arguments, as
// doing CHECK(FunctionWithSideEffect()) is a common idiom.
#define CHECK_WITH_MSG(condition, message)                        \
  do {                                                            \
    if (V8_UNLIKELY(!(condition))) {                              \
      V8_Fatal(__FILE__, __LINE__, "Check failed: %s.", message); \
    }                                                             \
  } while (0)
#define CHECK(condition) CHECK_WITH_MSG(condition, #condition)

#ifdef DEBUG

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.
#define CHECK_OP(name, op, lhs, rhs)                                     \
  do {                                                                   \
    if (std::string* _msg =                                              \
            ::v8::base::Check##name##Impl<decltype(lhs), decltype(rhs)>( \
                (lhs), (rhs), #lhs " " #op " " #rhs)) {                  \
      V8_Fatal(__FILE__, __LINE__, "Check failed: %s.", _msg->c_str());  \
      delete _msg;                                                       \
    }                                                                    \
  } while (0)

#else

// Make all CHECK functions discard their log strings to reduce code
// bloat for official release builds.

#define CHECK_OP(name, op, lhs, rhs)                                         \
  do {                                                                       \
    bool _cmp =                                                              \
        ::v8::base::Cmp##name##Impl<decltype(lhs), decltype(rhs)>(lhs, rhs); \
    CHECK_WITH_MSG(_cmp, #lhs " " #op " " #rhs);                             \
  } while (0)

#endif

// Helper to determine how to pass values: Pass scalars and arrays by value,
// others by const reference. std::decay<T> provides the type which should be
// used to pass T by value, e.g. converts array to pointer and removes const,
// volatile and reference.
template <typename T>
struct PassType : public std::conditional<
                      std::is_scalar<typename std::decay<T>::type>::value,
                      typename std::decay<T>::type, T const&> {};

// Build the error message string.  This is separate from the "Impl"
// function template because it is not performance critical and so can
// be out of line, while the "Impl" code should be inline. Caller
// takes ownership of the returned string.
template <typename Lhs, typename Rhs>
std::string* MakeCheckOpString(typename PassType<Lhs>::type lhs,
                               typename PassType<Rhs>::type rhs,
                               char const* msg) {
  std::ostringstream ss;
  ss << msg << " (" << lhs << " vs. " << rhs << ")";
  return new std::string(ss.str());
}

// Commonly used instantiations of MakeCheckOpString<>. Explicitly instantiated
// in logging.cc.
#define DEFINE_MAKE_CHECK_OP_STRING(type)                                    \
  extern template V8_BASE_EXPORT std::string* MakeCheckOpString<type, type>( \
      type, type, char const*);
DEFINE_MAKE_CHECK_OP_STRING(int)
DEFINE_MAKE_CHECK_OP_STRING(long)       // NOLINT(runtime/int)
DEFINE_MAKE_CHECK_OP_STRING(long long)  // NOLINT(runtime/int)
DEFINE_MAKE_CHECK_OP_STRING(unsigned int)
DEFINE_MAKE_CHECK_OP_STRING(unsigned long)       // NOLINT(runtime/int)
DEFINE_MAKE_CHECK_OP_STRING(unsigned long long)  // NOLINT(runtime/int)
DEFINE_MAKE_CHECK_OP_STRING(char const*)
DEFINE_MAKE_CHECK_OP_STRING(void const*)
#undef DEFINE_MAKE_CHECK_OP_STRING

// is_signed_vs_unsigned::value is true if both types are integral, Lhs is
// signed, and Rhs is unsigned. False in all other cases.
template <typename Lhs, typename Rhs>
struct is_signed_vs_unsigned {
  enum : bool {
    value = std::is_integral<typename std::decay<Lhs>::type>::value &&
            std::is_integral<typename std::decay<Rhs>::type>::value &&
            std::is_signed<typename std::decay<Lhs>::type>::value &&
            std::is_unsigned<typename std::decay<Rhs>::type>::value
  };
};
// Same thing, other way around: Lhs is unsigned, Rhs signed.
template <typename Lhs, typename Rhs>
struct is_unsigned_vs_signed : public is_signed_vs_unsigned<Rhs, Lhs> {};

// Specialize the compare functions for signed vs. unsigned comparisons.
// std::enable_if ensures that this template is only instantiable if both Lhs
// and Rhs are integral types, and their signedness does not match.
#define MAKE_UNSIGNED(Type, value)                                         \
  static_cast<                                                             \
      typename std::make_unsigned<typename std::decay<Type>::type>::type>( \
      value)
#define DEFINE_SIGNED_MISMATCH_COMP(CHECK, NAME, IMPL)                  \
  template <typename Lhs, typename Rhs>                                 \
  V8_INLINE typename std::enable_if<CHECK<Lhs, Rhs>::value, bool>::type \
      Cmp##NAME##Impl(Lhs const& lhs, Rhs const& rhs) {                 \
    return IMPL;                                                        \
  }
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, EQ,
                            lhs >= 0 && MAKE_UNSIGNED(Lhs, lhs) == rhs)
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, LT,
                            lhs < 0 || MAKE_UNSIGNED(Lhs, lhs) < rhs)
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, LE,
                            lhs <= 0 || MAKE_UNSIGNED(Lhs, lhs) <= rhs)
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, NE, !CmpEQImpl(lhs, rhs))
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, GT, !CmpLEImpl(lhs, rhs))
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, GE, !CmpLTImpl(lhs, rhs))
DEFINE_SIGNED_MISMATCH_COMP(is_unsigned_vs_signed, EQ, CmpEQImpl(rhs, lhs))
DEFINE_SIGNED_MISMATCH_COMP(is_unsigned_vs_signed, NE, CmpNEImpl(rhs, lhs))
DEFINE_SIGNED_MISMATCH_COMP(is_unsigned_vs_signed, LT, CmpGTImpl(rhs, lhs))
DEFINE_SIGNED_MISMATCH_COMP(is_unsigned_vs_signed, LE, CmpGEImpl(rhs, lhs))
DEFINE_SIGNED_MISMATCH_COMP(is_unsigned_vs_signed, GT, CmpLTImpl(rhs, lhs))
DEFINE_SIGNED_MISMATCH_COMP(is_unsigned_vs_signed, GE, CmpLEImpl(rhs, lhs))
#undef MAKE_UNSIGNED
#undef DEFINE_SIGNED_MISMATCH_COMP

// Helper functions for CHECK_OP macro.
// The (float, float) and (double, double) instantiations are explicitly
// externalized to ensure proper 32/64-bit comparisons on x86.
// The Cmp##NAME##Impl function is only instantiable if one of the two types is
// not integral or their signedness matches (i.e. whenever no specialization is
// required, see above). Otherwise it is disabled by the enable_if construct,
// and the compiler will pick a specialization from above.
#define DEFINE_CHECK_OP_IMPL(NAME, op)                                         \
  template <typename Lhs, typename Rhs>                                        \
  V8_INLINE                                                                    \
      typename std::enable_if<!is_signed_vs_unsigned<Lhs, Rhs>::value &&       \
                                  !is_unsigned_vs_signed<Lhs, Rhs>::value,     \
                              bool>::type                                      \
          Cmp##NAME##Impl(typename PassType<Lhs>::type lhs,                    \
                          typename PassType<Rhs>::type rhs) {                  \
    return lhs op rhs;                                                         \
  }                                                                            \
  template <typename Lhs, typename Rhs>                                        \
  V8_INLINE std::string* Check##NAME##Impl(typename PassType<Lhs>::type lhs,   \
                                           typename PassType<Rhs>::type rhs,   \
                                           char const* msg) {                  \
    bool cmp = Cmp##NAME##Impl<Lhs, Rhs>(lhs, rhs);                            \
    return V8_LIKELY(cmp) ? nullptr                                            \
                          : MakeCheckOpString<Lhs, Rhs>(lhs, rhs, msg);        \
  }                                                                            \
  extern template V8_BASE_EXPORT std::string* Check##NAME##Impl<float, float>( \
      float lhs, float rhs, char const* msg);                                  \
  extern template V8_BASE_EXPORT std::string*                                  \
      Check##NAME##Impl<double, double>(double lhs, double rhs,                \
                                        char const* msg);
DEFINE_CHECK_OP_IMPL(EQ, ==)
DEFINE_CHECK_OP_IMPL(NE, !=)
DEFINE_CHECK_OP_IMPL(LE, <=)
DEFINE_CHECK_OP_IMPL(LT, < )
DEFINE_CHECK_OP_IMPL(GE, >=)
DEFINE_CHECK_OP_IMPL(GT, > )
#undef DEFINE_CHECK_OP_IMPL

#define CHECK_EQ(lhs, rhs) CHECK_OP(EQ, ==, lhs, rhs)
#define CHECK_NE(lhs, rhs) CHECK_OP(NE, !=, lhs, rhs)
#define CHECK_LE(lhs, rhs) CHECK_OP(LE, <=, lhs, rhs)
#define CHECK_LT(lhs, rhs) CHECK_OP(LT, <, lhs, rhs)
#define CHECK_GE(lhs, rhs) CHECK_OP(GE, >=, lhs, rhs)
#define CHECK_GT(lhs, rhs) CHECK_OP(GT, >, lhs, rhs)
#define CHECK_NULL(val) CHECK((val) == nullptr)
#define CHECK_NOT_NULL(val) CHECK((val) != nullptr)
#define CHECK_IMPLIES(lhs, rhs) \
  CHECK_WITH_MSG(!(lhs) || (rhs), #lhs " implies " #rhs)

}  // namespace base
}  // namespace v8


// The DCHECK macro is equivalent to CHECK except that it only
// generates code in debug builds.
#ifdef DEBUG
#define DCHECK(condition)      CHECK(condition)
#define DCHECK_EQ(v1, v2)      CHECK_EQ(v1, v2)
#define DCHECK_NE(v1, v2)      CHECK_NE(v1, v2)
#define DCHECK_GT(v1, v2)      CHECK_GT(v1, v2)
#define DCHECK_GE(v1, v2)      CHECK_GE(v1, v2)
#define DCHECK_LT(v1, v2)      CHECK_LT(v1, v2)
#define DCHECK_LE(v1, v2)      CHECK_LE(v1, v2)
#define DCHECK_NULL(val)       CHECK_NULL(val)
#define DCHECK_NOT_NULL(val)   CHECK_NOT_NULL(val)
#define DCHECK_IMPLIES(v1, v2) CHECK_IMPLIES(v1, v2)
#else
#define DCHECK(condition)      ((void) 0)
#define DCHECK_EQ(v1, v2)      ((void) 0)
#define DCHECK_NE(v1, v2)      ((void) 0)
#define DCHECK_GT(v1, v2)      ((void) 0)
#define DCHECK_GE(v1, v2)      ((void) 0)
#define DCHECK_LT(v1, v2)      ((void) 0)
#define DCHECK_LE(v1, v2)      ((void) 0)
#define DCHECK_NULL(val)       ((void) 0)
#define DCHECK_NOT_NULL(val)   ((void) 0)
#define DCHECK_IMPLIES(v1, v2) ((void) 0)
#endif

#endif  // V8_BASE_LOGGING_H_
