// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"

#include "include/cppgc/visitor.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class CppgcAllocationTest : public testing::TestWithHeap {};

struct GCed final : GarbageCollected<GCed> {
  void Trace(cppgc::Visitor*) const {}
};

class HeapAllocatedArray final : public GarbageCollected<HeapAllocatedArray> {
 public:
  HeapAllocatedArray() {
    for (int i = 0; i < kArraySize; ++i) {
      array_[i] = i % 128;
    }
  }

  int8_t at(size_t i) { return array_[i]; }
  void Trace(Visitor* visitor) const {}

 private:
  static const int kArraySize = 1000;
  int8_t array_[kArraySize];
};

}  // namespace

TEST_F(CppgcAllocationTest, MakeGarbageCollectedPreservesPayload) {
  // Allocate an object in the heap.
  HeapAllocatedArray* array =
      MakeGarbageCollected<HeapAllocatedArray>(GetAllocationHandle());

  // Sanity check of the contents in the heap.
  EXPECT_EQ(0, array->at(0));
  EXPECT_EQ(42, array->at(42));
  EXPECT_EQ(0, array->at(128));
  EXPECT_EQ(999 % 128, array->at(999));
}

TEST_F(CppgcAllocationTest, ReuseMemoryFromFreelist) {
  // Allocate 3 objects so that the address we look for below is not at the
  // start of the page.
  MakeGarbageCollected<GCed>(GetAllocationHandle());
  MakeGarbageCollected<GCed>(GetAllocationHandle());
  GCed* p1 = MakeGarbageCollected<GCed>(GetAllocationHandle());
  // GC reclaims all objects. LABs are reset during the GC.
  PreciseGC();
  // Now the freed memory in the first GC should be reused. Allocating 3
  // objects again should suffice but allocating 5 to give the test some slack.
  bool reused_memory_found = false;
  for (int i = 0; i < 5; i++) {
    GCed* p2 = MakeGarbageCollected<GCed>(GetAllocationHandle());
    if (p1 == p2) {
      reused_memory_found = true;
      break;
    }
  }
  EXPECT_TRUE(reused_memory_found);
}

namespace {
class CallbackInCtor final : public GarbageCollected<CallbackInCtor> {
 public:
  template <typename Callback>
  explicit CallbackInCtor(Callback callback) {
    callback();
  }

  void Trace(Visitor*) const {}
};
}  // namespace

TEST_F(CppgcAllocationTest,
       ConservativeGCDuringAllocationDoesNotReclaimObject) {
  CallbackInCtor* obj = MakeGarbageCollected<CallbackInCtor>(
      GetAllocationHandle(), [this]() { ConservativeGC(); });
  EXPECT_FALSE(HeapObjectHeader::FromObject(obj).IsFree());
}

// The test below requires that a large object is reused in the GC. This only
// reliably works on 64-bit builds using caged heap. On 32-bit builds large
// objects are mapped in individually and returned to the OS as a whole on
// reclamation.
#if defined(CPPGC_CAGED_HEAP)

namespace {
class LargeObjectCheckingPayloadForZeroMemory final
    : public GarbageCollected<LargeObjectCheckingPayloadForZeroMemory> {
 public:
  static constexpr size_t kDataSize = kLargeObjectSizeThreshold + 1;
  static size_t destructor_calls;

  LargeObjectCheckingPayloadForZeroMemory() {
    for (size_t i = 0; i < kDataSize; ++i) {
      EXPECT_EQ(0, data[i]);
    }
  }
  ~LargeObjectCheckingPayloadForZeroMemory() { ++destructor_calls; }
  void Trace(Visitor*) const {}

  char data[kDataSize];
};
size_t LargeObjectCheckingPayloadForZeroMemory::destructor_calls = 0u;
}  // namespace

TEST_F(CppgcAllocationTest, LargePagesAreZeroedOut) {
  LargeObjectCheckingPayloadForZeroMemory::destructor_calls = 0u;
  auto* initial_object =
      MakeGarbageCollected<LargeObjectCheckingPayloadForZeroMemory>(
          GetAllocationHandle());
  memset(initial_object->data, 0xff,
         LargeObjectCheckingPayloadForZeroMemory::kDataSize);
  // GC ignores stack and thus frees the object.
  PreciseGC();
  EXPECT_EQ(1u, LargeObjectCheckingPayloadForZeroMemory::destructor_calls);
  auto* new_object =
      MakeGarbageCollected<LargeObjectCheckingPayloadForZeroMemory>(
          GetAllocationHandle());
  // If the following check fails, then the GC didn't reuse the underlying page
  // and the test doesn't check anything.
  EXPECT_EQ(initial_object, new_object);
}

#endif  // defined(CPPGC_CAGED_HEAP)

namespace {

constexpr size_t kDoubleWord = 2 * sizeof(void*);
constexpr size_t kWord = sizeof(void*);

class alignas(kDoubleWord) DoubleWordAligned final
    : public GarbageCollected<DoubleWordAligned> {
 public:
  void Trace(Visitor*) const {}
};

class alignas(kDoubleWord) LargeDoubleWordAligned
    : public GarbageCollected<LargeDoubleWordAligned> {
 public:
  virtual void Trace(cppgc::Visitor*) const {}
  char array[kLargeObjectSizeThreshold];
};

template <size_t Size>
class CustomPadding final : public GarbageCollected<CustomPadding<Size>> {
 public:
  void Trace(cppgc::Visitor* visitor) const {}
  char base_size[128];  // Gets allocated in using RegularSpaceType::kNormal4.
  char padding[Size];
};

template <size_t Size>
class alignas(kDoubleWord) AlignedCustomPadding final
    : public GarbageCollected<AlignedCustomPadding<Size>> {
 public:
  void Trace(cppgc::Visitor* visitor) const {}
  char base_size[128];  // Gets allocated in using RegularSpaceType::kNormal4.
  char padding[Size];
};

}  // namespace

TEST_F(CppgcAllocationTest, DoubleWordAlignedAllocation) {
  static constexpr size_t kAlignmentMask = kDoubleWord - 1;
  auto* gced = MakeGarbageCollected<DoubleWordAligned>(GetAllocationHandle());
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(gced) & kAlignmentMask);
}

TEST_F(CppgcAllocationTest, LargeDoubleWordAlignedAllocation) {
  static constexpr size_t kAlignmentMask = kDoubleWord - 1;
  auto* gced =
      MakeGarbageCollected<LargeDoubleWordAligned>(GetAllocationHandle());
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(gced) & kAlignmentMask);
}

TEST_F(CppgcAllocationTest, AlignToDoubleWordFromUnaligned) {
  static constexpr size_t kAlignmentMask = kDoubleWord - 1;
  // The address from which the next object can be allocated, i.e. the end of
  // |padding_object|, should not be double-word aligned. Allocate extra objects
  // to ensure padding in case payload start is 16-byte aligned.
  using PaddingObject = CustomPadding<kDoubleWord>;
  static_assert(((sizeof(HeapObjectHeader) + sizeof(PaddingObject)) %
                 kDoubleWord) == kWord);

  void* padding_object = nullptr;
  if (NormalPage::PayloadSize() % kDoubleWord == 0) {
    padding_object = MakeGarbageCollected<PaddingObject>(GetAllocationHandle());
    ASSERT_EQ(kWord, (reinterpret_cast<uintptr_t>(padding_object) +
                      sizeof(PaddingObject)) &
                         kAlignmentMask);
  }

  auto* aligned_object =
      MakeGarbageCollected<AlignedCustomPadding<16>>(GetAllocationHandle());
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(aligned_object) & kAlignmentMask);
  if (padding_object) {
    // Test only yielded a reliable result if objects are adjacent to each
    // other.
    ASSERT_EQ(reinterpret_cast<uintptr_t>(padding_object) +
                  sizeof(PaddingObject) + sizeof(HeapObjectHeader),
              reinterpret_cast<uintptr_t>(aligned_object));
  }
}

TEST_F(CppgcAllocationTest, AlignToDoubleWordFromAligned) {
  static constexpr size_t kAlignmentMask = kDoubleWord - 1;
  // The address from which the next object can be allocated, i.e. the end of
  // |padding_object|, should be double-word aligned. Allocate extra objects to
  // ensure padding in case payload start is 8-byte aligned.
  using PaddingObject = CustomPadding<kDoubleWord>;
  static_assert(((sizeof(HeapObjectHeader) + sizeof(PaddingObject)) %
                 kDoubleWord) == kWord);

  void* padding_object = nullptr;
  if (NormalPage::PayloadSize() % kDoubleWord == kWord) {
    padding_object = MakeGarbageCollected<PaddingObject>(GetAllocationHandle());
    ASSERT_EQ(0u, (reinterpret_cast<uintptr_t>(padding_object) +
                   sizeof(PaddingObject)) &
                      kAlignmentMask);
  }

  auto* aligned_object =
      MakeGarbageCollected<AlignedCustomPadding<16>>(GetAllocationHandle());
  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(aligned_object) & kAlignmentMask);
  if (padding_object) {
    // Test only yielded a reliable result if objects are adjacent to each
    // other.
    ASSERT_EQ(reinterpret_cast<uintptr_t>(padding_object) +
                  sizeof(PaddingObject) + 2 * sizeof(HeapObjectHeader),
              reinterpret_cast<uintptr_t>(aligned_object));
  }
}

}  // namespace internal
}  // namespace cppgc
