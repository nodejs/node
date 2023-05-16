// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/object-start-bitmap.h"

#include "include/cppgc/allocation.h"
#include "src/base/macros.h"
#include "src/base/page-allocator.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/page-memory.h"
#include "src/heap/cppgc/raw-heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class PageWithBitmap final {
 public:
  PageWithBitmap()
      : base_(allocator_.AllocatePages(
            nullptr, kPageSize, kPageSize,
            v8::base::PageAllocator::Permission::kReadWrite)),
        bitmap_(new(base_) ObjectStartBitmap) {}

  PageWithBitmap(const PageWithBitmap&) = delete;
  PageWithBitmap& operator=(const PageWithBitmap&) = delete;

  ~PageWithBitmap() { allocator_.FreePages(base_, kPageSize); }

  ObjectStartBitmap& bitmap() const { return *bitmap_; }

  void* base() const { return base_; }
  size_t size() const { return kPageSize; }

  v8::base::PageAllocator allocator_;
  void* base_;
  ObjectStartBitmap* bitmap_;
};

class ObjectStartBitmapTest : public ::testing::Test {
 protected:
  void AllocateObject(size_t object_position) {
    bitmap().SetBit(ObjectAddress(object_position));
  }

  void FreeObject(size_t object_position) {
    bitmap().ClearBit(ObjectAddress(object_position));
  }

  bool CheckObjectAllocated(size_t object_position) {
    return bitmap().CheckBit(ObjectAddress(object_position));
  }

  Address ObjectAddress(size_t pos) const {
    return reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(page.base()) +
                                     pos * ObjectStartBitmap::Granularity());
  }

  HeapObjectHeader* ObjectHeader(size_t pos) const {
    return reinterpret_cast<HeapObjectHeader*>(ObjectAddress(pos));
  }

  ObjectStartBitmap& bitmap() const { return page.bitmap(); }

  bool IsEmpty() const {
    size_t count = 0;
    bitmap().Iterate([&count](Address) { count++; });
    return count == 0;
  }

 private:
  PageWithBitmap page;
};

}  // namespace

TEST_F(ObjectStartBitmapTest, MoreThanZeroEntriesPossible) {
  const size_t max_entries = ObjectStartBitmap::MaxEntries();
  EXPECT_LT(0u, max_entries);
}

TEST_F(ObjectStartBitmapTest, InitialEmpty) { EXPECT_TRUE(IsEmpty()); }

TEST_F(ObjectStartBitmapTest, SetBitImpliesNonEmpty) {
  AllocateObject(0);
  EXPECT_FALSE(IsEmpty());
}

TEST_F(ObjectStartBitmapTest, SetBitCheckBit) {
  constexpr size_t object_num = 7;
  AllocateObject(object_num);
  EXPECT_TRUE(CheckObjectAllocated(object_num));
}

TEST_F(ObjectStartBitmapTest, SetBitClearbitCheckBit) {
  constexpr size_t object_num = 77;
  AllocateObject(object_num);
  FreeObject(object_num);
  EXPECT_FALSE(CheckObjectAllocated(object_num));
}

TEST_F(ObjectStartBitmapTest, SetBitClearBitImpliesEmpty) {
  constexpr size_t object_num = 123;
  AllocateObject(object_num);
  FreeObject(object_num);
  EXPECT_TRUE(IsEmpty());
}

TEST_F(ObjectStartBitmapTest, AdjacentObjectsAtBegin) {
  AllocateObject(0);
  AllocateObject(1);
  EXPECT_FALSE(CheckObjectAllocated(3));
  size_t count = 0;
  bitmap().Iterate([&count, this](Address current) {
    if (count == 0) {
      EXPECT_EQ(ObjectAddress(0), current);
    } else if (count == 1) {
      EXPECT_EQ(ObjectAddress(1), current);
    }
    count++;
  });
  EXPECT_EQ(2u, count);
}

TEST_F(ObjectStartBitmapTest, AdjacentObjectsAtEnd) {
  static constexpr size_t last_entry_index =
      ObjectStartBitmap::MaxEntries() - 1;
  AllocateObject(last_entry_index);
  AllocateObject(last_entry_index - 1);
  EXPECT_FALSE(CheckObjectAllocated(last_entry_index - 2));
  size_t count = 0;
  bitmap().Iterate([&count, this](Address current) {
    if (count == 0) {
      EXPECT_EQ(ObjectAddress(last_entry_index - 1), current);
    } else if (count == 1) {
      EXPECT_EQ(ObjectAddress(last_entry_index), current);
    }
    count++;
  });
  EXPECT_EQ(2u, count);
}

TEST_F(ObjectStartBitmapTest, FindHeaderExact) {
  constexpr size_t object_num = 654;
  AllocateObject(object_num);
  EXPECT_EQ(ObjectHeader(object_num),
            bitmap().FindHeader(ObjectAddress(object_num)));
}

TEST_F(ObjectStartBitmapTest, FindHeaderApproximate) {
  static const size_t kInternalDelta = 37;
  constexpr size_t object_num = 654;
  AllocateObject(object_num);
  EXPECT_EQ(ObjectHeader(object_num),
            bitmap().FindHeader(ObjectAddress(object_num) + kInternalDelta));
}

TEST_F(ObjectStartBitmapTest, FindHeaderIteratingWholeBitmap) {
  AllocateObject(0);
  Address hint_index = ObjectAddress(ObjectStartBitmap::MaxEntries() - 1);
  EXPECT_EQ(ObjectHeader(0), bitmap().FindHeader(hint_index));
}

TEST_F(ObjectStartBitmapTest, FindHeaderNextCell) {
  // This white box test makes use of the fact that cells are of type uint8_t.
  const size_t kCellSize = sizeof(uint8_t);
  AllocateObject(0);
  AllocateObject(kCellSize - 1);
  Address hint = ObjectAddress(kCellSize);
  EXPECT_EQ(ObjectHeader(kCellSize - 1), bitmap().FindHeader(hint));
}

TEST_F(ObjectStartBitmapTest, FindHeaderSameCell) {
  // This white box test makes use of the fact that cells are of type uint8_t.
  const size_t kCellSize = sizeof(uint8_t);
  AllocateObject(0);
  AllocateObject(kCellSize - 1);
  Address hint = ObjectAddress(kCellSize);
  EXPECT_EQ(ObjectHeader(kCellSize - 1), bitmap().FindHeader(hint));
  EXPECT_EQ(ObjectHeader(kCellSize - 1),
            bitmap().FindHeader(ObjectAddress(kCellSize - 1)));
}

}  // namespace internal
}  // namespace cppgc
