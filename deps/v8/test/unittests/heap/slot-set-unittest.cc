// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <map>

#include "src/common/globals.h"
#include "src/heap/slot-set.h"
#include "src/heap/spaces.h"
#include "src/objects/slots.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

TEST(SlotSet, InsertAndLookup1) {
  SlotSet set;
  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    EXPECT_FALSE(set.Lookup(i));
  }
  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    set.Insert<AccessMode::ATOMIC>(i);
  }
  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    EXPECT_TRUE(set.Lookup(i));
  }
}

TEST(SlotSet, InsertAndLookup2) {
  SlotSet set;
  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    if (i % 7 == 0) {
      set.Insert<AccessMode::ATOMIC>(i);
    }
  }
  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    if (i % 7 == 0) {
      EXPECT_TRUE(set.Lookup(i));
    } else {
      EXPECT_FALSE(set.Lookup(i));
    }
  }
}

TEST(SlotSet, Iterate) {
  SlotSet set;
  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    if (i % 7 == 0) {
      set.Insert<AccessMode::ATOMIC>(i);
    }
  }

  set.Iterate(
      kNullAddress,
      [](MaybeObjectSlot slot) {
        if (slot.address() % 3 == 0) {
          return KEEP_SLOT;
        } else {
          return REMOVE_SLOT;
        }
      },
      SlotSet::KEEP_EMPTY_BUCKETS);

  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    if (i % 21 == 0) {
      EXPECT_TRUE(set.Lookup(i));
    } else {
      EXPECT_FALSE(set.Lookup(i));
    }
  }
}

TEST(SlotSet, Remove) {
  SlotSet set;
  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    if (i % 7 == 0) {
      set.Insert<AccessMode::ATOMIC>(i);
    }
  }

  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    if (i % 3 != 0) {
      set.Remove(i);
    }
  }

  for (int i = 0; i < Page::kPageSize; i += kTaggedSize) {
    if (i % 21 == 0) {
      EXPECT_TRUE(set.Lookup(i));
    } else {
      EXPECT_FALSE(set.Lookup(i));
    }
  }
}

void CheckRemoveRangeOn(uint32_t start, uint32_t end) {
  SlotSet set;
  uint32_t first = start == 0 ? 0 : start - kTaggedSize;
  uint32_t last = end == Page::kPageSize ? end - kTaggedSize : end;
  for (const auto mode :
       {SlotSet::FREE_EMPTY_BUCKETS, SlotSet::KEEP_EMPTY_BUCKETS}) {
    for (uint32_t i = first; i <= last; i += kTaggedSize) {
      set.Insert<AccessMode::ATOMIC>(i);
    }
    set.RemoveRange(start, end, mode);
    if (first != start) {
      EXPECT_TRUE(set.Lookup(first));
    }
    if (last == end) {
      EXPECT_TRUE(set.Lookup(last));
    }
    for (uint32_t i = start; i < end; i += kTaggedSize) {
      EXPECT_FALSE(set.Lookup(i));
    }
  }
}

TEST(SlotSet, RemoveRange) {
  CheckRemoveRangeOn(0, Page::kPageSize);
  CheckRemoveRangeOn(1 * kTaggedSize, 1023 * kTaggedSize);
  for (uint32_t start = 0; start <= 32; start++) {
    CheckRemoveRangeOn(start * kTaggedSize, (start + 1) * kTaggedSize);
    CheckRemoveRangeOn(start * kTaggedSize, (start + 2) * kTaggedSize);
    const uint32_t kEnds[] = {32, 64, 100, 128, 1024, 1500, 2048};
    for (size_t i = 0; i < sizeof(kEnds) / sizeof(uint32_t); i++) {
      for (int k = -3; k <= 3; k++) {
        uint32_t end = (kEnds[i] + k);
        if (start < end) {
          CheckRemoveRangeOn(start * kTaggedSize, end * kTaggedSize);
        }
      }
    }
  }
  SlotSet set;
  for (const auto mode :
       {SlotSet::FREE_EMPTY_BUCKETS, SlotSet::KEEP_EMPTY_BUCKETS}) {
    set.Insert<AccessMode::ATOMIC>(Page::kPageSize / 2);
    set.RemoveRange(0, Page::kPageSize, mode);
    for (uint32_t i = 0; i < Page::kPageSize; i += kTaggedSize) {
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
  int added = 0;
  for (uint32_t i = 0; i < TypedSlotSet::kMaxOffset; i += kDelta) {
    SlotType type = static_cast<SlotType>(i % CLEARED_SLOT);
    set.Insert(type, i);
    ++added;
  }
  int iterated = 0;
  set.Iterate(
      [&iterated](SlotType type, Address addr) {
        uint32_t i = static_cast<uint32_t>(addr);
        EXPECT_EQ(i % CLEARED_SLOT, static_cast<uint32_t>(type));
        EXPECT_EQ(0u, i % kDelta);
        ++iterated;
        return i % 2 == 0 ? KEEP_SLOT : REMOVE_SLOT;
      },
      TypedSlotSet::KEEP_EMPTY_CHUNKS);
  EXPECT_EQ(added, iterated);
  iterated = 0;
  set.Iterate(
      [&iterated](SlotType type, Address addr) {
        uint32_t i = static_cast<uint32_t>(addr);
        EXPECT_EQ(0u, i % 2);
        ++iterated;
        return KEEP_SLOT;
      },
      TypedSlotSet::KEEP_EMPTY_CHUNKS);
  EXPECT_EQ(added / 2, iterated);
}

TEST(TypedSlotSet, ClearInvalidSlots) {
  TypedSlotSet set(0);
  const int kHostDelta = 100;
  uint32_t entries = 10;
  for (uint32_t i = 0; i < entries; i++) {
    SlotType type = static_cast<SlotType>(i % CLEARED_SLOT);
    set.Insert(type, i * kHostDelta);
  }

  std::map<uint32_t, uint32_t> invalid_ranges;
  for (uint32_t i = 1; i < entries; i += 2) {
    invalid_ranges.insert(
        std::pair<uint32_t, uint32_t>(i * kHostDelta, i * kHostDelta + 1));
  }

  set.ClearInvalidSlots(invalid_ranges);
  for (std::map<uint32_t, uint32_t>::iterator it = invalid_ranges.begin();
       it != invalid_ranges.end(); ++it) {
    uint32_t start = it->first;
    uint32_t end = it->second;
    set.Iterate(
        [=](SlotType slot_type, Address slot_addr) {
          CHECK(slot_addr < start || slot_addr >= end);
          return KEEP_SLOT;
        },
        TypedSlotSet::KEEP_EMPTY_CHUNKS);
  }
}

TEST(TypedSlotSet, Merge) {
  TypedSlotSet set0(0), set1(0);
  static const uint32_t kEntries = 10000;
  for (uint32_t i = 0; i < kEntries; i++) {
    set0.Insert(FULL_EMBEDDED_OBJECT_SLOT, 2 * i);
    set1.Insert(FULL_EMBEDDED_OBJECT_SLOT, 2 * i + 1);
  }
  uint32_t count = 0;
  set0.Merge(&set1);
  set0.Iterate(
      [&count](SlotType slot_type, Address slot_addr) {
        if (count < kEntries) {
          CHECK_EQ(slot_addr % 2, 0);
        } else {
          CHECK_EQ(slot_addr % 2, 1);
        }
        ++count;
        return KEEP_SLOT;
      },
      TypedSlotSet::KEEP_EMPTY_CHUNKS);
  CHECK_EQ(2 * kEntries, count);
  set1.Iterate(
      [](SlotType slot_type, Address slot_addr) {
        CHECK(false);  // Unreachable.
        return KEEP_SLOT;
      },
      TypedSlotSet::KEEP_EMPTY_CHUNKS);
}

}  // namespace internal
}  // namespace v8
