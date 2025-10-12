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

#include "absl/types/variant.h"

#include <memory>
#include <string>
#include <variant>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace absl {
namespace {

using ::testing::DoubleEq;
using ::testing::Pointee;
using ::testing::VariantWith;

struct Convertible2;
struct Convertible1 {
  Convertible1() {}
  Convertible1(const Convertible1&) {}
  Convertible1& operator=(const Convertible1&) { return *this; }

  // implicit conversion from Convertible2
  Convertible1(const Convertible2&) {}  // NOLINT(runtime/explicit)
};

struct Convertible2 {
  Convertible2() {}
  Convertible2(const Convertible2&) {}
  Convertible2& operator=(const Convertible2&) { return *this; }

  // implicit conversion from Convertible1
  Convertible2(const Convertible1&) {}  // NOLINT(runtime/explicit)
};

TEST(VariantTest, TestRvalueConversion) {
  std::variant<Convertible1, Convertible2> v(
      ConvertVariantTo<std::variant<Convertible1, Convertible2>>(
          (std::variant<Convertible2, Convertible1>(Convertible1()))));
  ASSERT_TRUE(absl::holds_alternative<Convertible1>(v));

  v = ConvertVariantTo<std::variant<Convertible1, Convertible2>>(
      std::variant<Convertible2, Convertible1>(Convertible2()));
  ASSERT_TRUE(absl::holds_alternative<Convertible2>(v));
}

TEST(VariantTest, TestLvalueConversion) {
  std::variant<Convertible2, Convertible1> source((Convertible1()));
  std::variant<Convertible1, Convertible2> v(
      ConvertVariantTo<std::variant<Convertible1, Convertible2>>(source));
  ASSERT_TRUE(absl::holds_alternative<Convertible1>(v));

  source = Convertible2();
  v = ConvertVariantTo<std::variant<Convertible1, Convertible2>>(source);
  ASSERT_TRUE(absl::holds_alternative<Convertible2>(v));
}

TEST(VariantTest, TestMoveConversion) {
  using Variant = std::variant<std::unique_ptr<const int>,
                               std::unique_ptr<const std::string>>;
  using OtherVariant =
      std::variant<std::unique_ptr<int>, std::unique_ptr<std::string>>;

  Variant var(
      ConvertVariantTo<Variant>(OtherVariant{std::make_unique<int>(0)}));
  ASSERT_TRUE(absl::holds_alternative<std::unique_ptr<const int>>(var));
  ASSERT_NE(absl::get<std::unique_ptr<const int>>(var), nullptr);
  EXPECT_EQ(0, *absl::get<std::unique_ptr<const int>>(var));

  var = ConvertVariantTo<Variant>(
      OtherVariant(std::make_unique<std::string>("foo")));
  ASSERT_TRUE(absl::holds_alternative<std::unique_ptr<const std::string>>(var));
  EXPECT_EQ("foo", *absl::get<std::unique_ptr<const std::string>>(var));
}

TEST(VariantTest, DoesNotMoveFromLvalues) {
  // We use shared_ptr here because it's both copyable and movable, and
  // a moved-from shared_ptr is guaranteed to be null, so we can detect
  // whether moving or copying has occurred.
  using Variant = std::variant<std::shared_ptr<const int>,
                               std::shared_ptr<const std::string>>;
  using OtherVariant =
      std::variant<std::shared_ptr<int>, std::shared_ptr<std::string>>;

  Variant v1(std::make_shared<const int>(0));

  // Test copy constructor
  Variant v2(v1);
  EXPECT_EQ(absl::get<std::shared_ptr<const int>>(v1),
            absl::get<std::shared_ptr<const int>>(v2));

  // Test copy-assignment operator
  v1 = std::make_shared<const std::string>("foo");
  v2 = v1;
  EXPECT_EQ(absl::get<std::shared_ptr<const std::string>>(v1),
            absl::get<std::shared_ptr<const std::string>>(v2));

  // Test converting copy constructor
  OtherVariant other(std::make_shared<int>(0));
  Variant v3(ConvertVariantTo<Variant>(other));
  EXPECT_EQ(absl::get<std::shared_ptr<int>>(other),
            absl::get<std::shared_ptr<const int>>(v3));

  other = std::make_shared<std::string>("foo");
  v3 = ConvertVariantTo<Variant>(other);
  EXPECT_EQ(absl::get<std::shared_ptr<std::string>>(other),
            absl::get<std::shared_ptr<const std::string>>(v3));
}

TEST(VariantTest, TestRvalueConversionViaConvertVariantTo) {
  variant<Convertible1, Convertible2> v(
      ConvertVariantTo<std::variant<Convertible1, Convertible2>>(
          (std::variant<Convertible2, Convertible1>(Convertible1()))));
  ASSERT_TRUE(absl::holds_alternative<Convertible1>(v));

  v = ConvertVariantTo<std::variant<Convertible1, Convertible2>>(
      std::variant<Convertible2, Convertible1>(Convertible2()));
  ASSERT_TRUE(absl::holds_alternative<Convertible2>(v));
}

TEST(VariantTest, TestLvalueConversionViaConvertVariantTo) {
  variant<Convertible2, Convertible1> source((Convertible1()));
  variant<Convertible1, Convertible2> v(
      ConvertVariantTo<std::variant<Convertible1, Convertible2>>(source));
  ASSERT_TRUE(absl::holds_alternative<Convertible1>(v));

  source = Convertible2();
  v = ConvertVariantTo<std::variant<Convertible1, Convertible2>>(source);
  ASSERT_TRUE(absl::holds_alternative<Convertible2>(v));
}

TEST(VariantTest, TestMoveConversionViaConvertVariantTo) {
  using Variant = std::variant<std::unique_ptr<const int>,
                               std::unique_ptr<const std::string>>;
  using OtherVariant =
      std::variant<std::unique_ptr<int>, std::unique_ptr<std::string>>;

  Variant var(
      ConvertVariantTo<Variant>(OtherVariant{std::make_unique<int>(3)}));
  EXPECT_THAT(absl::get_if<std::unique_ptr<const int>>(&var),
              Pointee(Pointee(3)));

  var = ConvertVariantTo<Variant>(
      OtherVariant(std::make_unique<std::string>("foo")));
  EXPECT_THAT(absl::get_if<std::unique_ptr<const std::string>>(&var),
              Pointee(Pointee(std::string("foo"))));
}

}  // namespace
}  // namespace absl
