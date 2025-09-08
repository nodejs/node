// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/meta/type_traits.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace {

using ::testing::StaticAssertTypeEq;

template <typename T>
using IsOwnerAndNotView =
    absl::conjunction<absl::type_traits_internal::IsOwner<T>,
                      absl::negation<absl::type_traits_internal::IsView<T>>>;

static_assert(IsOwnerAndNotView<std::vector<int>>::value,
              "vector is an owner, not a view");
static_assert(IsOwnerAndNotView<std::string>::value,
              "string is an owner, not a view");
static_assert(IsOwnerAndNotView<std::wstring>::value,
              "wstring is an owner, not a view");
static_assert(!IsOwnerAndNotView<std::string_view>::value,
              "string_view is a view, not an owner");
static_assert(!IsOwnerAndNotView<std::wstring_view>::value,
              "wstring_view is a view, not an owner");

template <class T, class U>
struct simple_pair {
  T first;
  U second;
};

struct Dummy {};

struct ReturnType {};
struct ConvertibleToReturnType {
  operator ReturnType() const;  // NOLINT
};

// Unique types used as parameter types for testing the detection idiom.
struct StructA {};
struct StructB {};
struct StructC {};

struct TypeWithBarFunction {
  template <class T,
            absl::enable_if_t<std::is_same<T&&, StructA&>::value, int> = 0>
  ReturnType bar(T&&, const StructB&, StructC&&) &&;  // NOLINT
};

struct TypeWithBarFunctionAndConvertibleReturnType {
  template <class T,
            absl::enable_if_t<std::is_same<T&&, StructA&>::value, int> = 0>
  ConvertibleToReturnType bar(T&&, const StructB&, StructC&&) &&;  // NOLINT
};

template <class Class, class... Ts>
using BarIsCallableImpl =
    decltype(std::declval<Class>().bar(std::declval<Ts>()...));

template <class Class, class... T>
using BarIsCallable =
    absl::type_traits_internal::is_detected<BarIsCallableImpl, Class, T...>;

// NOTE: Test of detail type_traits_internal::is_detected.
TEST(IsDetectedTest, BasicUsage) {
  EXPECT_TRUE((BarIsCallable<TypeWithBarFunction, StructA&, const StructB&,
                             StructC>::value));
  EXPECT_TRUE(
      (BarIsCallable<TypeWithBarFunction, StructA&, StructB&, StructC>::value));
  EXPECT_TRUE(
      (BarIsCallable<TypeWithBarFunction, StructA&, StructB, StructC>::value));

  EXPECT_FALSE((BarIsCallable<int, StructA&, const StructB&, StructC>::value));
  EXPECT_FALSE((BarIsCallable<TypeWithBarFunction&, StructA&, const StructB&,
                              StructC>::value));
  EXPECT_FALSE((BarIsCallable<TypeWithBarFunction, StructA, const StructB&,
                              StructC>::value));
}

TEST(VoidTTest, BasicUsage) {
  StaticAssertTypeEq<void, absl::void_t<Dummy>>();
  StaticAssertTypeEq<void, absl::void_t<Dummy, Dummy, Dummy>>();
}

TEST(TypeTraitsTest, TestRemoveCVRef) {
  EXPECT_TRUE(
      (std::is_same<typename absl::remove_cvref<int>::type, int>::value));
  EXPECT_TRUE(
      (std::is_same<typename absl::remove_cvref<int&>::type, int>::value));
  EXPECT_TRUE(
      (std::is_same<typename absl::remove_cvref<int&&>::type, int>::value));
  EXPECT_TRUE((
      std::is_same<typename absl::remove_cvref<const int&>::type, int>::value));
  EXPECT_TRUE(
      (std::is_same<typename absl::remove_cvref<int*>::type, int*>::value));
  // Does not remove const in this case.
  EXPECT_TRUE((std::is_same<typename absl::remove_cvref<const int*>::type,
                            const int*>::value));
  EXPECT_TRUE(
      (std::is_same<typename absl::remove_cvref<int[2]>::type, int[2]>::value));
  EXPECT_TRUE((std::is_same<typename absl::remove_cvref<int(&)[2]>::type,
                            int[2]>::value));
  EXPECT_TRUE((std::is_same<typename absl::remove_cvref<int(&&)[2]>::type,
                            int[2]>::value));
  EXPECT_TRUE((std::is_same<typename absl::remove_cvref<const int[2]>::type,
                            int[2]>::value));
  EXPECT_TRUE((std::is_same<typename absl::remove_cvref<const int(&)[2]>::type,
                            int[2]>::value));
  EXPECT_TRUE((std::is_same<typename absl::remove_cvref<const int(&&)[2]>::type,
                            int[2]>::value));
}

struct TypeA {};
struct TypeB {};
struct TypeC {};
struct TypeD {};

template <typename T>
struct Wrap {};

enum class TypeEnum { A, B, C, D };

struct GetTypeT {
  template <typename T,
            absl::enable_if_t<std::is_same<T, TypeA>::value, int> = 0>
  TypeEnum operator()(Wrap<T>) const {
    return TypeEnum::A;
  }

  template <typename T,
            absl::enable_if_t<std::is_same<T, TypeB>::value, int> = 0>
  TypeEnum operator()(Wrap<T>) const {
    return TypeEnum::B;
  }

  template <typename T,
            absl::enable_if_t<std::is_same<T, TypeC>::value, int> = 0>
  TypeEnum operator()(Wrap<T>) const {
    return TypeEnum::C;
  }

  // NOTE: TypeD is intentionally not handled
} constexpr GetType = {};

struct GetTypeExtT {
  template <typename T>
  absl::result_of_t<const GetTypeT&(T)> operator()(T&& arg) const {
    return GetType(std::forward<T>(arg));
  }

  TypeEnum operator()(Wrap<TypeD>) const { return TypeEnum::D; }
} constexpr GetTypeExt = {};

TEST(TypeTraitsTest, TestResultOf) {
  EXPECT_EQ(TypeEnum::A, GetTypeExt(Wrap<TypeA>()));
  EXPECT_EQ(TypeEnum::B, GetTypeExt(Wrap<TypeB>()));
  EXPECT_EQ(TypeEnum::C, GetTypeExt(Wrap<TypeC>()));
  EXPECT_EQ(TypeEnum::D, GetTypeExt(Wrap<TypeD>()));
}

namespace adl_namespace {

struct DeletedSwap {};

void swap(DeletedSwap&, DeletedSwap&) = delete;

struct SpecialNoexceptSwap {
  SpecialNoexceptSwap(SpecialNoexceptSwap&&) {}
  SpecialNoexceptSwap& operator=(SpecialNoexceptSwap&&) { return *this; }
  ~SpecialNoexceptSwap() = default;
};

void swap(SpecialNoexceptSwap&, SpecialNoexceptSwap&) noexcept {}

}  // namespace adl_namespace

TEST(TypeTraitsTest, IsSwappable) {
  using absl::type_traits_internal::IsSwappable;
  using absl::type_traits_internal::StdSwapIsUnconstrained;

  EXPECT_TRUE(IsSwappable<int>::value);

  struct S {};
  EXPECT_TRUE(IsSwappable<S>::value);

  struct NoConstruct {
    NoConstruct(NoConstruct&&) = delete;
    NoConstruct& operator=(NoConstruct&&) { return *this; }
    ~NoConstruct() = default;
  };

  EXPECT_EQ(IsSwappable<NoConstruct>::value, StdSwapIsUnconstrained::value);
  struct NoAssign {
    NoAssign(NoAssign&&) {}
    NoAssign& operator=(NoAssign&&) = delete;
    ~NoAssign() = default;
  };

  EXPECT_EQ(IsSwappable<NoAssign>::value, StdSwapIsUnconstrained::value);

  EXPECT_FALSE(IsSwappable<adl_namespace::DeletedSwap>::value);

  EXPECT_TRUE(IsSwappable<adl_namespace::SpecialNoexceptSwap>::value);
}

TEST(TypeTraitsTest, IsNothrowSwappable) {
  using absl::type_traits_internal::IsNothrowSwappable;
  using absl::type_traits_internal::StdSwapIsUnconstrained;

  EXPECT_TRUE(IsNothrowSwappable<int>::value);

  struct NonNoexceptMoves {
    NonNoexceptMoves(NonNoexceptMoves&&) {}
    NonNoexceptMoves& operator=(NonNoexceptMoves&&) { return *this; }
    ~NonNoexceptMoves() = default;
  };

  EXPECT_FALSE(IsNothrowSwappable<NonNoexceptMoves>::value);

  struct NoConstruct {
    NoConstruct(NoConstruct&&) = delete;
    NoConstruct& operator=(NoConstruct&&) { return *this; }
    ~NoConstruct() = default;
  };

  EXPECT_FALSE(IsNothrowSwappable<NoConstruct>::value);

  struct NoAssign {
    NoAssign(NoAssign&&) {}
    NoAssign& operator=(NoAssign&&) = delete;
    ~NoAssign() = default;
  };

  EXPECT_FALSE(IsNothrowSwappable<NoAssign>::value);

  EXPECT_FALSE(IsNothrowSwappable<adl_namespace::DeletedSwap>::value);

  EXPECT_TRUE(IsNothrowSwappable<adl_namespace::SpecialNoexceptSwap>::value);
}

TEST(TriviallyRelocatable, PrimitiveTypes) {
  static_assert(absl::is_trivially_relocatable<int>::value, "");
  static_assert(absl::is_trivially_relocatable<char>::value, "");
  static_assert(absl::is_trivially_relocatable<void*>::value, "");
}

// User-defined types can be trivially relocatable as long as they don't have a
// user-provided move constructor or destructor.
TEST(TriviallyRelocatable, UserDefinedTriviallyRelocatable) {
  struct S {
    int x;
    int y;
  };

  static_assert(absl::is_trivially_relocatable<S>::value, "");
}

// A user-provided move constructor disqualifies a type from being trivially
// relocatable.
TEST(TriviallyRelocatable, UserProvidedMoveConstructor) {
  struct S {
    S(S&&) {}  // NOLINT(modernize-use-equals-default)
  };

  static_assert(!absl::is_trivially_relocatable<S>::value, "");
}

// A user-provided copy constructor disqualifies a type from being trivially
// relocatable.
TEST(TriviallyRelocatable, UserProvidedCopyConstructor) {
  struct S {
    S(const S&) {}  // NOLINT(modernize-use-equals-default)
  };

  static_assert(!absl::is_trivially_relocatable<S>::value, "");
}

// A user-provided copy assignment operator disqualifies a type from
// being trivially relocatable.
TEST(TriviallyRelocatable, UserProvidedCopyAssignment) {
  struct S {
    S(const S&) = default;
    S& operator=(const S&) {  // NOLINT(modernize-use-equals-default)
      return *this;
    }
  };

  static_assert(!absl::is_trivially_relocatable<S>::value, "");
}

// A user-provided move assignment operator disqualifies a type from
// being trivially relocatable.
TEST(TriviallyRelocatable, UserProvidedMoveAssignment) {
  struct S {
    S(S&&) = default;
    S& operator=(S&&) { return *this; }  // NOLINT(modernize-use-equals-default)
  };

  static_assert(!absl::is_trivially_relocatable<S>::value, "");
}

// A user-provided destructor disqualifies a type from being trivially
// relocatable.
TEST(TriviallyRelocatable, UserProvidedDestructor) {
  struct S {
    ~S() {}  // NOLINT(modernize-use-equals-default)
  };

  static_assert(!absl::is_trivially_relocatable<S>::value, "");
}

#ifdef ABSL_HAVE_CONSTANT_EVALUATED

constexpr int64_t NegateIfConstantEvaluated(int64_t i) {
  if (absl::is_constant_evaluated()) {
    return -i;
  } else {
    return i;
  }
}

#endif  // ABSL_HAVE_CONSTANT_EVALUATED

TEST(IsConstantEvaluated, is_constant_evaluated) {
#ifdef ABSL_HAVE_CONSTANT_EVALUATED
  constexpr int64_t constant = NegateIfConstantEvaluated(42);
  EXPECT_EQ(constant, -42);

  int64_t now = absl::ToUnixSeconds(absl::Now());
  int64_t not_constant = NegateIfConstantEvaluated(now);
  EXPECT_EQ(not_constant, now);

  static int64_t const_init = NegateIfConstantEvaluated(42);
  EXPECT_EQ(const_init, -42);
#else
  GTEST_SKIP() << "absl::is_constant_evaluated is not defined";
#endif  // ABSL_HAVE_CONSTANT_EVALUATED
}

}  // namespace
