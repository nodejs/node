// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <numeric>

#include "include/cppgc/allocation.h"
#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/prefinalizer.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/object-view.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class WorkloadsTest : public testing::TestWithHeap {
 public:
  void ConservativeGC() {
    internal::Heap::From(GetHeap())->CollectGarbage(
        GCConfig::ConservativeAtomicConfig());
  }
  void PreciseGC() {
    internal::Heap::From(GetHeap())->CollectGarbage(
        GCConfig::PreciseAtomicConfig());
  }
};

class SuperClass;

class PointsBack final : public GarbageCollected<PointsBack> {
 public:
  PointsBack() { ++alive_count_; }
  ~PointsBack() { --alive_count_; }

  void SetBackPointer(SuperClass* back_pointer) {
    back_pointer_ = back_pointer;
  }

  SuperClass* BackPointer() const { return back_pointer_; }

  void Trace(Visitor* visitor) const { visitor->Trace(back_pointer_); }

  static int alive_count_;

 private:
  WeakMember<SuperClass> back_pointer_;
};
int PointsBack::alive_count_ = 0;

class SuperClass : public GarbageCollected<SuperClass> {
 public:
  explicit SuperClass(PointsBack* points_back) : points_back_(points_back) {
    points_back_->SetBackPointer(this);
    ++alive_count_;
  }
  virtual ~SuperClass() { --alive_count_; }

  void InvokeConservativeGCAndExpect(WorkloadsTest* test, SuperClass* target,
                                     PointsBack* points_back,
                                     int super_class_count) {
    test->ConservativeGC();
    EXPECT_EQ(points_back, target->GetPointsBack());
    EXPECT_EQ(super_class_count, SuperClass::alive_count_);
  }

  virtual void Trace(Visitor* visitor) const { visitor->Trace(points_back_); }

  PointsBack* GetPointsBack() const { return points_back_.Get(); }

  static int alive_count_;

 private:
  Member<PointsBack> points_back_;
};
int SuperClass::alive_count_ = 0;

class SubData final : public GarbageCollected<SubData> {
 public:
  SubData() { ++alive_count_; }
  ~SubData() { --alive_count_; }

  void Trace(Visitor* visitor) const {}

  static int alive_count_;
};
int SubData::alive_count_ = 0;

class SubClass final : public SuperClass {
 public:
  explicit SubClass(AllocationHandle& allocation_handle,
                    PointsBack* points_back)
      : SuperClass(points_back),
        data_(MakeGarbageCollected<SubData>(allocation_handle)) {
    ++alive_count_;
  }
  ~SubClass() final { --alive_count_; }

  void Trace(Visitor* visitor) const final {
    visitor->Trace(data_);
    SuperClass::Trace(visitor);
  }

  static int alive_count_;

 private:
  Member<SubData> data_;
};
int SubClass::alive_count_ = 0;

}  // namespace

TEST_F(WorkloadsTest, Transition) {
  PointsBack::alive_count_ = 0;
  SuperClass::alive_count_ = 0;
  SubClass::alive_count_ = 0;
  SubData::alive_count_ = 0;

  Persistent<PointsBack> points_back1 =
      MakeGarbageCollected<PointsBack>(GetAllocationHandle());
  Persistent<PointsBack> points_back2 =
      MakeGarbageCollected<PointsBack>(GetAllocationHandle());
  Persistent<SuperClass> super_class =
      MakeGarbageCollected<SuperClass>(GetAllocationHandle(), points_back1);
  Persistent<SubClass> sub_class = MakeGarbageCollected<SubClass>(
      GetAllocationHandle(), GetAllocationHandle(), points_back2);
  EXPECT_EQ(2, PointsBack::alive_count_);
  EXPECT_EQ(2, SuperClass::alive_count_);
  EXPECT_EQ(1, SubClass::alive_count_);
  EXPECT_EQ(1, SubData::alive_count_);

  PreciseGC();
  EXPECT_EQ(2, PointsBack::alive_count_);
  EXPECT_EQ(2, SuperClass::alive_count_);
  EXPECT_EQ(1, SubClass::alive_count_);
  EXPECT_EQ(1, SubData::alive_count_);

  super_class->InvokeConservativeGCAndExpect(this, super_class.Release(),
                                             points_back1.Get(), 2);
  PreciseGC();
  EXPECT_EQ(2, PointsBack::alive_count_);
  EXPECT_EQ(1, SuperClass::alive_count_);
  EXPECT_EQ(1, SubClass::alive_count_);
  EXPECT_EQ(1, SubData::alive_count_);
  EXPECT_EQ(nullptr, points_back1->BackPointer());

  points_back1.Release();
  PreciseGC();
  EXPECT_EQ(1, PointsBack::alive_count_);
  EXPECT_EQ(1, SuperClass::alive_count_);
  EXPECT_EQ(1, SubClass::alive_count_);
  EXPECT_EQ(1, SubData::alive_count_);

  sub_class->InvokeConservativeGCAndExpect(this, sub_class.Release(),
                                           points_back2.Get(), 1);
  PreciseGC();
  EXPECT_EQ(1, PointsBack::alive_count_);
  EXPECT_EQ(0, SuperClass::alive_count_);
  EXPECT_EQ(0, SubClass::alive_count_);
  EXPECT_EQ(0, SubData::alive_count_);
  EXPECT_EQ(nullptr, points_back2->BackPointer());

  points_back2.Release();
  PreciseGC();
  EXPECT_EQ(0, PointsBack::alive_count_);
  EXPECT_EQ(0, SuperClass::alive_count_);
  EXPECT_EQ(0, SubClass::alive_count_);
  EXPECT_EQ(0, SubData::alive_count_);

  EXPECT_EQ(super_class, sub_class);
}

namespace {

class DynamicallySizedObject final
    : public GarbageCollected<DynamicallySizedObject> {
 public:
  static DynamicallySizedObject* Create(AllocationHandle& allocation_handle,
                                        size_t size) {
    CHECK_GT(size, sizeof(DynamicallySizedObject));
    return MakeGarbageCollected<DynamicallySizedObject>(
        allocation_handle,
        AdditionalBytes(size - sizeof(DynamicallySizedObject)));
  }

  uint8_t Get(int i) { return *(reinterpret_cast<uint8_t*>(this) + i); }

  void Trace(Visitor* visitor) const {}
};

class ObjectSizeCounter final : private HeapVisitor<ObjectSizeCounter> {
  friend class HeapVisitor<ObjectSizeCounter>;

 public:
  size_t GetSize(RawHeap& heap) {
    Traverse(heap);
    return accumulated_size_;
  }

 private:
  static size_t ObjectSize(const HeapObjectHeader& header) {
    return ObjectView<>(header).Size();
  }

  bool VisitHeapObjectHeader(HeapObjectHeader& header) {
    if (header.IsFree()) return true;
    accumulated_size_ += ObjectSize(header);
    return true;
  }

  size_t accumulated_size_ = 0;
};

}  // namespace

TEST_F(WorkloadsTest, BasicFunctionality) {
  static_assert(kAllocationGranularity % 4 == 0,
                "Allocation granularity is expected to be a multiple of 4");
  Heap* heap = internal::Heap::From(GetHeap());
  size_t initial_object_payload_size =
      ObjectSizeCounter().GetSize(heap->raw_heap());
  {
    // When the test starts there may already have been leaked some memory
    // on the heap, so we establish a base line.
    size_t base_level = initial_object_payload_size;
    bool test_pages_allocated = !base_level;
    if (test_pages_allocated) {
      EXPECT_EQ(0ul, heap->stats_collector()->allocated_memory_size());
    }

    // This allocates objects on the general heap which should add a page of
    // memory.
    DynamicallySizedObject* alloc32 =
        DynamicallySizedObject::Create(GetAllocationHandle(), 32);
    memset(alloc32, 40, 32);
    DynamicallySizedObject* alloc64 =
        DynamicallySizedObject::Create(GetAllocationHandle(), 64);
    memset(alloc64, 27, 64);

    size_t total = 96;

    EXPECT_EQ(base_level + total,
              ObjectSizeCounter().GetSize(heap->raw_heap()));
    if (test_pages_allocated) {
      EXPECT_EQ(kPageSize * 2,
                heap->stats_collector()->allocated_memory_size());
    }

    EXPECT_EQ(alloc32->Get(0), 40);
    EXPECT_EQ(alloc32->Get(31), 40);
    EXPECT_EQ(alloc64->Get(0), 27);
    EXPECT_EQ(alloc64->Get(63), 27);

    ConservativeGC();

    EXPECT_EQ(alloc32->Get(0), 40);
    EXPECT_EQ(alloc32->Get(31), 40);
    EXPECT_EQ(alloc64->Get(0), 27);
    EXPECT_EQ(alloc64->Get(63), 27);
  }

  PreciseGC();
  size_t total = 0;
  size_t base_level = ObjectSizeCounter().GetSize(heap->raw_heap());
  bool test_pages_allocated = !base_level;
  if (test_pages_allocated) {
    EXPECT_EQ(0ul, heap->stats_collector()->allocated_memory_size());
  }

  size_t big = 1008;
  Persistent<DynamicallySizedObject> big_area =
      DynamicallySizedObject::Create(GetAllocationHandle(), big);
  total += big;

  size_t persistent_count = 0;
  const size_t kNumPersistents = 100000;
  Persistent<DynamicallySizedObject>* persistents[kNumPersistents];

  for (int i = 0; i < 1000; i++) {
    size_t size = 128 + i * 8;
    total += size;
    persistents[persistent_count++] = new Persistent<DynamicallySizedObject>(
        DynamicallySizedObject::Create(GetAllocationHandle(), size));
    // The allocations in the loop may trigger GC with lazy sweeping.
    heap->sweeper().FinishIfRunning();
    EXPECT_EQ(base_level + total,
              ObjectSizeCounter().GetSize(heap->raw_heap()));
    if (test_pages_allocated) {
      EXPECT_EQ(0ul, heap->stats_collector()->allocated_memory_size() &
                         (kPageSize - 1));
    }
  }

  {
    DynamicallySizedObject* alloc32b(
        DynamicallySizedObject::Create(GetAllocationHandle(), 32));
    memset(alloc32b, 40, 32);
    DynamicallySizedObject* alloc64b(
        DynamicallySizedObject::Create(GetAllocationHandle(), 64));
    memset(alloc64b, 27, 64);
    EXPECT_TRUE(alloc32b != alloc64b);

    total += 96;
    EXPECT_EQ(base_level + total,
              ObjectSizeCounter().GetSize(heap->raw_heap()));
    if (test_pages_allocated) {
      EXPECT_EQ(0ul, heap->stats_collector()->allocated_memory_size() &
                         (kPageSize - 1));
    }
  }

  PreciseGC();
  total -= 96;
  if (test_pages_allocated) {
    EXPECT_EQ(0ul, heap->stats_collector()->allocated_memory_size() &
                       (kPageSize - 1));
  }

  // Clear the persistent, so that the big area will be garbage collected.
  big_area.Release();
  PreciseGC();

  total -= big;
  EXPECT_EQ(base_level + total, ObjectSizeCounter().GetSize(heap->raw_heap()));
  if (test_pages_allocated) {
    EXPECT_EQ(0ul, heap->stats_collector()->allocated_memory_size() &
                       (kPageSize - 1));
  }

  EXPECT_EQ(base_level + total, ObjectSizeCounter().GetSize(heap->raw_heap()));
  if (test_pages_allocated) {
    EXPECT_EQ(0ul, heap->stats_collector()->allocated_memory_size() &
                       (kPageSize - 1));
  }

  for (size_t i = 0; i < persistent_count; i++) {
    delete persistents[i];
    persistents[i] = nullptr;
  }
}

}  // namespace internal
}  // namespace cppgc
