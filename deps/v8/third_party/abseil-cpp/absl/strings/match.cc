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

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/endian.h"
#include "absl/base/optimization.h"
#include "absl/numeric/bits.h"
#include "absl/strings/ascii.h"
#include "absl/strings/internal/memutil.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

bool EqualsIgnoreCase(absl::string_view piece1,
                      absl::string_view piece2) noexcept {
  return (piece1.size() == piece2.size() &&
          0 == absl::strings_internal::memcasecmp(piece1.data(), piece2.data(),
                                                  piece1.size()));
  // memcasecmp uses absl::ascii_tolower().
}

namespace {

// For larger haystacks (n >= 256), Case-Insensitive Boyer-Moore-Horspool
// provides sub-linear O(N / M) average-case performance, although it still has
// O(N * M) theoretical worst-case scaling.
// Scans the haystack from left to right using a search window of size `m`.
// For each window offset, Horspool inspects the rightmost character of the
// window (`haystack[pos + m - 1]`) first, enabling multi-byte shifts when
// mismatches occur and reducing average search time to O(N / M).
ABSL_ATTRIBUTE_NOINLINE bool StrContainsIgnoreCaseBMH(
    absl::string_view haystack, absl::string_view needle) noexcept {
  const size_t n = haystack.size();
  const size_t m = needle.size();

  // Step 1: Initialize the 256-entry shift table.
  // Unknown characters default to full window shift of `m` bytes.
  size_t shift[256];
  for (size_t i = 0; i < 256; ++i) {
    shift[i] = m;
  }

  // Populate shift distances for needle[0..m-2]. Dual assignment for
  // ascii_tolower and ascii_toupper stores distance from rightmost occurrence
  // to end of needle.
  for (size_t i = 0; i < m - 1; ++i) {
    const unsigned char c = static_cast<unsigned char>(needle[i]);
    shift[static_cast<unsigned char>(absl::ascii_tolower(c))] = m - 1 - i;
    shift[static_cast<unsigned char>(absl::ascii_toupper(c))] = m - 1 - i;
  }

  // Step 2: Search loop across candidate window offsets.
  size_t pos = 0;
  while (pos <= n - m) {
    const unsigned char last_hay =
        static_cast<unsigned char>(haystack[pos + m - 1]);
    const unsigned char last_needle = static_cast<unsigned char>(needle[m - 1]);

    // Check 1: Inspect right-most character of window first.
    if (last_hay == last_needle ||
        absl::ascii_tolower(last_hay) == absl::ascii_tolower(last_needle)) {
      // Check 2: Pre-filter on first character of window.
      const unsigned char first_hay = static_cast<unsigned char>(haystack[pos]);
      const unsigned char first_needle = static_cast<unsigned char>(needle[0]);
      if (first_hay == first_needle ||
          absl::ascii_tolower(first_hay) == absl::ascii_tolower(first_needle)) {
        // Check 3: Compare interior (m - 2) bytes.
        if (EqualsIgnoreCase(haystack.substr(pos + 1, m - 2),
                             needle.substr(1, m - 2))) {
          return true;
        }
      }
    }

    // Advance window by shift distance determined by right-most haystack byte.
    pos += shift[last_hay];
  }
  return false;
}

}  // namespace

bool StrContainsIgnoreCase(absl::string_view haystack,
                           absl::string_view needle) noexcept {
  const size_t n = haystack.size();
  const size_t m = needle.size();
  if (m == 0) return true;
  if (n < m) return false;
  if (m == 1) return StrContainsIgnoreCase(haystack, needle[0]);

  // For short haystacks (n < 256) or small needles (m == 2), avoid the
  // initialization overhead of a 256-entry shift table. Instead, use a fast
  // first-and-last character prefilter before inspecting interior bytes.
  if (n < 256 || m == 2) {
    const char first_needle =
        absl::ascii_tolower(static_cast<unsigned char>(needle[0]));
    const char last_needle =
        absl::ascii_tolower(static_cast<unsigned char>(needle[m - 1]));

    for (size_t pos = 0; pos <= n - m; ++pos) {
      const unsigned char first_hay = static_cast<unsigned char>(haystack[pos]);
      if (absl::ascii_tolower(first_hay) != first_needle) continue;

      const unsigned char last_hay =
          static_cast<unsigned char>(haystack[pos + m - 1]);
      if (absl::ascii_tolower(last_hay) != last_needle) continue;

      if (m == 2 || EqualsIgnoreCase(haystack.substr(pos + 1, m - 2),
                                     needle.substr(1, m - 2))) {
        return true;
      }
    }
    return false;
  }

  return StrContainsIgnoreCaseBMH(haystack, needle);
}

bool StrContainsIgnoreCase(absl::string_view haystack,
                           char needle) noexcept {
  char upper_needle = absl::ascii_toupper(static_cast<unsigned char>(needle));
  char lower_needle = absl::ascii_tolower(static_cast<unsigned char>(needle));
  if (upper_needle == lower_needle) {
    return StrContains(haystack, needle);
  }
  if (haystack.size() < 64) {
    for (char c : haystack) {
      if (c == lower_needle || c == upper_needle) return true;
    }
    return false;
  }
  const char both_cstr[3] = {lower_needle, upper_needle, '\0'};
  return haystack.find_first_of(both_cstr) != absl::string_view::npos;
}

bool StartsWithIgnoreCase(absl::string_view text,
                          absl::string_view prefix) noexcept {
  return (text.size() >= prefix.size()) &&
         EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

bool EndsWithIgnoreCase(absl::string_view text,
                        absl::string_view suffix) noexcept {
  return (text.size() >= suffix.size()) &&
         EqualsIgnoreCase(text.substr(text.size() - suffix.size()), suffix);
}

absl::string_view FindLongestCommonPrefix(absl::string_view a,
                                          absl::string_view b) {
  const absl::string_view::size_type limit = std::min(a.size(), b.size());
  const char* const pa = a.data();
  const char* const pb = b.data();
  absl::string_view::size_type count = (unsigned) 0;

  if (ABSL_PREDICT_FALSE(limit < 8)) {
    while (ABSL_PREDICT_TRUE(count + 2 <= limit)) {
      uint16_t xor_bytes = absl::little_endian::Load16(pa + count) ^
                           absl::little_endian::Load16(pb + count);
      if (ABSL_PREDICT_FALSE(xor_bytes != 0)) {
        if (ABSL_PREDICT_TRUE((xor_bytes & 0xff) == 0)) ++count;
        return absl::string_view(pa, count);
      }
      count += 2;
    }
    if (ABSL_PREDICT_TRUE(count != limit)) {
      if (ABSL_PREDICT_TRUE(pa[count] == pb[count])) ++count;
    }
    return absl::string_view(pa, count);
  }

  do {
    uint64_t xor_bytes = absl::little_endian::Load64(pa + count) ^
                         absl::little_endian::Load64(pb + count);
    if (ABSL_PREDICT_FALSE(xor_bytes != 0)) {
      count += static_cast<uint64_t>(absl::countr_zero(xor_bytes) >> 3);
      return absl::string_view(pa, count);
    }
    count += 8;
  } while (ABSL_PREDICT_TRUE(count + 8 < limit));

  count = limit - 8;
  uint64_t xor_bytes = absl::little_endian::Load64(pa + count) ^
                       absl::little_endian::Load64(pb + count);
  if (ABSL_PREDICT_TRUE(xor_bytes != 0)) {
    count += static_cast<uint64_t>(absl::countr_zero(xor_bytes) >> 3);
    return absl::string_view(pa, count);
  }
  return absl::string_view(pa, limit);
}

absl::string_view FindLongestCommonSuffix(absl::string_view a,
                                          absl::string_view b) {
  const absl::string_view::size_type limit = std::min(a.size(), b.size());
  if (limit == 0) return absl::string_view();

  const char* pa = a.data() + a.size() - 1;
  const char* pb = b.data() + b.size() - 1;
  absl::string_view::size_type count = (unsigned) 0;
  while (count < limit && *pa == *pb) {
    --pa;
    --pb;
    ++count;
  }

  return absl::string_view(++pa, count);
}

ABSL_NAMESPACE_END
}  // namespace absl
