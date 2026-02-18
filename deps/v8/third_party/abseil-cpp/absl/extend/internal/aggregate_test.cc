// Copyright 2026 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/extend/internal/aggregate.h"

#include <tuple>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace aggregate_internal {
namespace {

TEST(Qualifiers, Collapse) {
  EXPECT_EQ(kRef | kRefRef, kRef);
  EXPECT_NE(kConst, kVolatile);
  EXPECT_NE(kConst, kRef);
  EXPECT_NE(kConst, kRefRef);
  EXPECT_NE(kVolatile, kRef);
  EXPECT_NE(kVolatile, kRefRef);
  EXPECT_NE(kRef, kRefRef);
}

TEST(ExtractQualifiers, Works) {
  EXPECT_EQ(ExtractQualifiers<int>(), 0);
  EXPECT_EQ(ExtractQualifiers<int&>(), kRef);
  EXPECT_EQ(ExtractQualifiers<int&&>(), kRefRef);
  EXPECT_EQ(ExtractQualifiers<const int>(), kConst);
  EXPECT_EQ(ExtractQualifiers<const int&>(), kConst | kRef);
  EXPECT_EQ(ExtractQualifiers<const int&&>(), kConst | kRefRef);
  EXPECT_EQ(ExtractQualifiers<volatile int>(), kVolatile);
  EXPECT_EQ(ExtractQualifiers<volatile int&>(), kVolatile | kRef);
  EXPECT_EQ(ExtractQualifiers<volatile int&&>(), kVolatile | kRefRef);
  EXPECT_EQ(ExtractQualifiers<volatile const int>(), kVolatile | kConst);
  EXPECT_EQ(ExtractQualifiers<volatile const int&>(),
            kVolatile | kConst | kRef);
  EXPECT_EQ(ExtractQualifiers<volatile const int&&>(),
            kVolatile | kConst | kRefRef);
}

TEST(ApplyQualifiers, OnBareType) {
  EXPECT_TRUE((std::is_same_v<typename ApplyQualifiers<int, 0>::type, int>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<int, kRef>::type, int&>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<int, kRefRef>::type, int&&>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<int, kConst>::type, const int>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<int, kConst | kRef>::type,
                      const int&>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<int, kConst | kRefRef>::type,
                      const int&&>));
  EXPECT_TRUE((std::is_same_v<typename ApplyQualifiers<int, kVolatile>::type,
                              volatile int>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<int, kVolatile | kRef>::type,
                      volatile int&>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<int, kVolatile | kRefRef>::type,
                      volatile int&&>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<int, kVolatile | kConst>::type,
                      volatile const int>));
  EXPECT_TRUE((std::is_same_v<
               typename ApplyQualifiers<int, kVolatile | kConst | kRef>::type,
               volatile const int&>));
  EXPECT_TRUE(
      (std::is_same_v<
          typename ApplyQualifiers<int, kVolatile | kConst | kRefRef>::type,
          volatile const int&&>));
}

TEST(ApplyQualifiers, OnConstType) {
  EXPECT_TRUE((
      std::is_same_v<typename ApplyQualifiers<const int, 0>::type, const int>));
  EXPECT_TRUE((std::is_same_v<typename ApplyQualifiers<const int, kRef>::type,
                              const int&>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<const int, kRefRef>::type,
                      const int&&>));
  EXPECT_TRUE((std::is_same_v<typename ApplyQualifiers<const int, kConst>::type,
                              const int>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<const int, kConst | kRef>::type,
                      const int&>));
  EXPECT_TRUE((std::is_same_v<
               typename ApplyQualifiers<const int, kConst | kRefRef>::type,
               const int&&>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<const int, kVolatile>::type,
                      const volatile int>));
  EXPECT_TRUE((std::is_same_v<
               typename ApplyQualifiers<const int, kVolatile | kRef>::type,
               const volatile int&>));
  EXPECT_TRUE((std::is_same_v<
               typename ApplyQualifiers<const int, kVolatile | kRefRef>::type,
               const volatile int&&>));
  EXPECT_TRUE((std::is_same_v<
               typename ApplyQualifiers<const int, kVolatile | kConst>::type,
               volatile const int>));
  EXPECT_TRUE(
      (std::is_same_v<
          typename ApplyQualifiers<const int, kVolatile | kConst | kRef>::type,
          volatile const int&>));
  EXPECT_TRUE(
      (std::is_same_v<typename ApplyQualifiers<const int, kVolatile | kConst |
                                                              kRefRef>::type,
                      volatile const int&&>));
}

TEST(CorrectQualifiers, Type) {
  struct A {
    int val;
    int&& rval;
    const int cval;
    const int& clval;
    const int&& crval;
  };

  A a{
      3,                                // .val
      static_cast<int&&>(a.val),        // .rval
      3,                                // .cval
      a.cval,                           // .clval
      static_cast<const int&&>(a.cval)  // .crval
  };
  auto&& [v, r, c, cl, cr] = a;
  testing::StaticAssertTypeEq<decltype(CorrectQualifiers<A, decltype(v)>(v)),
                              int&&>();
  testing::StaticAssertTypeEq<decltype(CorrectQualifiers<A, decltype(r)>(r)),
                              int&&>();
  testing::StaticAssertTypeEq<decltype(CorrectQualifiers<A, decltype(c)>(c)),
                              const int&&>();
  testing::StaticAssertTypeEq<decltype(CorrectQualifiers<A, decltype(cl)>(cl)),
                              const int&>();

  testing::StaticAssertTypeEq<decltype(CorrectQualifiers<A&, decltype(v)>(v)),
                              int&>();
  testing::StaticAssertTypeEq<decltype(CorrectQualifiers<A&, decltype(r)>(r)),
                              int&&>();
  testing::StaticAssertTypeEq<decltype(CorrectQualifiers<A&, decltype(c)>(c)),
                              const int&>();
  testing::StaticAssertTypeEq<decltype(CorrectQualifiers<A&, decltype(cl)>(cl)),
                              const int&>();

  testing::StaticAssertTypeEq<
      decltype(CorrectQualifiers<const A&, decltype(v)>(v)), const int&>();
  testing::StaticAssertTypeEq<
      decltype(CorrectQualifiers<const A&, decltype(r)>(r)), int&&>();
}

TEST(RemoveQualifiersAndReferencesFromTuple, Type) {
  EXPECT_TRUE(
      (std::is_same_v<RemoveQualifiersAndReferencesFromTuple<std::tuple<
                          int, int&, const int&, volatile const int&>>::type,
                      std::tuple<int, int, int, int>>));
}

TEST(Unpack, Basic) {
  struct A1 {
    int a;
  };
  struct A2 {
    int a;
    int b;
  };
  struct A3 {
    int a;
    int&& b;
  };

  {
    A1 a;
    auto [f1] = Unpack(a);
    EXPECT_EQ(&f1, &a.a);
  }

  {
    A2 a;
    auto [f1, f2] = Unpack(a);
    EXPECT_EQ(&f1, &a.a);
    EXPECT_EQ(&f2, &a.b);
  }

  {
    int b = 0;
    A3 a{0, std::move(b)};  // NOLINT(performance-move-const-arg)
    auto [f1, f2] = Unpack(a);
    EXPECT_EQ(&f1, &a.a);
    EXPECT_EQ(&f2, &a.b);
  }
}

TEST(Unpack, Types) {
  struct A3 {
    int field_v;
    const int& field_l;
    int&& field_r;
  };

  int x = 0, y = 0;
  A3 my_struct{0, x, std::move(y)};  // NOLINT(performance-move-const-arg)
  auto unpacked = Unpack(my_struct);
  const auto& const_lvalue_ref = my_struct;
  auto unpacked_const_lvalue_ref = Unpack(const_lvalue_ref);
  auto unpacked_rvalue_ref = Unpack(std::move(my_struct));
  testing::StaticAssertTypeEq<decltype(unpacked),
                              std::tuple<int&, const int&, int&&>>();
  testing::StaticAssertTypeEq<decltype(unpacked_const_lvalue_ref),
                              std::tuple<const int&, const int&, int&&>>();
  testing::StaticAssertTypeEq<decltype(unpacked_rvalue_ref),
                              std::tuple<int&&, const int&, int&&>>();
}

TEST(Unpack, Const) {
  struct A1 {
    const int a;
  };
  struct A2 {
    const int a;
    const int b;
  };
  struct A3 {
    const int a;
    const int& b;
    int&& c;
  };

  {
    A1 a{};
    auto [f1] = Unpack(a);
    EXPECT_EQ(&f1, &a.a);
  }

  {
    A2 a{};
    auto [f1, f2] = Unpack(a);
    EXPECT_EQ(&f1, &a.a);
    EXPECT_EQ(&f2, &a.b);
  }

  {
    int b = 0, c = 0;
    A3 a{0, b, std::move(c)};  // NOLINT(performance-move-const-arg)
    auto [f1, f2, f3] = Unpack(a);
    EXPECT_EQ(&f1, &a.a);
    EXPECT_EQ(&f2, &a.b);
    EXPECT_EQ(&f3, &a.c);
  }
}

TEST(Unpack, BaseClass) {
  struct Base {};
  struct A2 : Base {
    int a1, a2;
  };

  A2 a;
  auto [f1, f2] = Unpack(a);
  EXPECT_EQ(&f1, &a.a1);
  EXPECT_EQ(&f2, &a.a2);
}

// Not supported on older compilers due to mysterious compiler bug(s) that are
// somewhat difficult to track down. (See b/478243383.)
//
// TODO(b/479561657): Enable this unconditionally.
#if !defined(_MSC_VER) || _MSC_VER >= 1939
TEST(Unpack, Immovable) {
  struct Immovable {
    Immovable() = default;
    Immovable(Immovable&&) = delete;
  };
  struct A2 {
    const Immovable& a;
    Immovable&& b;
  };

  Immovable i1, i2;
  A2 a{i1, std::move(i2)};
  auto [f1, f2] = Unpack(a);
  EXPECT_EQ(&f1, &a.a);
  EXPECT_EQ(&f2, &a.b);
}
#endif  // _MSC_VER >= 1939

TEST(Unpack, Empty) {
  struct Empty {};
  EXPECT_TRUE((std::is_same_v<std::tuple<>, decltype(Unpack(Empty{}))>));
  Empty lvalue_empty;
  EXPECT_TRUE((std::is_same_v<std::tuple<>, decltype(Unpack(lvalue_empty))>));

  struct EmptyWithBase : Empty {};
  EXPECT_TRUE(
      (std::is_same_v<std::tuple<>, decltype(Unpack(EmptyWithBase{}))>));
}

TEST(Unpack, Autodetect) {
  struct NoBases {
    int i = 7;
  } no_bases;
  auto [i2] = Unpack(no_bases);
  EXPECT_EQ(i2, 7);

  struct Base {};
  struct OneBase : Base {
    int j = 17;
  } one_base;
  auto [j2] = Unpack(one_base);
  EXPECT_EQ(j2, 17);

  constexpr auto try_unpack =
      [](auto&& v) -> decltype(NumBases(std::forward<decltype(v)>(v))) {};

  struct Base2 {};
  struct TwoBases : Base, Base2 {
    int k;
  };
  EXPECT_FALSE((std::is_invocable_v<decltype(try_unpack), TwoBases>));
}

TEST(Unpack, FailsSubstitution) {
  struct Aggregate {
    int i;
  };
  struct NonAggregate {
    explicit NonAggregate(int) {}
    int i;
  };

  const auto unpack =
      [](auto&& v) -> decltype(Unpack(std::forward<decltype(v)>(v))) {};
  EXPECT_TRUE((std::is_invocable_v<decltype(unpack), Aggregate>));
  EXPECT_FALSE((std::is_invocable_v<decltype(unpack), NonAggregate>));
}

// Not supported on older compilers due to mysterious compiler bug(s) that are
// somewhat difficult to track down. (See b/478243383.)
//
// TODO(b/479561657): Enable this unconditionally.
#if !defined(_MSC_VER) || _MSC_VER >= 1939
template <int N, class T>
void CheckTupleElementsEqualToIndex(const T& t) {
  if constexpr (N > 0) {
    EXPECT_EQ(std::get<N - 1>(t), N - 1);
    CheckTupleElementsEqualToIndex<N - 1>(t);
  }
}

TEST(Unpack, CorrectOrder1) {
  struct S1 {
    int a0 = 0;
  };
  CheckTupleElementsEqualToIndex<1>(Unpack(S1()));
}

TEST(Unpack, CorrectOrder2) {
  struct S2 {
    int a0 = 0, a1 = 1;
  };
  CheckTupleElementsEqualToIndex<2>(Unpack(S2()));
}

TEST(Unpack, CorrectOrder3) {
  struct S3 {
    int a0 = 0, a1 = 1, a2 = 2;
  };
  CheckTupleElementsEqualToIndex<3>(Unpack(S3()));
}

TEST(Unpack, CorrectOrder4) {
  struct S4 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3;
  };
  CheckTupleElementsEqualToIndex<4>(Unpack(S4()));
}

TEST(Unpack, CorrectOrder5) {
  struct S5 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4;
  };
  CheckTupleElementsEqualToIndex<5>(Unpack(S5()));
}

TEST(Unpack, CorrectOrder6) {
  struct S6 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5;
  };
  CheckTupleElementsEqualToIndex<6>(Unpack(S6()));
}

TEST(Unpack, CorrectOrder7) {
  struct S7 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6;
  };
  CheckTupleElementsEqualToIndex<7>(Unpack(S7()));
}

TEST(Unpack, CorrectOrder8) {
  struct S8 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7;
  };
  CheckTupleElementsEqualToIndex<8>(Unpack(S8()));
}

TEST(Unpack, CorrectOrder9) {
  struct S9 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8;
  };
  CheckTupleElementsEqualToIndex<9>(Unpack(S9()));
}

TEST(Unpack, CorrectOrder10) {
  struct S10 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9;
  };
  CheckTupleElementsEqualToIndex<10>(Unpack(S10()));
}

TEST(Unpack, CorrectOrder11) {
  struct S11 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10;
  };
  CheckTupleElementsEqualToIndex<11>(Unpack(S11()));
}

TEST(Unpack, CorrectOrder12) {
  struct S12 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11;
  };
  CheckTupleElementsEqualToIndex<12>(Unpack(S12()));
}

TEST(Unpack, CorrectOrder13) {
  struct S13 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12;
  };
  CheckTupleElementsEqualToIndex<13>(Unpack(S13()));
}

TEST(Unpack, CorrectOrder14) {
  struct S14 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13;
  };
  CheckTupleElementsEqualToIndex<14>(Unpack(S14()));
}

TEST(Unpack, CorrectOrder15) {
  struct S15 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14;
  };
  CheckTupleElementsEqualToIndex<15>(Unpack(S15()));
}

TEST(Unpack, CorrectOrder16) {
  struct S16 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15;
  };
  CheckTupleElementsEqualToIndex<16>(Unpack(S16()));
}

TEST(Unpack, CorrectOrder17) {
  struct S17 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16;
  };
  CheckTupleElementsEqualToIndex<17>(Unpack(S17()));
}

TEST(Unpack, CorrectOrder18) {
  struct S18 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17;
  };
  CheckTupleElementsEqualToIndex<18>(Unpack(S18()));
}

TEST(Unpack, CorrectOrder19) {
  struct S19 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18;
  };
  CheckTupleElementsEqualToIndex<19>(Unpack(S19()));
}

TEST(Unpack, CorrectOrder20) {
  struct S20 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19;
  };
  CheckTupleElementsEqualToIndex<20>(Unpack(S20()));
}

TEST(Unpack, CorrectOrder21) {
  struct S21 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20;
  };
  CheckTupleElementsEqualToIndex<21>(Unpack(S21()));
}

TEST(Unpack, CorrectOrder22) {
  struct S22 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21;
  };
  CheckTupleElementsEqualToIndex<22>(Unpack(S22()));
}

TEST(Unpack, CorrectOrder23) {
  struct S23 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22;
  };
  CheckTupleElementsEqualToIndex<23>(Unpack(S23()));
}

TEST(Unpack, CorrectOrder24) {
  struct S24 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23;
  };
  CheckTupleElementsEqualToIndex<24>(Unpack(S24()));
}

TEST(Unpack, CorrectOrder25) {
  struct S25 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24;
  };
  CheckTupleElementsEqualToIndex<25>(Unpack(S25()));
}

TEST(Unpack, CorrectOrder26) {
  struct S26 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25;
  };
  CheckTupleElementsEqualToIndex<26>(Unpack(S26()));
}

TEST(Unpack, CorrectOrder27) {
  struct S27 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26;
  };
  CheckTupleElementsEqualToIndex<27>(Unpack(S27()));
}

TEST(Unpack, CorrectOrder28) {
  struct S28 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27;
  };
  CheckTupleElementsEqualToIndex<28>(Unpack(S28()));
}

TEST(Unpack, CorrectOrder29) {
  struct S29 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28;
  };
  CheckTupleElementsEqualToIndex<29>(Unpack(S29()));
}

TEST(Unpack, CorrectOrder30) {
  struct S30 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29;
  };
  CheckTupleElementsEqualToIndex<30>(Unpack(S30()));
}

TEST(Unpack, CorrectOrder31) {
  struct S31 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30;
  };
  CheckTupleElementsEqualToIndex<31>(Unpack(S31()));
}

TEST(Unpack, CorrectOrder32) {
  struct S32 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31;
  };
  CheckTupleElementsEqualToIndex<32>(Unpack(S32()));
}

#ifndef _MSC_VER  // MSVC constexpr evaluation hits a default limit
TEST(Unpack, CorrectOrder33) {
  struct S33 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32;
  };
  CheckTupleElementsEqualToIndex<33>(Unpack(S33()));
}

TEST(Unpack, CorrectOrder34) {
  struct S34 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33;
  };
  CheckTupleElementsEqualToIndex<34>(Unpack(S34()));
}

TEST(Unpack, CorrectOrder35) {
  struct S35 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34;
  };
  CheckTupleElementsEqualToIndex<35>(Unpack(S35()));
}

TEST(Unpack, CorrectOrder36) {
  struct S36 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35;
  };
  CheckTupleElementsEqualToIndex<36>(Unpack(S36()));
}

TEST(Unpack, CorrectOrder37) {
  struct S37 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36;
  };
  CheckTupleElementsEqualToIndex<37>(Unpack(S37()));
}

TEST(Unpack, CorrectOrder38) {
  struct S38 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37;
  };
  CheckTupleElementsEqualToIndex<38>(Unpack(S38()));
}

TEST(Unpack, CorrectOrder39) {
  struct S39 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38;
  };
  CheckTupleElementsEqualToIndex<39>(Unpack(S39()));
}

TEST(Unpack, CorrectOrder40) {
  struct S40 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39;
  };
  CheckTupleElementsEqualToIndex<40>(Unpack(S40()));
}

TEST(Unpack, CorrectOrder41) {
  struct S41 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40;
  };
  CheckTupleElementsEqualToIndex<41>(Unpack(S41()));
}

TEST(Unpack, CorrectOrder42) {
  struct S42 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41;
  };
  CheckTupleElementsEqualToIndex<42>(Unpack(S42()));
}

TEST(Unpack, CorrectOrder43) {
  struct S43 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41, a42 = 42;
  };
  CheckTupleElementsEqualToIndex<43>(Unpack(S43()));
}

TEST(Unpack, CorrectOrder44) {
  struct S44 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41, a42 = 42, a43 = 43;
  };
  CheckTupleElementsEqualToIndex<44>(Unpack(S44()));
}

TEST(Unpack, CorrectOrder45) {
  struct S45 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41, a42 = 42, a43 = 43,
        a44 = 44;
  };
  CheckTupleElementsEqualToIndex<45>(Unpack(S45()));
}

TEST(Unpack, CorrectOrder46) {
  struct S46 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41, a42 = 42, a43 = 43,
        a44 = 44, a45 = 45;
  };
  CheckTupleElementsEqualToIndex<46>(Unpack(S46()));
}

TEST(Unpack, CorrectOrder47) {
  struct S47 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41, a42 = 42, a43 = 43,
        a44 = 44, a45 = 45, a46 = 46;
  };
  CheckTupleElementsEqualToIndex<47>(Unpack(S47()));
}

TEST(Unpack, CorrectOrder48) {
  struct S48 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41, a42 = 42, a43 = 43,
        a44 = 44, a45 = 45, a46 = 46, a47 = 47;
  };
  CheckTupleElementsEqualToIndex<48>(Unpack(S48()));
}

TEST(Unpack, CorrectOrder49) {
  struct S49 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41, a42 = 42, a43 = 43,
        a44 = 44, a45 = 45, a46 = 46, a47 = 47, a48 = 48;
  };
  CheckTupleElementsEqualToIndex<49>(Unpack(S49()));
}

TEST(Unpack, CorrectOrder50) {
  struct S50 {
    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5, a6 = 6, a7 = 7, a8 = 8,
        a9 = 9, a10 = 10, a11 = 11, a12 = 12, a13 = 13, a14 = 14, a15 = 15,
        a16 = 16, a17 = 17, a18 = 18, a19 = 19, a20 = 20, a21 = 21, a22 = 22,
        a23 = 23, a24 = 24, a25 = 25, a26 = 26, a27 = 27, a28 = 28, a29 = 29,
        a30 = 30, a31 = 31, a32 = 32, a33 = 33, a34 = 34, a35 = 35, a36 = 36,
        a37 = 37, a38 = 38, a39 = 39, a40 = 40, a41 = 41, a42 = 42, a43 = 43,
        a44 = 44, a45 = 45, a46 = 46, a47 = 47, a48 = 48, a49 = 49;
  };
  CheckTupleElementsEqualToIndex<50>(Unpack(S50()));
}

// When adding support for more fields, add corresponding CorrectOrderNN test(s)
// here.
#endif  // _MSC_VER
#endif  // _MSC_VER >= 1939

}  // namespace
}  // namespace aggregate_internal
ABSL_NAMESPACE_END
}  // namespace absl
