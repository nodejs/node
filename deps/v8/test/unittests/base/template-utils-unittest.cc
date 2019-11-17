// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/template-utils.h"

#include "test/unittests/test-utils.h"

namespace v8 {
namespace base {
namespace template_utils_unittest {

////////////////////////////
// Test make_array.
////////////////////////////

namespace {
template <typename T, size_t Size>
void CheckArrayEquals(const std::array<T, Size>& arr1,
                      const std::array<T, Size>& arr2) {
  for (size_t i = 0; i < Size; ++i) {
    CHECK_EQ(arr1[i], arr2[i]);
  }
}
}  // namespace

TEST(TemplateUtilsTest, MakeArraySimple) {
  auto computed_array = base::make_array<3>([](int i) { return 1 + (i * 3); });
  std::array<int, 3> expected{{1, 4, 7}};
  CheckArrayEquals(computed_array, expected);
}

namespace {
constexpr int doubleIntValue(int i) { return i * 2; }
}  // namespace

TEST(TemplateUtilsTest, MakeArrayConstexpr) {
  constexpr auto computed_array = base::make_array<3>(doubleIntValue);
  constexpr std::array<int, 3> expected{{0, 2, 4}};
  CheckArrayEquals(computed_array, expected);
}

////////////////////////////
// Test pass_value_or_ref.
////////////////////////////

// Wrap into this helper struct, such that the type is printed on errors.
template <typename T1, typename T2>
struct CheckIsSame {
  static_assert(std::is_same<T1, T2>::value, "test failure");
};

#define TEST_PASS_VALUE_OR_REF0(remove_extend, expected, given)               \
  static_assert(                                                              \
      sizeof(CheckIsSame<expected,                                            \
                         pass_value_or_ref<given, remove_extend>::type>) > 0, \
      "check")

#define TEST_PASS_VALUE_OR_REF(expected, given)                          \
  static_assert(                                                         \
      sizeof(CheckIsSame<expected, pass_value_or_ref<given>::type>) > 0, \
      "check")

TEST_PASS_VALUE_OR_REF(int, int&);
TEST_PASS_VALUE_OR_REF(int, int&&);
TEST_PASS_VALUE_OR_REF(const char*, const char[14]);
TEST_PASS_VALUE_OR_REF(const char*, const char*&&);
TEST_PASS_VALUE_OR_REF(const char*, const char (&)[14]);
TEST_PASS_VALUE_OR_REF(const std::string&, std::string);
TEST_PASS_VALUE_OR_REF(const std::string&, std::string&);
TEST_PASS_VALUE_OR_REF(const std::string&, const std::string&);
TEST_PASS_VALUE_OR_REF(int, const int);
TEST_PASS_VALUE_OR_REF(int, const int&);
TEST_PASS_VALUE_OR_REF(const int*, const int*);
TEST_PASS_VALUE_OR_REF(const int*, const int* const);
TEST_PASS_VALUE_OR_REF0(false, const char[14], const char[14]);
TEST_PASS_VALUE_OR_REF0(false, const char[14], const char (&)[14]);
TEST_PASS_VALUE_OR_REF0(false, const std::string&, std::string);
TEST_PASS_VALUE_OR_REF0(false, const std::string&, std::string&);
TEST_PASS_VALUE_OR_REF0(false, const std::string&, const std::string&);
TEST_PASS_VALUE_OR_REF0(false, int, const int);
TEST_PASS_VALUE_OR_REF0(false, int, const int&);

//////////////////////////////
// Test has_output_operator.
//////////////////////////////

// Intrinsic types:
static_assert(has_output_operator<int>::value, "int can be output");
static_assert(has_output_operator<void*>::value, "void* can be output");
static_assert(has_output_operator<uint64_t>::value, "int can be output");

// Classes:
class TestClass1 {};
class TestClass2 {};
extern std::ostream& operator<<(std::ostream& str, const TestClass2&);
class TestClass3 {};
extern std::ostream& operator<<(std::ostream& str, TestClass3);
static_assert(!has_output_operator<TestClass1>::value,
              "TestClass1 can not be output");
static_assert(has_output_operator<TestClass2>::value,
              "non-const TestClass2 can be output");
static_assert(has_output_operator<const TestClass2>::value,
              "const TestClass2 can be output");
static_assert(has_output_operator<TestClass3>::value,
              "non-const TestClass3 can be output");
static_assert(has_output_operator<const TestClass3>::value,
              "const TestClass3 can be output");

//////////////////////////////
// Test fold.
//////////////////////////////

struct FoldAllSameType {
  constexpr uint32_t operator()(uint32_t a, uint32_t b) const { return a | b; }
};
static_assert(base::fold(FoldAllSameType{}, 3, 6) == 7, "check fold");
// Test that it works if implicit conversion is needed for one of the
// parameters.
static_assert(base::fold(FoldAllSameType{}, uint8_t{1}, 256) == 257,
              "check correct type inference");
// Test a single parameter.
static_assert(base::fold(FoldAllSameType{}, 25) == 25,
              "check folding a single argument");

TEST(TemplateUtilsTest, FoldDifferentType) {
  auto fn = [](std::string str, char c) {
    str.push_back(c);
    return str;
  };
  CHECK_EQ(base::fold(fn, std::string("foo"), 'b', 'a', 'r'), "foobar");
}

TEST(TemplateUtilsTest, FoldMoveOnlyType) {
  auto fn = [](std::unique_ptr<std::string> str, char c) {
    str->push_back(c);
    return str;
  };
  std::unique_ptr<std::string> str = std::make_unique<std::string>("foo");
  std::unique_ptr<std::string> folded =
      base::fold(fn, std::move(str), 'b', 'a', 'r');
  CHECK_NULL(str);
  CHECK_NOT_NULL(folded);
  CHECK_EQ(*folded, "foobar");
}

struct TemplatizedFoldFunctor {
  template <typename T, typename... Tup>
  std::tuple<Tup..., typename std::decay<T>::type> operator()(
      std::tuple<Tup...> tup, T&& val) {
    return std::tuple_cat(std::move(tup),
                          std::make_tuple(std::forward<T>(val)));
  }
};
TEST(TemplateUtilsTest, FoldToTuple) {
  auto input = std::make_tuple(char{'x'}, int{4}, double{3.2},
                               std::unique_ptr<uint8_t>{}, std::string{"foo"});
  auto result =
      base::fold(TemplatizedFoldFunctor{}, std::make_tuple(),
                 std::get<0>(input), std::get<1>(input), std::get<2>(input),
                 std::unique_ptr<uint8_t>{}, std::get<4>(input));
  static_assert(std::is_same<decltype(result), decltype(input)>::value,
                "the resulting tuple should have the same type as the input");
  DCHECK_EQ(input, result);
}

}  // namespace template_utils_unittest
}  // namespace base
}  // namespace v8
