// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/object-start-bitmap.h"

#include "include/cppgc/allocation.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/object-start-bitmap-inl.h"
#include "src/heap/cppgc/page-memory-inl.h"
#include "src/heap/cppgc/raw-heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

bool IsEmpty(const ObjectStartBitmap& bitmap) {
  size_t count = 0;
  bitmap.Iterate([&count](Address) { count++; });
  return count == 0;
}

// Abstraction for objects that hides ObjectStartBitmap::kGranularity and
// the base address as getting either of it wrong will result in failed DCHECKs.
class Object {
 public:
  static Address kBaseOffset;

  explicit Object(size_t number) : number_(number) {
    const size_t max_entries = ObjectStartBitmap::MaxEntries();
    EXPECT_GE(max_entries, number_);
  }

  Address address() const {
    return kBaseOffset + ObjectStartBitmap::Granularity() * number_;
  }

  HeapObjectHeader* header() const {
    return reinterpret_cast<HeapObjectHeader*>(address());
  }

  // Allow implicitly converting Object to Address.
  operator Address() const { return address(); }

 private:
  const size_t number_;
};

Address Object::kBaseOffset = reinterpret_cast<Address>(0x4000);

}  // namespace

TEST(ObjectStartBitmapTest, MoreThanZeroEntriesPossible) {
  const size_t max_entries = ObjectStartBitmap::MaxEntries();
  EXPECT_LT(0u, max_entries);
}

TEST(ObjectStartBitmapTest, InitialEmpty) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  EXPECT_TRUE(IsEmpty(bitmap));
}

TEST(ObjectStartBitmapTest, SetBitImpliesNonEmpty) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  bitmap.SetBit(Object(0));
  EXPECT_FALSE(IsEmpty(bitmap));
}

TEST(ObjectStartBitmapTest, SetBitCheckBit) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object(7);
  bitmap.SetBit(object);
  EXPECT_TRUE(bitmap.CheckBit(object));
}

TEST(ObjectStartBitmapTest, SetBitClearbitCheckBit) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object(77);
  bitmap.SetBit(object);
  bitmap.ClearBit(object);
  EXPECT_FALSE(bitmap.CheckBit(object));
}

TEST(ObjectStartBitmapTest, SetBitClearBitImpliesEmpty) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object(123);
  bitmap.SetBit(object);
  bitmap.ClearBit(object);
  EXPECT_TRUE(IsEmpty(bitmap));
}

TEST(ObjectStartBitmapTest, AdjacentObjectsAtBegin) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object0(0);
  Object object1(1);
  bitmap.SetBit(object0);
  bitmap.SetBit(object1);
  EXPECT_FALSE(bitmap.CheckBit(Object(3)));
  size_t count = 0;
  bitmap.Iterate([&count, object0, object1](Address current) {
    if (count == 0) {
      EXPECT_EQ(object0.address(), current);
    } else if (count == 1) {
      EXPECT_EQ(object1.address(), current);
    }
    count++;
  });
  EXPECT_EQ(2u, count);
}

TEST(ObjectStartBitmapTest, AdjacentObjectsAtEnd) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  const size_t last_entry_index = ObjectStartBitmap::MaxEntries() - 1;
  Object object0(last_entry_index - 1);
  Object object1(last_entry_index);
  bitmap.SetBit(object0);
  bitmap.SetBit(object1);
  EXPECT_FALSE(bitmap.CheckBit(Object(last_entry_index - 2)));
  size_t count = 0;
  bitmap.Iterate([&count, object0, object1](Address current) {
    if (count == 0) {
      EXPECT_EQ(object0.address(), current);
    } else if (count == 1) {
      EXPECT_EQ(object1.address(), current);
    }
    count++;
  });
  EXPECT_EQ(2u, count);
}

TEST(ObjectStartBitmapTest, FindHeaderExact) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object(654);
  bitmap.SetBit(object);
  EXPECT_EQ(object.header(), bitmap.FindHeader(object.address()));
}

TEST(ObjectStartBitmapTest, FindHeaderApproximate) {
  static const size_t kInternalDelta = 37;
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object(654);
  bitmap.SetBit(object);
  EXPECT_EQ(object.header(),
            bitmap.FindHeader(object.address() + kInternalDelta));
}

TEST(ObjectStartBitmapTest, FindHeaderIteratingWholeBitmap) {
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object_to_find(Object(0));
  Address hint_index = Object(ObjectStartBitmap::MaxEntries() - 1);
  bitmap.SetBit(object_to_find);
  EXPECT_EQ(object_to_find.header(), bitmap.FindHeader(hint_index));
}

TEST(ObjectStartBitmapTest, FindHeaderNextCell) {
  // This white box test makes use of the fact that cells are of type uint8_t.
  const size_t kCellSize = sizeof(uint8_t);
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object_to_find(Object(kCellSize - 1));
  Address hint = Object(kCellSize);
  bitmap.SetBit(Object(0));
  bitmap.SetBit(object_to_find);
  EXPECT_EQ(object_to_find.header(), bitmap.FindHeader(hint));
}

TEST(ObjectStartBitmapTest, FindHeaderSameCell) {
  // This white box test makes use of the fact that cells are of type uint8_t.
  const size_t kCellSize = sizeof(uint8_t);
  ObjectStartBitmap bitmap(Object::kBaseOffset);
  Object object_to_find(Object(kCellSize - 1));
  bitmap.SetBit(Object(0));
  bitmap.SetBit(object_to_find);
  EXPECT_EQ(object_to_find.header(),
            bitmap.FindHeader(object_to_find.address()));
}

}  // namespace internal
}  // namespace cppgc
