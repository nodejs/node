// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <map>

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

  set.Iterate(
      [](Address slot_address) {
        uintptr_t intaddr = reinterpret_cast<uintptr_t>(slot_address);
        if (intaddr % 3 == 0) {
          return KEEP_SLOT;
        } else {
          return REMOVE_SLOT;
        }
      },
      SlotSet::KEEP_EMPTY_BUCKETS);

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
  for (const auto mode :
       {SlotSet::FREE_EMPTY_BUCKETS, SlotSet::KEEP_EMPTY_BUCKETS}) {
    for (uint32_t i = first; i <= last; i += kPointerSize) {
      set.Insert(i);
    }
    set.RemoveRange(start, end, mode);
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
}

TEST(SlotSet, RemoveRange) {
  CheckRemoveRangeOn(0, Page::kPageSize);
  CheckRemoveRangeOn(1 * kPointerSize, 1023 * kPointerSize);
  for (uint32_t start = 0; start <= 32; start++) {
    CheckRemoveRangeOn(start * kPointerSize, (start + 1) * kPointerSize);
    CheckRemoveRangeOn(start * kPointerSize, (start + 2) * kPointerSize);
    const uint32_t kEnds[] = {32, 64, 100, 128, 1024, 1500, 2048};
    for (size_t i = 0; i < sizeof(kEnds) / sizeof(uint32_t); i++) {
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
  for (const auto mode :
       {SlotSet::FREE_EMPTY_BUCKETS, SlotSet::KEEP_EMPTY_BUCKETS}) {
    set.Insert(Page::kPageSize / 2);
    set.RemoveRange(0, Page::kPageSize, mode);
    for (uint32_t i = 0; i < Page::kPageSize; i += kPointerSize) {
      EXPECT_FALSE(set.Lookup(i));
    }
  }
}

TEST(TypedSlotSet, Iterate) {
  TypedSlotSet set(0);
  // These two constants must be static as a workaround
  // for a MSVC++ bug about lambda captures, see the discussion at
  // https://social.msdn.microsoft.com/Forums/SqlServer/4abf18bd-4ae4-4c72-ba3e-3b13e7909d5f
  static const int kDelta = 10000001;
  static const int kHostDelta = 50001;
  int added = 0;
  uint32_t j = 0;
  for (uint32_t i = 0; i < TypedSlotSet::kMaxOffset;
       i += kDelta, j += kHostDelta) {
    SlotType type = static_cast<SlotType>(i % CLEARED_SLOT);
    set.Insert(type, j, i);
    ++added;
  }
  int iterated = 0;
  set.Iterate(
      [&iterated](SlotType type, Address host_addr,
                  Address addr) {
        uint32_t i = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(addr));
        uint32_t j =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(host_addr));
        EXPECT_EQ(i % CLEARED_SLOT, static_cast<uint32_t>(type));
        EXPECT_EQ(0u, i % kDelta);
        EXPECT_EQ(0u, j % kHostDelta);
        ++iterated;
        return i % 2 == 0 ? KEEP_SLOT : REMOVE_SLOT;
      },
      TypedSlotSet::KEEP_EMPTY_CHUNKS);
  EXPECT_EQ(added, iterated);
  iterated = 0;
  set.Iterate(
      [&iterated](SlotType type, Address host_addr, Address addr) {
        uint32_t i = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(addr));
        EXPECT_EQ(0u, i % 2);
        ++iterated;
        return KEEP_SLOT;
      },
      TypedSlotSet::KEEP_EMPTY_CHUNKS);
  EXPECT_EQ(added / 2, iterated);
}

TEST(TypedSlotSet, RemoveInvalidSlots) {
  TypedSlotSet set(0);
  const int kHostDelta = 100;
  uint32_t entries = 10;
  for (uint32_t i = 0; i < entries; i++) {
    SlotType type = static_cast<SlotType>(i % CLEARED_SLOT);
    set.Insert(type, i * kHostDelta, i * kHostDelta);
  }

  std::map<uint32_t, uint32_t> invalid_ranges;
  for (uint32_t i = 1; i < entries; i += 2) {
    invalid_ranges.insert(
        std::pair<uint32_t, uint32_t>(i * kHostDelta, i * kHostDelta + 1));
  }

  set.RemoveInvaldSlots(invalid_ranges);
  for (std::map<uint32_t, uint32_t>::iterator it = invalid_ranges.begin();
       it != invalid_ranges.end(); ++it) {
    uint32_t start = it->first;
    uint32_t end = it->second;
    set.Iterate(
        [start, end](SlotType slot_type, Address host_addr, Address slot_addr) {
          CHECK(reinterpret_cast<uintptr_t>(host_addr) < start ||
                reinterpret_cast<uintptr_t>(host_addr) >= end);
          return KEEP_SLOT;
        },
        TypedSlotSet::KEEP_EMPTY_CHUNKS);
  }
}

}  // namespace internal
}  // namespace v8
