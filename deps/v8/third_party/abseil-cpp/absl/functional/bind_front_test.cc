// Copyright 2018 The Abseil Authors.
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

#include "absl/functional/bind_front.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/memory/memory.h"

namespace {

constexpr char CharAt(const char* s, size_t index) { return s[index]; }

TEST(BindTest, Basics) {
  EXPECT_EQ('C', absl::bind_front(CharAt)("ABC", 2));
  EXPECT_EQ('C', absl::bind_front(CharAt, "ABC")(2));
  EXPECT_EQ('C', absl::bind_front(CharAt, "ABC", 2)());
}

TEST(BindTest, Lambda) {
  auto lambda = [](int x, int y, int z) { return x + y + z; };
  EXPECT_EQ(6, absl::bind_front(lambda)(1, 2, 3));
  EXPECT_EQ(6, absl::bind_front(lambda, 1)(2, 3));
  EXPECT_EQ(6, absl::bind_front(lambda, 1, 2)(3));
  EXPECT_EQ(6, absl::bind_front(lambda, 1, 2, 3)());
}

struct Functor {
  std::string operator()() & { return "&"; }
  std::string operator()() const& { return "const&"; }
  std::string operator()() && { return "&&"; }
  std::string operator()() const&& { return "const&&"; }
};

TEST(BindTest, PerfectForwardingOfBoundArgs) {
  auto f = absl::bind_front(Functor());
  const auto& cf = f;
  EXPECT_EQ("&", f());
  EXPECT_EQ("const&", cf());
  EXPECT_EQ("&&", std::move(f)());
  EXPECT_EQ("const&&", std::move(cf)());
}

struct ArgDescribe {
  std::string operator()(int&) const { return "&"; }             // NOLINT
  std::string operator()(const int&) const { return "const&"; }  // NOLINT
  std::string operator()(int&&) const { return "&&"; }
  std::string operator()(const int&&) const { return "const&&"; }
};

TEST(BindTest, PerfectForwardingOfFreeArgs) {
  ArgDescribe f;
  int i;
  EXPECT_EQ("&", absl::bind_front(f)(static_cast<int&>(i)));
  EXPECT_EQ("const&", absl::bind_front(f)(static_cast<const int&>(i)));
  EXPECT_EQ("&&", absl::bind_front(f)(static_cast<int&&>(i)));
  EXPECT_EQ("const&&", absl::bind_front(f)(static_cast<const int&&>(i)));
}

struct NonCopyableFunctor {
  NonCopyableFunctor() = default;
  NonCopyableFunctor(const NonCopyableFunctor&) = delete;
  NonCopyableFunctor& operator=(const NonCopyableFunctor&) = delete;
  const NonCopyableFunctor* operator()() const { return this; }
};

TEST(BindTest, RefToFunctor) {
  // It won't copy/move the functor and use the original object.
  NonCopyableFunctor ncf;
  auto bound_ncf = absl::bind_front(std::ref(ncf));
  auto bound_ncf_copy = bound_ncf;
  EXPECT_EQ(&ncf, bound_ncf_copy());
}

struct Struct {
  std::string value;
};

TEST(BindTest, StoreByCopy) {
  Struct s = {"hello"};
  auto f = absl::bind_front(&Struct::value, s);
  auto g = f;
  EXPECT_EQ("hello", f());
  EXPECT_EQ("hello", g());
  EXPECT_NE(&s.value, &f());
  EXPECT_NE(&s.value, &g());
  EXPECT_NE(&g(), &f());
}

struct NonCopyable {
  explicit NonCopyable(const std::string& s) : value(s) {}
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;

  std::string value;
};

const std::string& GetNonCopyableValue(const NonCopyable& n) { return n.value; }

TEST(BindTest, StoreByRef) {
  NonCopyable s("hello");
  auto f = absl::bind_front(&GetNonCopyableValue, std::ref(s));
  EXPECT_EQ("hello", f());
  EXPECT_EQ(&s.value, &f());
  auto g = std::move(f);  // NOLINT
  EXPECT_EQ("hello", g());
  EXPECT_EQ(&s.value, &g());
  s.value = "goodbye";
  EXPECT_EQ("goodbye", g());
}

TEST(BindTest, StoreByCRef) {
  NonCopyable s("hello");
  auto f = absl::bind_front(&GetNonCopyableValue, std::cref(s));
  EXPECT_EQ("hello", f());
  EXPECT_EQ(&s.value, &f());
  auto g = std::move(f);  // NOLINT
  EXPECT_EQ("hello", g());
  EXPECT_EQ(&s.value, &g());
  s.value = "goodbye";
  EXPECT_EQ("goodbye", g());
}

const std::string& GetNonCopyableValueByWrapper(
    std::reference_wrapper<NonCopyable> n) {
  return n.get().value;
}

TEST(BindTest, StoreByRefInvokeByWrapper) {
  NonCopyable s("hello");
  auto f = absl::bind_front(GetNonCopyableValueByWrapper, std::ref(s));
  EXPECT_EQ("hello", f());
  EXPECT_EQ(&s.value, &f());
  auto g = std::move(f);
  EXPECT_EQ("hello", g());
  EXPECT_EQ(&s.value, &g());
  s.value = "goodbye";
  EXPECT_EQ("goodbye", g());
}

TEST(BindTest, StoreByPointer) {
  NonCopyable s("hello");
  auto f = absl::bind_front(&NonCopyable::value, &s);
  EXPECT_EQ("hello", f());
  EXPECT_EQ(&s.value, &f());
  auto g = std::move(f);
  EXPECT_EQ("hello", g());
  EXPECT_EQ(&s.value, &g());
}

int Sink(std::unique_ptr<int> p) { return *p; }

std::unique_ptr<int> Factory(int n) { return std::make_unique<int>(n); }

TEST(BindTest, NonCopyableArg) {
  EXPECT_EQ(42, absl::bind_front(Sink)(std::make_unique<int>(42)));
  EXPECT_EQ(42, absl::bind_front(Sink, std::make_unique<int>(42))());
}

TEST(BindTest, NonCopyableResult) {
  EXPECT_THAT(absl::bind_front(Factory)(42), ::testing::Pointee(42));
  EXPECT_THAT(absl::bind_front(Factory, 42)(), ::testing::Pointee(42));
}

// is_copy_constructible<FalseCopyable<unique_ptr<T>> is true but an attempt to
// instantiate the copy constructor leads to a compile error. This is similar
// to how standard containers behave.
template <class T>
struct FalseCopyable {
  FalseCopyable() {}
  FalseCopyable(const FalseCopyable& other) : m(other.m) {}
  FalseCopyable(FalseCopyable&& other) : m(std::move(other.m)) {}
  T m;
};

int GetMember(FalseCopyable<std::unique_ptr<int>> x) { return *x.m; }

TEST(BindTest, WrappedMoveOnly) {
  FalseCopyable<std::unique_ptr<int>> x;
  x.m = std::make_unique<int>(42);
  auto f = absl::bind_front(&GetMember, std::move(x));
  EXPECT_EQ(42, std::move(f)());
}

constexpr int Plus(int a, int b) { return a + b; }

TEST(BindTest, ConstExpr) {
  constexpr auto f = absl::bind_front(CharAt);
  EXPECT_EQ(f("ABC", 1), 'B');
  static constexpr int five = 5;
  constexpr auto plus5 = absl::bind_front(Plus, five);
  EXPECT_EQ(plus5(1), 6);

  static constexpr char data[] = "DEF";
  constexpr auto g = absl::bind_front(CharAt, data);
  EXPECT_EQ(g(1), 'E');
}

struct ManglingCall {
  int operator()(int, double, std::string) const { return 0; }
};

TEST(BindTest, Mangling) {
  // We just want to generate a particular instantiation to see its mangling.
  absl::bind_front(ManglingCall{}, 1, 3.3)("A");
}

struct Math {
  constexpr int Add(int a, int b) const noexcept { return base + a + b; }
  int base = 10;
};

TEST(BindConstFnTest, Basics) {
  EXPECT_EQ('C', absl::bind_front<&CharAt>()("ABC", 2));
  EXPECT_EQ('C', absl::bind_front<&CharAt>("ABC")(2));
  EXPECT_EQ('C', absl::bind_front<&CharAt>("ABC", 2)());
}

TEST(BindConstFnTest, MemberPointers) {
  Math m{10};
  auto bound_ptr = absl::bind_front<&Math::Add>(&m);
  EXPECT_EQ(15, bound_ptr(2, 3));

  auto bound_ptr_and_arg = absl::bind_front<&Math::Add>(&m, 2);
  EXPECT_EQ(15, bound_ptr_and_arg(3));

  auto bound_zero_args = absl::bind_front<&Math::Add>();
  EXPECT_EQ(15, bound_zero_args(&m, 2, 3));
  EXPECT_EQ(15, bound_zero_args(m, 2, 3));
}

TEST(BindConstFnTest, StoreByCopyRefAndPointer) {
  Struct s = {"hello"};
  auto by_copy = absl::bind_front<&Struct::value>(s);
  EXPECT_EQ("hello", by_copy());
  EXPECT_NE(&s.value, &by_copy());

  NonCopyable nc("hello");
  auto by_ref = absl::bind_front<&GetNonCopyableValue>(std::ref(nc));
  EXPECT_EQ("hello", by_ref());
  EXPECT_EQ(&nc.value, &by_ref());

  auto by_cref = absl::bind_front<&GetNonCopyableValue>(std::cref(nc));
  EXPECT_EQ("hello", by_cref());
  EXPECT_EQ(&nc.value, &by_cref());

  auto by_ptr = absl::bind_front<&NonCopyable::value>(&nc);
  EXPECT_EQ("hello", by_ptr());
  EXPECT_EQ(&nc.value, &by_ptr());
  nc.value = "goodbye";
  EXPECT_EQ("goodbye", by_ref());
  EXPECT_EQ("goodbye", by_cref());
  EXPECT_EQ("goodbye", by_ptr());
}

TEST(BindConstFnTest, MoveOnlyArgsAndResult) {
  EXPECT_EQ(42, absl::bind_front<&Sink>()(std::make_unique<int>(42)));
  EXPECT_EQ(42, absl::bind_front<&Sink>(std::make_unique<int>(42))());
  EXPECT_THAT(absl::bind_front<&Factory>()(42), ::testing::Pointee(42));
  EXPECT_THAT(absl::bind_front<&Factory>(42)(), ::testing::Pointee(42));

  FalseCopyable<std::unique_ptr<int>> x;
  x.m = std::make_unique<int>(42);
  auto f = absl::bind_front<&GetMember>(std::move(x));
  EXPECT_EQ(42, std::move(f)());
}

TEST(BindConstFnTest, ConstExpr) {
  constexpr auto f = absl::bind_front<&CharAt>();
  EXPECT_EQ(f("ABC", 1), 'B');

  constexpr auto plus5 = absl::bind_front<&Plus>(5);
  EXPECT_EQ(plus5(1), 6);

  constexpr auto plus5_1 = absl::bind_front<&Plus>(5, 1);
  EXPECT_EQ(plus5_1(), 6);

  constexpr Math m{10};
  constexpr auto add_m = absl::bind_front<&Math::Add>(m, 2);
  EXPECT_EQ(add_m(3), 15);

  // Invoking the results is `constexpr` only where the runtime `bind_front`
  // that produces them is `std::bind_front` (C++20). The pre-C++20 fallback's
  // call operators and `std::invoke` are not `constexpr`.
#if defined(__cpp_lib_bind_front) && __cpp_lib_bind_front >= 201907L
  static_assert(f("ABC", 1) == 'B');
  static_assert(plus5(1) == 6);
  static_assert(plus5_1() == 6);
  static_assert(add_m(3) == 15);
#endif  // __cpp_lib_bind_front >= 201907L
}

// The results come from the runtime `bind_front`, which propagates `noexcept`
// where it is `std::bind_front` (C++20).
#if defined(__cpp_lib_bind_front) && __cpp_lib_bind_front >= 201907L
int NoexceptPlus(int a, int b) noexcept { return a + b; }
int PotentiallyThrowingPlus(int a, int b) { return a + b; }

TEST(BindConstFnTest, NoexceptPropagation) {
  auto noexcept_0 = absl::bind_front<&NoexceptPlus>();
  static_assert(noexcept(noexcept_0(1, 2)));

  auto throwing_0 = absl::bind_front<&PotentiallyThrowingPlus>();
  static_assert(!noexcept(throwing_0(1, 2)));

  Math m{10};
  auto math_0 = absl::bind_front<&Math::Add>();
  static_assert(noexcept(math_0(m, 1, 2)));
}
#endif  // __cpp_lib_bind_front >= 201907L

// Invalid calls must be substitution failures rather than hard errors, so that
// `std::is_invocable` works on the result.
TEST(BindConstFnTest, SfinaeFriendly) {
  using PlusBinder = decltype(absl::bind_front<&Plus>());
  static_assert(std::is_invocable_v<PlusBinder, int, int>);
  static_assert(!std::is_invocable_v<PlusBinder, std::string, int>);
  static_assert(!std::is_invocable_v<PlusBinder, int>);

  using Plus1Binder = decltype(absl::bind_front<&Plus>(1));
  static_assert(std::is_invocable_v<Plus1Binder, int>);
  static_assert(!std::is_invocable_v<Plus1Binder, std::string>);
}

// These tests pass class-type objects as template arguments (P0732R2,
// P1907R1), and `StatelessLambdaNTTP` puts a lambda in a template argument
// (P0315R4, which has no feature-test macro).
//
// We check the language version rather than
// `__cpp_nontype_template_args >= 201911L` because Clang (through at least
// 24) still defines it as 201411L: its P1907R1 support is incomplete, though
// everything used here works. Checking the feature macro would silently skip
// these tests on Clang. If a toolchain claims C++20 but can't compile these
// tests, exclude it here.
#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L
TEST(BindConstFnTest, StatelessLambdaNTTP) {
  constexpr auto f = absl::bind_front<[](int x, int y) { return x + y; }>();
  EXPECT_EQ(f(2, 3), 5);

  constexpr auto f2 = absl::bind_front<[](int x, int y) { return x + y; }>(2);
  EXPECT_EQ(f2(3), 5);
}

struct Adder {
  constexpr int operator()(int x) const& { return base + x; }
  // `ConstFn` names a template parameter object, which is a `const` lvalue
  // ([temp.param]), so it must never be invoked as an rvalue.
  int operator()(int) const&& = delete;

  int base;
};

TEST(BindConstFnTest, StatefulObjectNTTP) {
  constexpr auto add5 = absl::bind_front<Adder{5}>();
  EXPECT_EQ(add5(1), 6);

  constexpr auto add5_1 = absl::bind_front<Adder{5}>(1);
  EXPECT_EQ(add5_1(), 6);
}

struct DescribeCall {
  template <class... Args>
  std::string operator()(Args&&...) & {
    return "&";
  }
  template <class... Args>
  std::string operator()(Args&&...) const& {
    return "const&";
  }
  template <class... Args>
  std::string operator()(Args&&...) && {
    return "&&";
  }
  template <class... Args>
  std::string operator()(Args&&...) const&& {
    return "const&&";
  }
};

// Since `ConstFn` is a `const` lvalue, it is invoked through its `const&`
// overload whatever the value category of the result. Both zero and nonzero
// bound arguments are tested, since the standard library's
// `std::bind_front<ConstFn>` may implement them differently.
TEST(BindConstFnTest, InvokesConstFnAsConstLvalue) {
  auto f0 = absl::bind_front<DescribeCall{}>();
  const auto& cf0 = f0;
  EXPECT_EQ("const&", f0());
  EXPECT_EQ("const&", cf0());
  EXPECT_EQ("const&", std::move(f0)());
  EXPECT_EQ("const&", std::move(cf0)());

  auto f1 = absl::bind_front<DescribeCall{}>(1);
  const auto& cf1 = f1;
  EXPECT_EQ("const&", f1());
  EXPECT_EQ("const&", cf1());
  EXPECT_EQ("const&", std::move(f1)());
  EXPECT_EQ("const&", std::move(cf1)());
}

struct DeletedConstLvalueCall {
  template <class... Args>
  void operator()(Args&&...) &;
  template <class... Args>
  void operator()(Args&&...) const& = delete;
  template <class... Args>
  void operator()(Args&&...) &&;
  template <class... Args>
  void operator()(Args&&...) const&&;
};

// If `ConstFn`'s `const&` overload is deleted, the result isn't invocable,
// rather than falling back to another overload.
TEST(BindConstFnTest, NotInvocableIfConstLvalueCallDeleted) {
  using F0 = decltype(absl::bind_front<DeletedConstLvalueCall{}>());
  static_assert(!std::is_invocable_v<F0&>);
  static_assert(!std::is_invocable_v<const F0&>);
  static_assert(!std::is_invocable_v<F0>);
  static_assert(!std::is_invocable_v<const F0>);

  using F1 = decltype(absl::bind_front<DeletedConstLvalueCall{}>(1));
  static_assert(!std::is_invocable_v<F1&>);
  static_assert(!std::is_invocable_v<const F1&>);
  static_assert(!std::is_invocable_v<F1>);
  static_assert(!std::is_invocable_v<const F1>);
}
#endif  // ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L

}  // namespace
