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

#include "absl/strings/internal/utf8.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/port.h"
#include "absl/strings/string_view.h"

namespace {

using ::absl::strings_internal::kMaxEncodedUTF8Size;
using ::absl::strings_internal::ShiftState;
using ::absl::strings_internal::WideToUtf8;
using ::testing::StartsWith;
using ::testing::TestParamInfo;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

#if !defined(__cpp_char8_t)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++2a-compat"
#endif
TEST(EncodeUTF8Char, BasicFunction) {
  std::pair<char32_t, std::string> tests[] = {{0x0030, u8"\u0030"},
                                              {0x00A3, u8"\u00A3"},
                                              {0x00010000, u8"\U00010000"},
                                              {0x0000FFFF, u8"\U0000FFFF"},
                                              {0x0010FFFD, u8"\U0010FFFD"}};
  for (auto& test : tests) {
    char buf0[7] = {'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'};
    char buf1[7] = {'\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF', '\xFF'};
    char* buf0_written =
        &buf0[absl::strings_internal::EncodeUTF8Char(buf0, test.first)];
    char* buf1_written =
        &buf1[absl::strings_internal::EncodeUTF8Char(buf1, test.first)];
    int apparent_length = 7;
    while (buf0[apparent_length - 1] == '\x00' &&
           buf1[apparent_length - 1] == '\xFF') {
      if (--apparent_length == 0) break;
    }
    EXPECT_EQ(apparent_length, buf0_written - buf0);
    EXPECT_EQ(apparent_length, buf1_written - buf1);
    EXPECT_EQ(apparent_length, test.second.length());
    EXPECT_EQ(std::string(buf0, apparent_length), test.second);
    EXPECT_EQ(std::string(buf1, apparent_length), test.second);
  }
  char buf[32] = "Don't Tread On Me";
  EXPECT_LE(absl::strings_internal::EncodeUTF8Char(buf, 0x00110000),
            absl::strings_internal::kMaxEncodedUTF8Size);
  char buf2[32] = "Negative is invalid but sane";
  EXPECT_LE(absl::strings_internal::EncodeUTF8Char(buf2, -1),
            absl::strings_internal::kMaxEncodedUTF8Size);
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif  // !defined(__cpp_char8_t)

struct WideToUtf8TestCase {
  std::string description;
  wchar_t input;
  std::string expected_utf8_str;
  size_t expected_bytes_written;
  ShiftState initial_state = {false, 0};
  ShiftState expected_state = {false, 0};
};

std::vector<WideToUtf8TestCase> GetWideToUtf8TestCases() {
  constexpr size_t kError = static_cast<size_t>(-1);
  std::vector<WideToUtf8TestCase> cases = {
      {"ASCII_A", L'A', "A", 1},
      {"NullChar", L'\0', std::string("\0", 1), 1},
      {"ASCII_Max_7F", L'\x7F', "\x7F", 1},

      {"TwoByte_Min_80", L'\u0080', "\xC2\x80", 2},
      {"PoundSign_A3", L'\u00A3', "\xC2\xA3", 2},
      {"TwoByte_Max_7FF", L'\u07FF', "\xDF\xBF", 2},

      {"ThreeByte_Min_800", L'\u0800', "\xE0\xA0\x80", 3},
      {"EuroSign_20AC", L'\u20AC', "\xE2\x82\xAC", 3},
      {"BMP_MaxBeforeSurrogates_D7FF", L'\uD7FF', "\xED\x9F\xBF", 3},
      {"BMP_FFFF", L'\uFFFF', "\xEF\xBF\xBF", 3},

      {"IsolatedHighSurr_D800", L'\xD800', "\xF0\x90", 2, {}, {true, 0}},
      {"IsolatedHighSurr_DBFF", L'\xDBFF', "\xF4\x8F", 2, {}, {true, 3}},

      {"HighSurr_D800_after_HighD800",
       L'\xD800',
       "\xF0\x90",
       2,
       {true, 0},
       {true, 0}},
      {"HighSurr_DBFF_after_HighDBFF",
       L'\xDBFF',
       "\xF4\x8F",
       2,
       {true, 3},
       {true, 3}},

      {"LowSurr_DC00_after_HighD800", L'\xDC00', "\x80\x80", 2, {true, 0}, {}},
      {"LowSurr_DFFD_after_HighDBFF", L'\xDFFD', "\xBF\xBD", 2, {true, 3}, {}},
      {"LowSurr_DC00_with_InitialState_saw_high_bits_1",
       L'\xDC00',
       "\x90\x80",
       2,
       {true, 1},
       {}},

      // Final state = initial on error.
      {"Error_IsolatedLowSurr_DC00_NoPriorHigh", L'\xDC00', "", kError, {}, {}},
      {"Error_IsolatedLowSurr_DFFF_NoPriorHigh", L'\xDFFF', "", kError, {}, {}},

#if (defined(WCHAR_MAX) && WCHAR_MAX > 0xFFFF)
      {"DirectSupplementaryChars_U10000", static_cast<wchar_t>(0x10000),
       "\xF0\x90\x80\x80", 4},
      {"DirectSupplementaryChars_U10FFFD", static_cast<wchar_t>(0x10FFFD),
       "\xF4\x8F\xBF\xBD", 4},
#endif
  };

  wchar_t minus_one = static_cast<wchar_t>(-1);
  if constexpr (sizeof(wchar_t) == 2) {
    cases.push_back({"WChar_MinusOne_as_FFFF", minus_one, "\xEF\xBF\xBF", 3});
  } else {
    cases.push_back(
        {"Error_WChar_MinusOne_as_FFFFFFFF", minus_one, "", kError, {}, {}});
  }

  if constexpr (sizeof(wchar_t) >= 4) {
#ifdef WCHAR_MAX
    if (static_cast<uintmax_t>(WCHAR_MAX) >= 0x110000UL) {
      cases.push_back({"Error_OutOfRange_110000",
                       static_cast<wchar_t>(0x110000UL),
                       "",
                       kError,
                       {},
                       {}});
    }
#else
    cases.push_back({"Error_OutOfRange_110000_fallback",
                     static_cast<wchar_t>(0x110000UL),
                     "",
                     kError,
                     {},
                     {}});
#endif
  }
  return cases;
}

class WideToUtf8ParamTest : public TestWithParam<WideToUtf8TestCase> {};

TEST_P(WideToUtf8ParamTest, SingleCharConversion) {
  const auto& test_case = GetParam();
  ShiftState state = test_case.initial_state;
  constexpr char kFillChar = '\xAB';
  std::string buffer(32, kFillChar);

  size_t bytes_written = WideToUtf8(test_case.input, buffer.data(), state);

  EXPECT_EQ(bytes_written, test_case.expected_bytes_written);
  EXPECT_THAT(buffer, StartsWith(test_case.expected_utf8_str));

  // The remaining bytes should be unchanged.
  ASSERT_LT(test_case.expected_utf8_str.length(), buffer.size());
  EXPECT_EQ(buffer[test_case.expected_utf8_str.length()], kFillChar);

  EXPECT_EQ(state.saw_high_surrogate,
            test_case.expected_state.saw_high_surrogate);
  EXPECT_EQ(state.bits, test_case.expected_state.bits);
}

INSTANTIATE_TEST_SUITE_P(WideCharToUtf8Conversion, WideToUtf8ParamTest,
                         ValuesIn(GetWideToUtf8TestCases()),
                         [](auto info) { return info.param.description; });

// Comprehensive test string for validating wchar_t to UTF-8 conversion.
// This string is designed to cover a variety of Unicode character types and
// sequences:
// 1. Basic ASCII characters (within names, numbers, and spacing).
// 2. Common 2-byte UTF-8 sequences:
//    - Accented Latin characters (e.g., 'á' in "Holá").
//    - Hebrew text with combining vowel points (e.g., "שָׁלוֹם").
// 3. Common 3-byte UTF-8 sequences:
//    - Currency symbols (e.g., '€').
//    - CJK characters (e.g., "你好", "中").
//    - Components of complex emojis like the Zero Width Joiner (ZWJ) and
//      Heart symbol.
// 4. Various 4-byte UTF-8 sequences (representing Supplementary Plane
// characters):
//    - An emoji with a skin tone modifier ("👍🏻").
//    - A flag emoji composed of regional indicators ("🇺🇸").
//    - A complex ZWJ emoji sequence ("👩‍❤️‍💋‍👨") combining
//      SP characters (👩, 💋, 👨) with BMP characters (ZWJ and ❤️).
//    - These are critical for testing the correct handling of surrogate pairs
//      when wchar_t is 2 bytes (e.g., on Windows).
// The goal is to ensure accurate conversion across a diverse set of
// characters.
//
// clang-format off
#define WIDE_STRING_LITERAL L"Holá €1 你好 שָׁלוֹם 👍🏻🇺🇸👩‍❤️‍💋‍👨 中"
#define UTF8_STRING_LITERAL u8"Holá €1 你好 שָׁלוֹם 👍🏻🇺🇸👩‍❤️‍💋‍👨 中"
// clang-format on

absl::string_view GetUtf8TestString() {
  // `u8""` forces UTF-8 encoding; MSVC will default to e.g. CP1252 (and warn)
  // without it. However, the resulting character type differs between pre-C++20
  // (`char`) and C++20 (`char8_t`). So deduce the right character type for all
  // C++ versions, init it with UTF-8, then `memcpy()` to get the result as a
  // `char*`
  static absl::string_view kUtf8TestString = [] {
    using ConstChar8T = std::remove_reference_t<decltype(*u8"a")>;
    constexpr ConstChar8T kOutputUtf8[] = UTF8_STRING_LITERAL;
    static char output[sizeof kOutputUtf8];
    std::memcpy(output, kOutputUtf8, sizeof kOutputUtf8);
    return output;
  }();

  return kUtf8TestString;
}

TEST(WideToUtf8, FullString) {
  std::string buffer(kMaxEncodedUTF8Size * sizeof(WIDE_STRING_LITERAL), '\0');
  char* buffer_ptr = buffer.data();

  ShiftState state;
  for (const wchar_t wc : WIDE_STRING_LITERAL) {
    buffer_ptr += WideToUtf8(wc, buffer_ptr, state);
  }

  EXPECT_THAT(buffer, StartsWith(GetUtf8TestString()));
}

#undef WIDE_STRING_LITERAL
#undef UTF8_STRING_LITERAL

}  // namespace
