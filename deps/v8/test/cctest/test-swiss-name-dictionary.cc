// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/swiss-name-dictionary-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_swiss_hash_table {

TEST(CapacityFor) {
  for (int elements = 0; elements <= 32; elements++) {
    int capacity = SwissNameDictionary::CapacityFor(elements);
    if (elements == 0) {
      CHECK_EQ(0, capacity);
    } else if (elements <= 3) {
      CHECK_EQ(4, capacity);
    } else if (elements == 4) {
      CHECK_IMPLIES(SwissNameDictionary::kGroupWidth == 8, capacity == 8);
      CHECK_IMPLIES(SwissNameDictionary::kGroupWidth == 16, capacity == 4);
    } else if (elements <= 7) {
      CHECK_EQ(8, capacity);
    } else if (elements <= 14) {
      CHECK_EQ(16, capacity);
    } else if (elements <= 28) {
      CHECK_EQ(32, capacity);
    } else if (elements <= 32) {
      CHECK_EQ(64, capacity);
    }
  }
}

TEST(MaxUsableCapacity) {
  CHECK_EQ(0, SwissNameDictionary::MaxUsableCapacity(0));
  CHECK_IMPLIES(SwissNameDictionary::kGroupWidth == 8,
                SwissNameDictionary::MaxUsableCapacity(4) == 3);
  CHECK_IMPLIES(SwissNameDictionary::kGroupWidth == 16,
                SwissNameDictionary::MaxUsableCapacity(4) == 4);
  CHECK_EQ(7, SwissNameDictionary::MaxUsableCapacity(8));
  CHECK_EQ(14, SwissNameDictionary::MaxUsableCapacity(16));
  CHECK_EQ(28, SwissNameDictionary::MaxUsableCapacity(32));
}

TEST(SizeFor) {
  int baseline = HeapObject::kHeaderSize +
                 // prefix:
                 4 +
                 // capacity:
                 4 +
                 // meta table:
                 kTaggedSize;

  int size_0 = baseline +
               // ctrl table:
               SwissNameDictionary::kGroupWidth;

  int size_4 = baseline +
               // data table:
               4 * 2 * kTaggedSize +
               // ctrl table:
               4 + SwissNameDictionary::kGroupWidth +
               // property details table:
               4;

  int size_8 = baseline +
               // data table:
               8 * 2 * kTaggedSize +
               // ctrl table:
               8 + SwissNameDictionary::kGroupWidth +
               // property details table:
               8;

  CHECK_EQ(SwissNameDictionary::SizeFor(0), size_0);
  CHECK_EQ(SwissNameDictionary::SizeFor(4), size_4);
  CHECK_EQ(SwissNameDictionary::SizeFor(8), size_8);
}

}  // namespace test_swiss_hash_table
}  // namespace internal
}  // namespace v8
