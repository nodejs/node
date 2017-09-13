// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_INTL_SUPPORT

#include "src/builtins/builtins-intl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

// This operator overloading enables CHECK_EQ to be used with
// std::vector<NumberFormatSpan>
bool operator==(const NumberFormatSpan& lhs, const NumberFormatSpan& rhs) {
  return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}
template <typename _CharT, typename _Traits, typename T>
std::basic_ostream<_CharT, _Traits>& operator<<(
    std::basic_ostream<_CharT, _Traits>& self, const std::vector<T>& v) {
  self << "[";
  for (auto it = v.begin(); it < v.end();) {
    self << *it;
    it++;
    if (it < v.end()) self << ", ";
  }
  return self << "]";
}
template <typename _CharT, typename _Traits>
std::basic_ostream<_CharT, _Traits>& operator<<(
    std::basic_ostream<_CharT, _Traits>& self, const NumberFormatSpan& part) {
  return self << "{" << part.field_id << "," << part.begin_pos << ","
              << part.end_pos << "}";
}

void test_flatten_regions_to_parts(
    const std::vector<NumberFormatSpan>& regions,
    const std::vector<NumberFormatSpan>& expected_parts) {
  std::vector<NumberFormatSpan> mutable_regions = regions;
  std::vector<NumberFormatSpan> parts = FlattenRegionsToParts(&mutable_regions);
  CHECK_EQ(expected_parts, parts);
}

TEST(FlattenRegionsToParts) {
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 10), NumberFormatSpan(1, 2, 8),
          NumberFormatSpan(2, 2, 4), NumberFormatSpan(3, 6, 8),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 2), NumberFormatSpan(2, 2, 4),
          NumberFormatSpan(1, 4, 6), NumberFormatSpan(3, 6, 8),
          NumberFormatSpan(-1, 8, 10),
      });
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1),
      });
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 1), NumberFormatSpan(0, 0, 1),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1),
      });
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1), NumberFormatSpan(-1, 0, 1),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1),
      });
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 10), NumberFormatSpan(1, 0, 1),
          NumberFormatSpan(2, 0, 2), NumberFormatSpan(3, 0, 3),
          NumberFormatSpan(4, 0, 4), NumberFormatSpan(5, 0, 5),
          NumberFormatSpan(15, 5, 10), NumberFormatSpan(16, 6, 10),
          NumberFormatSpan(17, 7, 10), NumberFormatSpan(18, 8, 10),
          NumberFormatSpan(19, 9, 10),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(1, 0, 1), NumberFormatSpan(2, 1, 2),
          NumberFormatSpan(3, 2, 3), NumberFormatSpan(4, 3, 4),
          NumberFormatSpan(5, 4, 5), NumberFormatSpan(15, 5, 6),
          NumberFormatSpan(16, 6, 7), NumberFormatSpan(17, 7, 8),
          NumberFormatSpan(18, 8, 9), NumberFormatSpan(19, 9, 10),
      });

  //              :          4
  //              :      22 33    3
  //              :      11111   22
  // input regions:     0000000  111
  //              :     ------------
  // output parts:      0221340--231
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 12), NumberFormatSpan(0, 0, 7),
          NumberFormatSpan(1, 9, 12), NumberFormatSpan(1, 1, 6),
          NumberFormatSpan(2, 9, 11), NumberFormatSpan(2, 1, 3),
          NumberFormatSpan(3, 10, 11), NumberFormatSpan(3, 4, 6),
          NumberFormatSpan(4, 5, 6),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1), NumberFormatSpan(2, 1, 3),
          NumberFormatSpan(1, 3, 4), NumberFormatSpan(3, 4, 5),
          NumberFormatSpan(4, 5, 6), NumberFormatSpan(0, 6, 7),
          NumberFormatSpan(-1, 7, 9), NumberFormatSpan(2, 9, 10),
          NumberFormatSpan(3, 10, 11), NumberFormatSpan(1, 11, 12),
      });
}

}  // namespace internal
}  // namespace v8

#endif  // V8_INTL_SUPPORT
