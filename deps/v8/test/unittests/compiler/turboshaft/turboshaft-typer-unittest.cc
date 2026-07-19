// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/common/globals.h"
#include "src/compiler/turboshaft/typer.h"
#include "src/handles/handles.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler::turboshaft {

template <typename T>
class WordTyperTest : public TestWithNativeContextAndZone {
 public:
  using word_t = typename T::word_t;
  static constexpr size_t Bits = sizeof(word_t) * kBitsPerByte;

  WordTyperTest() : TestWithNativeContextAndZone() {}
};

template <typename T>
class FloatTyperTest : public TestWithNativeContextAndZone {
 public:
  using float_t = typename T::float_t;
  static constexpr size_t Bits = sizeof(float_t) * kBitsPerByte;

  FloatTyperTest() : TestWithNativeContextAndZone() {}
};

template <typename T>
struct Slices {
  Slices(std::initializer_list<T> slices) : slices(slices) {}

  std::vector<T> slices;
};
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const Slices<T>& slices) {
  os << "Slices{";
  for (const auto& s : slices.slices) os << s << ", ";
  return os << "}";
}

// We define operator<= here for Type so that we can use gtest's EXPECT_LE to
// check for subtyping and have the default printing.
inline bool operator<=(const Type& lhs, const Type& rhs) {
  return lhs.IsSubtypeOf(rhs);
}
template <typename T>
inline bool operator<=(const Slices<T>& lhs, const T& rhs) {
  for (const auto& s : lhs.slices) {
    if (!s.IsSubtypeOf(rhs)) return false;
  }
  return true;
}

using WordTypes = ::testing::Types<Word32Type, Word64Type>;
TYPED_TEST_SUITE(WordTyperTest, WordTypes);

#define DEFINE_TEST_HELPERS()                                                 \
  using T = TypeParam;                                                        \
  using word_t = typename TestFixture::word_t;                                \
  using Slices = Slices<T>;                                                   \
  constexpr word_t max = std::numeric_limits<word_t>::max();                  \
  auto Constant = [&](word_t value) { return T::Constant(value); };           \
  auto Set = [&](std::initializer_list<word_t> elements) {                    \
    return WordOperationTyper<TestFixture::Bits>::FromElements(elements,      \
                                                               this->zone()); \
  };                                                                          \
  auto Range = [&](word_t from, word_t to) {                                  \
    return T::Range(from, to, this->zone());                                  \
  };                                                                          \
  USE(Slices{}, Constant, Set, Range);

TYPED_TEST(WordTyperTest, Add) {
  DEFINE_TEST_HELPERS()
#define EXPECT_ADD(lhs, rhs, result)                                           \
  EXPECT_LE(result, WordOperationTyper<TestFixture::Bits>::Add(lhs, rhs,       \
                                                               this->zone())); \
  EXPECT_LE(result, WordOperationTyper<TestFixture::Bits>::Add(rhs, lhs,       \
                                                               this->zone()))

  // Adding any.
  {
    // Any + Any
    EXPECT_ADD(T::Any(), T::Any(), T::Any());
    // c + Any
    EXPECT_ADD(Constant(42), T::Any(), T::Any());
    // {x1, ..., xn} + Any
    EXPECT_ADD(Set({8, 11, 922}), T::Any(), T::Any());
    // [a, b] + Any
    EXPECT_ADD(Range(800, 1020), T::Any(), T::Any());
  }

  // Adding constants.
  {
    // c' + c
    EXPECT_ADD(Constant(8), Constant(10003), Constant(8 + 10003));
    EXPECT_ADD(Constant(max), Constant(0), Constant(max));
    EXPECT_ADD(Constant(max - 8), Constant(12), Constant(3));
    EXPECT_ADD(Constant(max), Constant(max), Constant(max - 1));
    // {x1, ..., xn} + c
    auto set1 = Set({0, 87});
    EXPECT_ADD(set1, Constant(0), set1);
    EXPECT_ADD(set1, Constant(2005), Set({2005, 2092}));
    EXPECT_ADD(set1, Constant(max - 4), Set({82, max - 4}));
    EXPECT_ADD(set1, Constant(max), Set({86, max}));
    auto set2 = Set({15, 25025, max - 99});
    EXPECT_ADD(set2, Constant(0), set2);
    EXPECT_ADD(set2, Constant(4), Set({19, 25029, max - 95}));
    EXPECT_ADD(set2, Constant(max - 50), Set({24974, max - 150, max - 35}));
    EXPECT_ADD(set2, Constant(max), Set({14, 25024, max - 100}));
    // [a, b](non-wrapping) + c
    auto range1 = Range(13, 288);
    EXPECT_ADD(range1, Constant(0), range1);
    EXPECT_ADD(range1, Constant(812), Range(825, 1100));
    EXPECT_ADD(range1, Constant(max - 103), Range(max - 90, 184));
    EXPECT_ADD(range1, Constant(max - 5), Range(7, 282));
    EXPECT_ADD(range1, Constant(max), Range(12, 287));
    // [a, b](wrapping) + c
    auto range2 = Range(max - 100, 70);
    EXPECT_ADD(range2, Constant(0), range2);
    EXPECT_ADD(range2, Constant(14), Range(max - 86, 84));
    EXPECT_ADD(range2, Constant(101), Range(0, 171));
    EXPECT_ADD(range2, Constant(200), Range(99, 270));
    EXPECT_ADD(range2, Constant(max), Range(max - 101, 69));
  }

  // Adding sets.
  {
    // {y1, ..., ym} + {x1, ..., xn}
    auto set1 = Set({0, 87});
    EXPECT_ADD(set1, set1, Set({0, 87, (87 + 87)}));
    EXPECT_ADD(set1, Set({3, 4, 5}), Set({3, 4, 5, 90, 91}));
    EXPECT_ADD(set1, Set({3, 7, 11, 114}),
               Set({3, 7, 11, 90, 94, 98, 114, 201}));
    EXPECT_ADD(set1, Set({0, 1, 87, 200, max}),
               Set({0, 1, 86, 87, 88, 174, 200, 287, max}));
    EXPECT_ADD(set1, Set({max - 86, max - 9, max}),
               Set({0, 77, 86, max - 86, max - 9, max}));
    // [a, b](non-wrapping) + {x1, ..., xn}
    auto range1 = Range(400, 991);
    EXPECT_ADD(range1, Set({0, 55}), Range(400, 1046));
    EXPECT_ADD(range1, Set({49, 110, 100009}), Range(449, 101000));
    EXPECT_ADD(
        range1, Set({112, max - 10094, max - 950}),
        Slices({Range(0, 40), Range(512, 1103), Range(max - 9694, max)}));
    EXPECT_ADD(range1, Set({112, max - 850}),
               Slices({Range(512, 1103), Range(max - 450, 140)}));
    EXPECT_ADD(range1, Set({max - 3, max - 1, max}), Range(396, 990));
    // [a,b](wrapping) + {x1, ..., xn}
    auto range2 = Range(max - 30, 82);
    EXPECT_ADD(range2, Set({0, 20}),
               Slices({Range(max - 30, 82), Range(max - 10, 102)}));
    EXPECT_ADD(range2, Set({20, 30, 32, max}),
               Slices({Range(max - 10, 101), Range(0, 112), Range(1, 114),
                       Range(max - 31, 81)}));
    EXPECT_ADD(range2, Set({1000, 2000}),
               Slices({Range(969, 1082), Range(1969, 2082)}));
    EXPECT_ADD(range2, Set({max - 8, max - 2}),
               Slices({Range(max - 39, 73), Range(max - 33, 79)}));
  }

  // Adding ranges.
  {
    // [a, b](non-wrapping) + [c, d](non-wrapping)
    auto range1 = Range(30, 990);
    EXPECT_ADD(range1, Range(0, 2), Range(30, 992));
    EXPECT_ADD(range1, Range(1000, 22000), Range(1030, 22990));
    EXPECT_ADD(range1, Range(0, max - 1000), Range(30, max - 10));
    EXPECT_ADD(range1, Range(max - 800, max - 700), Range(max - 770, 289));
    EXPECT_ADD(range1, Range(max - 5, max), Range(24, 989));
    // [a, b](wrapping) + [c, d](non-wrapping)
    auto range2 = Range(max - 40, 40);
    EXPECT_ADD(range2, Range(0, 8), Range(max - 40, 48));
    EXPECT_ADD(range2, Range(2000, 90000), Range(1959, 90040));
    EXPECT_ADD(range2, Range(max - 400, max - 200),
               Range(max - 441, max - 160));
    EXPECT_ADD(range2, Range(0, max - 82), Range(max - 40, max - 42));
    EXPECT_ADD(range2, Range(0, max - 81), T::Any());
    EXPECT_ADD(range2, Range(20, max - 20), T::Any());
    // [a, b](wrapping) + [c, d](wrapping)
    EXPECT_ADD(range2, range2, Range(max - 81, 80));
    EXPECT_ADD(range2, Range(max - 2, 2), Range(max - 43, 42));
    EXPECT_ADD(range2, Range(1000, 100), Range(959, 140));
  }

#undef EXPECT_ADD
}

TYPED_TEST(WordTyperTest, WidenExponential) {
  DEFINE_TEST_HELPERS()

  auto SizeOf = [&](const T& type) -> word_t {
    DCHECK(!type.is_any());
    if (type.is_set()) return type.set_size();
    if (type.is_wrapping()) {
      return type.range_to() + (max - type.range_from()) + word_t{2};
    }
    return type.range_to() - type.range_from() + word_t{1};
  };
  auto DoubledInSize = [&](const T& old_type, const T& new_type) {
    // If the `new_type` is any, we accept it.
    if (new_type.is_any()) return true;
    return SizeOf(old_type) <= 2 * SizeOf(new_type);
  };

#define EXPECT_WEXP(old_type, new_type)                                    \
  {                                                                        \
    const T ot = old_type;                                                 \
    const T nt = new_type;                                                 \
    auto result = WordOperationTyper<TestFixture::Bits>::WidenExponential( \
        ot, nt, this->zone());                                             \
    EXPECT_LE(ot, result);                                                 \
    EXPECT_LE(nt, result);                                                 \
    EXPECT_TRUE(DoubledInSize(ot, result));                                \
  }

  // c W set
  EXPECT_WEXP(Constant(0), Set({0, 1}));
  EXPECT_WEXP(Constant(0), Set({0, 3}));
  EXPECT_WEXP(Constant(0), Set({0, 1, max}));
  EXPECT_WEXP(Constant(0), Set({0, 1, 2, max - 2, max - 1, max}));
  EXPECT_WEXP(Constant(max), Set({0, 1, 2, max - 2, max}));
  // c W range
  EXPECT_WEXP(Constant(0), Range(0, 100));
  EXPECT_WEXP(Constant(100), Range(50, 100));
  EXPECT_WEXP(Constant(100), Range(50, 150));
  EXPECT_WEXP(Constant(0), Range(max - 10, 0));
  EXPECT_WEXP(Constant(0), Range(max - 10, 10));
  EXPECT_WEXP(Constant(50), Range(max - 10000, 100));
  EXPECT_WEXP(Constant(max), T::Any());
  // set W set
  EXPECT_WEXP(Set({0, 1}), Set({0, 1, 2}));
  EXPECT_WEXP(Set({0, 1}), Set({0, 1, 2, 3, 4}));
  EXPECT_WEXP(Set({0, max}), Set({0, 1, max}));
  EXPECT_WEXP(Set({8, max - 8}), Set({7, 8, max - 8, max - 7}));
  EXPECT_WEXP(Set({3, 5, 7, 11}), Set({2, 3, 5, 7, 11}));
  // set W range
  EXPECT_WEXP(Set({3, 5, 7, 11}), Range(3, 11));
  EXPECT_WEXP(Set({3, 5, 7, 11}), Range(0, 11));
  EXPECT_WEXP(Set({3, 5, 7, 11}), Range(3, 100));
  EXPECT_WEXP(Set({3, 5, 7, 11}), Range(max, 11));
  EXPECT_WEXP(Set({3, 5, 7, 11}), Range(max - 100, 100));
  EXPECT_WEXP(Set({3, 5, 7, 11}), T::Any());
  // range W range
  EXPECT_WEXP(Range(0, 20), Range(0, 21));
  EXPECT_WEXP(Range(0, 20), Range(0, 220));
  EXPECT_WEXP(Range(0, 20), Range(max, 20));
  EXPECT_WEXP(Range(0, 20), Range(max - 200, 20));
  EXPECT_WEXP(Range(0, 20), T::Any());
  EXPECT_WEXP(Range(max - 100, max - 80), Range(max - 101, max - 80));
  EXPECT_WEXP(Range(max - 100, max - 80), Range(max - 100, max - 79));
  EXPECT_WEXP(Range(max - 100, max - 80), Range(max - 101, max - 79));
  EXPECT_WEXP(Range(max - 100, max - 80), Range(max - 200, 20));
  EXPECT_WEXP(Range(max - 100, max - 80), T::Any());
  EXPECT_WEXP(Range(max - 20, 0), Range(max - 20, 1));
  EXPECT_WEXP(Range(max - 20, 20), Range(max - 20, 21));
  EXPECT_WEXP(Range(max - 20, 20), Range(max - 21, 20));
  EXPECT_WEXP(Range(max - 20, 20), Range(max - 21, 21));
  EXPECT_WEXP(Range(max - 20, 20), Range(max - 2000, 2000));
  EXPECT_WEXP(Range(max - 20, 20), T::Any());

#undef EXPECT_WEXP
}

#undef DEFINE_TEST_HELPERS

using FloatTypes = ::testing::Types<Float32Type, Float64Type>;
TYPED_TEST_SUITE(FloatTyperTest, FloatTypes);

#define DEFINE_TEST_HELPERS()                                               \
  using T = TypeParam;                                                      \
  using float_t = typename TestFixture::float_t;                            \
  using Slices = Slices<T>;                                                 \
  auto Constant = [&](float_t value) { return T::Constant(value); };        \
  auto Set = [&](std::initializer_list<float_t> elements,                   \
                 uint32_t special_values = 0) {                             \
    return T::Set(elements, special_values, this->zone());                  \
  };                                                                        \
  auto Range = [&](float_t from, float_t to, uint32_t special_values = 0) { \
    return T::Range(from, to, special_values, this->zone());                \
  };                                                                        \
  constexpr uint32_t kNaN = T::kNaN;                                        \
  constexpr uint32_t kMZ = T::kMinusZero;                                   \
  constexpr float_t nan = nan_v<TestFixture::Bits>;                         \
  constexpr float_t inf = std::numeric_limits<float_t>::infinity();         \
  USE(Slices{}, Constant, Set, Range);                                      \
  USE(kNaN, kMZ, nan, inf);

TYPED_TEST(FloatTyperTest, Divide) {
  DEFINE_TEST_HELPERS()
#define EXPECT_DIV(lhs, rhs, result)                                \
  EXPECT_LE(result, FloatOperationTyper<TestFixture::Bits>::Divide( \
                        lhs, rhs, this->zone()))

  // 0 / x
  EXPECT_DIV(Constant(0.0), T::Any(), Set({0}, kNaN | kMZ));
  EXPECT_DIV(T::MinusZero(), T::Any(), Set({0}, kNaN | kMZ));
  EXPECT_DIV(Constant(0.0), Range(0.001, inf), Constant(0));
  EXPECT_DIV(T::MinusZero(), Range(0.001, inf), T::MinusZero());
  EXPECT_DIV(Constant(0.0), Range(-inf, -0.001), T::MinusZero());
  EXPECT_DIV(T::MinusZero(), Range(-inf, -0.001), Constant(0));
  EXPECT_DIV(Set({0.0}, kMZ), Constant(3), Set({0}, kMZ));
  EXPECT_DIV(Set({0.0}), Set({-2.5, 0.0, 1.5}), Set({0.0}, kNaN | kMZ));
  EXPECT_DIV(Set({0.0}, kMZ), Set({-2.5, 0.0, 1.5}), Set({0.0}, kNaN | kMZ));
  EXPECT_DIV(Set({0.0}), Set({1.5}, kMZ), Set({0.0}, kNaN));
  EXPECT_DIV(Set({0.0}, kMZ), Set({1.5}, kMZ), Set({0.0}, kNaN | kMZ));

  // x / 0
  EXPECT_DIV(Constant(1.0), Constant(0), Constant(inf));
  EXPECT_DIV(Constant(1.0), T::MinusZero(), Constant(-inf));
  EXPECT_DIV(Constant(inf), Constant(0), Constant(inf));
  EXPECT_DIV(Constant(inf), T::MinusZero(), Constant(-inf));
  EXPECT_DIV(Constant(-1.0), Constant(0), Constant(-inf));
  EXPECT_DIV(Constant(-1.0), T::MinusZero(), Constant(inf));
  EXPECT_DIV(Constant(-inf), Constant(0), Constant(-inf));
  EXPECT_DIV(Constant(-inf), T::MinusZero(), Constant(inf));
  EXPECT_DIV(Constant(1.5), Set({0.0}, kMZ), Set({-inf, inf}));
  EXPECT_DIV(Constant(-1.5), Set({0.0}, kMZ), Set({-inf, inf}));
  EXPECT_DIV(Set({1.5}, kMZ), Set({0.0}, kMZ), Set({-inf, inf}, kNaN));
  EXPECT_DIV(Set({-1.5}, kMZ), Set({0.0}, kMZ), Set({-inf, inf}, kNaN));

  // 0 / 0
  EXPECT_DIV(Constant(0), Constant(0), T::NaN());
  EXPECT_DIV(Constant(0), T::MinusZero(), T::NaN());
  EXPECT_DIV(T::MinusZero(), Constant(0), T::NaN());
  EXPECT_DIV(T::MinusZero(), T::MinusZero(), T::NaN());
  EXPECT_DIV(Set({0}, kMZ), Set({1}, kMZ), Set({0}, kNaN | kMZ));

  // inf / inf
  EXPECT_DIV(Constant(inf), Constant(inf), T::NaN());
  EXPECT_DIV(Constant(inf), Constant(-inf), T::NaN());
  EXPECT_DIV(Constant(-inf), Constant(inf), T::NaN());
  EXPECT_DIV(Constant(-inf), Constant(-inf), T::NaN());
  EXPECT_DIV(Set({-inf, inf}), Constant(inf), T::NaN());
  EXPECT_DIV(Set({-inf, inf}), Constant(-inf), T::NaN());
  EXPECT_DIV(Set({-inf, inf}), Set({-inf, inf}), T::NaN());
}

#undef DEFINE_TEST_HELPERS

}  // namespace v8::internal::compiler::turboshaft
