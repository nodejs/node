// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/mark-compact.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_MB
namespace {

class InnerPointerResolutionTest : public TestWithIsolate {
 public:
  struct ObjectRequest {
    int size;  // The only required field.
    enum { REGULAR, FREE } type = REGULAR;
    enum { WHITE, GREY, BLACK, BLACK_AREA } marked = WHITE;
    // If index_in_cell >= 0, the object is placed at the lowest address s.t.
    // Bitmap::IndexInCell(AddressToMarkbitIndex(address)) == index_in_cell.
    // To achieve this, padding (i.e., introducing a free-space object of the
    // appropriate size) may be necessary. If padding == CONSECUTIVE, no such
    // padding is allowed and it is just checked that object layout is as
    // intended.
    int index_in_cell = -1;
    enum { CONSECUTIVE, PAD_WHITE, PAD_BLACK } padding = CONSECUTIVE;
    Address address = kNullAddress;  // The object's address is stored here.
  };

  InnerPointerResolutionTest() {
    OldSpace* old_space = heap()->old_space();
    EXPECT_NE(nullptr, old_space);
    page_ = allocator()->AllocatePage(MemoryAllocator::AllocationMode::kRegular,
                                      old_space, NOT_EXECUTABLE);
    EXPECT_NE(nullptr, page_);
  }

  ~InnerPointerResolutionTest() override {
    allocator()->Free(MemoryAllocator::FreeMode::kImmediately, page_);
  }

  InnerPointerResolutionTest(const InnerPointerResolutionTest&) = delete;
  InnerPointerResolutionTest& operator=(const InnerPointerResolutionTest&) =
      delete;

  Heap* heap() { return isolate()->heap(); }
  Page* page() { return page_; }
  MemoryAllocator* allocator() { return heap()->memory_allocator(); }
  MarkCompactCollector* collector() { return heap()->mark_compact_collector(); }

  // Creates a list of objects in the page and ensures that the page is
  // iterable.
  void CreateObjects(std::vector<ObjectRequest>& objects) {
    Address ptr = page()->area_start();
    for (size_t i = 0; i < objects.size(); ++i) {
      CHECK_EQ(0, objects[i].size % kTaggedSize);

      // Check if padding is needed.
      const uint32_t index = page()->AddressToMarkbitIndex(ptr);
      const int index_in_cell = Bitmap::IndexInCell(index);
      if (objects[i].index_in_cell < 0) {
        objects[i].index_in_cell = index_in_cell;
      } else if (objects[i].padding != ObjectRequest::CONSECUTIVE) {
        DCHECK_LE(0, objects[i].index_in_cell);
        DCHECK_GT(Bitmap::kBitsPerCell, objects[i].index_in_cell);
        const bool black = objects[i].padding == ObjectRequest::PAD_BLACK;
        objects[i].padding = ObjectRequest::CONSECUTIVE;
        const int needed_padding_size =
            ((Bitmap::kBitsPerCell + objects[i].index_in_cell - index_in_cell) %
             Bitmap::kBitsPerCell) *
            Bitmap::kBytesPerCell;
        if (needed_padding_size > 0) {
          ObjectRequest pad{
              needed_padding_size,
              ObjectRequest::FREE,
              black ? ObjectRequest::BLACK_AREA : ObjectRequest::WHITE,
              index_in_cell,
              ObjectRequest::CONSECUTIVE,
              ptr};
          objects.insert(objects.begin() + i, pad);
          CreateObject(pad);
          ptr += needed_padding_size;
          continue;
        }
      }

      // This will fail if the marking bitmap's implementation parameters change
      // (e.g., Bitmap::kBitsPerCell) or the size of the page header changes.
      // In this case, the tests will need to be revised accordingly.
      EXPECT_EQ(index_in_cell, objects[i].index_in_cell);

      objects[i].address = ptr;
      CreateObject(objects[i]);
      ptr += objects[i].size;
    }

    // Create one last object that uses the remaining space on the page; this
    // simulates freeing the page's LAB.
    const int remaining_size = static_cast<int>(page_->area_end() - ptr);
    const uint32_t index = page()->AddressToMarkbitIndex(ptr);
    const int index_in_cell = Bitmap::IndexInCell(index);
    ObjectRequest last{
        remaining_size, ObjectRequest::FREE,        ObjectRequest::WHITE,
        index_in_cell,  ObjectRequest::CONSECUTIVE, ptr};
    objects.push_back(last);
    CreateObject(last);
  }

  void CreateObject(const ObjectRequest& object) {
    // "Allocate" (i.e., manually place) the object in the page, set the map
    // and the size.
    switch (object.type) {
      case ObjectRequest::REGULAR: {
        CHECK_LE(2 * kTaggedSize, object.size);
        ReadOnlyRoots roots(heap());
        HeapObject heap_object(HeapObject::FromAddress(object.address));
        heap_object.set_map_after_allocation(roots.unchecked_fixed_array_map(),
                                             SKIP_WRITE_BARRIER);
        FixedArray arr(FixedArray::cast(heap_object));
        arr.set_length((object.size - FixedArray::SizeFor(0)) / kTaggedSize);
        CHECK_EQ(object.size, arr.AllocatedSize());
        break;
      }
      case ObjectRequest::FREE:
        heap()->CreateFillerObjectAt(object.address, object.size);
        break;
    }

    // Mark the object in the bitmap, if necessary.
    switch (object.marked) {
      case ObjectRequest::WHITE:
        break;
      case ObjectRequest::GREY:
        collector()->marking_state()->WhiteToGrey(
            HeapObject::FromAddress(object.address));
        break;
      case ObjectRequest::BLACK:
        CHECK_LE(2 * kTaggedSize, object.size);
        collector()->marking_state()->WhiteToBlack(
            HeapObject::FromAddress(object.address));
        break;
      case ObjectRequest::BLACK_AREA:
        collector()->marking_state()->bitmap(page_)->SetRange(
            page_->AddressToMarkbitIndex(object.address),
            page_->AddressToMarkbitIndex(object.address + object.size));
        break;
    }
  }

  // This must be called with a created object and an offset inside it.
  void RunTestInside(const ObjectRequest& object, int offset) {
    CHECK_LE(0, offset);
    CHECK_GT(object.size, offset);
    Address base_ptr =
        collector()->FindBasePtrForMarking(object.address + offset);
    if (object.type == ObjectRequest::FREE ||
        object.marked == ObjectRequest::BLACK_AREA ||
        (object.marked == ObjectRequest::BLACK && offset < 2 * kTaggedSize) ||
        (object.marked == ObjectRequest::GREY && offset < kTaggedSize))
      EXPECT_EQ(kNullAddress, base_ptr);
    else
      EXPECT_EQ(object.address, base_ptr);
  }

  // This must be called with an address not contained in any created object.
  void RunTestOutside(Address ptr) {
    CHECK(!page()->Contains(ptr));
    Address base_ptr = collector()->FindBasePtrForMarking(ptr);
    EXPECT_EQ(kNullAddress, base_ptr);
  }

  void TestWith(std::vector<ObjectRequest> objects) {
    CreateObjects(objects);
    for (auto object : objects) {
      RunTestInside(object, 0);
      RunTestInside(object, 1);
      RunTestInside(object, object.size / 2);
      RunTestInside(object, object.size - 1);
    }
    const Address outside_ptr = page()->area_start() - 42;
    CHECK_LE(page()->address(), outside_ptr);
    RunTestOutside(outside_ptr);
  }

 private:
  Page* page_;
};

}  // namespace

TEST_F(InnerPointerResolutionTest, EmptyPage) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({});
}

// Tests with some objects layed out randomly.

TEST_F(InnerPointerResolutionTest, NothingMarked) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {64},
      {48},
      {52},
      {512},
      {4, ObjectRequest::FREE},
      {60},
      {8, ObjectRequest::FREE},
      {8},
      {42176},
  });
}

TEST_F(InnerPointerResolutionTest, AllMarked) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {64, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {48, ObjectRequest::REGULAR, ObjectRequest::GREY},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {42176, ObjectRequest::REGULAR, ObjectRequest::BLACK},
  });
}

TEST_F(InnerPointerResolutionTest, SomeMarked) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {42176, ObjectRequest::REGULAR, ObjectRequest::GREY},
  });
}

TEST_F(InnerPointerResolutionTest, BlackAreas) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {52, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {512, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {4, ObjectRequest::FREE, ObjectRequest::GREY},
      {60, ObjectRequest::REGULAR, ObjectRequest::BLACK},
      {8, ObjectRequest::FREE, ObjectRequest::GREY},
      {8, ObjectRequest::REGULAR, ObjectRequest::WHITE},
      {42176, ObjectRequest::REGULAR, ObjectRequest::GREY},
  });
}

// Tests with specific object layout, to cover interesting and corner cases.

TEST_F(InnerPointerResolutionTest, ThreeMarkedObjectsInSameCell) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      // Some initial large unmarked object, followed by a small marked object
      // towards the end of the cell.
      {512},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK, 20,
       ObjectRequest::PAD_WHITE},
      // Then three marked objects in the same cell.
      {32, ObjectRequest::REGULAR, ObjectRequest::BLACK, 3,
       ObjectRequest::PAD_WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK, 11},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK, 23},
      // This marked object is in the next cell.
      {64, ObjectRequest::REGULAR, ObjectRequest::BLACK, 17,
       ObjectRequest::PAD_WHITE},
  });
}

TEST_F(InnerPointerResolutionTest, ThreeBlackAreasInSameCell) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      // Some initial large unmarked object, followed by a small black area
      // towards the end of the cell.
      {512},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 20,
       ObjectRequest::PAD_WHITE},
      // Then three black areas in the same cell.
      {32, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 3,
       ObjectRequest::PAD_WHITE},
      {48, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 11},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 23},
      // This black area is in the next cell.
      {64, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 17,
       ObjectRequest::PAD_WHITE},
  });
}

TEST_F(InnerPointerResolutionTest, SmallBlackAreaAtPageStart) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 30,
       ObjectRequest::PAD_BLACK},
  });
}

TEST_F(InnerPointerResolutionTest, SmallBlackAreaAtPageStartUntilCellBoundary) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {8, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
}

TEST_F(InnerPointerResolutionTest, LargeBlackAreaAtPageStart) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {42 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 30,
       ObjectRequest::PAD_BLACK},
  });
}

TEST_F(InnerPointerResolutionTest, LargeBlackAreaAtPageStartUntilCellBoundary) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {42 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
}

TEST_F(InnerPointerResolutionTest, SmallBlackAreaStartingAtCellBoundary) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {512},
      {20, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
  });
}

TEST_F(InnerPointerResolutionTest, LargeBlackAreaStartingAtCellBoundary) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {512},
      {42 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell + 64,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
  });
}

TEST_F(InnerPointerResolutionTest, SmallBlackAreaEndingAtCellBoundary) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {512},
      {8, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 13,
       ObjectRequest::PAD_WHITE},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
}

TEST_F(InnerPointerResolutionTest, LargeBlackAreaEndingAtCellBoundary) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {512},
      {42 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell + 64,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
}
TEST_F(InnerPointerResolutionTest, TwoSmallBlackAreasAtCellBoundaries) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {512},
      {24, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
      {8, ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 25,
       ObjectRequest::PAD_WHITE},
      {64, ObjectRequest::REGULAR, ObjectRequest::WHITE, 0,
       ObjectRequest::PAD_BLACK},
  });
}

TEST_F(InnerPointerResolutionTest, BlackAreaOfOneCell) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {512},
      {Bitmap::kBitsPerCell * Bitmap::kBytesPerCell, ObjectRequest::REGULAR,
       ObjectRequest::BLACK_AREA, 0, ObjectRequest::PAD_WHITE},
  });
}

TEST_F(InnerPointerResolutionTest, BlackAreaOfManyCells) {
  if (FLAG_enable_third_party_heap) return;
  TestWith({
      {512},
      {17 * Bitmap::kBitsPerCell * Bitmap::kBytesPerCell,
       ObjectRequest::REGULAR, ObjectRequest::BLACK_AREA, 0,
       ObjectRequest::PAD_WHITE},
  });
}

#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_MB

}  // namespace internal
}  // namespace v8
