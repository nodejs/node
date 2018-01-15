// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/template-utils.h"

#include "test/unittests/test-utils.h"

namespace v8 {
namespace base {

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
};  // namespace

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

}  // namespace base
}  // namespace v8
