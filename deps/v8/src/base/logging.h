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
#include "src/base/template-utils.h"

[[noreturn]] PRINTF_FORMAT(3, 4) V8_BASE_EXPORT V8_NOINLINE
    void V8_Fatal(const char* file, int line, const char* format, ...);

V8_BASE_EXPORT V8_NOINLINE void V8_Dcheck(const char* file, int line,
                                          const char* message);

#ifdef DEBUG
#define FATAL(...) V8_Fatal(__FILE__, __LINE__, __VA_ARGS__)
#else
#define FATAL(...) V8_Fatal("", 0, __VA_ARGS__)
#endif
#define UNIMPLEMENTED() FATAL("unimplemented code")
#define UNREACHABLE() FATAL("unreachable code")

namespace v8 {
namespace base {

// Overwrite the default function that prints a stack trace.
V8_BASE_EXPORT void SetPrintStackTrace(void (*print_stack_trace_)());

// Override the default function that handles DCHECKs.
V8_BASE_EXPORT void SetDcheckFunction(void (*dcheck_Function)(const char*, int,
                                                              const char*));

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

#define DCHECK_WITH_MSG(condition, message)   \
  do {                                        \
    if (V8_UNLIKELY(!(condition))) {          \
      V8_Dcheck(__FILE__, __LINE__, message); \
    }                                         \
  } while (0)
#define DCHECK(condition) DCHECK_WITH_MSG(condition, #condition)

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.
#define CHECK_OP(name, op, lhs, rhs)                                      \
  do {                                                                    \
    if (std::string* _msg = ::v8::base::Check##name##Impl<                \
            typename ::v8::base::pass_value_or_ref<decltype(lhs)>::type,  \
            typename ::v8::base::pass_value_or_ref<decltype(rhs)>::type>( \
            (lhs), (rhs), #lhs " " #op " " #rhs)) {                       \
      V8_Fatal(__FILE__, __LINE__, "Check failed: %s.", _msg->c_str());   \
      delete _msg;                                                        \
    }                                                                     \
  } while (0)

#define DCHECK_OP(name, op, lhs, rhs)                                     \
  do {                                                                    \
    if (std::string* _msg = ::v8::base::Check##name##Impl<                \
            typename ::v8::base::pass_value_or_ref<decltype(lhs)>::type,  \
            typename ::v8::base::pass_value_or_ref<decltype(rhs)>::type>( \
            (lhs), (rhs), #lhs " " #op " " #rhs)) {                       \
      V8_Dcheck(__FILE__, __LINE__, _msg->c_str());                       \
      delete _msg;                                                        \
    }                                                                     \
  } while (0)

#else

// Make all CHECK functions discard their log strings to reduce code
// bloat for official release builds.

#define CHECK_OP(name, op, lhs, rhs)                                         \
  do {                                                                       \
    bool _cmp = ::v8::base::Cmp##name##Impl<                                 \
        typename ::v8::base::pass_value_or_ref<decltype(lhs)>::type,         \
        typename ::v8::base::pass_value_or_ref<decltype(rhs)>::type>((lhs),  \
                                                                     (rhs)); \
    CHECK_WITH_MSG(_cmp, #lhs " " #op " " #rhs);                             \
  } while (0)

#define DCHECK_WITH_MSG(condition, msg) void(0);

#endif

// Define PrintCheckOperand<T> for each T which defines operator<< for ostream.
template <typename T>
typename std::enable_if<has_output_operator<T>::value>::type PrintCheckOperand(
    std::ostream& os, T val) {
  os << std::forward<T>(val);
}

// Define PrintCheckOperand<T> for enums which have no operator<<.
template <typename T>
typename std::enable_if<std::is_enum<T>::value &&
                        !has_output_operator<T>::value>::type
PrintCheckOperand(std::ostream& os, T val) {
  using underlying_t = typename std::underlying_type<T>::type;
  // 8-bit types are not printed as number, so extend them to 16 bit.
  using int_t = typename std::conditional<
      std::is_same<underlying_t, uint8_t>::value, uint16_t,
      typename std::conditional<std::is_same<underlying_t, int8_t>::value,
                                int16_t, underlying_t>::type>::type;
  PrintCheckOperand(os, static_cast<int_t>(static_cast<underlying_t>(val)));
}

// Define default PrintCheckOperand<T> for non-printable types.
template <typename T>
typename std::enable_if<!has_output_operator<T>::value &&
                        !std::is_enum<T>::value>::type
PrintCheckOperand(std::ostream& os, T val) {
  os << "<unprintable>";
}

// Define specializations for character types, defined in logging.cc.
#define DEFINE_PRINT_CHECK_OPERAND_CHAR(type)                              \
  template <>                                                              \
  V8_BASE_EXPORT void PrintCheckOperand<type>(std::ostream & os, type ch); \
  template <>                                                              \
  V8_BASE_EXPORT void PrintCheckOperand<type*>(std::ostream & os,          \
                                               type * cstr);               \
  template <>                                                              \
  V8_BASE_EXPORT void PrintCheckOperand<const type*>(std::ostream & os,    \
                                                     const type* cstr);

DEFINE_PRINT_CHECK_OPERAND_CHAR(char)
DEFINE_PRINT_CHECK_OPERAND_CHAR(signed char)
DEFINE_PRINT_CHECK_OPERAND_CHAR(unsigned char)
#undef DEFINE_PRINT_CHECK_OPERAND_CHAR

// Build the error message string.  This is separate from the "Impl"
// function template because it is not performance critical and so can
// be out of line, while the "Impl" code should be inline. Caller
// takes ownership of the returned string.
template <typename Lhs, typename Rhs>
std::string* MakeCheckOpString(Lhs lhs, Rhs rhs, char const* msg) {
  std::ostringstream ss;
  ss << msg << " (";
  PrintCheckOperand<Lhs>(ss, lhs);
  ss << " vs. ";
  PrintCheckOperand<Rhs>(ss, rhs);
  ss << ")";
  return new std::string(ss.str());
}

// Commonly used instantiations of MakeCheckOpString<>. Explicitly instantiated
// in logging.cc.
#define EXPLICIT_CHECK_OP_INSTANTIATION(type)                                \
  extern template V8_BASE_EXPORT std::string* MakeCheckOpString<type, type>( \
      type, type, char const*);                                              \
  extern template V8_BASE_EXPORT void PrintCheckOperand<type>(std::ostream&, \
                                                              type);

EXPLICIT_CHECK_OP_INSTANTIATION(int)
EXPLICIT_CHECK_OP_INSTANTIATION(long)       // NOLINT(runtime/int)
EXPLICIT_CHECK_OP_INSTANTIATION(long long)  // NOLINT(runtime/int)
EXPLICIT_CHECK_OP_INSTANTIATION(unsigned int)
EXPLICIT_CHECK_OP_INSTANTIATION(unsigned long)       // NOLINT(runtime/int)
EXPLICIT_CHECK_OP_INSTANTIATION(unsigned long long)  // NOLINT(runtime/int)
EXPLICIT_CHECK_OP_INSTANTIATION(void const*)
#undef EXPLICIT_CHECK_OP_INSTANTIATION

// comparison_underlying_type provides the underlying integral type of an enum,
// or std::decay<T>::type if T is not an enum. Booleans are converted to
// "unsigned int", to allow "unsigned int == bool" comparisons.
template <typename T>
struct comparison_underlying_type {
  // std::underlying_type must only be used with enum types, thus use this
  // {Dummy} type if the given type is not an enum.
  enum Dummy {};
  using decay = typename std::decay<T>::type;
  static constexpr bool is_enum = std::is_enum<decay>::value;
  using underlying = typename std::underlying_type<
      typename std::conditional<is_enum, decay, Dummy>::type>::type;
  using type_or_bool =
      typename std::conditional<is_enum, underlying, decay>::type;
  using type =
      typename std::conditional<std::is_same<type_or_bool, bool>::value,
                                unsigned int, type_or_bool>::type;
};
// Cast a value to its underlying type
#define MAKE_UNDERLYING(Type, value) \
  static_cast<typename comparison_underlying_type<Type>::type>(value)

// is_signed_vs_unsigned::value is true if both types are integral, Lhs is
// signed, and Rhs is unsigned. False in all other cases.
template <typename Lhs, typename Rhs>
struct is_signed_vs_unsigned {
  using lhs_underlying = typename comparison_underlying_type<Lhs>::type;
  using rhs_underlying = typename comparison_underlying_type<Rhs>::type;
  static constexpr bool value = std::is_integral<lhs_underlying>::value &&
                                std::is_integral<rhs_underlying>::value &&
                                std::is_signed<lhs_underlying>::value &&
                                std::is_unsigned<rhs_underlying>::value;
};
// Same thing, other way around: Lhs is unsigned, Rhs signed.
template <typename Lhs, typename Rhs>
struct is_unsigned_vs_signed : public is_signed_vs_unsigned<Rhs, Lhs> {};

// Specialize the compare functions for signed vs. unsigned comparisons.
// std::enable_if ensures that this template is only instantiable if both Lhs
// and Rhs are integral types, and their signedness does not match.
#define MAKE_UNSIGNED(Type, value)         \
  static_cast<typename std::make_unsigned< \
      typename comparison_underlying_type<Type>::type>::type>(value)
#define DEFINE_SIGNED_MISMATCH_COMP(CHECK, NAME, IMPL)                  \
  template <typename Lhs, typename Rhs>                                 \
  V8_INLINE typename std::enable_if<CHECK<Lhs, Rhs>::value, bool>::type \
      Cmp##NAME##Impl(Lhs lhs, Rhs rhs) {                               \
    return IMPL;                                                        \
  }
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, EQ,
                            lhs >= 0 && MAKE_UNSIGNED(Lhs, lhs) ==
                                            MAKE_UNDERLYING(Rhs, rhs))
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, LT,
                            lhs < 0 || MAKE_UNSIGNED(Lhs, lhs) <
                                           MAKE_UNDERLYING(Rhs, rhs))
DEFINE_SIGNED_MISMATCH_COMP(is_signed_vs_unsigned, LE,
                            lhs <= 0 || MAKE_UNSIGNED(Lhs, lhs) <=
                                            MAKE_UNDERLYING(Rhs, rhs))
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
                              bool>::type Cmp##NAME##Impl(Lhs lhs, Rhs rhs) {  \
    return lhs op rhs;                                                         \
  }                                                                            \
  template <typename Lhs, typename Rhs>                                        \
  V8_INLINE std::string* Check##NAME##Impl(Lhs lhs, Rhs rhs,                   \
                                           char const* msg) {                  \
    using LhsPassT = typename pass_value_or_ref<Lhs>::type;                    \
    using RhsPassT = typename pass_value_or_ref<Rhs>::type;                    \
    bool cmp = Cmp##NAME##Impl<LhsPassT, RhsPassT>(lhs, rhs);                  \
    return V8_LIKELY(cmp)                                                      \
               ? nullptr                                                       \
               : MakeCheckOpString<LhsPassT, RhsPassT>(lhs, rhs, msg);         \
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
#define DCHECK_EQ(lhs, rhs) DCHECK_OP(EQ, ==, lhs, rhs)
#define DCHECK_NE(lhs, rhs) DCHECK_OP(NE, !=, lhs, rhs)
#define DCHECK_GT(lhs, rhs) DCHECK_OP(GT, >, lhs, rhs)
#define DCHECK_GE(lhs, rhs) DCHECK_OP(GE, >=, lhs, rhs)
#define DCHECK_LT(lhs, rhs) DCHECK_OP(LT, <, lhs, rhs)
#define DCHECK_LE(lhs, rhs) DCHECK_OP(LE, <=, lhs, rhs)
#define DCHECK_NULL(val) DCHECK((val) == nullptr)
#define DCHECK_NOT_NULL(val) DCHECK((val) != nullptr)
#define DCHECK_IMPLIES(lhs, rhs) \
  DCHECK_WITH_MSG(!(lhs) || (rhs), #lhs " implies " #rhs)
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
