// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/object-start-bitmap.h"

#include "src/base/macros.h"
#include "src/heap/object-start-bitmap-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

bool IsEmpty(const ObjectStartBitmap& bitmap) {
  size_t count = 0;
  bitmap.Iterate([&count](Address) { count++; });
  return count == 0;
}

// Abstraction for objects that hides ObjectStartBitmap::kGranularity and
// the base address as getting either of it wrong will result in failed DCHECKs.
class TestObject {
 public:
  static PtrComprCageBase kCageBase;
  static Address kBaseOffset;

  explicit TestObject(size_t number) : number_(number) {
    const size_t max_entries = ObjectStartBitmap::MaxEntries();
    EXPECT_GE(max_entries, number_);
  }

  Address base_ptr() const {
    return kBaseOffset + ObjectStartBitmap::Granularity() * number_;
  }

  // Allow implicitly converting Object to Address.
  operator Address() const { return base_ptr(); }

 private:
  const size_t number_;
};

PtrComprCageBase TestObject::kCageBase{0xca6e00000000ul};
Address TestObject::kBaseOffset = reinterpret_cast<Address>(0x4000ul);

}  // namespace

TEST(V8ObjectStartBitmapTest, MoreThanZeroEntriesPossible) {
  const size_t max_entries = ObjectStartBitmap::MaxEntries();
  EXPECT_LT(0u, max_entries);
}

TEST(V8ObjectStartBitmapTest, InitialEmpty) {
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  EXPECT_TRUE(IsEmpty(bitmap));
}

TEST(V8ObjectStartBitmapTest, SetBitImpliesNonEmpty) {
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  bitmap.SetBit(TestObject(0));
  EXPECT_FALSE(IsEmpty(bitmap));
}

TEST(V8ObjectStartBitmapTest, SetBitCheckBit) {
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object(7);
  bitmap.SetBit(object);
  EXPECT_TRUE(bitmap.CheckBit(object));
}

TEST(V8ObjectStartBitmapTest, SetBitClearbitCheckBit) {
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object(77);
  bitmap.SetBit(object);
  bitmap.ClearBit(object);
  EXPECT_FALSE(bitmap.CheckBit(object));
}

TEST(V8ObjectStartBitmapTest, SetBitClearBitImpliesEmpty) {
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object(123);
  bitmap.SetBit(object);
  bitmap.ClearBit(object);
  EXPECT_TRUE(IsEmpty(bitmap));
}

TEST(V8ObjectStartBitmapTest, AdjacentObjectsAtBegin) {
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object0(0);
  TestObject object1(1);
  bitmap.SetBit(object0);
  bitmap.SetBit(object1);
  EXPECT_FALSE(bitmap.CheckBit(TestObject(3)));
  size_t count = 0;
  bitmap.Iterate([&count, object0, object1](Address current) {
    if (count == 0) {
      EXPECT_EQ(object0.base_ptr(), current);
    } else if (count == 1) {
      EXPECT_EQ(object1.base_ptr(), current);
    }
    count++;
  });
  EXPECT_EQ(2u, count);
}

TEST(V8ObjectStartBitmapTest, AdjacentObjectsAtEnd) {
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  const size_t last_entry_index = ObjectStartBitmap::MaxEntries() - 1;
  TestObject object0(last_entry_index - 1);
  TestObject object1(last_entry_index);
  bitmap.SetBit(object0);
  bitmap.SetBit(object1);
  EXPECT_FALSE(bitmap.CheckBit(TestObject(last_entry_index - 2)));
  size_t count = 0;
  bitmap.Iterate([&count, object0, object1](Address current) {
    if (count == 0) {
      EXPECT_EQ(object0.base_ptr(), current);
    } else if (count == 1) {
      EXPECT_EQ(object1.base_ptr(), current);
    }
    count++;
  });
  EXPECT_EQ(2u, count);
}

TEST(V8ObjectStartBitmapTest, FindBasePtrExact) {
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object(654);
  bitmap.SetBit(object);
  EXPECT_EQ(object.base_ptr(), bitmap.FindBasePtrImpl(object.base_ptr()));
}

TEST(V8ObjectStartBitmapTest, FindBasePtrApproximate) {
  const size_t kInternalDelta = 37;
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object(654);
  bitmap.SetBit(object);
  EXPECT_EQ(object.base_ptr(),
            bitmap.FindBasePtrImpl(object.base_ptr() + kInternalDelta));
}

TEST(V8ObjectStartBitmapTest, FindBasePtrIteratingWholeBitmap) {
  const size_t kLastWordDelta = ObjectStartBitmap::MaxEntries() - 1;
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object_to_find(0);
  bitmap.SetBit(object_to_find);
  Address hint_index = TestObject(kLastWordDelta);
  EXPECT_EQ(object_to_find.base_ptr(), bitmap.FindBasePtrImpl(hint_index));
}

TEST(V8ObjectStartBitmapTest, FindBasePtrNextCell) {
  // This white box test makes use of the fact that cells are of type uint32_t.
  const size_t kCellSize = sizeof(uint32_t);
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object_to_find(kCellSize - 1);
  Address hint = TestObject(kCellSize);
  bitmap.SetBit(TestObject(0));
  bitmap.SetBit(object_to_find);
  EXPECT_EQ(object_to_find.base_ptr(), bitmap.FindBasePtrImpl(hint));
}

TEST(V8ObjectStartBitmapTest, FindBasePtrSameCell) {
  // This white box test makes use of the fact that cells are of type uint32_t.
  const size_t kCellSize = sizeof(uint32_t);
  ObjectStartBitmap bitmap(TestObject::kCageBase, TestObject::kBaseOffset);
  TestObject object_to_find(kCellSize - 1);
  Address hint = object_to_find;
  bitmap.SetBit(TestObject(0));
  bitmap.SetBit(object_to_find);
  EXPECT_EQ(object_to_find.base_ptr(), bitmap.FindBasePtrImpl(hint));
}

// TODO(v8:12851): If the ObjectStartBitmap implementation stays, unit tests
// should be added to test the functionality of method FindBasePtr.

}  // namespace internal
}  // namespace v8
