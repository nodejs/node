// Copyright 2019 The Abseil Authors.
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

#include "absl/functional/function_ref.h"

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/internal/test_instance_tracker.h"
#include "absl/functional/any_invocable.h"
#include "absl/memory/memory.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace {

struct Class {
  int Func() { return 42; }
  int CFunc() const { return 43; }
};

int Function() { return 1337; }

template <typename T>
T Dereference(const T* v) {
  return *v;
}

template <typename T>
T Copy(const T& v) {
  return v;
}

TEST(FunctionRefTest, Function1) {
  FunctionRef<int()> ref(&Function);
  EXPECT_EQ(1337, ref());
}

TEST(FunctionRefTest, Function2) {
  FunctionRef<int()> ref(Function);
  EXPECT_EQ(1337, ref());
}

TEST(FunctionRefTest, ConstFunction) {
  FunctionRef<int() const> ref(Function);
  EXPECT_EQ(1337, ref());
}

TEST(FunctionRefTest, CopyAssignment) {
  FunctionRef<int()> a = +[]() -> int {
    ADD_FAILURE() << "Unexpectedly called";
    return 0;
  };
  FunctionRef<int()> b = +[] { return 1337; };
  a = b;
  EXPECT_EQ(1337, a());
}

int NoExceptFunction() noexcept { return 1337; }

// TODO(jdennett): Add a test for noexcept member functions.
TEST(FunctionRefTest, NoExceptFunction) {
  FunctionRef<int()> ref(NoExceptFunction);
  EXPECT_EQ(1337, ref());
}

TEST(FunctionRefTest, ForwardsArgs) {
  auto l = [](std::unique_ptr<int> i) { return *i; };
  FunctionRef<int(std::unique_ptr<int>)> ref(l);
  EXPECT_EQ(42, ref(absl::make_unique<int>(42)));
}

TEST(FunctionRef, ReturnMoveOnly) {
  auto l = [] { return absl::make_unique<int>(29); };
  FunctionRef<std::unique_ptr<int>()> ref(l);
  EXPECT_EQ(29, *ref());
}

TEST(FunctionRef, ManyArgs) {
  auto l = [](int a, int b, int c) { return a + b + c; };
  FunctionRef<int(int, int, int)> ref(l);
  EXPECT_EQ(6, ref(1, 2, 3));
}

TEST(FunctionRef, VoidResultFromNonVoidFunctor) {
  bool ran = false;
  auto l = [&]() -> int {
    ran = true;
    return 2;
  };
  FunctionRef<void()> ref(l);
  ref();
  EXPECT_TRUE(ran);
}

TEST(FunctionRef, CastFromDerived) {
  struct Base {};
  struct Derived : public Base {};

  Derived d;
  auto l1 = [&](Base* b) { EXPECT_EQ(&d, b); };
  FunctionRef<void(Derived*)> ref1(l1);
  ref1(&d);

  auto l2 = [&]() -> Derived* { return &d; };
  FunctionRef<Base*()> ref2(l2);
  EXPECT_EQ(&d, ref2());
}

TEST(FunctionRef, VoidResultFromNonVoidFuncton) {
  FunctionRef<void()> ref(Function);
  ref();
}

TEST(FunctionRef, MemberPtr) {
  struct S {
    int i;
  };

  S s{1100111};
  auto mem_ptr = &S::i;
  FunctionRef<int(const S& s)> ref(mem_ptr);
  EXPECT_EQ(1100111, ref(s));
}

TEST(FunctionRef, MemberFun) {
  struct S {
    int i;
    int get_i() const { return i; }
  };

  S s{22};
  auto mem_fun_ptr = &S::get_i;
  FunctionRef<int(const S& s)> ref(mem_fun_ptr);
  EXPECT_EQ(22, ref(s));
}

TEST(FunctionRef, MemberFunRefqualified) {
  struct S {
    int i;
    int get_i() && { return i; }
  };
  auto mem_fun_ptr = &S::get_i;
  S s{22};
  FunctionRef<int(S && s)> ref(mem_fun_ptr);
  EXPECT_EQ(22, ref(std::move(s)));
}

#if !defined(_WIN32) && defined(GTEST_HAS_DEATH_TEST)

TEST(FunctionRef, MemberFunRefqualifiedNull) {
  struct S {
    int i;
    int get_i() && { return i; }
  };
  auto mem_fun_ptr = &S::get_i;
  mem_fun_ptr = nullptr;
  EXPECT_DEBUG_DEATH({ FunctionRef<int(S && s)> ref(mem_fun_ptr); }, "");
}

TEST(FunctionRef, NullMemberPtrAssertFails) {
  struct S {
    int i;
  };
  using MemberPtr = int S::*;
  MemberPtr mem_ptr = nullptr;
  EXPECT_DEBUG_DEATH({ FunctionRef<int(const S& s)> ref(mem_ptr); }, "");
}

TEST(FunctionRef, NullStdFunctionAssertPasses) {
  std::function<void()> function = []() {};
  FunctionRef<void()> ref(function);
}

TEST(FunctionRef, NullStdFunctionAssertFails) {
  std::function<void()> function = nullptr;
  EXPECT_DEBUG_DEATH({ FunctionRef<void()> ref(function); }, "");
}

TEST(FunctionRef, NullAnyInvocableAssertPasses) {
  AnyInvocable<void() const> invocable = []() {};
  FunctionRef<void()> ref(invocable);
}
TEST(FunctionRef, NullAnyInvocableAssertFails) {
  AnyInvocable<void() const> invocable = nullptr;
  EXPECT_DEBUG_DEATH({ FunctionRef<void()> ref(invocable); }, "");
}

#endif  // GTEST_HAS_DEATH_TEST

TEST(FunctionRef, CopiesAndMovesPerPassByValue) {
  absl::test_internal::InstanceTracker tracker;
  absl::test_internal::CopyableMovableInstance instance(0);
  auto l = [](absl::test_internal::CopyableMovableInstance) {};
  FunctionRef<void(absl::test_internal::CopyableMovableInstance)> ref(l);
  ref(instance);
  EXPECT_EQ(tracker.copies(), 1);
  EXPECT_EQ(tracker.moves(), 1);
}

TEST(FunctionRef, CopiesAndMovesPerPassByRef) {
  absl::test_internal::InstanceTracker tracker;
  absl::test_internal::CopyableMovableInstance instance(0);
  auto l = [](const absl::test_internal::CopyableMovableInstance&) {};
  FunctionRef<void(const absl::test_internal::CopyableMovableInstance&)> ref(l);
  ref(instance);
  EXPECT_EQ(tracker.copies(), 0);
  EXPECT_EQ(tracker.moves(), 0);
}

TEST(FunctionRef, CopiesAndMovesPerPassByValueCallByMove) {
  absl::test_internal::InstanceTracker tracker;
  absl::test_internal::CopyableMovableInstance instance(0);
  auto l = [](absl::test_internal::CopyableMovableInstance) {};
  FunctionRef<void(absl::test_internal::CopyableMovableInstance)> ref(l);
  ref(std::move(instance));
  EXPECT_EQ(tracker.copies(), 0);
  EXPECT_EQ(tracker.moves(), 2);
}

TEST(FunctionRef, CopiesAndMovesPerPassByValueToRef) {
  absl::test_internal::InstanceTracker tracker;
  absl::test_internal::CopyableMovableInstance instance(0);
  auto l = [](const absl::test_internal::CopyableMovableInstance&) {};
  FunctionRef<void(absl::test_internal::CopyableMovableInstance)> ref(l);
  ref(std::move(instance));
  EXPECT_EQ(tracker.copies(), 0);
  EXPECT_EQ(tracker.moves(), 1);
}

TEST(FunctionRef, PassByValueTypes) {
  using absl::functional_internal::Invoker;
  using absl::functional_internal::VoidPtr;
  using absl::test_internal::CopyableMovableInstance;
  struct Trivial {
    void* p[2];
  };
  struct LargeTrivial {
    void* p[3];
  };

  static_assert(std::is_same<Invoker<void, int>, void (*)(VoidPtr, int)>::value,
                "Scalar types should be passed by value");
  static_assert(
      std::is_same<Invoker<void, Trivial>, void (*)(VoidPtr, Trivial)>::value,
      "Small trivial types should be passed by value");
  static_assert(std::is_same<Invoker<void, LargeTrivial>,
                             void (*)(VoidPtr, LargeTrivial&&)>::value,
                "Large trivial types should be passed by rvalue reference");
  static_assert(
      std::is_same<Invoker<void, CopyableMovableInstance>,
                   void (*)(VoidPtr, CopyableMovableInstance&&)>::value,
      "Types with copy/move ctor should be passed by rvalue reference");

  // References are passed as references.
  static_assert(
      std::is_same<Invoker<void, int&>, void (*)(VoidPtr, int&)>::value,
      "Reference types should be preserved");
  static_assert(
      std::is_same<Invoker<void, CopyableMovableInstance&>,
                   void (*)(VoidPtr, CopyableMovableInstance&)>::value,
      "Reference types should be preserved");
  static_assert(
      std::is_same<Invoker<void, CopyableMovableInstance&&>,
                   void (*)(VoidPtr, CopyableMovableInstance&&)>::value,
      "Reference types should be preserved");

  // Make sure the address of an object received by reference is the same as the
  // address of the object passed by the caller.
  {
    LargeTrivial obj;
    auto test = [&obj](LargeTrivial& input) { ASSERT_EQ(&input, &obj); };
    absl::FunctionRef<void(LargeTrivial&)> ref(test);
    ref(obj);
  }

  {
    Trivial obj;
    auto test = [&obj](Trivial& input) { ASSERT_EQ(&input, &obj); };
    absl::FunctionRef<void(Trivial&)> ref(test);
    ref(obj);
  }
}

TEST(FunctionRef, ReferenceToIncompleteType) {
  struct IncompleteType;
  auto test = [](IncompleteType&) {};
  absl::FunctionRef<void(IncompleteType&)> ref(test);

  struct IncompleteType {};
  IncompleteType obj;
  ref(obj);
}

TEST(FunctionRefTest, CorrectConstQualifiers) {
  struct S {
    int operator()() { return 42; }
    int operator()() const { return 1337; }
  };
  S s;
  EXPECT_EQ(42, FunctionRef<int()>(s)());
  EXPECT_EQ(1337, FunctionRef<int() const>(s)());
  EXPECT_EQ(1337, FunctionRef<int()>(std::as_const(s))());
  EXPECT_EQ(1337, FunctionRef<int() const>(std::as_const(s))());
}

TEST(FunctionRefTest, Lambdas) {
  // Stateless lambdas implicitly convert to function pointers, so their
  // mutability is irrelevant.
  EXPECT_TRUE(FunctionRef<bool()>([]() /*const*/ { return true; })());
  EXPECT_TRUE(FunctionRef<bool()>([]() mutable { return true; })());
  EXPECT_TRUE(FunctionRef<bool() const>([]() /*const*/ { return true; })());
#if defined(__clang__) || (ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L && \
                           defined(_MSC_VER) && !defined(__EDG__))
  // MSVC has problems compiling the following code pre-C++20:
  //   const auto f = []() mutable {};
  //   f();
  // EDG's MSVC-compatible mode (which Visual C++ uses for Intellisense)
  // exhibits the bug in C++20 as well. So we don't support them.
  EXPECT_TRUE(FunctionRef<bool() const>([]() mutable { return true; })());
#endif

  // Stateful lambdas are not implicitly convertible to function pointers, so
  // a const stateful lambda is not mutably callable.
  EXPECT_TRUE(FunctionRef<bool()>([v = true]() /*const*/ { return v; })());
  EXPECT_TRUE(FunctionRef<bool()>([v = true]() mutable { return v; })());
  EXPECT_TRUE(
      FunctionRef<bool() const>([v = true]() /*const*/ { return v; })());
  const auto func = [v = true]() mutable { return v; };
  static_assert(
      !std::is_convertible_v<decltype(func), FunctionRef<bool() const>>);
}

#if ABSL_INTERNAL_CPLUSPLUS_LANG >= 202002L
static_assert(std::is_same_v<decltype(FunctionRef(nontype<&Class::Func>,
                                                  std::declval<Class&>())),
                             FunctionRef<int()>>);
static_assert(std::is_same_v<decltype(FunctionRef(nontype<&Class::CFunc>,
                                                  std::declval<Class&>())),
                             FunctionRef<int() const>>);

static_assert(std::is_same_v<decltype(FunctionRef(nontype<&Class::Func>,
                                                  std::declval<Class*>())),
                             FunctionRef<int()>>);
static_assert(std::is_same_v<decltype(FunctionRef(nontype<&Class::CFunc>,
                                                  std::declval<Class*>())),
                             FunctionRef<int() const>>);

TEST(FunctionRefTest, NonTypeParameterWithTemporaries) {
  static_assert(!std::is_constructible_v<FunctionRef<int()>,
                                         nontype_t<&Class::Func>, Class&&>);
  static_assert(
      !std::is_constructible_v<FunctionRef<int()>, nontype_t<&Class::Func>,
                               const Class&&>);
  static_assert(!std::is_constructible_v<FunctionRef<int() const>,
                                         nontype_t<&Class::CFunc>, Class&&>);
  static_assert(
      !std::is_constructible_v<FunctionRef<int() const>,
                               nontype_t<&Class::CFunc>, const Class&&>);
}

TEST(FunctionRefTest, NonTypeParameterWithDeductionGuides) {
  EXPECT_EQ(1337, FunctionRef(nontype<&Function>)());
  EXPECT_EQ(42, FunctionRef(nontype<&Copy<int>>,
                            std::integral_constant<int, 42>::value)());
  EXPECT_EQ(42, FunctionRef(nontype<&Dereference<int>>,
                            &std::integral_constant<int, 42>::value)());

  Class c;
  EXPECT_EQ(42, FunctionRef<int()>(nontype<&Class::Func>, c)());
  EXPECT_EQ(43, FunctionRef<int() const>(nontype<&Class::CFunc>, c)());

  EXPECT_EQ(42, FunctionRef<int()>(nontype<&Class::Func>, &c)());
  EXPECT_EQ(43, FunctionRef<int() const>(nontype<&Class::CFunc>, &c)());
}
#endif

TEST(FunctionRefTest, OptionalArguments) {
  struct S {
    int operator()(int = 0) const { return 1337; }
  };
  S s;
  EXPECT_EQ(1337, FunctionRef<int()>(s)());
}

TEST(FunctionRefTest, NonConstToConstConversion) {
  // The const-qualified version might inherit from the non-const version.
  // We want to make sure that this doesn't introduce a bug when an instance of
  // the base (non-const) class is forwarded through the derived (const) class.
  // This has the potential to trigger the copy constructor, thus incorrectly
  // producing a copy rather than another indirection.
  absl::FunctionRef<int()> a = +[]() { return 1; };
  absl::FunctionRef<int() const> b = a;
  a = +[]() { return 2; };
  EXPECT_EQ(b(), 2);
}

}  // namespace
ABSL_NAMESPACE_END
}  // namespace absl
