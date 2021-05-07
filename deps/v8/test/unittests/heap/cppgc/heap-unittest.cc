// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap.h"

#include <algorithm>
#include <iterator>
#include <numeric>

#include "include/cppgc/allocation.h"
#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/persistent.h"
#include "include/cppgc/prefinalizer.h"
#include "src/heap/cppgc/globals.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class GCHeapTest : public testing::TestWithHeap {
 public:
  void ConservativeGC() {
    internal::Heap::From(GetHeap())->CollectGarbage(
        Heap::Config::ConservativeAtomicConfig());
  }
  void PreciseGC() {
    internal::Heap::From(GetHeap())->CollectGarbage(
        Heap::Config::PreciseAtomicConfig());
  }
};

class GCHeapDeathTest : public GCHeapTest {};

class Foo : public GarbageCollected<Foo> {
 public:
  static size_t destructor_callcount;

  Foo() { destructor_callcount = 0; }
  ~Foo() { destructor_callcount++; }

  void Trace(cppgc::Visitor*) const {}
};

size_t Foo::destructor_callcount;

template <size_t Size>
class GCed : public GarbageCollected<Foo> {
 public:
  void Trace(cppgc::Visitor*) const {}
  char buf[Size];
};

}  // namespace

TEST_F(GCHeapTest, PreciseGCReclaimsObjectOnStack) {
  Foo* volatile do_not_access =
      MakeGarbageCollected<Foo>(GetAllocationHandle());
  USE(do_not_access);
  EXPECT_EQ(0u, Foo::destructor_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Foo::destructor_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Foo::destructor_callcount);
}

namespace {

const void* ConservativeGCReturningObject(cppgc::Heap* heap,
                                          const void* volatile object) {
  internal::Heap::From(heap)->CollectGarbage(
      Heap::Config::ConservativeAtomicConfig());
  return object;
}

}  // namespace

TEST_F(GCHeapTest, ConservativeGCRetainsObjectOnStack) {
  Foo* volatile object = MakeGarbageCollected<Foo>(GetAllocationHandle());
  EXPECT_EQ(0u, Foo::destructor_callcount);
  EXPECT_EQ(object, ConservativeGCReturningObject(GetHeap(), object));
  EXPECT_EQ(0u, Foo::destructor_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Foo::destructor_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Foo::destructor_callcount);
}

TEST_F(GCHeapTest, ObjectPayloadSize) {
  static constexpr size_t kNumberOfObjectsPerArena = 16;
  static constexpr size_t kObjectSizes[] = {1, 32, 64, 128,
                                            2 * kLargeObjectSizeThreshold};

  Heap::From(GetHeap())->CollectGarbage(
      GarbageCollector::Config::ConservativeAtomicConfig());

  subtle::NoGarbageCollectionScope no_gc(*Heap::From(GetHeap()));

  for (size_t k = 0; k < kNumberOfObjectsPerArena; ++k) {
    MakeGarbageCollected<GCed<kObjectSizes[0]>>(GetAllocationHandle());
    MakeGarbageCollected<GCed<kObjectSizes[1]>>(GetAllocationHandle());
    MakeGarbageCollected<GCed<kObjectSizes[2]>>(GetAllocationHandle());
    MakeGarbageCollected<GCed<kObjectSizes[3]>>(GetAllocationHandle());
    MakeGarbageCollected<GCed<kObjectSizes[4]>>(GetAllocationHandle());
  }

  size_t aligned_object_sizes[arraysize(kObjectSizes)];
  std::transform(std::cbegin(kObjectSizes), std::cend(kObjectSizes),
                 std::begin(aligned_object_sizes), [](size_t size) {
                   return RoundUp(size, kAllocationGranularity);
                 });
  const size_t expected_size = std::accumulate(
      std::cbegin(aligned_object_sizes), std::cend(aligned_object_sizes), 0u,
      [](size_t acc, size_t size) {
        return acc + kNumberOfObjectsPerArena * size;
      });
  // TODO(chromium:1056170): Change to EXPECT_EQ when proper sweeping is
  // implemented.
  EXPECT_LE(expected_size, Heap::From(GetHeap())->ObjectPayloadSize());
}

TEST_F(GCHeapTest, AllocateWithAdditionalBytes) {
  static constexpr size_t kBaseSize = sizeof(HeapObjectHeader) + sizeof(Foo);
  static constexpr size_t kAdditionalBytes = 10u * kAllocationGranularity;
  {
    Foo* object = MakeGarbageCollected<Foo>(GetAllocationHandle());
    EXPECT_LE(kBaseSize, HeapObjectHeader::FromPayload(object).GetSize());
  }
  {
    Foo* object = MakeGarbageCollected<Foo>(GetAllocationHandle(),
                                            AdditionalBytes(kAdditionalBytes));
    EXPECT_LE(kBaseSize + kAdditionalBytes,
              HeapObjectHeader::FromPayload(object).GetSize());
  }
  {
    Foo* object = MakeGarbageCollected<Foo>(
        GetAllocationHandle(),
        AdditionalBytes(kAdditionalBytes * kAdditionalBytes));
    EXPECT_LE(kBaseSize + kAdditionalBytes * kAdditionalBytes,
              HeapObjectHeader::FromPayload(object).GetSize());
  }
}

TEST_F(GCHeapTest, AllocatedSizeDependOnAdditionalBytes) {
  static constexpr size_t kAdditionalBytes = 10u * kAllocationGranularity;
  Foo* object = MakeGarbageCollected<Foo>(GetAllocationHandle());
  Foo* object_with_bytes = MakeGarbageCollected<Foo>(
      GetAllocationHandle(), AdditionalBytes(kAdditionalBytes));
  Foo* object_with_more_bytes = MakeGarbageCollected<Foo>(
      GetAllocationHandle(),
      AdditionalBytes(kAdditionalBytes * kAdditionalBytes));
  EXPECT_LT(HeapObjectHeader::FromPayload(object).GetSize(),
            HeapObjectHeader::FromPayload(object_with_bytes).GetSize());
  EXPECT_LT(HeapObjectHeader::FromPayload(object_with_bytes).GetSize(),
            HeapObjectHeader::FromPayload(object_with_more_bytes).GetSize());
}

TEST_F(GCHeapTest, Epoch) {
  const size_t epoch_before = internal::Heap::From(GetHeap())->epoch();
  PreciseGC();
  const size_t epoch_after_gc = internal::Heap::From(GetHeap())->epoch();
  EXPECT_EQ(epoch_after_gc, epoch_before + 1);
}

TEST_F(GCHeapTest, NoGarbageCollectionScope) {
  const size_t epoch_before = internal::Heap::From(GetHeap())->epoch();
  {
    subtle::NoGarbageCollectionScope scope(GetHeap()->GetHeapHandle());
    PreciseGC();
  }
  const size_t epoch_after_gc = internal::Heap::From(GetHeap())->epoch();
  EXPECT_EQ(epoch_after_gc, epoch_before);
}

TEST_F(GCHeapTest, IsGarbageCollectionAllowed) {
  EXPECT_TRUE(
      subtle::DisallowGarbageCollectionScope::IsGarbageCollectionAllowed(
          GetHeap()->GetHeapHandle()));
  {
    subtle::DisallowGarbageCollectionScope disallow_gc(*Heap::From(GetHeap()));
    EXPECT_FALSE(
        subtle::DisallowGarbageCollectionScope::IsGarbageCollectionAllowed(
            GetHeap()->GetHeapHandle()));
  }
}

TEST_F(GCHeapTest, IsMarking) {
  GarbageCollector::Config config = GarbageCollector::Config::
      PreciseIncrementalMarkingConcurrentSweepingConfig();
  auto* heap = Heap::From(GetHeap());
  EXPECT_FALSE(subtle::HeapState::IsMarking(*heap));
  heap->StartIncrementalGarbageCollection(config);
  EXPECT_TRUE(subtle::HeapState::IsMarking(*heap));
  heap->FinalizeIncrementalGarbageCollectionIfRunning(config);
  EXPECT_FALSE(subtle::HeapState::IsMarking(*heap));
  heap->AsBase().sweeper().FinishIfRunning();
  EXPECT_FALSE(subtle::HeapState::IsMarking(*heap));
}

TEST_F(GCHeapTest, IsSweeping) {
  GarbageCollector::Config config = GarbageCollector::Config::
      PreciseIncrementalMarkingConcurrentSweepingConfig();
  auto* heap = Heap::From(GetHeap());
  EXPECT_FALSE(subtle::HeapState::IsSweeping(*heap));
  heap->StartIncrementalGarbageCollection(config);
  EXPECT_FALSE(subtle::HeapState::IsSweeping(*heap));
  heap->FinalizeIncrementalGarbageCollectionIfRunning(config);
  EXPECT_TRUE(subtle::HeapState::IsSweeping(*heap));
  heap->AsBase().sweeper().FinishIfRunning();
  EXPECT_FALSE(subtle::HeapState::IsSweeping(*heap));
}

namespace {

class ExpectAtomicPause final : public GarbageCollected<ExpectAtomicPause> {
  CPPGC_USING_PRE_FINALIZER(ExpectAtomicPause, PreFinalizer);

 public:
  explicit ExpectAtomicPause(HeapHandle& handle) : handle_(handle) {}
  ~ExpectAtomicPause() {
    EXPECT_TRUE(subtle::HeapState::IsInAtomicPause(handle_));
  }
  void PreFinalizer() {
    EXPECT_TRUE(subtle::HeapState::IsInAtomicPause(handle_));
  }
  void Trace(Visitor*) const {}

 private:
  HeapHandle& handle_;
};

}  // namespace

TEST_F(GCHeapTest, IsInAtomicPause) {
  GarbageCollector::Config config =
      GarbageCollector::Config::PreciseIncrementalConfig();
  auto* heap = Heap::From(GetHeap());
  MakeGarbageCollected<ExpectAtomicPause>(heap->object_allocator(), *heap);
  EXPECT_FALSE(subtle::HeapState::IsInAtomicPause(*heap));
  heap->StartIncrementalGarbageCollection(config);
  EXPECT_FALSE(subtle::HeapState::IsInAtomicPause(*heap));
  heap->FinalizeIncrementalGarbageCollectionIfRunning(config);
  EXPECT_FALSE(subtle::HeapState::IsInAtomicPause(*heap));
  heap->AsBase().sweeper().FinishIfRunning();
  EXPECT_FALSE(subtle::HeapState::IsInAtomicPause(*heap));
}

TEST_F(GCHeapTest, TerminateEmptyHeap) { Heap::From(GetHeap())->Terminate(); }

TEST_F(GCHeapTest, TerminateClearsPersistent) {
  Persistent<Foo> foo = MakeGarbageCollected<Foo>(GetAllocationHandle());
  EXPECT_TRUE(foo.Get());
  Heap::From(GetHeap())->Terminate();
  EXPECT_FALSE(foo.Get());
}

TEST_F(GCHeapTest, TerminateInvokesDestructor) {
  Persistent<Foo> foo = MakeGarbageCollected<Foo>(GetAllocationHandle());
  EXPECT_EQ(0u, Foo::destructor_callcount);
  Heap::From(GetHeap())->Terminate();
  EXPECT_EQ(1u, Foo::destructor_callcount);
}

namespace {

class Cloner final : public GarbageCollected<Cloner> {
 public:
  static size_t destructor_count;

  Cloner(cppgc::AllocationHandle& handle, size_t count)
      : handle_(handle), count_(count) {}

  ~Cloner() {
    EXPECT_FALSE(new_instance_);
    destructor_count++;
    if (count_) {
      new_instance_ =
          MakeGarbageCollected<Cloner>(handle_, handle_, count_ - 1);
    }
  }

  void Trace(Visitor*) const {}

 private:
  static Persistent<Cloner> new_instance_;

  cppgc::AllocationHandle& handle_;
  size_t count_;
};

Persistent<Cloner> Cloner::new_instance_;
size_t Cloner::destructor_count;

}  // namespace

TEST_F(GCHeapTest, TerminateReclaimsNewState) {
  Persistent<Cloner> cloner = MakeGarbageCollected<Cloner>(
      GetAllocationHandle(), GetAllocationHandle(), 1);
  Cloner::destructor_count = 0;
  EXPECT_TRUE(cloner.Get());
  Heap::From(GetHeap())->Terminate();
  EXPECT_FALSE(cloner.Get());
  EXPECT_EQ(2u, Cloner::destructor_count);
}

TEST_F(GCHeapDeathTest, TerminateProhibitsAllocation) {
  Heap::From(GetHeap())->Terminate();
  EXPECT_DEATH_IF_SUPPORTED(MakeGarbageCollected<Foo>(GetAllocationHandle()),
                            "");
}

TEST_F(GCHeapDeathTest, LargeChainOfNewStates) {
  Persistent<Cloner> cloner = MakeGarbageCollected<Cloner>(
      GetAllocationHandle(), GetAllocationHandle(), 1000);
  Cloner::destructor_count = 0;
  EXPECT_TRUE(cloner.Get());
  // Terminate() requires destructors to stop creating new state within a few
  // garbage collections.
  EXPECT_DEATH_IF_SUPPORTED(Heap::From(GetHeap())->Terminate(), "");
}

}  // namespace internal
}  // namespace cppgc
