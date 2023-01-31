// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_TYPE_INFERENCE_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_TYPE_INFERENCE_REDUCER_H_

#include <limits>

#include "src/base/logging.h"
#include "src/base/vector.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/compiler/turboshaft/snapshot-table.h"
#include "src/compiler/turboshaft/types.h"

// #define TRACE_TYPING(...) PrintF(__VA_ARGS__)
#define TRACE_TYPING(...) ((void)0)

namespace v8::internal::compiler::turboshaft {

// Returns the array's least element, ignoring NaN.
// There must be at least one non-NaN element.
// Any -0 is converted to 0.
template <typename T, size_t N>
T array_min(const std::array<T, N>& a) {
  DCHECK_NE(0, N);
  T x = +std::numeric_limits<T>::infinity();
  for (size_t i = 0; i < N; ++i) {
    if (!std::isnan(a[i])) {
      x = std::min(a[i], x);
    }
  }
  DCHECK(!std::isnan(x));
  return x == T{0} ? T{0} : x;  // -0 -> 0
}

// Returns the array's greatest element, ignoring NaN.
// There must be at least one non-NaN element.
// Any -0 is converted to 0.
template <typename T, size_t N>
T array_max(const std::array<T, N>& a) {
  DCHECK_NE(0, N);
  T x = -std::numeric_limits<T>::infinity();
  for (size_t i = 0; i < N; ++i) {
    if (!std::isnan(a[i])) {
      x = std::max(a[i], x);
    }
  }
  DCHECK(!std::isnan(x));
  return x == T{0} ? T{0} : x;  // -0 -> 0
}

template <size_t Bits>
struct WordOperationTyper {
  static_assert(Bits == 32 || Bits == 64);
  using word_t = uint_type<Bits>;
  using type_t = WordType<Bits>;
  using ElementsVector = base::SmallVector<word_t, type_t::kMaxSetSize * 2>;
  static constexpr word_t max = std::numeric_limits<word_t>::max();

  static type_t FromElements(ElementsVector elements, Zone* zone) {
    base::sort(elements);
    auto it = std::unique(elements.begin(), elements.end());
    elements.pop_back(std::distance(it, elements.end()));
    DCHECK(!elements.empty());
    if (elements.size() <= type_t::kMaxSetSize) {
      return type_t::Set(elements, zone);
    }

    auto range =
        MakeRange(base::Vector<const word_t>{elements.data(), elements.size()});
    auto result = type_t::Range(range.first, range.second, zone);
    DCHECK(
        base::all_of(elements, [&](word_t e) { return result.Contains(e); }));
    return result;
  }

  static std::pair<word_t, word_t> MakeRange(const type_t& t) {
    if (t.is_range()) return t.range();
    DCHECK(t.is_set());
    return MakeRange(t.set_elements());
  }

  static std::pair<word_t, word_t> MakeRange(
      const base::Vector<const word_t>& elements) {
    DCHECK(!elements.empty());
    DCHECK(detail::is_unique_and_sorted(elements));
    if (elements[elements.size() - 1] - elements[0] <= max / 2) {
      // Construct a non-wrapping range.
      return {elements[0], elements[elements.size() - 1]};
    }
    // Construct a wrapping range.
    size_t from_index = elements.size() - 1;
    size_t to_index = 0;
    while (to_index + 1 < from_index) {
      if ((elements[to_index + 1] - elements[to_index]) <
          (elements[from_index] - elements[from_index - 1])) {
        ++to_index;
      } else {
        ++from_index;
      }
    }
    return {elements[from_index], elements[to_index]};
  }

  static word_t distance(const std::pair<word_t, word_t>& range) {
    return is_wrapping(range) ? (max - range.first + range.second)
                              : range.second - range.first;
  }

  static bool is_wrapping(const std::pair<word_t, word_t>& range) {
    return range.first > range.second;
  }

  static Type Add(const type_t& lhs, const type_t& rhs, Zone* zone) {
    if (lhs.is_any() || rhs.is_any()) return type_t::Any();

    // If both sides are decently small sets, we produce the product set.
    if (lhs.is_set() && rhs.is_set()) {
      ElementsVector result_elements;
      for (int i = 0; i < lhs.set_size(); ++i) {
        for (int j = 0; j < rhs.set_size(); ++j) {
          result_elements.push_back(lhs.set_element(i) + rhs.set_element(j));
        }
      }
      return FromElements(std::move(result_elements), zone);
    }

    // Otherwise just construct a range.
    std::pair<word_t, word_t> x = MakeRange(lhs);
    std::pair<word_t, word_t> y = MakeRange(rhs);

    // If the result would not be a complete range, we compute it.
    // Check: (lhs.to + rhs.to + 1) - (lhs.from + rhs.from + 1) < max
    // =====> (lhs.to - lhs.from) + (rhs.to - rhs.from) < max
    // =====> (lhs.to - lhs.from) < max - (rhs.to - rhs.from)
    if (distance(x) < max - distance(y)) {
      return type_t::Range(x.first + y.first, x.second + y.second, zone);
    }

    return type_t::Any();
  }

  static Type Subtract(const type_t& lhs, const type_t& rhs, Zone* zone) {
    if (lhs.is_any() || rhs.is_any()) return type_t::Any();

    // If both sides are decently small sets, we produce the product set.
    if (lhs.is_set() && rhs.is_set()) {
      ElementsVector result_elements;
      for (int i = 0; i < lhs.set_size(); ++i) {
        for (int j = 0; j < rhs.set_size(); ++j) {
          result_elements.push_back(lhs.set_element(i) - rhs.set_element(j));
        }
      }
      return FromElements(std::move(result_elements), zone);
    }

    // Otherwise just construct a range.
    std::pair<word_t, word_t> x = MakeRange(lhs);
    std::pair<word_t, word_t> y = MakeRange(rhs);

    if (is_wrapping(x) && is_wrapping(y)) {
      return type_t::Range(x.first - y.second, x.second - y.first, zone);
    }

    // TODO(nicohartmann@): Improve the wrapping cases.
    return type_t::Any();
  }

  // Computes the ranges to which the sides of the unsigned comparison (lhs <
  // rhs) can be restricted when the comparison is true. When the comparison is
  // true, we learn: lhs cannot be >= rhs.max and rhs cannot be <= lhs.min.
  static std::pair<Type, Type> RestrictionForUnsignedLessThan_True(
      const type_t& lhs, const type_t& rhs, Zone* zone) {
    Type restrict_lhs;
    if (rhs.unsigned_max() == 0) {
      // There is no value for lhs that could make (lhs < 0) true.
      restrict_lhs = Type::None();
    } else {
      restrict_lhs = type_t::Range(0, next_smaller(rhs.unsigned_max()), zone);
    }

    Type restrict_rhs;
    if (lhs.unsigned_min() == max) {
      // There is no value for rhs that could make (max < rhs) true.
      restrict_rhs = Type::None();
    } else {
      restrict_rhs = type_t::Range(next_larger(lhs.unsigned_min()), max, zone);
    }

    return {restrict_lhs, restrict_rhs};
  }

  // Computes the ranges to which the sides of the unsigned comparison (lhs <
  // rhs) can be restricted when the comparison is false. When the comparison is
  // false, we learn: lhs cannot be < rhs.min and rhs cannot be > lhs.max.
  static std::pair<Type, Type> RestrictionForUnsignedLessThan_False(
      const type_t& lhs, const type_t& rhs, Zone* zone) {
    return {type_t::Range(rhs.unsigned_min(), max, zone),
            type_t::Range(0, lhs.unsigned_max(), zone)};
  }

  // Computes the ranges to which the sides of the unsigned comparison (lhs <=
  // rhs) can be restricted when the comparison is true. When the comparison is
  // true, we learn: lhs cannot be > rhs.max and rhs cannot be < lhs.min.
  static std::pair<Type, Type> RestrictionForUnsignedLessThanOrEqual_True(
      const type_t& lhs, const type_t& rhs, Zone* zone) {
    return {type_t::Range(0, rhs.unsigned_max(), zone),
            type_t::Range(lhs.unsigned_min(), max, zone)};
  }

  // Computes the ranges to which the sides of the unsigned comparison (lhs <=
  // rhs) can be restricted when the comparison is false. When the comparison is
  // false, we learn: lhs cannot be <= rhs.min and rhs cannot be >= lhs.max.
  static std::pair<Type, Type> RestrictionForUnsignedLessThanOrEqual_False(
      const type_t& lhs, const type_t& rhs, Zone* zone) {
    Type restrict_lhs;
    if (rhs.unsigned_min() == max) {
      // There is no value for lhs that could make (lhs <= max) false.
      restrict_lhs = Type::None();
    } else {
      restrict_lhs = type_t::Range(next_larger(rhs.unsigned_min()), max, zone);
    }

    Type restrict_rhs;
    if (lhs.unsigned_max() == 0) {
      // There is no value for rhs that could make (0 <= rhs) false.
      restrict_rhs = Type::None();
    } else {
      restrict_rhs = type_t::Range(0, next_smaller(lhs.unsigned_max()), zone);
    }

    return {restrict_lhs, restrict_rhs};
  }
};

template <size_t Bits>
struct FloatOperationTyper {
  static_assert(Bits == 32 || Bits == 64);
  using float_t = std::conditional_t<Bits == 32, float, double>;
  using type_t = FloatType<Bits>;
  static constexpr float_t inf = std::numeric_limits<float_t>::infinity();
  static constexpr int kSetThreshold = type_t::kMaxSetSize;

  static type_t Range(float_t min, float_t max, uint32_t special_values,
                      Zone* zone) {
    DCHECK_LE(min, max);
    if (min == max) return Set({min}, special_values, zone);
    return type_t::Range(min, max, special_values, zone);
  }

  static type_t Set(std::vector<float_t> elements, uint32_t special_values,
                    Zone* zone) {
    base::sort(elements);
    elements.erase(std::unique(elements.begin(), elements.end()),
                   elements.end());
    if (base::erase_if(elements, [](float_t v) { return std::isnan(v); }) > 0) {
      special_values |= type_t::kNaN;
    }
    if (base::erase_if(elements, [](float_t v) { return IsMinusZero(v); }) >
        0) {
      special_values |= type_t::kMinusZero;
    }
    return type_t::Set(elements, special_values, zone);
  }

  static bool IsIntegerSet(const type_t& t) {
    if (!t.is_set()) return false;
    int size = t.set_size();
    DCHECK_LT(0, size);

    float_t unused_ipart;
    float_t min = t.set_element(0);
    if (std::modf(min, &unused_ipart) != 0.0) return false;
    if (min == -inf) return false;
    float_t max = t.set_element(size - 1);
    if (std::modf(max, &unused_ipart) != 0.0) return false;
    if (max == inf) return false;

    for (int i = 1; i < size - 1; ++i) {
      if (std::modf(t.set_element(i), &unused_ipart) != 0.0) return false;
    }
    return true;
  }

  static bool IsZeroish(const type_t& l) {
    return l.has_nan() || l.has_minus_zero() || l.Contains(0);
  }

  // Tries to construct the product of two sets where values are generated using
  // {combine}. Returns Type::Invalid() if a set cannot be constructed (e.g.
  // because the result exceeds the maximal number of set elements).
  static Type ProductSet(const type_t& l, const type_t& r,
                         uint32_t special_values, Zone* zone,
                         std::function<float_t(float_t, float_t)> combine) {
    DCHECK(l.is_set());
    DCHECK(r.is_set());
    std::vector<float_t> results;
    for (int i = 0; i < l.set_size(); ++i) {
      for (int j = 0; j < r.set_size(); ++j) {
        results.push_back(combine(l.set_element(i), r.set_element(j)));
      }
    }
    if (base::erase_if(results, [](float_t v) { return std::isnan(v); }) > 0) {
      special_values |= type_t::kNaN;
    }
    if (base::erase_if(results, [](float_t v) { return IsMinusZero(v); }) > 0) {
      special_values |= type_t::kMinusZero;
    }
    base::sort(results);
    auto it = std::unique(results.begin(), results.end());
    if (std::distance(results.begin(), it) > kSetThreshold)
      return Type::Invalid();
    results.erase(it, results.end());
    if (results.empty()) return type_t::OnlySpecialValues(special_values);
    return Set(std::move(results), special_values, zone);
  }

  static Type Add(type_t l, type_t r, Zone* zone) {
    // Addition can return NaN if either input can be NaN or we try to compute
    // the sum of two infinities of opposite sign.
    if (l.is_only_nan() || r.is_only_nan()) return type_t::NaN();
    bool maybe_nan = l.has_nan() || r.has_nan();

    // Addition can yield minus zero only if both inputs can be minus zero.
    bool maybe_minuszero = true;
    if (l.has_minus_zero()) {
      l = type_t::LeastUpperBound(l, type_t::Constant(0), zone);
    } else {
      maybe_minuszero = false;
    }
    if (r.has_minus_zero()) {
      r = type_t::LeastUpperBound(r, type_t::Constant(0), zone);
    } else {
      maybe_minuszero = false;
    }

    uint32_t special_values = (maybe_nan ? type_t::kNaN : 0) |
                              (maybe_minuszero ? type_t::kMinusZero : 0);
    // If both sides are decently small sets, we produce the product set.
    auto combine = [](float_t a, float_t b) { return a + b; };
    if (l.is_set() && r.is_set()) {
      auto result = ProductSet(l, r, special_values, zone, combine);
      if (!result.IsInvalid()) return result;
    }

    // Otherwise just construct a range.
    auto [l_min, l_max] = l.minmax();
    auto [r_min, r_max] = r.minmax();

    std::array<float_t, 4> results;
    results[0] = l_min + r_min;
    results[1] = l_min + r_max;
    results[2] = l_max + r_min;
    results[3] = l_max + r_max;

    int nans = 0;
    for (int i = 0; i < 4; ++i) {
      if (std::isnan(results[i])) ++nans;
    }
    if (nans > 0) {
      special_values |= type_t::kNaN;
      if (nans >= 4) {
        // All combinations of inputs produce NaN.
        return type_t::OnlySpecialValues(special_values);
      }
    }
    const float_t result_min = array_min(results);
    const float_t result_max = array_max(results);
    return Range(result_min, result_max, special_values, zone);
  }

  static Type Subtract(type_t l, type_t r, Zone* zone) {
    // Subtraction can return NaN if either input can be NaN or we try to
    // compute the sum of two infinities of opposite sign.
    if (l.is_only_nan() || r.is_only_nan()) return type_t::NaN();
    bool maybe_nan = l.has_nan() || r.has_nan();

    // Subtraction can yield minus zero if {lhs} can be minus zero and {rhs}
    // can be zero.
    bool maybe_minuszero = false;
    if (l.has_minus_zero()) {
      l = type_t::LeastUpperBound(l, type_t::Constant(0), zone);
      maybe_minuszero = r.Contains(0);
    }
    if (r.has_minus_zero()) {
      r = type_t::LeastUpperBound(r, type_t::Constant(0), zone);
    }

    uint32_t special_values = (maybe_nan ? type_t::kNaN : 0) |
                              (maybe_minuszero ? type_t::kMinusZero : 0);
    // If both sides are decently small sets, we produce the product set.
    auto combine = [](float_t a, float_t b) { return a - b; };
    if (l.is_set() && r.is_set()) {
      auto result = ProductSet(l, r, special_values, zone, combine);
      if (!result.IsInvalid()) return result;
    }

    // Otherwise just construct a range.
    auto [l_min, l_max] = l.minmax();
    auto [r_min, r_max] = r.minmax();

    std::array<float_t, 4> results;
    results[0] = l_min - r_min;
    results[1] = l_min - r_max;
    results[2] = l_max - r_min;
    results[3] = l_max - r_max;

    int nans = 0;
    for (int i = 0; i < 4; ++i) {
      if (std::isnan(results[i])) ++nans;
    }
    if (nans > 0) {
      special_values |= type_t::kNaN;
      if (nans >= 4) {
        // All combinations of inputs produce NaN.
        return type_t::NaN();
      }
    }
    const float_t result_min = array_min(results);
    const float_t result_max = array_max(results);
    return Range(result_min, result_max, special_values, zone);
  }

  static Type Multiply(type_t l, type_t r, Zone* zone) {
    // Multiplication propagates NaN:
    //   NaN * x = NaN         (regardless of sign of x)
    //   0 * Infinity = NaN    (regardless of signs)
    if (l.is_only_nan() || r.is_only_nan()) return type_t::NaN();
    bool maybe_nan = l.has_nan() || r.has_nan() ||
                     (IsZeroish(l) && (r.min() == -inf || r.max() == inf)) ||
                     (IsZeroish(r) && (l.min() == -inf || r.max() == inf));

    // Try to rule out -0.
    bool maybe_minuszero = l.has_minus_zero() || r.has_minus_zero() ||
                           (IsZeroish(l) && r.min() < 0.0) ||
                           (IsZeroish(r) && l.min() < 0.0);
    if (l.has_minus_zero()) {
      l = type_t::LeastUpperBound(l, type_t::Constant(0), zone);
    }
    if (r.has_minus_zero()) {
      r = type_t::LeastUpperBound(r, type_t::Constant(0), zone);
    }

    uint32_t special_values = (maybe_nan ? type_t::kNaN : 0) |
                              (maybe_minuszero ? type_t::kMinusZero : 0);
    // If both sides are decently small sets, we produce the product set.
    auto combine = [](float_t a, float_t b) { return a * b; };
    if (l.is_set() && r.is_set()) {
      auto result = ProductSet(l, r, special_values, zone, combine);
      if (!result.IsInvalid()) return result;
    }

    // Otherwise just construct a range.
    auto [l_min, l_max] = l.minmax();
    auto [r_min, r_max] = r.minmax();

    std::array<float_t, 4> results;
    results[0] = l_min * r_min;
    results[1] = l_min * r_max;
    results[2] = l_max * r_min;
    results[3] = l_max * r_max;

    for (int i = 0; i < 4; ++i) {
      if (std::isnan(results[i])) {
        return type_t::Any();
      }
    }

    float_t result_min = array_min(results);
    float_t result_max = array_max(results);
    if (result_min <= 0.0 && 0.0 <= result_max &&
        (l_min < 0.0 || r_min < 0.0)) {
      special_values |= type_t::kMinusZero;
      // Remove -0.
      result_min += 0.0;
      result_max += 0.0;
    }
    // 0 * V8_INFINITY is NaN, regardless of sign
    if (((l_min == -inf || l_max == inf) && (r_min <= 0.0 && 0.0 <= r_max)) ||
        ((r_min == -inf || r_max == inf) && (l_min <= 0.0 && 0.0 <= l_max))) {
      special_values |= type_t::kNaN;
    }

    type_t type = Range(result_min, result_max, special_values, zone);
    return type;
  }

  static Type Divide(const type_t& l, const type_t& r, Zone* zone) {
    // Division is tricky, so all we do is try ruling out -0 and NaN.
    if (l.is_only_nan() || r.is_only_nan()) return type_t::NaN();
    auto [l_min, l_max] = l.minmax();
    auto [r_min, r_max] = r.minmax();

    bool maybe_nan =
        (IsZeroish(l) && IsZeroish(r)) ||
        ((l_min == -inf || l_max == inf) && (r_min == -inf || r_max == inf));

    // Try to rule out -0.
    bool maybe_minuszero =
        (IsZeroish(l) && r.min() < 0.0) || (r.min() == -inf || r.max() == inf);

    uint32_t special_values = (maybe_nan ? type_t::kNaN : 0) |
                              (maybe_minuszero ? type_t::kMinusZero : 0);
    // If both sides are decently small sets, we produce the product set.
    auto combine = [](float_t a, float_t b) {
      if (b == 0) return nan_v<Bits>;
      return a / b;
    };
    if (l.is_set() && r.is_set()) {
      auto result = ProductSet(l, r, special_values, zone, combine);
      if (!result.IsInvalid()) return result;
    }

    const bool r_all_positive = r_min >= 0 && !r.has_minus_zero();
    const bool r_all_negative = r_max < 0;

    // If r doesn't span 0, we can try to compute a more precise type.
    if (r_all_positive || r_all_negative) {
      // If r does not contain 0 or -0, we can compute a range.
      if (r_min > 0 && !r.has_minus_zero()) {
        std::array<float_t, 4> results;
        results[0] = l_min / r_min;
        results[1] = l_min / r_max;
        results[2] = l_max / r_min;
        results[3] = l_max / r_max;

        const float_t result_min = array_min(results);
        const float_t result_max = array_max(results);
        return Range(result_min, result_max, special_values, zone);
      }

      // Otherwise we try to check for the sign of the result.
      if (l_max < 0) {
        if (r_all_positive) {
          // All values are negative.
          DCHECK_NE(special_values & type_t::kMinusZero, 0);
          return Range(-inf, next_smaller(float_t{0}), special_values, zone);
        } else {
          DCHECK(r_all_negative);
          // All values are positive.
          return Range(0, inf, special_values, zone);
        }
      } else if (l_min >= 0 && !l.has_minus_zero()) {
        if (r_all_positive) {
          // All values are positive.
          DCHECK_EQ(special_values & type_t::kMinusZero, 0);
          return Range(0, inf, special_values, zone);
        } else {
          DCHECK(r_all_negative);
          // All values are negative.
          return Range(-inf, next_smaller(float_t{0}), special_values, zone);
        }
      }
    }

    // Otherwise we give up on a precise type.
    return type_t::Any(special_values);
  }

  static Type Modulus(type_t l, type_t r, Zone* zone) {
    // Modulus can yield NaN if either {lhs} or {rhs} are NaN, or
    // {lhs} is not finite, or the {rhs} is a zero value.
    if (l.is_only_nan() || r.is_only_nan()) return type_t::NaN();
    bool maybe_nan =
        l.has_nan() || IsZeroish(r) || l.min() == -inf || l.max() == inf;

    // Deal with -0 inputs, only the signbit of {lhs} matters for the result.
    bool maybe_minuszero = false;
    if (l.has_minus_zero()) {
      maybe_minuszero = true;
      l = type_t::LeastUpperBound(l, type_t::Constant(0), zone);
    }
    if (r.has_minus_zero()) {
      r = type_t::LeastUpperBound(r, type_t::Constant(0), zone);
    }

    uint32_t special_values = (maybe_nan ? type_t::kNaN : 0) |
                              (maybe_minuszero ? type_t::kMinusZero : 0);
    // For integer inputs {l} and {r} we can infer a precise type.
    if (IsIntegerSet(l) && IsIntegerSet(r)) {
      auto [l_min, l_max] = l.minmax();
      auto [r_min, r_max] = r.minmax();
      auto l_abs = std::max(std::abs(l_min), std::abs(l_max));
      auto r_abs = std::max(std::abs(r_min), std::abs(r_max)) - 1;
      auto abs = std::min(l_abs, r_abs);
      float_t min = 0.0, max = 0.0;
      if (l_min >= 0.0) {
        // {l} positive.
        max = abs;
      } else if (l_max <= 0.0) {
        // {l} negative.
        min = 0.0 - abs;
      } else {
        // {l} positive or negative.
        min = 0.0 - abs;
        max = abs;
      }
      if (min == max) return Set({min}, special_values, zone);
      return Range(min, max, special_values, zone);
    }

    // Otherwise, we give up.
    return type_t::Any(special_values);
  }

  static Type Min(type_t l, type_t r, Zone* zone) {
    if (l.is_only_nan() || r.is_only_nan()) return type_t::NaN();
    bool maybe_nan = l.has_nan() || r.has_nan();

    // In order to ensure monotonicity of the computation below, we additionally
    // pretend +0 is present (for simplicity on both sides).
    bool maybe_minuszero = false;
    if (l.has_minus_zero() && !(r.max() < 0.0)) {
      maybe_minuszero = true;
      l = type_t::LeastUpperBound(l, type_t::Constant(0), zone);
    }
    if (r.has_minus_zero() && !(l.max() < 0.0)) {
      maybe_minuszero = true;
      r = type_t::LeastUpperBound(r, type_t::Constant(0), zone);
    }

    uint32_t special_values = (maybe_nan ? type_t::kNaN : 0) |
                              (maybe_minuszero ? type_t::kMinusZero : 0);
    // If both sides are decently small sets, we produce the product set.
    auto combine = [](float_t a, float_t b) { return std::min(a, b); };
    if (l.is_set() && r.is_set()) {
      // TODO(nicohartmann@): There is a faster way to compute this set.
      auto result = ProductSet(l, r, special_values, zone, combine);
      if (!result.IsInvalid()) return result;
    }

    // Otherwise just construct a range.
    auto [l_min, l_max] = l.minmax();
    auto [r_min, r_max] = r.minmax();

    auto min = std::min(l_min, r_min);
    auto max = std::min(l_max, r_max);
    return Range(min, max, special_values, zone);
  }

  static Type Max(type_t l, type_t r, Zone* zone) {
    if (l.is_only_nan() || r.is_only_nan()) return type_t::NaN();
    bool maybe_nan = l.has_nan() || r.has_nan();

    // In order to ensure monotonicity of the computation below, we additionally
    // pretend +0 is present (for simplicity on both sides).
    bool maybe_minuszero = false;
    if (l.has_minus_zero() && !(r.min() > 0.0)) {
      maybe_minuszero = true;
      l = type_t::LeastUpperBound(l, type_t::Constant(0), zone);
    }
    if (r.has_minus_zero() && !(l.min() > 0.0)) {
      maybe_minuszero = true;
      r = type_t::LeastUpperBound(r, type_t::Constant(0), zone);
    }

    uint32_t special_values = (maybe_nan ? type_t::kNaN : 0) |
                              (maybe_minuszero ? type_t::kMinusZero : 0);
    // If both sides are decently small sets, we produce the product set.
    auto combine = [](float_t a, float_t b) { return std::max(a, b); };
    if (l.is_set() && r.is_set()) {
      // TODO(nicohartmann@): There is a faster way to compute this set.
      auto result = ProductSet(l, r, special_values, zone, combine);
      if (!result.IsInvalid()) return result;
    }

    // Otherwise just construct a range.
    auto [l_min, l_max] = l.minmax();
    auto [r_min, r_max] = r.minmax();

    auto min = std::max(l_min, r_min);
    auto max = std::max(l_max, r_max);
    return Range(min, max, special_values, zone);
  }

  static Type Power(const type_t& l, const type_t& r, Zone* zone) {
    if (l.is_only_nan() || r.is_only_nan()) return type_t::NaN();
    bool maybe_nan = l.has_nan() || r.has_nan();

    // a ** b produces NaN if a < 0 && b is fraction.
    if (l.min() <= 0.0 && !IsIntegerSet(r)) maybe_nan = true;

    // a ** b produces -0 iff a == -0 and b is odd. Checking for all the cases
    // where b does only contain odd integer values seems not worth the
    // additional information we get here. We accept this over-approximation for
    // now. We could refine this whenever we see a benefit.
    uint32_t special_values =
        (maybe_nan ? type_t::kNaN : 0) | l.special_values();

    // If both sides are decently small sets, we produce the product set.
    auto combine = [](float_t a, float_t b) { return std::pow(a, b); };
    if (l.is_set() && r.is_set()) {
      auto result = ProductSet(l, r, special_values, zone, combine);
      if (!result.IsInvalid()) return result;
    }

    // TODO(nicohartmann@): Maybe we can produce a more precise range here.
    return type_t::Any(special_values);
  }

  static Type Atan2(const type_t& l, const type_t& r, Zone* zone) {
    // TODO(nicohartmann@): Maybe we can produce a more precise range her.e
    return type_t::Any(type_t::kNaN);
  }

  // Computes the ranges to which the sides of the comparison (lhs < rhs) can be
  // restricted when the comparison is true. When the comparison is true, we
  // learn: lhs cannot be >= rhs.max and rhs cannot be <= lhs.min and neither
  // can be NaN.
  static std::pair<Type, Type> RestrictionForLessThan_True(const type_t& lhs,
                                                           const type_t& rhs,
                                                           Zone* zone) {
    Type restrict_lhs;
    if (rhs.max() == -inf) {
      // There is no value for lhs that could make (lhs < -inf) true.
      restrict_lhs = Type::None();
    } else {
      const auto max = next_smaller(rhs.max());
      uint32_t sv = max >= 0 ? type_t::kMinusZero : type_t::kNoSpecialValues;
      restrict_lhs = type_t::Range(-inf, max, sv, zone);
    }

    Type restrict_rhs;
    if (lhs.min() == inf) {
      // There is no value for rhs that could make (inf < rhs) true.
      restrict_rhs = Type::None();
    } else {
      const auto min = next_larger(lhs.min());
      uint32_t sv = min <= 0 ? type_t::kMinusZero : type_t::kNoSpecialValues;
      restrict_rhs = type_t::Range(min, inf, sv, zone);
    }

    return {restrict_lhs, restrict_rhs};
  }

  // Computes the ranges to which the sides of the comparison (lhs < rhs) can be
  // restricted when the comparison is false. When the comparison is false, we
  // learn: lhs cannot be < rhs.min and rhs cannot be > lhs.max.
  static std::pair<Type, Type> RestrictionForLessThan_False(const type_t& lhs,
                                                            const type_t& rhs,
                                                            Zone* zone) {
    uint32_t lhs_sv =
        type_t::kNaN |
        (rhs.min() <= 0 ? type_t::kMinusZero : type_t::kNoSpecialValues);
    uint32_t rhs_sv =
        type_t::kNaN |
        (lhs.max() >= 0 ? type_t::kMinusZero : type_t::kNoSpecialValues);
    return {type_t::Range(rhs.min(), inf, lhs_sv, zone),
            type_t::Range(-inf, lhs.max(), rhs_sv, zone)};
  }

  // Computes the ranges to which the sides of the comparison (lhs <= rhs) can
  // be restricted when the comparison is true. When the comparison is true, we
  // learn: lhs cannot be > rhs.max and rhs cannot be < lhs.min and neither can
  // be NaN.
  static std::pair<Type, Type> RestrictionForLessThanOrEqual_True(
      const type_t& lhs, const type_t& rhs, Zone* zone) {
    uint32_t lhs_sv =
        rhs.max() >= 0 ? type_t::kMinusZero : type_t::kNoSpecialValues;
    uint32_t rhs_sv =
        lhs.min() <= 0 ? type_t::kMinusZero : type_t::kNoSpecialValues;
    return {type_t::Range(-inf, rhs.max(), lhs_sv, zone),
            type_t::Range(lhs.min(), inf, rhs_sv, zone)};
  }

  // Computes the ranges to which the sides of the comparison (lhs <= rhs) can
  // be restricted when the comparison is false. When the comparison is false,
  // we learn: lhs cannot be <= rhs.min and rhs cannot be >= lhs.max.
  static std::pair<Type, Type> RestrictionForLessThanOrEqual_False(
      const type_t& lhs, const type_t& rhs, Zone* zone) {
    Type restrict_lhs;
    if (rhs.min() == inf) {
      // The only value for lhs that could make (lhs <= inf) false is NaN.
      restrict_lhs = type_t::NaN();
    } else {
      const auto min = next_larger(rhs.min());
      uint32_t sv = type_t::kNaN |
                    (min <= 0 ? type_t::kMinusZero : type_t::kNoSpecialValues);
      restrict_lhs = type_t::Range(min, inf, sv, zone);
    }

    Type restrict_rhs;
    if (lhs.max() == -inf) {
      // The only value for rhs that could make (-inf <= rhs) false is NaN.
      restrict_rhs = type_t::NaN();
    } else {
      const auto max = next_smaller(lhs.max());
      uint32_t sv = type_t::kNaN |
                    (max >= 0 ? type_t::kMinusZero : type_t::kNoSpecialValues);
      restrict_rhs = type_t::Range(-inf, max, sv, zone);
    }

    return {restrict_lhs, restrict_rhs};
  }
};

class Typer {
 public:
  static Type TypeConstant(ConstantOp::Kind kind, ConstantOp::Storage value) {
    switch (kind) {
      case ConstantOp::Kind::kFloat32:
        if (std::isnan(value.float32)) return Float32Type::NaN();
        if (IsMinusZero(value.float32)) return Float32Type::MinusZero();
        return Float32Type::Constant(value.float32);
      case ConstantOp::Kind::kFloat64:
        if (std::isnan(value.float64)) return Float64Type::NaN();
        if (IsMinusZero(value.float64)) return Float64Type::MinusZero();
        return Float64Type::Constant(value.float64);
      case ConstantOp::Kind::kWord32:
        return Word32Type::Constant(static_cast<uint32_t>(value.integral));
      case ConstantOp::Kind::kWord64:
        return Word64Type::Constant(static_cast<uint64_t>(value.integral));
      default:
        // TODO(nicohartmann@): Support remaining {kind}s.
        return Type::Invalid();
    }
  }

  static Type LeastUpperBound(const Type& lhs, const Type& rhs, Zone* zone) {
    if (lhs.IsAny() || rhs.IsAny()) return Type::Any();
    if (lhs.IsNone()) return rhs;
    if (rhs.IsNone()) return lhs;

    // TODO(nicohartmann@): We might use more precise types here but currently
    // there is not much benefit in that.
    if (lhs.kind() != rhs.kind()) return Type::Any();

    switch (lhs.kind()) {
      case Type::Kind::kInvalid:
        UNREACHABLE();
      case Type::Kind::kNone:
        UNREACHABLE();
      case Type::Kind::kWord32:
        return Word32Type::LeastUpperBound(lhs.AsWord32(), rhs.AsWord32(),
                                           zone);
      case Type::Kind::kWord64:
        return Word64Type::LeastUpperBound(lhs.AsWord64(), rhs.AsWord64(),
                                           zone);
      case Type::Kind::kFloat32:
        return Float32Type::LeastUpperBound(lhs.AsFloat32(), rhs.AsFloat32(),
                                            zone);
      case Type::Kind::kFloat64:
        return Float64Type::LeastUpperBound(lhs.AsFloat64(), rhs.AsFloat64(),
                                            zone);
      case Type::Kind::kAny:
        UNREACHABLE();
    }
  }

  static Type TypeWord32Add(const Type& lhs, const Type& rhs, Zone* zone) {
    if (lhs.IsNone() || rhs.IsNone()) return Type::None();
    auto l = TruncateWord32Input(lhs, true, zone);
    auto r = TruncateWord32Input(rhs, true, zone);
    return WordOperationTyper<32>::Add(l, r, zone);
  }

  static Type TypeWord32Sub(const Type& lhs, const Type& rhs, Zone* zone) {
    if (lhs.IsNone() || rhs.IsNone()) return Type::None();
    auto l = TruncateWord32Input(lhs, true, zone);
    auto r = TruncateWord32Input(rhs, true, zone);
    return WordOperationTyper<32>::Subtract(l, r, zone);
  }

  static Type TypeWord64Add(const Type& lhs, const Type& rhs, Zone* zone) {
    if (lhs.IsNone() || rhs.IsNone()) return Type::None();
    if (!InputIs(lhs, Type::Kind::kWord64) ||
        !InputIs(rhs, Type::Kind::kWord64)) {
      return Word64Type::Any();
    }
    const auto& l = lhs.AsWord64();
    const auto& r = rhs.AsWord64();

    return WordOperationTyper<64>::Add(l, r, zone);
  }

  static Type TypeWord64Sub(const Type& lhs, const Type& rhs, Zone* zone) {
    if (lhs.IsNone() || rhs.IsNone()) return Type::None();
    if (!InputIs(lhs, Type::Kind::kWord64) ||
        !InputIs(rhs, Type::Kind::kWord64)) {
      return Word64Type::Any();
    }

    const auto& l = lhs.AsWord64();
    const auto& r = rhs.AsWord64();

    return WordOperationTyper<64>::Subtract(l, r, zone);
  }

#define FLOAT_BINOP(op, bits, float_typer_handler)                     \
  static Type TypeFloat##bits##op(const Type& lhs, const Type& rhs,    \
                                  Zone* zone) {                        \
    if (lhs.IsNone() || rhs.IsNone()) return Type::None();             \
    if (!InputIs(lhs, Type::Kind::kFloat##bits) ||                     \
        !InputIs(rhs, Type::Kind::kFloat##bits)) {                     \
      return Float##bits##Type::Any();                                 \
    }                                                                  \
    const auto& l = lhs.AsFloat##bits();                               \
    const auto& r = rhs.AsFloat##bits();                               \
    return FloatOperationTyper<bits>::float_typer_handler(l, r, zone); \
  }

  // Float32 operations
  FLOAT_BINOP(Add, 32, Add)
  FLOAT_BINOP(Sub, 32, Subtract)
  FLOAT_BINOP(Mul, 32, Multiply)
  FLOAT_BINOP(Div, 32, Divide)
  FLOAT_BINOP(Mod, 32, Modulus)
  FLOAT_BINOP(Min, 32, Min)
  FLOAT_BINOP(Max, 32, Max)
  FLOAT_BINOP(Power, 32, Power)
  FLOAT_BINOP(Atan2, 32, Atan2)
  // Float64 operations
  FLOAT_BINOP(Add, 64, Add)
  FLOAT_BINOP(Sub, 64, Subtract)
  FLOAT_BINOP(Mul, 64, Multiply)
  FLOAT_BINOP(Div, 64, Divide)
  FLOAT_BINOP(Mod, 64, Modulus)
  FLOAT_BINOP(Min, 64, Min)
  FLOAT_BINOP(Max, 64, Max)
  FLOAT_BINOP(Power, 64, Power)
  FLOAT_BINOP(Atan2, 64, Atan2)
#undef FLOAT_BINOP

  static Word64Type ExtendWord32ToWord64(const Word32Type& t, Zone* zone) {
    // We cannot infer much, but the lower bound of the word32 is also the lower
    // bound of the word64 type.
    if (t.is_wrapping()) return Word64Type::Any();
    return Word64Type::Range(static_cast<uint64_t>(t.unsigned_min()),
                             std::numeric_limits<uint64_t>::max(), zone);
  }

  static Word32Type TruncateWord32Input(const Type& input,
                                        bool implicit_word64_narrowing,
                                        Zone* zone) {
    DCHECK(!input.IsInvalid());
    DCHECK(!input.IsNone());

    if (input.IsAny()) {
      if (allow_invalid_inputs()) return Word32Type::Any();
    } else if (input.IsWord32()) {
      return input.AsWord32();
    } else if (input.IsWord64() && implicit_word64_narrowing) {
      // The input is implicitly converted to word32.
      const auto& w64 = input.AsWord64();
      if (w64.is_set()) {
        WordOperationTyper<32>::ElementsVector elements;
        for (uint64_t e : w64.set_elements()) {
          elements.push_back(static_cast<uint32_t>(e));
        }
        return WordOperationTyper<32>::FromElements(std::move(elements), zone);
      }

      if (w64.is_any() || w64.is_wrapping()) return Word32Type::Any();

      if (w64.range_to() <= std::numeric_limits<uint32_t>::max()) {
        DCHECK_LE(w64.range_from(), std::numeric_limits<uint32_t>::max());
        return Word32Type::Range(static_cast<uint32_t>(w64.range_from()),
                                 static_cast<uint32_t>(w64.range_to()), zone);
      }

      // TODO(nicohartmann@): Might compute a more precise range here.
      return Word32Type::Any();
    }
    UNREACHABLE();
  }

  static bool InputIs(const Type& input, Type::Kind expected) {
    if (input.IsInvalid()) {
      if (allow_invalid_inputs()) return false;
    } else if (input.kind() == expected) {
      return true;
    } else if (input.IsAny()) {
      if (allow_invalid_inputs()) return false;
    }
    UNREACHABLE();
  }

  // For now we allow invalid inputs (which will then just lead to very generic
  // typing). Once all operations are implemented, we are going to disable this.
  static bool allow_invalid_inputs() { return true; }
};

template <class Next>
class TypeInferenceReducer : public Next {
  static_assert(next_is_bottom_of_assembler_stack<Next>::value);
  using table_t = SnapshotTable<Type>;

 public:
  using Next::Asm;
  using ArgT =
      base::append_tuple_type<typename Next::ArgT, TypeInferenceReducerArgs>;

  template <typename... Args>
  explicit TypeInferenceReducer(const std::tuple<Args...>& args)
      : Next(args),
        types_(Asm().output_graph().operation_types()),
        table_(Asm().phase_zone()),
        op_to_key_mapping_(Asm().phase_zone()),
        block_to_snapshot_mapping_(Asm().input_graph().block_count(),
                                   base::nullopt, Asm().phase_zone()),
        predecessors_(Asm().phase_zone()),
        isolate_(std::get<TypeInferenceReducerArgs>(args).isolate) {}

  void Bind(Block* new_block, const Block* origin) {
    Next::Bind(new_block, origin);

    // Seal the current block first.
    if (table_.IsSealed()) {
      DCHECK_NULL(current_block_);
    } else {
      // If we bind a new block while the previous one is still unsealed, we
      // finalize it.
      DCHECK_NOT_NULL(current_block_);
      DCHECK(current_block_->index().valid());
      block_to_snapshot_mapping_[current_block_->index()] = table_.Seal();
      current_block_ = nullptr;
    }

    // Collect the snapshots of all predecessors.
    {
      predecessors_.clear();
      for (const Block* pred = new_block->LastPredecessor(); pred != nullptr;
           pred = pred->NeighboringPredecessor()) {
        base::Optional<table_t::Snapshot> pred_snapshot =
            block_to_snapshot_mapping_[pred->index()];
        DCHECK(pred_snapshot.has_value());
        predecessors_.push_back(pred_snapshot.value());
      }
      std::reverse(predecessors_.begin(), predecessors_.end());
    }

    // Start a new snapshot for this block by merging information from
    // predecessors.
    {
      auto MergeTypes = [&](table_t::Key,
                            base::Vector<Type> predecessors) -> Type {
        DCHECK_GT(predecessors.size(), 0);
        Type result_type = predecessors[0];
        for (size_t i = 1; i < predecessors.size(); ++i) {
          result_type = Typer::LeastUpperBound(result_type, predecessors[i],
                                               Asm().graph_zone());
        }
        return result_type;
      };

      table_.StartNewSnapshot(base::VectorOf(predecessors_), MergeTypes);
    }

    // Check if the predecessor is a branch that allows us to refine a few
    // types.
    if (new_block->HasExactlyNPredecessors(1)) {
      Block* predecessor = new_block->LastPredecessor();
      const Operation& terminator =
          predecessor->LastOperation(Asm().output_graph());
      if (const BranchOp* branch = terminator.TryCast<BranchOp>()) {
        DCHECK(branch->if_true == new_block || branch->if_false == new_block);
        RefineTypesAfterBranch(branch, branch->if_true == new_block);
      }
    }
    current_block_ = new_block;
  }

  template <bool allow_implicit_word64_truncation>
  Type RefineWord32Type(const Type& type, const Type& refinement, Zone* zone) {
    // If refinement is Type::None(), the operation/branch is unreachable.
    if (refinement.IsNone()) return Type::None();
    DCHECK(refinement.IsWord32());
    if constexpr (allow_implicit_word64_truncation) {
      // Turboshaft allows implicit trunction of Word64 values to Word32. When
      // an operation on Word32 representation computes a refinement type, this
      // is going to be a Type::Word32() even if the actual {type} was Word64
      // before truncation. To correctly refine this type, we need to extend the
      // {refinement} to Word64 such that it reflects the corresponding values
      // in the original type (before truncation) before we intersect.
      if (type.IsWord64()) {
        return Word64Type::Intersect(
            type.AsWord64(),
            Typer::ExtendWord32ToWord64(refinement.AsWord32(), zone),
            Type::ResolutionMode::kOverApproximate, zone);
      }
    }
    // We limit the values of {type} to those in {refinement}.
    return Word32Type::Intersect(type.AsWord32(), refinement.AsWord32(),
                                 Type::ResolutionMode::kOverApproximate, zone);
  }

  void RefineTypesAfterBranch(const BranchOp* branch, bool then_branch) {
    Zone* zone = Asm().graph_zone();
    // Inspect branch condition.
    const Operation& condition = Asm().output_graph().Get(branch->condition());
    if (const ComparisonOp* comparison = condition.TryCast<ComparisonOp>()) {
      Type lhs = GetType(comparison->left());
      Type rhs = GetType(comparison->right());
      // If we don't have proper types, there is nothing we can do.
      if (lhs.IsInvalid() || rhs.IsInvalid()) return;

      // TODO(nicohartmann@): Might get rid of this once everything is properly
      // typed.
      if (lhs.IsAny() || rhs.IsAny()) return;
      DCHECK(!lhs.IsNone());
      DCHECK(!rhs.IsNone());

      const bool is_signed = ComparisonOp::IsSigned(comparison->kind);
      const bool is_less_than = ComparisonOp::IsLessThan(comparison->kind);
      Type l_refined;
      Type r_refined;

      switch (comparison->rep.value()) {
        case RegisterRepresentation::Word32(): {
          if (is_signed) {
            // TODO(nicohartmann@): Support signed comparison.
            return;
          }
          Word32Type l = Typer::TruncateWord32Input(lhs, true, zone).AsWord32();
          Word32Type r = Typer::TruncateWord32Input(rhs, true, zone).AsWord32();
          Type l_restrict, r_restrict;
          using OpTyper = WordOperationTyper<32>;
          if (is_less_than) {
            std::tie(l_restrict, r_restrict) =
                then_branch
                    ? OpTyper::RestrictionForUnsignedLessThan_True(l, r, zone)
                    : OpTyper ::RestrictionForUnsignedLessThan_False(l, r,
                                                                     zone);
          } else {
            std::tie(l_restrict, r_restrict) =
                then_branch
                    ? OpTyper::RestrictionForUnsignedLessThanOrEqual_True(l, r,
                                                                          zone)
                    : OpTyper::RestrictionForUnsignedLessThanOrEqual_False(
                          l, r, zone);
          }

          // Special handling for word32 restriction, because the inputs might
          // have been truncated from word64 implicitly.
          l_refined = RefineWord32Type<true>(lhs, l_restrict, zone);
          r_refined = RefineWord32Type<true>(rhs, r_restrict, zone);
          break;
        }
        case RegisterRepresentation::Float64(): {
          Float64Type l = lhs.AsFloat64();
          Float64Type r = rhs.AsFloat64();
          Type l_restrict, r_restrict;
          using OpTyper = FloatOperationTyper<64>;
          if (is_less_than) {
            std::tie(l_restrict, r_restrict) =
                then_branch ? OpTyper::RestrictionForLessThan_True(l, r, zone)
                            : OpTyper::RestrictionForLessThan_False(l, r, zone);
          } else {
            std::tie(l_restrict, r_restrict) =
                then_branch
                    ? OpTyper::RestrictionForLessThanOrEqual_True(l, r, zone)
                    : OpTyper::RestrictionForLessThanOrEqual_False(l, r, zone);
          }

          l_refined =
              l_restrict.IsNone()
                  ? Type::None()
                  : Float64Type::Intersect(l, l_restrict.AsFloat64(), zone);
          r_refined =
              l_restrict.IsNone()
                  ? Type::None()
                  : Float64Type::Intersect(r, r_restrict.AsFloat64(), zone);
          break;
        }
        default:
          return;
      }

      // TODO(nicohartmann@):
      // DCHECK(l_refined.IsSubtypeOf(lhs));
      // DCHECK(r_refined.IsSubtypeOf(rhs));
      const std::string branch_str = branch->ToString().substr(0, 40);
      USE(branch_str);
      TRACE_TYPING("\033[32mBr   %3d:%-40s\033[0m\n",
                   Asm().output_graph().Index(*branch).id(),
                   branch_str.c_str());
      RefineOperationType(comparison->left(), l_refined,
                          then_branch ? 'T' : 'F');
      RefineOperationType(comparison->right(), r_refined,
                          then_branch ? 'T' : 'F');
    }
  }

  void RefineOperationType(OpIndex op, const Type& type,
                           char case_for_tracing) {
    DCHECK(op.valid());
    DCHECK(!type.IsInvalid());

    TRACE_TYPING("\033[32m  %c: %3d:%-40s ~~> %s\033[0m\n", case_for_tracing,
                 op.id(),
                 Asm().output_graph().Get(op).ToString().substr(0, 40).c_str(),
                 type.ToString().c_str());

    auto key_opt = op_to_key_mapping_[op];
    DCHECK(key_opt.has_value());
    table_.Set(*key_opt, type);

    // TODO(nicohartmann@): One could push the refined type deeper into the
    // operations.
  }

  Type TypeForRepresentation(RegisterRepresentation rep) {
    switch (rep.value()) {
      case RegisterRepresentation::Word32():
        return Word32Type::Any();
      case RegisterRepresentation::Word64():
        return Word64Type::Any();
      case RegisterRepresentation::Float32():
        return Float32Type::Any();
      case RegisterRepresentation::Float64():
        return Float64Type::Any();

      case RegisterRepresentation::Tagged():
      case RegisterRepresentation::Compressed():
        // TODO(nicohartmann@): Support these representations.
        return Type::Any();
    }
  }

  OpIndex ReducePhi(base::Vector<const OpIndex> inputs,
                    RegisterRepresentation rep) {
    OpIndex index = Next::ReducePhi(inputs, rep);

    Type result_type = Type::None();
    for (const OpIndex input : inputs) {
      Type type = types_[input];
      if (type.IsInvalid()) {
        type = TypeForRepresentation(rep);
      }
      // TODO(nicohartmann@): Should all temporary types be in the
      // graph_zone()?
      result_type =
          Typer::LeastUpperBound(result_type, type, Asm().graph_zone());
    }

    SetType(index, result_type);
    return index;
  }

  OpIndex ReduceConstant(ConstantOp::Kind kind, ConstantOp::Storage value) {
    OpIndex index = Next::ReduceConstant(kind, value);
    if (!index.valid()) return index;

    Type type = Typer::TypeConstant(kind, value);
    SetType(index, type);
    return index;
  }

  OpIndex ReduceWordBinop(OpIndex left, OpIndex right, WordBinopOp::Kind kind,
                          WordRepresentation rep) {
    OpIndex index = Next::ReduceWordBinop(left, right, kind, rep);
    if (!index.valid()) return index;

    Type left_type = GetType(left);
    Type right_type = GetType(right);
    if (left_type.IsInvalid() || right_type.IsInvalid()) return index;

    Zone* zone = Asm().graph_zone();
    Type result_type = Type::Invalid();
    if (rep == WordRepresentation::Word32()) {
      switch (kind) {
        case WordBinopOp::Kind::kAdd:
          result_type = Typer::TypeWord32Add(left_type, right_type, zone);
          break;
        case WordBinopOp::Kind::kSub:
          result_type = Typer::TypeWord32Sub(left_type, right_type, zone);
          break;
        default:
          // TODO(nicohartmann@): Support remaining {kind}s.
          break;
      }
    } else {
      DCHECK_EQ(rep, WordRepresentation::Word64());
      switch (kind) {
        case WordBinopOp::Kind::kAdd:
          result_type = Typer::TypeWord64Add(left_type, right_type, zone);
          break;
        case WordBinopOp::Kind::kSub:
          result_type = Typer::TypeWord64Sub(left_type, right_type, zone);
          break;
        default:
          // TODO(nicohartmann@): Support remaining {kind}s.
          break;
      }
    }

    SetType(index, result_type);
    return index;
  }

  OpIndex ReduceFloatBinop(OpIndex left, OpIndex right, FloatBinopOp::Kind kind,
                           FloatRepresentation rep) {
    OpIndex index = Next::ReduceFloatBinop(left, right, kind, rep);
    if (!index.valid()) return index;

    Type result_type = Type::Invalid();
    Type left_type = GetType(left);
    Type right_type = GetType(right);
    Zone* zone = Asm().graph_zone();

    if (!left_type.IsInvalid() && !right_type.IsInvalid()) {
#define CASE(op, bits)                                                     \
  case FloatBinopOp::Kind::k##op:                                          \
    result_type = Typer::TypeFloat##bits##op(left_type, right_type, zone); \
    break;
      if (rep == FloatRepresentation::Float32()) {
        switch (kind) {
          CASE(Add, 32)
          CASE(Sub, 32)
          CASE(Mul, 32)
          CASE(Div, 32)
          CASE(Mod, 32)
          CASE(Min, 32)
          CASE(Max, 32)
          CASE(Power, 32)
          CASE(Atan2, 32)
        }
      } else {
        DCHECK_EQ(rep, FloatRepresentation::Float64());
        switch (kind) {
          CASE(Add, 64)
          CASE(Sub, 64)
          CASE(Mul, 64)
          CASE(Div, 64)
          CASE(Mod, 64)
          CASE(Min, 64)
          CASE(Max, 64)
          CASE(Power, 64)
          CASE(Atan2, 64)
        }
      }
#undef CASE
    }

    SetType(index, result_type);
    return index;
  }

  OpIndex ReduceCheckTurboshaftTypeOf(OpIndex input, RegisterRepresentation rep,
                                      Type type, bool successful) {
    Type input_type = GetType(input);
    if (input_type.IsSubtypeOf(type)) {
      OpIndex index = Next::ReduceCheckTurboshaftTypeOf(input, rep, type, true);
      TRACE_TYPING(
          "\033[32mCTOF %3d:%-40s\n  P: %3d:%-40s ~~> %s\033[0m\n", index.id(),
          Asm().output_graph().Get(index).ToString().substr(0, 40).c_str(),
          input.id(),
          Asm().output_graph().Get(input).ToString().substr(0, 40).c_str(),
          input_type.ToString().c_str());
      return index;
    }
    if (successful) {
      FATAL(
          "Checking type %s of operation %d:%s failed after it passed in a "
          "previous phase",
          type.ToString().c_str(), input.id(),
          Asm().output_graph().Get(input).ToString().c_str());
    }
    OpIndex index =
        Next::ReduceCheckTurboshaftTypeOf(input, rep, type, successful);
    TRACE_TYPING(
        "\033[31mCTOF %3d:%-40s\n  F: %3d:%-40s ~~> %s\033[0m\n", index.id(),
        Asm().output_graph().Get(index).ToString().substr(0, 40).c_str(),
        input.id(),
        Asm().output_graph().Get(input).ToString().substr(0, 40).c_str(),
        input_type.ToString().c_str());
    return index;
  }

  Type GetType(const OpIndex index) {
    if (auto key = op_to_key_mapping_[index]) return table_.Get(*key);
    return Type::Invalid();
  }

  void SetType(const OpIndex index, const Type& result_type) {
    if (!result_type.IsInvalid()) {
      if (auto key_opt = op_to_key_mapping_[index]) {
        table_.Set(*key_opt, result_type);
        // The new type we see might be unrelated to the existing type because
        // of value numbering. It may run the typer multiple times on the same
        // index if the last operation gets removed and the index reused.
        DCHECK(result_type.IsSubtypeOf(types_[index]));
        types_[index] = result_type;
        DCHECK(!types_[index].IsInvalid());
      } else {
        auto key = table_.NewKey(Type::None());
        table_.Set(key, result_type);
        types_[index] = result_type;
        op_to_key_mapping_[index] = key;
      }
    }

    TRACE_TYPING(
        "\033[%smType %3d:%-40s ==> %s\033[0m\n",
        (result_type.IsInvalid() ? "31" : "32"), index.id(),
        Asm().output_graph().Get(index).ToString().substr(0, 40).c_str(),
        (result_type.IsInvalid() ? "" : result_type.ToString().c_str()));
  }

#ifdef DEBUG
  void Verify(OpIndex input_index, OpIndex output_index) {
    DCHECK(input_index.valid());
    DCHECK(output_index.valid());

    const auto& input_type = Asm().input_graph().operation_types()[input_index];
    const auto& output_type = types_[output_index];

    if (input_type.IsInvalid()) return;
    DCHECK(!output_type.IsInvalid());

    const bool is_okay = output_type.IsSubtypeOf(input_type);

    TRACE_TYPING(
        "\033[%s %3d:%-40s %-40s\n     %3d:%-40s %-40s\033[0m\n",
        is_okay ? "32mOK  " : "31mFAIL", input_index.id(),
        Asm().input_graph().Get(input_index).ToString().substr(0, 40).c_str(),
        input_type.ToString().substr(0, 40).c_str(), output_index.id(),
        Asm().output_graph().Get(output_index).ToString().substr(0, 40).c_str(),
        output_type.ToString().substr(0, 40).c_str());

    if (V8_UNLIKELY(!is_okay)) {
      FATAL(
          "\033[%s %3d:%-40s %-40s\n     %3d:%-40s %-40s\033[0m\n",
          is_okay ? "32mOK  " : "31mFAIL", input_index.id(),
          Asm().input_graph().Get(input_index).ToString().substr(0, 40).c_str(),
          input_type.ToString().substr(0, 40).c_str(), output_index.id(),
          Asm()
              .output_graph()
              .Get(output_index)
              .ToString()
              .substr(0, 40)
              .c_str(),
          output_type.ToString().substr(0, 40).c_str());
    }
  }
#endif

  void RemoveLast(OpIndex index_of_last_operation) {
    if (auto key_opt = op_to_key_mapping_[index_of_last_operation]) {
      op_to_key_mapping_[index_of_last_operation] = base::nullopt;
      TRACE_TYPING(
          "\033[32mREM  %3d:%-40s %-40s\033[0m\n", index_of_last_operation.id(),
          Asm()
              .output_graph()
              .Get(index_of_last_operation)
              .ToString()
              .substr(0, 40)
              .c_str(),
          types_[index_of_last_operation].ToString().substr(0, 40).c_str());
      types_[index_of_last_operation] = Type::Invalid();
    } else {
      DCHECK(types_[index_of_last_operation].IsInvalid());
    }
    Next::RemoveLast(index_of_last_operation);
  }

 private:
  GrowingSidetable<Type>& types_;
  table_t table_;
  const Block* current_block_ = nullptr;
  GrowingSidetable<base::Optional<table_t::Key>> op_to_key_mapping_;
  GrowingBlockSidetable<base::Optional<table_t::Snapshot>>
      block_to_snapshot_mapping_;
  // {predecessors_} is used during merging, but we use an instance variable for
  // it, in order to save memory and not reallocate it for each merge.
  ZoneVector<table_t::Snapshot> predecessors_;
  Isolate* isolate_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_TYPE_INFERENCE_REDUCER_H_
