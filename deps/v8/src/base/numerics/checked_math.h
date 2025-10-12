// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_NUMERICS_CHECKED_MATH_H_
#define V8_BASE_NUMERICS_CHECKED_MATH_H_

#include <stdint.h>

#include <limits>
#include <type_traits>

#include "src/base/numerics/checked_math_impl.h"  // IWYU pragma: export
#include "src/base/numerics/safe_conversions.h"
#include "src/base/numerics/safe_math_shared_impl.h"  // IWYU pragma: export

namespace v8::base {
namespace internal {

template <typename T>
  requires std::is_arithmetic_v<T>
class CheckedNumeric {
 public:
  using type = T;

  constexpr CheckedNumeric() = default;

  // Copy constructor.
  template <typename Src>
  constexpr CheckedNumeric(const CheckedNumeric<Src>& rhs)
      : state_(rhs.state_.value(), rhs.IsValid()) {}

  // This is not an explicit constructor because we implicitly upgrade regular
  // numerics to CheckedNumerics to make them easier to use.
  template <typename Src>
    requires(std::is_arithmetic_v<Src>)
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr CheckedNumeric(Src value) : state_(value) {}

  // This is not an explicit constructor because we want a seamless conversion
  // from StrictNumeric types.
  template <typename Src>
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr CheckedNumeric(StrictNumeric<Src> value)
      : state_(static_cast<Src>(value)) {}

  // IsValid() - The public API to test if a CheckedNumeric is currently valid.
  // A range checked destination type can be supplied using the Dst template
  // parameter.
  template <typename Dst = T>
  constexpr bool IsValid() const {
    return state_.is_valid() &&
           IsValueInRangeForNumericType<Dst>(state_.value());
  }

  // AssignIfValid(Dst) - Assigns the underlying value if it is currently valid
  // and is within the range supported by the destination type. Returns true if
  // successful and false otherwise.
  template <typename Dst>
#if defined(__clang__) || defined(__GNUC__)
  __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
  _Check_return_
#endif
  constexpr bool
  AssignIfValid(Dst* result) const {
    if (IsValid<Dst>()) [[likely]] {
      *result = static_cast<Dst>(state_.value());
      return true;
    }
    return false;
  }

  // ValueOrDie() - The primary accessor for the underlying value. If the
  // current state is not valid it will CHECK and crash.
  // A range checked destination type can be supplied using the Dst template
  // parameter, which will trigger a CHECK if the value is not in bounds for
  // the destination.
  // The CHECK behavior can be overridden by supplying a handler as a
  // template parameter, for test code, etc. However, the handler cannot access
  // the underlying value, and it is not available through other means.
  template <typename Dst = T, class CheckHandler = CheckOnFailure>
  constexpr StrictNumeric<Dst> ValueOrDie() const {
    if (IsValid<Dst>()) [[likely]] {
      return static_cast<Dst>(state_.value());
    }
    return CheckHandler::template HandleFailure<Dst>();
  }

  // ValueOrDefault(T default_value) - A convenience method that returns the
  // current value if the state is valid, and the supplied default_value for
  // any other state.
  // A range checked destination type can be supplied using the Dst template
  // parameter. WARNING: This function may fail to compile or CHECK at runtime
  // if the supplied default_value is not within range of the destination type.
  template <typename Dst = T, typename Src>
  constexpr StrictNumeric<Dst> ValueOrDefault(Src default_value) const {
    if (IsValid<Dst>()) [[likely]] {
      return static_cast<Dst>(state_.value());
    }
    return checked_cast<Dst>(default_value);
  }

  // Returns a checked numeric of the specified type, cast from the current
  // CheckedNumeric. If the current state is invalid or the destination cannot
  // represent the result then the returned CheckedNumeric will be invalid.
  template <typename Dst>
  constexpr CheckedNumeric<UnderlyingType<Dst>> Cast() const {
    return *this;
  }

  // This friend method is available solely for providing more detailed logging
  // in the tests. Do not implement it in production code, because the
  // underlying values may change at any time.
  template <typename U>
  friend U GetNumericValueForTest(const CheckedNumeric<U>& src);

  // Prototypes for the supported arithmetic operator overloads.
  template <typename Src>
  constexpr CheckedNumeric& operator+=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator-=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator*=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator/=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator%=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator<<=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator>>=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator&=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator|=(const Src rhs);
  template <typename Src>
  constexpr CheckedNumeric& operator^=(const Src rhs);

  constexpr CheckedNumeric operator-() const {
    // Use an optimized code path for a known run-time variable.
    if (!std::is_constant_evaluated() && std::is_signed_v<T> &&
        std::is_floating_point_v<T>) {
      return FastRuntimeNegate();
    }
    // The negation of two's complement int min is int min.
    const bool is_valid =
        IsValid() &&
        (!std::is_signed_v<T> || std::is_floating_point_v<T> ||
         NegateWrapper(state_.value()) != std::numeric_limits<T>::lowest());
    return CheckedNumeric<T>(NegateWrapper(state_.value()), is_valid);
  }

  constexpr CheckedNumeric operator~() const {
    return CheckedNumeric<decltype(InvertWrapper(T()))>(
        InvertWrapper(state_.value()), IsValid());
  }

  constexpr CheckedNumeric Abs() const {
    return !IsValueNegative(state_.value()) ? *this : -*this;
  }

  template <typename U>
  constexpr CheckedNumeric<typename MathWrapper<CheckedMaxOp, T, U>::type> Max(
      U rhs) const {
    return CheckMax(*this, rhs);
  }

  template <typename U>
  constexpr CheckedNumeric<typename MathWrapper<CheckedMinOp, T, U>::type> Min(
      U rhs) const {
    return CheckMin(*this, rhs);
  }

  // This function is available only for integral types. It returns an unsigned
  // integer of the same width as the source type, containing the absolute value
  // of the source, and properly handling signed min.
  constexpr CheckedNumeric<typename UnsignedOrFloatForSize<T>::type>
  UnsignedAbs() const {
    return CheckedNumeric<typename UnsignedOrFloatForSize<T>::type>(
        SafeUnsignedAbs(state_.value()), state_.is_valid());
  }

  constexpr CheckedNumeric& operator++() {
    *this += 1;
    return *this;
  }

  constexpr CheckedNumeric operator++(int) {
    const CheckedNumeric value = *this;
    ++*this;
    return value;
  }

  constexpr CheckedNumeric& operator--() {
    *this -= 1;
    return *this;
  }

  constexpr CheckedNumeric operator--(int) {
    const CheckedNumeric value = *this;
    --*this;
    return value;
  }

  // These perform the actual math operations on the CheckedNumerics.
  // Binary arithmetic operations.
  template <template <typename, typename> class M, typename L, typename R>
  static constexpr CheckedNumeric MathOp(L lhs, R rhs) {
    using Math = typename MathWrapper<M, L, R>::math;
    T result = 0;
    const bool is_valid =
        Wrapper<L>::is_valid(lhs) && Wrapper<R>::is_valid(rhs) &&
        Math::Do(Wrapper<L>::value(lhs), Wrapper<R>::value(rhs), &result);
    return CheckedNumeric<T>(result, is_valid);
  }

  // Assignment arithmetic operations.
  template <template <typename, typename> class M, typename R>
  constexpr CheckedNumeric& MathOp(R rhs) {
    using Math = typename MathWrapper<M, T, R>::math;
    T result = 0;  // Using T as the destination saves a range check.
    const bool is_valid =
        state_.is_valid() && Wrapper<R>::is_valid(rhs) &&
        Math::Do(state_.value(), Wrapper<R>::value(rhs), &result);
    *this = CheckedNumeric<T>(result, is_valid);
    return *this;
  }

 private:
  template <typename U>
    requires std::is_arithmetic_v<U>
  friend class CheckedNumeric;

  CheckedNumericState<T> state_;

  CheckedNumeric FastRuntimeNegate() const {
    T result;
    const bool success = CheckedSubOp<T, T>::Do(T(0), state_.value(), &result);
    return CheckedNumeric<T>(result, IsValid() && success);
  }

  template <typename Src>
  constexpr CheckedNumeric(Src value, bool is_valid)
      : state_(value, is_valid) {}

  // These wrappers allow us to handle state the same way for both
  // CheckedNumeric and POD arithmetic types.
  template <typename Src>
  struct Wrapper {
    static constexpr bool is_valid(Src) { return true; }
    static constexpr Src value(Src value) { return value; }
  };

  template <typename Src>
  struct Wrapper<CheckedNumeric<Src>> {
    static constexpr bool is_valid(CheckedNumeric<Src> v) {
      return v.IsValid();
    }
    static constexpr Src value(CheckedNumeric<Src> v) {
      return v.state_.value();
    }
  };

  template <typename Src>
  struct Wrapper<StrictNumeric<Src>> {
    static constexpr bool is_valid(StrictNumeric<Src>) { return true; }
    static constexpr Src value(StrictNumeric<Src> v) {
      return static_cast<Src>(v);
    }
  };
};

template <typename T>
CheckedNumeric(T) -> CheckedNumeric<T>;

// Convenience functions to avoid the ugly template disambiguator syntax.
template <typename Dst, typename Src>
constexpr bool IsValidForType(const CheckedNumeric<Src> value) {
  return value.template IsValid<Dst>();
}

template <typename Dst, typename Src>
constexpr StrictNumeric<Dst> ValueOrDieForType(
    const CheckedNumeric<Src> value) {
  return value.template ValueOrDie<Dst>();
}

template <typename Dst, typename Src, typename Default>
constexpr StrictNumeric<Dst> ValueOrDefaultForType(CheckedNumeric<Src> value,
                                                   Default default_value) {
  return value.template ValueOrDefault<Dst>(default_value);
}

// Convenience wrapper to return a new CheckedNumeric from the provided
// arithmetic or CheckedNumericType.
template <typename T>
constexpr CheckedNumeric<UnderlyingType<T>> MakeCheckedNum(T value) {
  return value;
}

// These implement the variadic wrapper for the math operations.
template <template <typename, typename> class M, typename L, typename R>
constexpr CheckedNumeric<typename MathWrapper<M, L, R>::type> CheckMathOp(
    L lhs, R rhs) {
  using Math = typename MathWrapper<M, L, R>::math;
  return CheckedNumeric<typename Math::result_type>::template MathOp<M>(lhs,
                                                                        rhs);
}

// General purpose wrapper template for arithmetic operations.
template <template <typename, typename> class M, typename L, typename R,
          typename... Args>
constexpr auto CheckMathOp(L lhs, R rhs, Args... args) {
  return CheckMathOp<M>(CheckMathOp<M>(lhs, rhs), args...);
}

BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Add, +, +=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Sub, -, -=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Mul, *, *=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Div, /, /=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Mod, %, %=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Lsh, <<, <<=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Rsh, >>, >>=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, And, &, &=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Or, |, |=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Checked, Check, Xor, ^, ^=)
BASE_NUMERIC_ARITHMETIC_VARIADIC(Checked, Check, Max)
BASE_NUMERIC_ARITHMETIC_VARIADIC(Checked, Check, Min)

// These are some extra StrictNumeric operators to support simple pointer
// arithmetic with our result types. Since wrapping on a pointer is always
// bad, we trigger the CHECK condition here.
template <typename L, typename R>
L* operator+(L* lhs, StrictNumeric<R> rhs) {
  const uintptr_t result = CheckAdd(reinterpret_cast<uintptr_t>(lhs),
                                    CheckMul(sizeof(L), static_cast<R>(rhs)))
                               .template ValueOrDie<uintptr_t>();
  return reinterpret_cast<L*>(result);
}

template <typename L, typename R>
L* operator-(L* lhs, StrictNumeric<R> rhs) {
  const uintptr_t result = CheckSub(reinterpret_cast<uintptr_t>(lhs),
                                    CheckMul(sizeof(L), static_cast<R>(rhs)))
                               .template ValueOrDie<uintptr_t>();
  return reinterpret_cast<L*>(result);
}

}  // namespace internal

using internal::CheckAdd;
using internal::CheckAnd;
using internal::CheckDiv;
using internal::CheckedNumeric;
using internal::CheckLsh;
using internal::CheckMax;
using internal::CheckMin;
using internal::CheckMod;
using internal::CheckMul;
using internal::CheckOr;
using internal::CheckRsh;
using internal::CheckSub;
using internal::CheckXor;
using internal::IsValidForType;
using internal::MakeCheckedNum;
using internal::ValueOrDefaultForType;
using internal::ValueOrDieForType;

}  // namespace v8::base

#endif  // V8_BASE_NUMERICS_CHECKED_MATH_H_
