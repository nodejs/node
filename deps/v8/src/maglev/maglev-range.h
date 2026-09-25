// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_RANGE_H_
#define V8_MAGLEV_MAGLEV_RANGE_H_

#include <algorithm>
#include <cmath>

#include "src/common/globals.h"

namespace v8::internal::maglev {

inline constexpr bool IsInt64Representable(double value) {
  constexpr double min = -9223372036854775808.0;  // -2^63.
  // INT64_MAX (2^63 - 1) is not representable to double, but 2^63 is, so we
  // check if it is strictly below it.
  constexpr double max_bound = 9223372036854775808.0;  // 2^63.
  return value >= min && value < max_bound;
}

inline constexpr bool IsSafeInteger(int64_t value) {
  return value >= kMinSafeInteger && value <= kMaxSafeInteger;
}

inline constexpr bool IsSafeInteger(double value) {
  if (!std::isfinite(value)) return false;
  if (value != std::trunc(value)) return false;
  double max = static_cast<double>(kMaxSafeInteger);
  double min = static_cast<double>(kMinSafeInteger);
  return value >= min && value <= max;
}

class Range {
 public:
  static const int64_t kInfMin = INT64_MIN;
  static const int64_t kInfMax = INT64_MAX;

  constexpr Range(int64_t min, int64_t max) : min_(min), max_(max) {
    if (!is_empty()) {
      DCHECK_IMPLIES(min != kInfMin, IsSafeInteger(min_));
      DCHECK_IMPLIES(max != kInfMax, IsSafeInteger(max_));
      DCHECK_LE(min_, max_);
    }
  }

  explicit Range(int64_t value) : Range(value, value) {}

  std::optional<int64_t> min() const {
    if (min_ == kInfMin) return {};
    return min_;
  }

  std::optional<int64_t> max() const {
    if (max_ == kInfMax) return {};
    return max_;
  }

#define KNOWN_RANGES(V)                  \
  V(All, kInfMin, kInfMax)               \
  V(Empty, kInfMax, kInfMin)             \
  V(Smi, Smi::kMinValue, Smi::kMaxValue) \
  V(Int32, INT32_MIN, INT32_MAX)         \
  V(Uint32, 0, UINT32_MAX)               \
  V(SafeInt, kMinSafeInteger, kMaxSafeInteger)

#define DEF_RANGE(Name, Min, Max)                           \
  static constexpr Range Name() { return Range(Min, Max); } \
  bool Is##Name() const { return Name().contains(*this); }
  KNOWN_RANGES(DEF_RANGE)
#undef DEF_RANGE
#undef KNOWN_RANGES

  bool is_all() const { return min_ == kInfMin && max_ == kInfMax; }
  constexpr bool is_empty() const { return max_ < min_; }

  bool is_constant() const { return max_ == min_ && max_ != kInfMax; }

  bool contains(int64_t value) const { return min_ <= value && value <= max_; }

  bool contains(Range other) const {
    if (other.is_empty()) return false;
    if (is_empty()) return false;
    return min_ <= other.min_ && other.max_ <= max_;
  }

  bool overlaps(Range other) {
    if (is_empty() || other.is_empty()) return false;
    return min_ <= other.max_ && max_ >= other.min_;
  }

  bool operator==(const Range& other) const = default;

  bool operator<=(const Range& other) const { return max_ <= other.min_; }

  bool operator<(const Range& other) const { return max_ < other.min_; }

  bool operator>=(const Range& other) const { return min_ >= other.max_; }

  bool operator>(const Range& other) const { return min_ < other.max_; }

  static Range Union(Range r1, Range r2) {
    if (r1.is_empty()) return r2;
    if (r2.is_empty()) return r1;
    int64_t min = std::min(r1.min_, r2.min_);
    int64_t max = std::max(r1.max_, r2.max_);
    return Range(min, max);
  }

  static Range Intersect(Range r1, Range r2) {
    if (r1.is_empty() || r2.is_empty()) return Range::Empty();
    int64_t min = std::max(r1.min_, r2.min_);
    int64_t max = std::min(r1.max_, r2.max_);
    if (min <= max) return Range(min, max);
    return Range::Empty();
  }

  static Range Widen(Range range, Range new_range) {
    if (range.is_empty()) return new_range;
    if (new_range.is_empty()) return range;
    int64_t min = (new_range.min_ < range.min_) ? kInfMin : range.min_;
    int64_t max = (new_range.max_ > range.max_) ? kInfMax : range.max_;
    Range widened_range = Range(min, max);
    // For soundness, widen operation must be an over-approximation.
    DCHECK(widened_range.contains(Range::Union(range, new_range)));
    return widened_range;
  }

  static Range Negate(Range r) {
    if (r.is_empty()) return Range::Empty();
    int64_t min = r.max_ == kInfMax ? kInfMin : -r.max_;
    int64_t max = r.min_ == kInfMin ? kInfMax : -r.min_;
    return Range(min, max);
  }

  // [a, b] + [c, d] = [a+c, b+d]
  static Range Add(Range r1, Range r2) {
    if (r1.is_empty() || r2.is_empty()) return Range::Empty();
    int64_t min = kInfMin;
    if (r1.min_ != kInfMin && r2.min_ != kInfMin) {
      min = r1.min_ + r2.min_;
      if (!IsSafeInteger(min)) min = kInfMin;
    }
    int64_t max = kInfMax;
    if (r1.max_ != kInfMax && r2.max_ != kInfMax) {
      max = r1.max_ + r2.max_;
      if (!IsSafeInteger(max)) max = kInfMax;
    }
    return Range(min, max);
  }

  // [a, b] - [c, d] = [a-c, b-d]
  static Range Sub(Range r1, Range r2) {
    return Range::Add(r1, Range::Negate(r2));
  }

  // [a, b] * [c, d] = [min(ac,ad,bc,bd), max(ac,ad,bc,bd)]
  static Range Mul(Range r1, Range r2) {
    if (r1.is_empty() || r2.is_empty()) return Range::Empty();
    if (r1.is_all() || r2.is_all()) return Range::All();
    int64_t results[4];
    if (base::bits::SignedMulOverflow64(r1.min_, r2.min_, &results[0])) {
      return Range::All();
    }
    if (base::bits::SignedMulOverflow64(r1.min_, r2.max_, &results[1])) {
      return Range::All();
    }
    if (base::bits::SignedMulOverflow64(r1.max_, r2.min_, &results[2])) {
      return Range::All();
    }
    if (base::bits::SignedMulOverflow64(r1.max_, r2.max_, &results[3])) {
      return Range::All();
    }
    int64_t min = *std::ranges::min_element(results);
    int64_t max = *std::ranges::max_element(results);
    if (!IsSafeInteger(min)) min = kInfMin;
    if (!IsSafeInteger(max)) max = kInfMax;
    return Range(min, max);
  }

  static Range BitwiseAnd(Range r1, Range r2) {
    // TODO(victorgomes): This is copying OperationTyper::NumberBitwiseAnd. Not
    // sure if we need to force both sides to be int32s. I guess a safe int
    // would be enough.
    if (!r1.IsInt32() || !r2.IsInt32()) return Range::All();
    int64_t lmin = r1.min_;
    int64_t rmin = r2.min_;
    int64_t lmax = r1.max_;
    int64_t rmax = r2.max_;
    int64_t min = INT32_MIN;
    // And-ing any two values results in a value no larger than their maximum.
    // Even no larger than their minimum if both values are non-negative.
    int64_t max =
        lmin >= 0 && rmin >= 0 ? std::min(lmax, rmax) : std::max(lmax, rmax);
    // And-ing with a non-negative value x causes the result to be between
    // zero and x.
    if (lmin >= 0) {
      min = 0;
      max = std::min(max, lmax);
    }
    if (rmin >= 0) {
      min = 0;
      max = std::min(max, rmax);
    }
    return Range(min, max);
  }

  static Range BitwiseOr(Range r1, Range r2) {
    if (!r1.IsInt32() || !r2.IsInt32()) return Range::All();
    int64_t lmin = r1.min_;
    int64_t rmin = r2.min_;
    int64_t lmax = r1.max_;
    int64_t rmax = r2.max_;
    // Or-ing any two values results in a value no smaller than their minimum.
    // Even no smaller than their maximum if both values are non-negative.
    int64_t min =
        lmin >= 0 && rmin >= 0 ? std::max(lmin, rmin) : std::min(lmin, rmin);
    int64_t max = INT32_MAX;
    // Or-ing with 0 is essentially a conversion to int32.
    if (rmin == 0 && rmax == 0) {
      min = lmin;
      max = lmax;
    }
    if (lmin == 0 && lmax == 0) {
      min = rmin;
      max = rmax;
    }
    if (lmax < 0 || rmax < 0) {
      // Or-ing two values of which at least one is negative results in a
      // negative value.
      max = std::min<int64_t>(max, -1);
    }
    return Range(min, max);
  }

  static Range BitwiseXor(Range r1, Range r2) {
    // TODO(victorgomes): Improve/refine this case.
    if (!r1.IsInt32() || !r2.IsInt32()) return Range::All();
    int64_t lmin = r1.min_;
    int64_t rmin = r2.min_;
    int64_t lmax = r1.max_;
    int64_t rmax = r2.max_;
    if ((lmin >= 0 && rmin >= 0) || (lmax < 0 && rmax < 0)) {
      // Xor-ing negative or non-negative values results in a non-negative
      // value.
      return Range(0, INT32_MAX);
    }
    if ((lmax < 0 && rmin >= 0) || (lmin >= 0 && rmax < 0)) {
      // Xor-ing a negative and a non-negative value results in a negative
      // value.
      return Range(INT32_MIN, -1);
    }
    return Range::Int32();
  }

  static Range ShiftLeft(Range r1, Range r2) {
    if (!r1.IsInt32() || !r2.IsUint32()) return Range::All();
    int64_t min_lhs = r1.min_;
    int64_t max_lhs = r1.max_;
    int64_t min_rhs = r2.min_;
    int64_t max_rhs = r2.max_;
    DCHECK_GE(min_rhs, 0);
    if (max_rhs > 31) {
      // rhs can be larger than the bitmask
      max_rhs = 31;
      min_rhs = 0;
    }
    if (max_lhs > (INT32_MAX >> max_rhs) || min_lhs < (INT32_MIN >> max_rhs)) {
      // overflow possible
      return Range::All();
    }
    int64_t min = std::min(
        static_cast<int32_t>(static_cast<uint32_t>(min_lhs) << min_rhs),
        static_cast<int32_t>(static_cast<uint32_t>(min_lhs) << max_rhs));
    int64_t max = std::max(
        static_cast<int32_t>(static_cast<uint32_t>(max_lhs) << min_rhs),
        static_cast<int32_t>(static_cast<uint32_t>(max_lhs) << max_rhs));
    return Range(min, max);
  }

  static Range ShiftRight(Range r1, Range r2) {
    if (!r1.IsInt32() || !r2.IsUint32()) return Range::All();
    int64_t min_lhs = r1.min_;
    int64_t max_lhs = r1.max_;
    int64_t min_rhs = r2.min_;
    int64_t max_rhs = r2.max_;
    DCHECK_GE(min_rhs, 0);
    if (max_rhs > 31) {
      // rhs can be larger than the bitmask
      max_rhs = 31;
      min_rhs = 0;
    }
    int64_t min = std::min(min_lhs >> min_rhs, min_lhs >> max_rhs);
    int64_t max = std::max(max_lhs >> min_rhs, max_lhs >> max_rhs);
    return Range(min, max);
  }

  static Range ShiftRightLogical(Range r1, Range r2) {
    if (!r1.IsUint32() || !r2.IsUint32()) return Range::All();
    int64_t min_lhs = r1.min_;
    int64_t max_lhs = r1.max_;
    DCHECK_GE(min_lhs, 0);
    int64_t min_rhs = r2.min_;
    int64_t max_rhs = r2.max_;
    DCHECK_GE(min_rhs, 0);
    if (max_rhs > 31) {
      // rhs can be larger than the bitmask
      max_rhs = 31;
      min_rhs = 0;
    }
    int64_t min = min_lhs >> max_rhs;
    int64_t max = max_lhs >> min_rhs;
    DCHECK_LE(0, min);
    DCHECK_LE(max, UINT32_MAX);
    return Range(min, max);
  }

  static Range ConstrainLessThan(Range lhs, Range rhs) {
    if (lhs.is_empty() || rhs.is_empty()) {
      return Range::Empty();
    }
    if (rhs.max_ == kInfMax) return lhs;
    if (rhs.max_ == kMinSafeInteger) {
      // TODO(victorgomes): The range should actually be then be greater than
      // [kInfMin, kMinSafeInteger].
      return Range::Empty();
    }
    if (lhs.min_ >= rhs.max_) return lhs;
    lhs.max_ = std::min(lhs.max_, rhs.max_ - 1);
    DCHECK_LE(lhs.min_, lhs.max_);
    return lhs;
  }

  static Range ConstrainLessThanOrEqual(Range lhs, Range rhs) {
    if (lhs.is_empty() || rhs.is_empty()) {
      return Range::Empty();
    }
    if (rhs.is_all()) return lhs;
    if (lhs.min_ >= rhs.max_) return lhs;
    lhs.max_ = std::min(lhs.max_, rhs.max_);
    DCHECK_LE(lhs.min_, lhs.max_);
    return lhs;
  }

  static Range ConstrainGreaterThan(Range lhs, Range rhs) {
    if (lhs.is_empty() || rhs.is_empty()) {
      return Range::Empty();
    }
    if (rhs.min_ == kInfMin) return lhs;
    if (rhs.min_ == kMaxSafeInteger) {
      // TODO(victorgomes): The range should actually be then be greater than
      // [kMaxSafeInteger, kInfMax].
      return Range::Empty();
    }
    if (lhs.max_ <= rhs.min_) return lhs;
    lhs.min_ = std::max(lhs.min_, rhs.min_ + 1);
    DCHECK_LE(lhs.min_, lhs.max_);
    return lhs;
  }

  static Range ConstrainGreaterThanOrEqual(Range lhs, Range rhs) {
    if (lhs.is_empty() || rhs.is_empty()) {
      return Range::Empty();
    }
    if (rhs.is_all()) return lhs;
    if (lhs.max_ <= rhs.min_) return lhs;
    lhs.min_ = std::max(lhs.min_, rhs.min_);
    DCHECK_LE(lhs.min_, lhs.max_);
    return lhs;
  }

  friend std::ostream& operator<<(std::ostream& os, const Range& r) {
    if (r.is_empty()) {
      os << "[]";
      return os;
    }
    os << "[";
    if (r.min_ == kInfMin) {
      os << "-∞";
    } else {
      os << r.min_;
    }
    os << ", ";
    if (r.max_ == kInfMax) {
      os << "+∞";
    } else {
      os << r.max_;
    }
    os << "]";
    return os;
  }

 private:
  // These values are either in the safe integer range or they are kInfMin
  // and/or kInfMax.
  int64_t min_;
  int64_t max_;
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_RANGE_H_
