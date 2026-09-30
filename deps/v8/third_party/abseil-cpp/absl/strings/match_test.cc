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

#include "absl/strings/match.h"

#include <cstddef>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/ascii.h"
#include "absl/strings/string_view.h"

namespace {

TEST(MatchTest, StartsWith) {
  const std::string s1("123\0abc", 7);
  const absl::string_view a("foobar");
  const absl::string_view b(s1);
  const absl::string_view e;
  EXPECT_TRUE(absl::StartsWith(a, a));
  EXPECT_TRUE(absl::StartsWith(a, "foo"));
  EXPECT_TRUE(absl::StartsWith(a, e));
  EXPECT_TRUE(absl::StartsWith(b, s1));
  EXPECT_TRUE(absl::StartsWith(b, b));
  EXPECT_TRUE(absl::StartsWith(b, e));
  EXPECT_TRUE(absl::StartsWith(e, ""));
  EXPECT_FALSE(absl::StartsWith(a, b));
  EXPECT_FALSE(absl::StartsWith(b, a));
  EXPECT_FALSE(absl::StartsWith(e, a));
}

TEST(MatchTest, EndsWith) {
  const std::string s1("123\0abc", 7);
  const absl::string_view a("foobar");
  const absl::string_view b(s1);
  const absl::string_view e;
  EXPECT_TRUE(absl::EndsWith(a, a));
  EXPECT_TRUE(absl::EndsWith(a, "bar"));
  EXPECT_TRUE(absl::EndsWith(a, e));
  EXPECT_TRUE(absl::EndsWith(b, s1));
  EXPECT_TRUE(absl::EndsWith(b, b));
  EXPECT_TRUE(absl::EndsWith(b, e));
  EXPECT_TRUE(absl::EndsWith(e, ""));
  EXPECT_FALSE(absl::EndsWith(a, b));
  EXPECT_FALSE(absl::EndsWith(b, a));
  EXPECT_FALSE(absl::EndsWith(e, a));
}

TEST(MatchTest, Contains) {
  absl::string_view a("abcdefg");
  absl::string_view b("abcd");
  absl::string_view c("efg");
  absl::string_view d("gh");
  EXPECT_TRUE(absl::StrContains(a, a));
  EXPECT_TRUE(absl::StrContains(a, b));
  EXPECT_TRUE(absl::StrContains(a, c));
  EXPECT_FALSE(absl::StrContains(a, d));
  EXPECT_TRUE(absl::StrContains("", ""));
  EXPECT_TRUE(absl::StrContains("abc", ""));
  EXPECT_FALSE(absl::StrContains("", "a"));
}

TEST(MatchTest, ContainsChar) {
  absl::string_view a("abcdefg");
  absl::string_view b("abcd");
  EXPECT_TRUE(absl::StrContains(a, 'a'));
  EXPECT_TRUE(absl::StrContains(a, 'b'));
  EXPECT_TRUE(absl::StrContains(a, 'e'));
  EXPECT_FALSE(absl::StrContains(a, 'h'));

  EXPECT_TRUE(absl::StrContains(b, 'a'));
  EXPECT_TRUE(absl::StrContains(b, 'b'));
  EXPECT_FALSE(absl::StrContains(b, 'e'));
  EXPECT_FALSE(absl::StrContains(b, 'h'));

  EXPECT_FALSE(absl::StrContains("", 'a'));
  EXPECT_FALSE(absl::StrContains("", 'a'));
}

TEST(MatchTest, ContainsNull) {
  const std::string s = "foo";
  const char* cs = "foo";
  const absl::string_view sv("foo");
  const absl::string_view sv2("foo\0bar", 4);
  EXPECT_EQ(s, "foo");
  EXPECT_EQ(sv, "foo");
  EXPECT_NE(sv2, "foo");
  EXPECT_TRUE(absl::EndsWith(s, sv));
  EXPECT_TRUE(absl::StartsWith(cs, sv));
  EXPECT_TRUE(absl::StrContains(cs, sv));
  EXPECT_FALSE(absl::StrContains(cs, sv2));
}

TEST(MatchTest, EqualsIgnoreCase) {
  std::string text = "the";
  absl::string_view data(text);

  EXPECT_TRUE(absl::EqualsIgnoreCase(data, "The"));
  EXPECT_TRUE(absl::EqualsIgnoreCase(data, "THE"));
  EXPECT_TRUE(absl::EqualsIgnoreCase(data, "the"));
  EXPECT_FALSE(absl::EqualsIgnoreCase(data, "Quick"));
  EXPECT_FALSE(absl::EqualsIgnoreCase(data, "then"));
}

TEST(MatchTest, StartsWithIgnoreCase) {
  EXPECT_TRUE(absl::StartsWithIgnoreCase("foo", "foo"));
  EXPECT_TRUE(absl::StartsWithIgnoreCase("foo", "Fo"));
  EXPECT_TRUE(absl::StartsWithIgnoreCase("foo", ""));
  EXPECT_FALSE(absl::StartsWithIgnoreCase("foo", "fooo"));
  EXPECT_FALSE(absl::StartsWithIgnoreCase("", "fo"));
}

TEST(MatchTest, EndsWithIgnoreCase) {
  EXPECT_TRUE(absl::EndsWithIgnoreCase("foo", "foo"));
  EXPECT_TRUE(absl::EndsWithIgnoreCase("foo", "Oo"));
  EXPECT_TRUE(absl::EndsWithIgnoreCase("foo", ""));
  EXPECT_FALSE(absl::EndsWithIgnoreCase("foo", "fooo"));
  EXPECT_FALSE(absl::EndsWithIgnoreCase("", "fo"));
}

TEST(MatchTest, ContainsIgnoreCase) {
  EXPECT_TRUE(absl::StrContainsIgnoreCase("foo", "foo"));
  EXPECT_TRUE(absl::StrContainsIgnoreCase("FOO", "Foo"));
  EXPECT_TRUE(absl::StrContainsIgnoreCase("--FOO", "Foo"));
  EXPECT_TRUE(absl::StrContainsIgnoreCase("FOO--", "Foo"));
  EXPECT_FALSE(absl::StrContainsIgnoreCase("BAR", "Foo"));
  EXPECT_FALSE(absl::StrContainsIgnoreCase("BAR", "Foo"));
  EXPECT_TRUE(absl::StrContainsIgnoreCase("123456", "123456"));
  EXPECT_TRUE(absl::StrContainsIgnoreCase("123456", "234"));
  EXPECT_TRUE(absl::StrContainsIgnoreCase("", ""));
  EXPECT_TRUE(absl::StrContainsIgnoreCase("abc", ""));
  EXPECT_FALSE(absl::StrContainsIgnoreCase("", "a"));
}

TEST(MatchTest, ContainsCharIgnoreCase) {
  absl::string_view a("AaBCdefg!");
  absl::string_view b("AaBCd!");
  EXPECT_TRUE(absl::StrContainsIgnoreCase(a, 'a'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(a, 'A'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(a, 'b'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(a, 'B'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(a, 'e'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(a, 'E'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(a, 'h'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(a, 'H'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(a, '!'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(a, '?'));

  EXPECT_TRUE(absl::StrContainsIgnoreCase(b, 'a'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(b, 'A'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(b, 'b'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(b, 'B'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(b, 'e'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(b, 'E'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(b, 'h'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(b, 'H'));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(b, '!'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(b, '?'));

  EXPECT_FALSE(absl::StrContainsIgnoreCase("", 'a'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase("", 'A'));
  EXPECT_FALSE(absl::StrContainsIgnoreCase("", '0'));
}

TEST(MatchTest, ContainsIgnoreCaseLinearScaling) {
  // Adversarial case where naive shift-by-one substring scan exhibits
  // quadratic O(N * M) time: haystack = 'a' * N, needle = 'a' * (M - 1) + 'b'.
  //
  // Boyer-Moore-Horspool inspects the last byte first, immediately detecting
  // the mismatch ('a' != 'b') at each window offset and shifting by 1,
  // completing in linear O(N) time.
  const std::string haystack(1 << 17, 'a');  // 128 KiB
  std::string needle(1 << 16, 'a');          // 64 KiB
  needle.push_back('b');

  EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, needle));

  std::string matching_needle(1 << 16, 'A');
  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, matching_needle));
}

std::string MakeBMHNeedle(size_t m) {
  std::string needle(m, 'y');
  for (size_t i = 0; i < m; i += 2) {
    needle[i] = static_cast<char>('A' + (i % 26));
    needle[i + 1] = static_cast<char>('a' + ((i + 1) % 26));
  }
  return needle;
}

TEST(MatchTest, ContainsIgnoreCaseBMHMatchAtStart) {
  // Test BMH search (N >= 256, M >= 256) when match occurs at pos == 0.
  constexpr size_t n = 1000;
  constexpr size_t m = 300;
  std::string needle = MakeBMHNeedle(m);
  std::string haystack(n, 'x');
  haystack.replace(0, m, needle);

  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, needle));
}

TEST(MatchTest, ContainsIgnoreCaseBMHMatchInMiddle) {
  // Test BMH search (N >= 256, M >= 256) when match occurs in the middle of
  // haystack.
  constexpr size_t n = 1000;
  constexpr size_t m = 300;
  std::string needle = MakeBMHNeedle(m);
  std::string haystack(n, 'x');
  haystack.replace(200, m, needle);

  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, needle));
}

TEST(MatchTest, ContainsIgnoreCaseBMHMatchAtEnd) {
  // Test BMH search (N >= 256, M >= 256) when match occurs at the very end (pos
  // == n - m). Verifies that while (pos <= n - m) boundary condition includes
  // the final window.
  constexpr size_t n = 1000;
  constexpr size_t m = 300;
  std::string needle = MakeBMHNeedle(m);
  std::string haystack(n, 'x');
  haystack.replace(n - m, m, needle);

  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, needle));
}

TEST(MatchTest, ContainsIgnoreCaseBMHMismatchAtEnd) {
  // Test BMH search (N >= 256, M >= 256) when needle is placed at pos == n - m
  // but fails on the last character.
  constexpr size_t n = 1000;
  constexpr size_t m = 300;
  std::string needle = MakeBMHNeedle(m);
  std::string haystack(n, 'x');
  haystack.replace(n - m, m, needle);
  haystack.back() = 'Z';

  EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, needle));
}

TEST(MatchTest, ContainsIgnoreCaseBMHInteriorMismatch) {
  // Test BMH search (N >= 256, M >= 256) when the first and last characters
  // of a window match, but interior bytes mismatch.
  // This exercises the false branch of EqualsIgnoreCase(...) inside BMH,
  // falling through line 91 to advance the window.
  constexpr size_t n = 1000;
  constexpr size_t m = 300;
  std::string needle = MakeBMHNeedle(m);

  // 1. First and last chars match at pos == 100, but interior byte 10
  // mismatches.
  std::string haystack(n, 'x');
  haystack.replace(100, m, needle);
  haystack[100 + 10] = (needle[10] == 'Q') ? 'W' : 'Q';

  EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, needle));

  // 2. Add a valid match at the end to verify BMH continues scanning after
  // falling through an interior mismatch.
  haystack.replace(n - m, m, needle);
  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, needle));
}

TEST(MatchTest, ContainsIgnoreCaseBMHMixedCaseAndShifts) {
  // Test large needle (M >= 256) with mixed casing and partial matches
  // triggering multi-byte skip shifts during BMH scanning.
  std::string needle;
  needle.reserve(300);
  for (size_t i = 0; i < 300; ++i) {
    needle.push_back(static_cast<char>('a' + (i % 10)));
  }

  std::string haystack;
  haystack.reserve(2000);
  for (int i = 0; i < 5; ++i) {
    haystack.append(needle.substr(0, 250));
    haystack.append("XYZ");
  }

  std::string needle_upper = needle;
  for (char& c : needle_upper) {
    c = static_cast<char>(absl::ascii_toupper(static_cast<unsigned char>(c)));
  }

  EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, needle_upper));

  haystack.append(needle_upper);
  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, needle));
}

TEST(MatchTest, ContainsIgnoreCaseHaystackThresholdBoundary) {
  // Test haystack threshold boundary N = 255 (prefilter branch) vs N = 256 (BMH
  // branch).
  std::string needle = "AbC";

  // N = 255 (prefilter branch)
  std::string h255(255, '.');
  h255.replace(100, 3, "aBc");
  EXPECT_TRUE(absl::StrContainsIgnoreCase(h255, needle));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(h255, "abc"));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(std::string(255, '.'), needle));

  // N = 256 (BMH branch)
  std::string h256(256, '.');
  h256.replace(100, 3, "aBc");
  EXPECT_TRUE(absl::StrContainsIgnoreCase(h256, needle));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(h256, "abc"));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(std::string(256, '.'), needle));
}

TEST(MatchTest, ContainsIgnoreCaseSmallNeedleFastPath) {
  // Test small needle (M = 2) fast path, which uses prefilter regardless of
  // haystack size (N >= 256).
  std::string needle = "xY";
  std::string h1000(1000, 'a');

  // Match at start
  h1000.replace(0, 2, "Xy");
  EXPECT_TRUE(absl::StrContainsIgnoreCase(h1000, needle));

  // Match at end (pos == 998)
  h1000 = std::string(1000, 'a');
  h1000.replace(998, 2, "Xy");
  EXPECT_TRUE(absl::StrContainsIgnoreCase(h1000, needle));

  // Mismatch
  h1000 = std::string(1000, 'a');
  EXPECT_FALSE(absl::StrContainsIgnoreCase(h1000, needle));
}

TEST(MatchTest, ContainsIgnoreCaseNeedleThresholdBoundary) {
  // Test needle length boundaries M = 255, M = 256, and M = 257 in N = 300
  // haystack.
  std::string h300(300, 'a');
  std::string n255(255, 'A');
  std::string n256(256, 'A');
  std::string n257(257, 'A');

  EXPECT_TRUE(absl::StrContainsIgnoreCase(h300, n255));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(h300, n256));
  EXPECT_TRUE(absl::StrContainsIgnoreCase(h300, n257));

  n255.back() = 'B';
  n256.back() = 'B';
  n257.back() = 'B';

  EXPECT_FALSE(absl::StrContainsIgnoreCase(h300, n255));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(h300, n256));
  EXPECT_FALSE(absl::StrContainsIgnoreCase(h300, n257));
}

TEST(MatchTest, ContainsIgnoreCaseBMHNonAsciiBytes) {
  // Test BMH algorithm handling of high-bit non-ASCII bytes (values > 127) in
  // needle and haystack.
  std::string needle(300, '\0');
  for (size_t i = 0; i < 300; ++i) {
    needle[i] = static_cast<char>(128 + (i % 128));
  }
  std::string haystack(1000, '\x7F');
  haystack.replace(500, 300, needle);
  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, needle));

  haystack[799] = '\x00';
  EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, needle));
}

TEST(MatchTest, ContainsIgnoreCaseBMHExactLengthMatch) {
  // Test BMH search when haystack size equals needle size (N == M == 300).
  std::string needle(300, 'X');
  std::string haystack(300, 'x');
  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, needle));

  haystack.back() = 'y';
  EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, needle));
}

TEST(MatchTest, ContainsIgnoreCaseBMHAllIdenticalCharNeedle) {
  // Test BMH search with needle consisting of all identical characters (M >=
  // 256).
  std::string needle(300, 'K');
  std::string haystack(1000, 'k');
  EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, needle));

  std::string short_haystack(299, 'k');
  EXPECT_FALSE(absl::StrContainsIgnoreCase(short_haystack, needle));
}

TEST(MatchTest, ContainsCharIgnoreCaseLargeHaystack) {
  // Test StrContainsIgnoreCase(haystack, char) for N >= 64, exercising the
  // find_first_of fallback branch.
  constexpr size_t sizes[] = {64, 100, 1000};
  for (size_t n : sizes) {
    std::string haystack(n, '.');

    // 1. Alphabetic character (exercises find_first_of fallback branch)
    haystack[0] = 'z';
    EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, 'Z'));
    EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, 'z'));

    haystack = std::string(n, '.');
    haystack[n / 2] = 'Z';
    EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, 'z'));
    EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, 'Z'));

    haystack = std::string(n, '.');
    haystack[n - 1] = 'z';
    EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, 'Z'));
    EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, 'z'));

    haystack = std::string(n, '.');
    EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, 'z'));
    EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, 'Z'));

    // 2. Non-alphabetic character (upper_needle == lower_needle)
    haystack[n / 2] = '9';
    EXPECT_TRUE(absl::StrContainsIgnoreCase(haystack, '9'));
    EXPECT_FALSE(absl::StrContainsIgnoreCase(haystack, '8'));
  }
}

TEST(MatchTest, FindLongestCommonPrefix) {
  EXPECT_EQ(absl::FindLongestCommonPrefix("", ""), "");
  EXPECT_EQ(absl::FindLongestCommonPrefix("", "abc"), "");
  EXPECT_EQ(absl::FindLongestCommonPrefix("abc", ""), "");
  EXPECT_EQ(absl::FindLongestCommonPrefix("ab", "abc"), "ab");
  EXPECT_EQ(absl::FindLongestCommonPrefix("abc", "ab"), "ab");
  EXPECT_EQ(absl::FindLongestCommonPrefix("abc", "abd"), "ab");
  EXPECT_EQ(absl::FindLongestCommonPrefix("abc", "abcd"), "abc");
  EXPECT_EQ(absl::FindLongestCommonPrefix("abcd", "abcd"), "abcd");
  EXPECT_EQ(absl::FindLongestCommonPrefix("abcd", "efgh"), "");

  // "abcde" v. "abc" but in the middle of other data
  EXPECT_EQ(absl::FindLongestCommonPrefix(
                absl::string_view("1234 abcdef").substr(5, 5),
                absl::string_view("5678 abcdef").substr(5, 3)),
            "abc");
}

// Since the little-endian implementation involves a bit of if-else and various
// return paths, the following tests aims to provide full test coverage of the
// implementation.
TEST(MatchTest, FindLongestCommonPrefixLoad16Mismatch) {
  const std::string x1 = "abcdefgh";
  const std::string x2 = "abcde_";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "abcde");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "abcde");
}

TEST(MatchTest, FindLongestCommonPrefixLoad16MatchesNoLast) {
  const std::string x1 = "abcdef";
  const std::string x2 = "abcdef";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "abcdef");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "abcdef");
}

TEST(MatchTest, FindLongestCommonPrefixLoad16MatchesLastCharMismatches) {
  const std::string x1 = "abcdefg";
  const std::string x2 = "abcdef_h";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "abcdef");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "abcdef");
}

TEST(MatchTest, FindLongestCommonPrefixLoad16MatchesLastMatches) {
  const std::string x1 = "abcde";
  const std::string x2 = "abcdefgh";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "abcde");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "abcde");
}

TEST(MatchTest, FindLongestCommonPrefixSize8Load64Mismatches) {
  const std::string x1 = "abcdefghijk";
  const std::string x2 = "abcde_g_";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "abcde");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "abcde");
}

TEST(MatchTest, FindLongestCommonPrefixSize8Load64Matches) {
  const std::string x1 = "abcdefgh";
  const std::string x2 = "abcdefgh";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "abcdefgh");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "abcdefgh");
}

TEST(MatchTest, FindLongestCommonPrefixSize15Load64Mismatches) {
  const std::string x1 = "012345670123456";
  const std::string x2 = "0123456701_34_6";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "0123456701");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "0123456701");
}

TEST(MatchTest, FindLongestCommonPrefixSize15Load64Matches) {
  const std::string x1 = "012345670123456";
  const std::string x2 = "0123456701234567";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "012345670123456");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "012345670123456");
}

TEST(MatchTest, FindLongestCommonPrefixSizeFirstByteOfLast8BytesMismatch) {
  const std::string x1 = "012345670123456701234567";
  const std::string x2 = "0123456701234567_1234567";
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), "0123456701234567");
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), "0123456701234567");
}

TEST(MatchTest, FindLongestCommonPrefixLargeLastCharMismatches) {
  const std::string x1(300, 'x');
  std::string x2 = x1;
  x2.back() = '#';
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), std::string(299, 'x'));
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), std::string(299, 'x'));
}

TEST(MatchTest, FindLongestCommonPrefixLargeFullMatch) {
  const std::string x1(300, 'x');
  const std::string x2 = x1;
  EXPECT_EQ(absl::FindLongestCommonPrefix(x1, x2), std::string(300, 'x'));
  EXPECT_EQ(absl::FindLongestCommonPrefix(x2, x1), std::string(300, 'x'));
}

TEST(MatchTest, FindLongestCommonSuffix) {
  EXPECT_EQ(absl::FindLongestCommonSuffix("", ""), "");
  EXPECT_EQ(absl::FindLongestCommonSuffix("", "abc"), "");
  EXPECT_EQ(absl::FindLongestCommonSuffix("abc", ""), "");
  EXPECT_EQ(absl::FindLongestCommonSuffix("bc", "abc"), "bc");
  EXPECT_EQ(absl::FindLongestCommonSuffix("abc", "bc"), "bc");
  EXPECT_EQ(absl::FindLongestCommonSuffix("abc", "dbc"), "bc");
  EXPECT_EQ(absl::FindLongestCommonSuffix("bcd", "abcd"), "bcd");
  EXPECT_EQ(absl::FindLongestCommonSuffix("abcd", "abcd"), "abcd");
  EXPECT_EQ(absl::FindLongestCommonSuffix("abcd", "efgh"), "");

  // "abcde" v. "cde" but in the middle of other data
  EXPECT_EQ(absl::FindLongestCommonSuffix(
                absl::string_view("1234 abcdef").substr(5, 5),
                absl::string_view("5678 abcdef").substr(7, 3)),
            "cde");
}

}  // namespace
