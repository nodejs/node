//
// Copyright 2022 The Abseil Authors.
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

// SKIP_ABSL_INLINE_NAMESPACE_CHECK

#ifndef ABSL_LOG_CHECK_TEST_IMPL_H_
#define ABSL_LOG_CHECK_TEST_IMPL_H_

// Verify that both sets of macros behave identically by parameterizing the
// entire test file.
#ifndef ABSL_TEST_CHECK
#error ABSL_TEST_CHECK must be defined for these tests to work.
#endif

#include <cstdint>
#include <limits>
#include <ostream>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/log/internal/test_helpers.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"

// NOLINTBEGIN(misc-definitions-in-headers)

namespace absl_log_internal {

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::Not;

auto* test_env ABSL_ATTRIBUTE_UNUSED = ::testing::AddGlobalTestEnvironment(
    new absl::log_internal::LogTestEnvironment);

#if GTEST_HAS_DEATH_TEST

TEST(CHECKDeathTest, TestBasicValues) {
  ABSL_TEST_CHECK(true);

  EXPECT_DEATH(ABSL_TEST_CHECK(false), "Check failed: false");

  int i = 2;
  ABSL_TEST_CHECK(i != 3);  // NOLINT
}

#endif  // GTEST_HAS_DEATH_TEST

TEST(CHECKTest, TestLogicExpressions) {
  int i = 5;
  ABSL_TEST_CHECK(i > 0 && i < 10);
  ABSL_TEST_CHECK(i < 0 || i > 3);
}

ABSL_CONST_INIT const auto global_var_check = [](int i) {
  ABSL_TEST_CHECK(i > 0);  // NOLINT
  return i + 1;
}(3);

ABSL_CONST_INIT const auto global_var = [](int i) {
  ABSL_TEST_CHECK_GE(i, 0);  // NOLINT
  return i + 1;
}(global_var_check);

TEST(CHECKTest, TestPlacementsInCompoundStatements) {
  // check placement inside if/else clauses
  if (true) ABSL_TEST_CHECK(true);

  if (false)
    ;  // NOLINT
  else
    ABSL_TEST_CHECK(true);

  switch (0)
  case 0:
    ABSL_TEST_CHECK(true);  // NOLINT

  constexpr auto var = [](int i) {
    ABSL_TEST_CHECK(i > 0);  // NOLINT
    return i + 1;
  }(global_var);
  (void)var;
}

TEST(CHECKTest, TestBoolConvertible) {
  struct Tester {
  } tester;
  ABSL_TEST_CHECK([&]() { return &tester; }());
}

#if GTEST_HAS_DEATH_TEST

TEST(CHECKDeathTest, TestChecksWithSideEffects) {
  int var = 0;
  ABSL_TEST_CHECK([&var]() {
    ++var;
    return true;
  }());
  EXPECT_EQ(var, 1);

  EXPECT_DEATH(ABSL_TEST_CHECK([&var]() {
                 ++var;
                 return false;
               }()) << var,
               "Check failed: .* 2");
}

#endif  // GTEST_HAS_DEATH_TEST

template <int a, int b>
constexpr int sum() {
  return a + b;
}
#define MACRO_ONE 1
#define TEMPLATE_SUM(a, b) sum<a, b>()
#define CONCAT(a, b) a b
#define IDENTITY(x) x

TEST(CHECKTest, TestPassingMacroExpansion) {
  ABSL_TEST_CHECK(IDENTITY(true));
  ABSL_TEST_CHECK_EQ(TEMPLATE_SUM(MACRO_ONE, 2), 3);
  ABSL_TEST_CHECK_STREQ(CONCAT("x", "y"), "xy");
}

#if GTEST_HAS_DEATH_TEST

TEST(CHECKTest, TestMacroExpansionInMessage) {
  auto MessageGen = []() { ABSL_TEST_CHECK(IDENTITY(false)); };
  EXPECT_DEATH(MessageGen(), HasSubstr("IDENTITY(false)"));
}

TEST(CHECKTest, TestNestedMacroExpansionInMessage) {
  EXPECT_DEATH(ABSL_TEST_CHECK(IDENTITY(false)), HasSubstr("IDENTITY(false)"));
}

TEST(CHECKTest, TestMacroExpansionCompare) {
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(IDENTITY(false), IDENTITY(true)),
               HasSubstr("IDENTITY(false) == IDENTITY(true)"));
  EXPECT_DEATH(ABSL_TEST_CHECK_GT(IDENTITY(1), IDENTITY(2)),
               HasSubstr("IDENTITY(1) > IDENTITY(2)"));
}

TEST(CHECKTest, TestMacroExpansionStrCompare) {
  EXPECT_DEATH(ABSL_TEST_CHECK_STREQ(IDENTITY("x"), IDENTITY("y")),
               HasSubstr("IDENTITY(\"x\") == IDENTITY(\"y\")"));
  EXPECT_DEATH(ABSL_TEST_CHECK_STRCASENE(IDENTITY("a"), IDENTITY("A")),
               HasSubstr("IDENTITY(\"a\") != IDENTITY(\"A\")"));
}

TEST(CHECKTest, TestMacroExpansionStatus) {
  EXPECT_DEATH(
      ABSL_TEST_CHECK_OK(IDENTITY(absl::FailedPreconditionError("message"))),
      HasSubstr("IDENTITY(absl::FailedPreconditionError(\"message\"))"));
}

TEST(CHECKTest, TestMacroExpansionComma) {
  EXPECT_DEATH(ABSL_TEST_CHECK(TEMPLATE_SUM(MACRO_ONE, 2) == 4),
               HasSubstr("TEMPLATE_SUM(MACRO_ONE, 2) == 4"));
}

TEST(CHECKTest, TestMacroExpansionCommaCompare) {
  EXPECT_DEATH(
      ABSL_TEST_CHECK_EQ(TEMPLATE_SUM(2, MACRO_ONE), TEMPLATE_SUM(3, 2)),
      HasSubstr("TEMPLATE_SUM(2, MACRO_ONE) == TEMPLATE_SUM(3, 2)"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_GT(TEMPLATE_SUM(2, MACRO_ONE), TEMPLATE_SUM(3, 2)),
      HasSubstr("TEMPLATE_SUM(2, MACRO_ONE) > TEMPLATE_SUM(3, 2)"));
}

TEST(CHECKTest, TestMacroExpansionCommaStrCompare) {
  EXPECT_DEATH(ABSL_TEST_CHECK_STREQ(CONCAT("x", "y"), "z"),
               HasSubstr("CONCAT(\"x\", \"y\") == \"z\""));
  EXPECT_DEATH(ABSL_TEST_CHECK_STRNE(CONCAT("x", "y"), "xy"),
               HasSubstr("CONCAT(\"x\", \"y\") != \"xy\""));
}

#endif  // GTEST_HAS_DEATH_TEST

#undef TEMPLATE_SUM
#undef CONCAT
#undef MACRO
#undef ONE

#if GTEST_HAS_DEATH_TEST

TEST(CHECKDeachTest, TestOrderOfInvocationsBetweenCheckAndMessage) {
  int counter = 0;

  auto GetStr = [&counter]() -> std::string {
    return counter++ == 0 ? "" : "non-empty";
  };

  EXPECT_DEATH(ABSL_TEST_CHECK(!GetStr().empty()) << GetStr(),
               HasSubstr("non-empty"));
}

TEST(CHECKTest, TestSecondaryFailure) {
  auto FailingRoutine = []() {
    ABSL_TEST_CHECK(false) << "Secondary";
    return false;
  };
  EXPECT_DEATH(ABSL_TEST_CHECK(FailingRoutine()) << "Primary",
               AllOf(HasSubstr("Secondary"), Not(HasSubstr("Primary"))));
}

TEST(CHECKTest, TestSecondaryFailureInMessage) {
  auto MessageGen = []() {
    ABSL_TEST_CHECK(false) << "Secondary";
    return "Primary";
  };
  EXPECT_DEATH(ABSL_TEST_CHECK(false) << MessageGen(),
               AllOf(HasSubstr("Secondary"), Not(HasSubstr("Primary"))));
}

#endif  // GTEST_HAS_DEATH_TEST

TEST(CHECKTest, TestBinaryChecksWithPrimitives) {
  ABSL_TEST_CHECK_EQ(1, 1);
  ABSL_TEST_CHECK_NE(1, 2);
  ABSL_TEST_CHECK_GE(1, 1);
  ABSL_TEST_CHECK_GE(2, 1);
  ABSL_TEST_CHECK_LE(1, 1);
  ABSL_TEST_CHECK_LE(1, 2);
  ABSL_TEST_CHECK_GT(2, 1);
  ABSL_TEST_CHECK_LT(1, 2);
}

TEST(CHECKTest, TestBinaryChecksWithStringComparison) {
  const std::string a = "a";
  ABSL_TEST_CHECK_EQ(a, "a");
  ABSL_TEST_CHECK_NE(a, "b");
  ABSL_TEST_CHECK_GE(a, a);
  ABSL_TEST_CHECK_GE("b", a);
  ABSL_TEST_CHECK_LE(a, "a");
  ABSL_TEST_CHECK_LE(a, "b");
  ABSL_TEST_CHECK_GT("b", a);
  ABSL_TEST_CHECK_LT(a, "b");
}

// For testing using CHECK*() on anonymous enums.
enum { CASE_A, CASE_B };

TEST(CHECKTest, TestBinaryChecksWithEnumValues) {
  // Tests using CHECK*() on anonymous enums.
  ABSL_TEST_CHECK_EQ(CASE_A, CASE_A);
  ABSL_TEST_CHECK_NE(CASE_A, CASE_B);
  ABSL_TEST_CHECK_GE(CASE_A, CASE_A);
  ABSL_TEST_CHECK_GE(CASE_B, CASE_A);
  ABSL_TEST_CHECK_LE(CASE_A, CASE_A);
  ABSL_TEST_CHECK_LE(CASE_A, CASE_B);
  ABSL_TEST_CHECK_GT(CASE_B, CASE_A);
  ABSL_TEST_CHECK_LT(CASE_A, CASE_B);
}

TEST(CHECKTest, TestBinaryChecksWithNullptr) {
  const void* p_null = nullptr;
  const void* p_not_null = &p_null;
  ABSL_TEST_CHECK_EQ(p_null, nullptr);
  ABSL_TEST_CHECK_EQ(nullptr, p_null);
  ABSL_TEST_CHECK_NE(p_not_null, nullptr);
  ABSL_TEST_CHECK_NE(nullptr, p_not_null);
}

struct ExampleTypeThatHasNoStreamOperator {
  bool x;

  bool operator==(const ExampleTypeThatHasNoStreamOperator& other) const {
    return x == other.x;
  }
  bool operator==(const bool& other) const { return x == other; }
};

TEST(CHECKDeathTest, TestBinaryChecksWithUnprintable) {
  ExampleTypeThatHasNoStreamOperator a{true};
  ExampleTypeThatHasNoStreamOperator b{false};
  ABSL_TEST_CHECK_EQ(a, a);
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, b), "Check failed: a == b");
  ABSL_TEST_CHECK_EQ(a, true);
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, false),
               "Check failed: a == false \\(UNPRINTABLE vs. 0\\)");
}

#if GTEST_HAS_DEATH_TEST

// Test logging of various char-typed values by failing CHECK*().
TEST(CHECKDeathTest, TestComparingCharsValues) {
  {
    char a = ';';
    char b = 'b';
    EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. 'b'\\)");
    b = 1;
    EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. char value 1\\)");
  }
  {
    signed char a = ';';
    signed char b = 'b';
    EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. 'b'\\)");
    b = -128;
    EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. signed char value -128\\)");
  }
  {
    unsigned char a = ';';
    unsigned char b = 'b';
    EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. 'b'\\)");
    b = 128;
    EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, b),
                 "Check failed: a == b \\(';' vs. unsigned char value 128\\)");
  }
}

TEST(CHECKDeathTest, TestNullValuesAreReportedCleanly) {
  const char* a = nullptr;
  const char* b = nullptr;
  EXPECT_DEATH(ABSL_TEST_CHECK_NE(a, b),
               "Check failed: a != b \\(\\(null\\) vs. \\(null\\)\\)");

  a = "xx";
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(a, b),
               "Check failed: a == b \\(xx vs. \\(null\\)\\)");
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(b, a),
               "Check failed: b == a \\(\\(null\\) vs. xx\\)");

  std::nullptr_t n{};
  EXPECT_DEATH(ABSL_TEST_CHECK_NE(n, nullptr),
               "Check failed: n != nullptr \\(\\(null\\) vs. \\(null\\)\\)");
}

#endif  // GTEST_HAS_DEATH_TEST

TEST(CHECKTest, TestSTREQ) {
  ABSL_TEST_CHECK_STREQ("this", "this");
  ABSL_TEST_CHECK_STREQ(nullptr, nullptr);
  ABSL_TEST_CHECK_STRCASEEQ("this", "tHiS");
  ABSL_TEST_CHECK_STRCASEEQ(nullptr, nullptr);
  ABSL_TEST_CHECK_STRNE("this", "tHiS");
  ABSL_TEST_CHECK_STRNE("this", nullptr);
  ABSL_TEST_CHECK_STRCASENE("this", "that");
  ABSL_TEST_CHECK_STRCASENE(nullptr, "that");
  ABSL_TEST_CHECK_STREQ((std::string("a") + "b").c_str(), "ab");
  ABSL_TEST_CHECK_STREQ(std::string("test").c_str(),
                        (std::string("te") + std::string("st")).c_str());
}

TEST(CHECKTest, TestComparisonPlacementsInCompoundStatements) {
  // check placement inside if/else clauses
  if (true) ABSL_TEST_CHECK_EQ(1, 1);
  if (true) ABSL_TEST_CHECK_STREQ("c", "c");

  if (false)
    ;  // NOLINT
  else
    ABSL_TEST_CHECK_LE(0, 1);

  if (false)
    ;  // NOLINT
  else
    ABSL_TEST_CHECK_STRNE("a", "b");

  switch (0)
  case 0:
    ABSL_TEST_CHECK_NE(1, 0);

  switch (0)
  case 0:
    ABSL_TEST_CHECK_STRCASEEQ("A", "a");

  constexpr auto var = [](int i) {
    ABSL_TEST_CHECK_GT(i, 0);
    return i + 1;
  }(global_var);
  (void)var;

  // CHECK_STR... checks are not supported in constexpr routines.
  // constexpr auto var2 = [](int i) {
  //  ABSL_TEST_CHECK_STRNE("c", "d");
  //  return i + 1;
  // }(global_var);

#if defined(__GNUC__)
  int var3 = (({ ABSL_TEST_CHECK_LE(1, 2); }), global_var < 10) ? 1 : 0;
  (void)var3;

  int var4 = (({ ABSL_TEST_CHECK_STREQ("a", "a"); }), global_var < 10) ? 1 : 0;
  (void)var4;
#endif  // __GNUC__
}

TEST(CHECKTest, TestDCHECK) {
#ifdef NDEBUG
  ABSL_TEST_DCHECK(1 == 2) << " DCHECK's shouldn't be compiled in normal mode";
#endif
  ABSL_TEST_DCHECK(1 == 1);  // NOLINT(readability/check)
  ABSL_TEST_DCHECK_EQ(1, 1);
  ABSL_TEST_DCHECK_NE(1, 2);
  ABSL_TEST_DCHECK_GE(1, 1);
  ABSL_TEST_DCHECK_GE(2, 1);
  ABSL_TEST_DCHECK_LE(1, 1);
  ABSL_TEST_DCHECK_LE(1, 2);
  ABSL_TEST_DCHECK_GT(2, 1);
  ABSL_TEST_DCHECK_LT(1, 2);

  // Test DCHECK on std::nullptr_t
  const void* p_null = nullptr;
  const void* p_not_null = &p_null;
  ABSL_TEST_DCHECK_EQ(p_null, nullptr);
  ABSL_TEST_DCHECK_EQ(nullptr, p_null);
  ABSL_TEST_DCHECK_NE(p_not_null, nullptr);
  ABSL_TEST_DCHECK_NE(nullptr, p_not_null);
}

TEST(CHECKTest, TestQCHECK) {
  // The tests that QCHECK does the same as CHECK
  ABSL_TEST_QCHECK(1 == 1);  // NOLINT(readability/check)
  ABSL_TEST_QCHECK_EQ(1, 1);
  ABSL_TEST_QCHECK_NE(1, 2);
  ABSL_TEST_QCHECK_GE(1, 1);
  ABSL_TEST_QCHECK_GE(2, 1);
  ABSL_TEST_QCHECK_LE(1, 1);
  ABSL_TEST_QCHECK_LE(1, 2);
  ABSL_TEST_QCHECK_GT(2, 1);
  ABSL_TEST_QCHECK_LT(1, 2);

  // Tests using QCHECK*() on anonymous enums.
  ABSL_TEST_QCHECK_EQ(CASE_A, CASE_A);
  ABSL_TEST_QCHECK_NE(CASE_A, CASE_B);
  ABSL_TEST_QCHECK_GE(CASE_A, CASE_A);
  ABSL_TEST_QCHECK_GE(CASE_B, CASE_A);
  ABSL_TEST_QCHECK_LE(CASE_A, CASE_A);
  ABSL_TEST_QCHECK_LE(CASE_A, CASE_B);
  ABSL_TEST_QCHECK_GT(CASE_B, CASE_A);
  ABSL_TEST_QCHECK_LT(CASE_A, CASE_B);
}

TEST(CHECKTest, TestQCHECKPlacementsInCompoundStatements) {
  // check placement inside if/else clauses
  if (true) ABSL_TEST_QCHECK(true);

  if (false)
    ;  // NOLINT
  else
    ABSL_TEST_QCHECK(true);

  if (false)
    ;  // NOLINT
  else
    ABSL_TEST_QCHECK(true);

  switch (0)
  case 0:
    ABSL_TEST_QCHECK(true);

  constexpr auto var = [](int i) {
    ABSL_TEST_QCHECK(i > 0);  // NOLINT
    return i + 1;
  }(global_var);
  (void)var;

#if defined(__GNUC__)
  int var2 = (({ ABSL_TEST_CHECK_LE(1, 2); }), global_var < 10) ? 1 : 0;
  (void)var2;
#endif  // __GNUC__
}

class ComparableType {
 public:
  explicit ComparableType(int v) : v_(v) {}

  void MethodWithCheck(int i) {
    ABSL_TEST_CHECK_EQ(*this, i);
    ABSL_TEST_CHECK_EQ(i, *this);
  }

  int Get() const { return v_; }

 private:
  friend bool operator==(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ == rhs.v_;
  }
  friend bool operator!=(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ != rhs.v_;
  }
  friend bool operator<(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ < rhs.v_;
  }
  friend bool operator<=(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ <= rhs.v_;
  }
  friend bool operator>(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ > rhs.v_;
  }
  friend bool operator>=(const ComparableType& lhs, const ComparableType& rhs) {
    return lhs.v_ >= rhs.v_;
  }
  friend bool operator==(const ComparableType& lhs, int rhs) {
    return lhs.v_ == rhs;
  }
  friend bool operator==(int lhs, const ComparableType& rhs) {
    return lhs == rhs.v_;
  }

  friend std::ostream& operator<<(std::ostream& out, const ComparableType& v) {
    return out << "ComparableType{" << v.Get() << "}";
  }

  int v_;
};

TEST(CHECKTest, TestUserDefinedCompOp) {
  ABSL_TEST_CHECK_EQ(ComparableType{0}, ComparableType{0});
  ABSL_TEST_CHECK_NE(ComparableType{1}, ComparableType{2});
  ABSL_TEST_CHECK_LT(ComparableType{1}, ComparableType{2});
  ABSL_TEST_CHECK_LE(ComparableType{1}, ComparableType{2});
  ABSL_TEST_CHECK_GT(ComparableType{2}, ComparableType{1});
  ABSL_TEST_CHECK_GE(ComparableType{2}, ComparableType{2});
}

TEST(CHECKTest, TestCheckInMethod) {
  ComparableType v{1};
  v.MethodWithCheck(1);
}

TEST(CHECKDeathTest, TestUserDefinedStreaming) {
  ComparableType v1{1};
  ComparableType v2{2};

  EXPECT_DEATH(
      ABSL_TEST_CHECK_EQ(v1, v2),
      HasSubstr(
          "Check failed: v1 == v2 (ComparableType{1} vs. ComparableType{2})"));
}

// A type that can be printed using AbslStringify.
struct StringifiableType {
  int x = 0;
  explicit StringifiableType(int x) : x(x) {}
  friend bool operator==(const StringifiableType& lhs,
                         const StringifiableType& rhs) {
    return lhs.x == rhs.x;
  }
  friend bool operator!=(const StringifiableType& lhs,
                         const StringifiableType& rhs) {
    return lhs.x != rhs.x;
  }
  friend bool operator<(const StringifiableType& lhs,
                        const StringifiableType& rhs) {
    return lhs.x < rhs.x;
  }
  friend bool operator>(const StringifiableType& lhs,
                        const StringifiableType& rhs) {
    return lhs.x > rhs.x;
  }
  friend bool operator<=(const StringifiableType& lhs,
                         const StringifiableType& rhs) {
    return lhs.x <= rhs.x;
  }
  friend bool operator>=(const StringifiableType& lhs,
                         const StringifiableType& rhs) {
    return lhs.x >= rhs.x;
  }
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const StringifiableType& obj) {
    absl::Format(&sink, "StringifiableType{%d}", obj.x);
  }

  // Make sure no unintended copy happens.
  StringifiableType(const StringifiableType&) = delete;
};

TEST(CHECKTest, TestUserDefinedAbslStringify) {
  const StringifiableType v1(1);
  const StringifiableType v2(2);

  ABSL_TEST_CHECK_EQ(v1, v1);
  ABSL_TEST_CHECK_NE(v1, v2);
  ABSL_TEST_CHECK_LT(v1, v2);
  ABSL_TEST_CHECK_LE(v1, v2);
  ABSL_TEST_CHECK_GT(v2, v1);
  ABSL_TEST_CHECK_GE(v2, v1);
}

TEST(CHECKDeathTest, TestUserDefinedAbslStringify) {
  const StringifiableType v1(1);
  const StringifiableType v2(2);

  // Returns a matcher for the expected check failure message when comparing two
  // values.
  auto expected_output = [](int lhs, absl::string_view condition, int rhs) {
    return HasSubstr(
        absl::Substitute("Check failed: v$0 $1 v$2 (StringifiableType{$0} vs. "
                         "StringifiableType{$2})",
                         lhs, condition, rhs));
  };
  // Test comparisons where the check fails.
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(v1, v2), expected_output(1, "==", 2));
  EXPECT_DEATH(ABSL_TEST_CHECK_NE(v1, v1), expected_output(1, "!=", 1));
  EXPECT_DEATH(ABSL_TEST_CHECK_LT(v2, v1), expected_output(2, "<", 1));
  EXPECT_DEATH(ABSL_TEST_CHECK_LE(v2, v1), expected_output(2, "<=", 1));
  EXPECT_DEATH(ABSL_TEST_CHECK_GT(v1, v2), expected_output(1, ">", 2));
  EXPECT_DEATH(ABSL_TEST_CHECK_GE(v1, v2), expected_output(1, ">=", 2));
}

// A type that can be printed using both AbslStringify and operator<<.
struct StringifiableStreamableType {
  int x = 0;
  explicit StringifiableStreamableType(int x) : x(x) {}

  friend bool operator==(const StringifiableStreamableType& lhs,
                         const StringifiableStreamableType& rhs) {
    return lhs.x == rhs.x;
  }
  friend bool operator!=(const StringifiableStreamableType& lhs,
                         const StringifiableStreamableType& rhs) {
    return lhs.x != rhs.x;
  }
  template <typename Sink>
  friend void AbslStringify(Sink& sink,
                            const StringifiableStreamableType& obj) {
    absl::Format(&sink, "Strigified{%d}", obj.x);
  }
  friend std::ostream& operator<<(std::ostream& out,
                                  const StringifiableStreamableType& obj) {
    return out << "Streamed{" << obj.x << "}";
  }

  // Avoid unintentional copy.
  StringifiableStreamableType(const StringifiableStreamableType&) = delete;
};

TEST(CHECKDeathTest, TestStreamingPreferredOverAbslStringify) {
  StringifiableStreamableType v1(1);
  StringifiableStreamableType v2(2);

  EXPECT_DEATH(
      ABSL_TEST_CHECK_EQ(v1, v2),
      HasSubstr("Check failed: v1 == v2 (Streamed{1} vs. Streamed{2})"));
}

// A type whose pointer can be passed to AbslStringify.
struct PointerIsStringifiable {};
template <typename Sink>
void AbslStringify(Sink& sink, const PointerIsStringifiable* var) {
  sink.Append("PointerIsStringifiable");
}

// Verifies that a pointer is printed as a number despite having AbslStringify
// defined. Users may implement AbslStringify that dereferences the pointer, and
// doing so as part of DCHECK would not be good.
TEST(CHECKDeathTest, TestPointerPrintedAsNumberDespiteAbslStringify) {
  const auto* p = reinterpret_cast<const PointerIsStringifiable*>(0x1234);

  EXPECT_DEATH(
      ABSL_TEST_CHECK_EQ(p, nullptr),
      AnyOf(
          HasSubstr("Check failed: p == nullptr (0000000000001234 vs. (null))"),
          HasSubstr("Check failed: p == nullptr (0x1234 vs. (null))")));
}

// An uncopyable object with operator<<.
struct Uncopyable {
  int x;
  explicit Uncopyable(int x) : x(x) {}
  Uncopyable(const Uncopyable&) = delete;
  friend bool operator==(const Uncopyable& lhs, const Uncopyable& rhs) {
    return lhs.x == rhs.x;
  }
  friend bool operator!=(const Uncopyable& lhs, const Uncopyable& rhs) {
    return lhs.x != rhs.x;
  }
  friend std::ostream& operator<<(std::ostream& os, const Uncopyable& obj) {
    return os << "Uncopyable{" << obj.x << "}";
  }
};

// Test that an uncopyable object can be used.
// Will catch us if implementation has an unintended copy.
TEST(CHECKDeathTest, TestUncopyable) {
  const Uncopyable v1(1);
  const Uncopyable v2(2);

  EXPECT_DEATH(
      ABSL_TEST_CHECK_EQ(v1, v2),
      HasSubstr("Check failed: v1 == v2 (Uncopyable{1} vs. Uncopyable{2})"));
}

enum class ScopedEnum { kValue1 = 1, kValue2 = 2 };

TEST(CHECKTest, TestScopedEnumComparisonChecks) {
  ABSL_TEST_CHECK_EQ(ScopedEnum::kValue1, ScopedEnum::kValue1);
  ABSL_TEST_CHECK_NE(ScopedEnum::kValue1, ScopedEnum::kValue2);
  ABSL_TEST_CHECK_LT(ScopedEnum::kValue1, ScopedEnum::kValue2);
  ABSL_TEST_CHECK_LE(ScopedEnum::kValue1, ScopedEnum::kValue2);
  ABSL_TEST_CHECK_GT(ScopedEnum::kValue2, ScopedEnum::kValue1);
  ABSL_TEST_CHECK_GE(ScopedEnum::kValue2, ScopedEnum::kValue2);
  ABSL_TEST_DCHECK_EQ(ScopedEnum::kValue1, ScopedEnum::kValue1);
  ABSL_TEST_DCHECK_NE(ScopedEnum::kValue1, ScopedEnum::kValue2);
  ABSL_TEST_DCHECK_LT(ScopedEnum::kValue1, ScopedEnum::kValue2);
  ABSL_TEST_DCHECK_LE(ScopedEnum::kValue1, ScopedEnum::kValue2);
  ABSL_TEST_DCHECK_GT(ScopedEnum::kValue2, ScopedEnum::kValue1);
  ABSL_TEST_DCHECK_GE(ScopedEnum::kValue2, ScopedEnum::kValue2);

  // Check that overloads work correctly with references as well.
  const ScopedEnum x = ScopedEnum::kValue1;
  const ScopedEnum& x_ref = x;
  ABSL_TEST_CHECK_EQ(x, x_ref);
  ABSL_TEST_CHECK_EQ(x_ref, x_ref);
}

#if GTEST_HAS_DEATH_TEST
TEST(CHECKDeathTest, TestScopedEnumCheckFailureMessagePrintsIntegerValues) {
  const auto e1 = ScopedEnum::kValue1;
  const auto e2 = ScopedEnum::kValue2;
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(e1, e2),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 2\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_NE(e1, e1),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 1\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_GT(e1, e1),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 1\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_GE(e1, e2),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 2\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_LT(e2, e2),
               ContainsRegex(R"re(Check failed:.*\(2 vs. 2\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_LE(e2, e1),
               ContainsRegex(R"re(Check failed:.*\(2 vs. 1\))re"));

  const auto& e1_ref = e1;
  EXPECT_DEATH(ABSL_TEST_CHECK_NE(e1_ref, e1),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 1\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_NE(e1_ref, e1_ref),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 1\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(e2, e1_ref),
               ContainsRegex(R"re(Check failed:.*\(2 vs. 1\))re"));

#ifndef NDEBUG
  EXPECT_DEATH(ABSL_TEST_DCHECK_EQ(e2, e1),
               ContainsRegex(R"re(Check failed:.*\(2 vs. 1\))re"));
#else
  // DHECK_EQ is not evaluated in non-debug mode.
  ABSL_TEST_DCHECK_EQ(e2, e1);
#endif  // NDEBUG
}
#endif  // GTEST_HAS_DEATH_TEST

enum class ScopedInt8Enum : int8_t {
  kValue1 = 1,
  kValue2 = 66  // Printable ascii value 'B'.
};

TEST(CHECKDeathTest, TestScopedInt8EnumCheckFailureMessagePrintsCharValues) {
  const auto e1 = ScopedInt8Enum::kValue1;
  const auto e2 = ScopedInt8Enum::kValue2;
  EXPECT_DEATH(
      ABSL_TEST_CHECK_EQ(e1, e2),
      ContainsRegex(R"re(Check failed:.*\(signed char value 1 vs. 'B'\))re"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_NE(e1, e1),
      ContainsRegex(
          R"re(Check failed:.*\(signed char value 1 vs. signed char value 1\))re"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_GT(e1, e1),
      ContainsRegex(
          R"re(Check failed:.*\(signed char value 1 vs. signed char value 1\))re"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_GE(e1, e2),
      ContainsRegex(R"re(Check failed:.*\(signed char value 1 vs. 'B'\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_LT(e2, e2),
               ContainsRegex(R"re(Check failed:.*\('B' vs. 'B'\))re"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_LE(e2, e1),
      ContainsRegex(R"re(Check failed:.*\('B' vs. signed char value 1\))re"));
}

enum class ScopedUnsignedEnum : uint16_t {
  kValue1 = std::numeric_limits<uint16_t>::min(),
  kValue2 = std::numeric_limits<uint16_t>::max()
};

TEST(CHECKDeathTest,
     TestScopedUnsignedEnumCheckFailureMessagePrintsCorrectValues) {
  const auto e1 = ScopedUnsignedEnum::kValue1;
  const auto e2 = ScopedUnsignedEnum::kValue2;
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(e1, e2),
               ContainsRegex(R"re(Check failed:.*\(0 vs. 65535\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_NE(e1, e1),
               ContainsRegex(R"re(Check failed:.*\(0 vs. 0\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_GT(e1, e1),
               ContainsRegex(R"re(Check failed:.*\(0 vs. 0\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_GE(e1, e2),
               ContainsRegex(R"re(Check failed:.*\(0 vs. 65535\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_LT(e1, e1),
               ContainsRegex(R"re(Check failed:.*\(0 vs. 0\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_LE(e2, e1),
               ContainsRegex(R"re(Check failed:.*\(65535 vs. 0\))re"));
}

enum class ScopedInt64Enum : int64_t {
  kMin = std::numeric_limits<int64_t>::min(),
  kMax = std::numeric_limits<int64_t>::max(),
};

// Tests that int64-backed enums are printed correctly even for very large and
// very small values.
TEST(CHECKDeathTest, TestScopedInt64EnumCheckFailureMessage) {
  const auto min = ScopedInt64Enum::kMin;
  const auto max = ScopedInt64Enum::kMax;
  EXPECT_DEATH(
      ABSL_TEST_CHECK_EQ(max, min),
      ContainsRegex(
          "Check failed:.*9223372036854775807 vs. -9223372036854775808"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_NE(max, max),
      ContainsRegex(
          "Check failed:.*9223372036854775807 vs. 9223372036854775807"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_GT(min, min),
      ContainsRegex(
          "Check failed:.*-9223372036854775808 vs. -9223372036854775808"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_GE(min, max),
      ContainsRegex(
          R"(Check failed:.*-9223372036854775808 vs. 9223372036854775807)"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_LT(max, max),
      ContainsRegex(
          R"(Check failed:.*9223372036854775807 vs. 9223372036854775807)"));
  EXPECT_DEATH(
      ABSL_TEST_CHECK_LE(max, min),
      ContainsRegex(
          R"(Check failed:.*9223372036854775807 vs. -9223372036854775808)"));
}

enum class ScopedBoolEnum : bool {
  kFalse,
  kTrue,
};

TEST(CHECKDeathTest, TestScopedBoolEnumCheckFailureMessagePrintsCorrectValues) {
  const auto t = ScopedBoolEnum::kTrue;
  const auto f = ScopedBoolEnum::kFalse;
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(t, f),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 0\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_NE(f, f),
               ContainsRegex(R"re(Check failed:.*\(0 vs. 0\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_GT(f, f),
               ContainsRegex(R"re(Check failed:.*\(0 vs. 0\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_GE(f, t),
               ContainsRegex(R"re(Check failed:.*\(0 vs. 1\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_LT(t, t),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 1\))re"));
  EXPECT_DEATH(ABSL_TEST_CHECK_LE(t, f),
               ContainsRegex(R"re(Check failed:.*\(1 vs. 0\))re"));
}

enum class ScopedEnumWithAbslStringify {
  kValue1 = 1,
  kValue2 = 2,
  kValue3 = 3
};

template <typename Sink>
void AbslStringify(Sink& sink, ScopedEnumWithAbslStringify v) {
  switch (v) {
    case ScopedEnumWithAbslStringify::kValue1:
      sink.Append("AbslStringify: kValue1");
      break;
    case ScopedEnumWithAbslStringify::kValue2:
      sink.Append("AbslStringify: kValue2");
      break;
    case ScopedEnumWithAbslStringify::kValue3:
      sink.Append("AbslStringify: kValue3");
      break;
  }
}

#if GTEST_HAS_DEATH_TEST
TEST(CHECKDeathTest, TestScopedEnumUsesAbslStringify) {
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(ScopedEnumWithAbslStringify::kValue1,
                                  ScopedEnumWithAbslStringify::kValue2),
               ContainsRegex("Check failed:.*AbslStringify: kValue1 vs. "
                             "AbslStringify: kValue2"));
}
#endif  // GTEST_HAS_DEATH_TEST

enum class ScopedEnumWithOutputOperator {
  kValue1 = 1,
  kValue2 = 2,
};

std::ostream& operator<<(std::ostream& os, ScopedEnumWithOutputOperator v) {
  switch (v) {
    case ScopedEnumWithOutputOperator::kValue1:
      os << "OutputOperator: kValue1";
      break;
    case ScopedEnumWithOutputOperator::kValue2:
      os << "OutputOperator: kValue2";
      break;
  }
  return os;
}

#if GTEST_HAS_DEATH_TEST
TEST(CHECKDeathTest, TestOutputOperatorIsUsedForScopedEnum) {
  EXPECT_DEATH(ABSL_TEST_CHECK_EQ(ScopedEnumWithOutputOperator::kValue1,
                                  ScopedEnumWithOutputOperator::kValue2),
               ContainsRegex("Check failed:.*OutputOperator: kValue1 vs. "
                             "OutputOperator: kValue2"));
}
#endif  // GTEST_HAS_DEATH_TEST

enum class ScopedEnumWithAbslStringifyAndOutputOperator {
  kValue1 = 1,
  kValue2 = 2,
};

template <typename Sink>
void AbslStringify(Sink& sink, ScopedEnumWithAbslStringifyAndOutputOperator v) {
  switch (v) {
    case ScopedEnumWithAbslStringifyAndOutputOperator::kValue1:
      sink.Append("AbslStringify: kValue1");
      break;
    case ScopedEnumWithAbslStringifyAndOutputOperator::kValue2:
      sink.Append("AbslStringify: kValue2");
      break;
  }
}

std::ostream& operator<<(std::ostream& os,
                         ScopedEnumWithAbslStringifyAndOutputOperator v) {
  switch (v) {
    case ScopedEnumWithAbslStringifyAndOutputOperator::kValue1:
      os << "OutputOperator: kValue1";
      break;
    case ScopedEnumWithAbslStringifyAndOutputOperator::kValue2:
      os << "OutputOperator: kValue2";
      break;
  }
  return os;
}

#if GTEST_HAS_DEATH_TEST

// Test that, if operator<< and AbslStringify are both defined for a scoped
// enum, streaming takes precedence over AbslStringify.
TEST(CHECKDeathTest, TestScopedEnumPrefersOutputOperatorOverAbslStringify) {
  EXPECT_DEATH(
      ABSL_TEST_CHECK_EQ(ScopedEnumWithAbslStringifyAndOutputOperator::kValue1,
                         ScopedEnumWithAbslStringifyAndOutputOperator::kValue2),
      ContainsRegex("Check failed:.*OutputOperator: kValue1 vs. "
                    "OutputOperator: kValue2"));
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace absl_log_internal

// NOLINTEND(misc-definitions-in-headers)

#endif  // ABSL_LOG_CHECK_TEST_IMPL_H_
