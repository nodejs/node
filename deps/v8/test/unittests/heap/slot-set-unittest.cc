// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/globals.h"
#include "src/heap/slot-set.h"
#include "src/heap/spaces.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(SlotSet, InsertAndLookup1) {
  SlotSet set;
  set.SetPageStart(0);
  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    EXPECT_FALSE(set.Lookup(i));
  }
  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    set.Insert(i);
  }
  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    EXPECT_TRUE(set.Lookup(i));
  }
}

TEST(SlotSet, InsertAndLookup2) {
  SlotSet set;
  set.SetPageStart(0);
  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    if (i % 7 == 0) {
      set.Insert(i);
    }
  }
  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    if (i % 7 == 0) {
      EXPECT_TRUE(set.Lookup(i));
    } else {
      EXPECT_FALSE(set.Lookup(i));
    }
  }
}

TEST(SlotSet, Iterate) {
  SlotSet set;
  set.SetPageStart(0);
  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    if (i % 7 == 0) {
      set.Insert(i);
    }
  }

  set.Iterate([](Address slot_address) {
    uintptr_t intaddr = reinterpret_cast<uintptr_t>(slot_address);
    if (intaddr % 3 == 0) {
      return SlotSet::KEEP_SLOT;
    } else {
      return SlotSet::REMOVE_SLOT;
    }
  });

  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    if (i % 21 == 0) {
      EXPECT_TRUE(set.Lookup(i));
    } else {
      EXPECT_FALSE(set.Lookup(i));
    }
  }
}

TEST(SlotSet, Remove) {
  SlotSet set;
  set.SetPageStart(0);
  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    if (i % 7 == 0) {
      set.Insert(i);
    }
  }

  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    if (i % 3 != 0) {
      set.Remove(i);
    }
  }

  for (int i = 0; i < Page::kPageSize; i += kPointerSize) {
    if (i % 21 == 0) {
      EXPECT_TRUE(set.Lookup(i));
    } else {
      EXPECT_FALSE(set.Lookup(i));
    }
  }
}

void CheckRemoveRangeOn(uint32_t start, uint32_t end) {
  SlotSet set;
  set.SetPageStart(0);
  uint32_t first = start == 0 ? 0 : start - kPointerSize;
  uint32_t last = end == Page::kPageSize ? end - kPointerSize : end;
  for (uint32_t i = first; i <= last; i += kPointerSize) {
    set.Insert(i);
  }
  set.RemoveRange(start, end);
  if (first != start) {
    EXPECT_TRUE(set.Lookup(first));
  }
  if (last == end) {
    EXPECT_TRUE(set.Lookup(last));
  }
  for (uint32_t i = start; i < end; i += kPointerSize) {
    EXPECT_FALSE(set.Lookup(i));
  }
}

TEST(SlotSet, RemoveRange) {
  CheckRemoveRangeOn(0, Page::kPageSize);
  CheckRemoveRangeOn(1 * kPointerSize, 1023 * kPointerSize);
  for (uint32_t start = 0; start <= 32; start++) {
    CheckRemoveRangeOn(start * kPointerSize, (start + 1) * kPointerSize);
    CheckRemoveRangeOn(start * kPointerSize, (start + 2) * kPointerSize);
    const uint32_t kEnds[] = {32, 64, 100, 128, 1024, 1500, 2048};
    for (int i = 0; i < sizeof(kEnds) / sizeof(uint32_t); i++) {
      for (int k = -3; k <= 3; k++) {
        uint32_t end = (kEnds[i] + k);
        if (start < end) {
          CheckRemoveRangeOn(start * kPointerSize, end * kPointerSize);
        }
      }
    }
  }
  SlotSet set;
  set.SetPageStart(0);
  set.Insert(Page::kPageSize / 2);
  set.RemoveRange(0, Page::kPageSize);
  for (uint32_t i = 0; i < Page::kPageSize; i += kPointerSize) {
    EXPECT_FALSE(set.Lookup(i));
  }
}

}  // namespace internal
}  // namespace v8
