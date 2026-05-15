// Copyright 2026 The Abseil Authors.
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

#include "absl/types/optional_ref.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace {

using ::testing::Optional;
using ::testing::Pointee;

TEST(OptionalRefTest, SimpleType) {
  int val = 5;
  optional_ref<int> ref = optional_ref(val);
  optional_ref<int> empty_ref = std::nullopt;
  EXPECT_THAT(ref, Optional(5));
  EXPECT_TRUE(ref.has_value());
  EXPECT_EQ(*ref, val);
  EXPECT_EQ(ref.value(), val);
  EXPECT_EQ(ref, ref);
  EXPECT_EQ(ref, val);
  EXPECT_EQ(val, ref);
  EXPECT_NE(ref, empty_ref);
  EXPECT_NE(empty_ref, ref);
}

TEST(OptionalRefTest, SimpleConstType) {
  const int val = 5;
  optional_ref<const int> ref = optional_ref(val);
  EXPECT_THAT(ref, Optional(5));
}

TEST(OptionalRefTest, DefaultConstructed) {
  optional_ref<int> ref;
  EXPECT_EQ(ref, std::nullopt);
  EXPECT_EQ(std::nullopt, ref);
}

TEST(OptionalRefTest, EmptyOptional) {
  auto ref = optional_ref<int>(std::nullopt);
  EXPECT_EQ(ref, std::nullopt);
  EXPECT_EQ(std::nullopt, ref);
}

TEST(OptionalRefTest, OptionalType) {
  const std::optional<int> val = 5;
  optional_ref<const int> ref = val;
  EXPECT_THAT(ref, Optional(5));
  EXPECT_EQ(ref.as_pointer(), &*val);

  const std::optional<int> empty;
  optional_ref<const int> empty_ref = empty;
  EXPECT_EQ(empty_ref, std::nullopt);

  // Cannot make non-const reference to const optional.
  static_assert(
      !std::is_convertible_v<const std::optional<int>&, optional_ref<int>>);
}

TEST(OptionalRefTest, NonConstOptionalType) {
  std::optional<int> val = 5;
  optional_ref<int> ref = val;
  EXPECT_THAT(ref, Optional(5));
  EXPECT_EQ(ref.as_pointer(), &*val);

  std::optional<int> empty;
  optional_ref<int> empty_ref = empty;
  EXPECT_EQ(empty_ref, std::nullopt);
}

TEST(OptionalRefTest, NonConstOptionalTypeToConstRef) {
  std::optional<int> val = 5;
  optional_ref<const int> ref = val;
  EXPECT_THAT(ref, Optional(5));
  EXPECT_EQ(ref.as_pointer(), &*val);

  std::optional<int> empty;
  optional_ref<const int> empty_ref = empty;
  EXPECT_EQ(empty_ref, std::nullopt);
}

TEST(OptionalRefTest, NonConstOptionalWithConstValueType) {
  std::optional<const int> val = 5;
  optional_ref<const int> ref = val;
  EXPECT_THAT(ref, Optional(5));
  EXPECT_EQ(ref.as_pointer(), &*val);

  std::optional<const int> empty;
  optional_ref<const int> empty_ref = empty;
  EXPECT_EQ(empty_ref, std::nullopt);

  // Not possible to convert to non-const reference.
  static_assert(
      !std::is_convertible_v<std::optional<const int>&, optional_ref<int>>);
}

class TestInterface {};
class TestDerivedClass : public TestInterface {};

TEST(OptionalRefTest, BaseDerivedConvertibleOptionalType) {
  const std::optional<TestDerivedClass> dc = TestDerivedClass{};
  optional_ref<const TestInterface> ref = dc;
  EXPECT_NE(ref, std::nullopt);
  EXPECT_EQ(ref.as_pointer(), &*dc);

  const std::optional<TestDerivedClass> empty;
  optional_ref<const TestInterface> empty_ref = empty;
  EXPECT_EQ(empty_ref, std::nullopt);

  // Not possible in the other direction.
  static_assert(!std::is_convertible_v<const std::optional<TestInterface>&,
                                       optional_ref<const TestDerivedClass>>);
  static_assert(!std::is_convertible_v<const std::optional<TestDerivedClass>&,
                                       optional_ref<TestInterface>>);
}

TEST(OptionalRefTest, NonConstBaseDerivedConvertibleOptionalType) {
  std::optional<TestDerivedClass> dc = TestDerivedClass{};
  optional_ref<TestInterface> ref = dc;
  EXPECT_NE(ref, std::nullopt);
  EXPECT_EQ(ref.as_pointer(), &*dc);

  std::optional<TestDerivedClass> empty;
  optional_ref<TestInterface> empty_ref = empty;
  EXPECT_EQ(empty_ref, std::nullopt);

  // Not possible in the other direction.
  static_assert(!std::is_convertible_v<std::optional<TestInterface>&,
                                       optional_ref<TestDerivedClass>>);
}

TEST(OptionalRefTest, NonConstBaseDerivedConvertibleOptionalTypeToConstRef) {
  std::optional<TestDerivedClass> dc = TestDerivedClass{};
  optional_ref<const TestInterface> ref = dc;
  EXPECT_NE(ref, std::nullopt);
  EXPECT_EQ(ref.as_pointer(), &*dc);

  std::optional<TestDerivedClass> empty;
  optional_ref<const TestInterface> empty_ref = empty;
  EXPECT_EQ(empty_ref, std::nullopt);

  // Not possible in the other direction.
  static_assert(!std::is_convertible_v<std::optional<TestInterface>&,
                                       optional_ref<const TestDerivedClass>>);
  static_assert(!std::is_convertible_v<const std::optional<TestInterface>&,
                                       optional_ref<const TestDerivedClass>>);
}

TEST(OptionalRefTest, PointerCtor) {
  int val = 5;
  optional_ref<const int> ref = &val;
  EXPECT_THAT(ref, Optional(5));

  auto auto_ref = optional_ref(&val);
  static_assert(std::is_same_v<decltype(auto_ref), optional_ref<int>>,
                "optional_ref(T*) should deduce to optional_ref<T>.");
  EXPECT_THAT(auto_ref, Optional(5));

  int* foo = nullptr;
  optional_ref<const int> empty_ref = foo;
  EXPECT_EQ(empty_ref, std::nullopt);

  optional_ref<int*> ptr_ref = foo;
  EXPECT_THAT(ptr_ref, Optional(nullptr));
  static_assert(
      !std::is_constructible_v<optional_ref<int*>, std::nullptr_t>,
      "optional_ref should not be constructible with std::nullptr_t.");

  // Pointer polymorphism works.
  TestDerivedClass dc;
  optional_ref<const TestInterface> dc_ref = &dc;
  EXPECT_NE(dc_ref, std::nullopt);
}

TEST(OptionalRefTest, ValueDeathWhenEmpty) {
  optional_ref<int> ref;
#ifdef ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(ref.value(), std::bad_optional_access);
#else
  EXPECT_DEATH_IF_SUPPORTED(ref.value(), "");
#endif
}

TEST(OptionalRefTest, ImplicitCtor) {
  const int val = 5;
  optional_ref<const int> ref = val;
  EXPECT_THAT(ref, Optional(5));
}

TEST(OptionalRefTest, DoesNotCopy) {
  // Non-copyable type.
  auto val = std::make_unique<int>(5);
  optional_ref<std::unique_ptr<int>> ref = optional_ref(val);
  EXPECT_THAT(ref, Optional(Pointee(5)));
}

TEST(OptionalRefTest, DoesNotCopyConst) {
  // Non-copyable type.
  const auto val = std::make_unique<int>(5);
  optional_ref<const std::unique_ptr<int>> ref = optional_ref(val);
  EXPECT_THAT(ref, Optional(Pointee(5)));
}

TEST(OptionalRefTest, RefCopyable) {
  auto val = std::make_unique<int>(5);
  optional_ref<std::unique_ptr<int>> ref = optional_ref(val);
  optional_ref<std::unique_ptr<int>> copy = ref;
  EXPECT_THAT(copy, Optional(Pointee(5)));
}

TEST(OptionalRefTest, ConstConvertible) {
  auto val = std::make_unique<int>(5);
  optional_ref<std::unique_ptr<int>> ref = optional_ref(val);
  optional_ref<const std::unique_ptr<int>> converted = ref;
  EXPECT_THAT(converted, Optional(Pointee(5)));
  EXPECT_EQ(converted.as_pointer(), &val);

  // Not possible in the other direction.
  static_assert(!std::is_convertible_v<optional_ref<const std::unique_ptr<int>>,
                                       optional_ref<std::unique_ptr<int>>>);
}

TEST(OptionalRefTest, BaseDerivedConvertible) {
  TestDerivedClass dc;
  optional_ref<TestDerivedClass> dc_ref = dc;
  optional_ref<TestInterface> converted = dc_ref;
  EXPECT_NE(converted, std::nullopt);
  EXPECT_EQ(converted.as_pointer(), &dc);

  // Not possible in the other direction.
  static_assert(!std::is_convertible_v<optional_ref<TestInterface>,
                                       optional_ref<TestDerivedClass>>);
}

TEST(OptionalRefTest, BaseDerivedConstConvertible) {
  TestDerivedClass dc;
  optional_ref<TestDerivedClass> dc_ref = dc;
  optional_ref<const TestInterface> converted = dc_ref;
  EXPECT_NE(converted, std::nullopt);
  EXPECT_EQ(converted.as_pointer(), &dc);

  // Not possible in the other direction.
  static_assert(!std::is_convertible_v<optional_ref<const TestInterface>,
                                       optional_ref<TestDerivedClass>>);
  static_assert(!std::is_convertible_v<optional_ref<const TestDerivedClass>,
                                       optional_ref<TestInterface>>);
}

TEST(OptionalRefTest, BaseDerivedBothConstConvertible) {
  TestDerivedClass dc;
  optional_ref<const TestDerivedClass> dc_ref = dc;
  optional_ref<const TestInterface> converted = dc_ref;
  EXPECT_NE(converted, std::nullopt);
  EXPECT_EQ(converted.as_pointer(), &dc);

  // Not possible in the other direction.
  static_assert(!std::is_convertible_v<optional_ref<const TestInterface>,
                                       optional_ref<const TestDerivedClass>>);
}

TEST(OptionalRefTest, TriviallyCopyable) {
  static_assert(
      std::is_trivially_copyable_v<optional_ref<std::unique_ptr<int>>>);
}

TEST(OptionalRefTest, TriviallyDestructible) {
  static_assert(
      std::is_trivially_destructible_v<optional_ref<std::unique_ptr<int>>>);
}

TEST(OptionalRefTest, RefNotAssignable) {
  static_assert(!std::is_copy_assignable_v<optional_ref<int>>);
  static_assert(!std::is_move_assignable_v<optional_ref<int>>);
}

struct TestStructWithCopy {
  TestStructWithCopy() = default;
  TestStructWithCopy(TestStructWithCopy&&) {
    LOG(FATAL) << "Move constructor should not be called";
  }
  TestStructWithCopy(const TestStructWithCopy&) {
    LOG(FATAL) << "Copy constructor should not be called";
  }
  TestStructWithCopy& operator=(const TestStructWithCopy&) {
    LOG(FATAL) << "Assign operator should not be called";
  }
};

TEST(OptionalRefTest, DoesNotCopyUsingFatalCopyAssignOps) {
  TestStructWithCopy val;
  optional_ref<TestStructWithCopy> ref = optional_ref(val);
  EXPECT_NE(ref, std::nullopt);
  EXPECT_NE(optional_ref(TestStructWithCopy{}), std::nullopt);
}

std::string AddExclamation(optional_ref<const std::string> input) {
  if (!input.has_value()) {
    return "";
  }
  return absl::StrCat(*input, "!");
}

TEST(OptionalRefTest, RefAsFunctionParameter) {
  EXPECT_EQ(AddExclamation(std::nullopt), "");
  EXPECT_EQ(AddExclamation(std::string("abc")), "abc!");
  std::string s = "def";
  EXPECT_EQ(AddExclamation(s), "def!");
  EXPECT_EQ(AddExclamation(std::make_optional<std::string>(s)), "def!");
}

TEST(OptionalRefTest, ValueOrWhenHasValue) {
  std::optional<int> val = 5;
  EXPECT_EQ(optional_ref(val).value_or(2), 5);
}

TEST(OptionalRefTest, ValueOrWhenEmpty) {
  std::optional<int> val = std::nullopt;
  EXPECT_EQ(optional_ref(val).value_or(2), 2);
}

TEST(OptionalRefTest, AsOptional) {
  EXPECT_EQ(optional_ref<int>().as_optional(), std::nullopt);
  std::string val = "foo";
  optional_ref<const std::string> ref = val;
  static_assert(
      std::is_same_v<decltype(ref.as_optional()), std::optional<std::string>>,
      "The type parameter of optional_ref should decay by default for the "
      "return type in as_optional().");
  std::optional<std::string> opt_string = ref.as_optional();
  EXPECT_THAT(opt_string, Optional(val));

  std::optional<std::string_view> opt_view =
      ref.as_optional<std::string_view>();
  EXPECT_THAT(opt_view, Optional(val));
}

TEST(OptionalRefTest, Constexpr) {
  static constexpr int foo = 123;
  constexpr optional_ref<const int> ref(foo);
  static_assert(ref.has_value() && *ref == foo && ref.value() == foo, "");
}

}  // namespace
ABSL_NAMESPACE_END
}  // namespace absl
