// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap.h"

#include <algorithm>
#include <iterator>
#include <numeric>

#include "include/cppgc/allocation.h"
#include "include/cppgc/cross-thread-persistent.h"
#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/heap-state.h"
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
        GCConfig::ConservativeAtomicConfig());
  }
  void PreciseGC() {
    internal::Heap::From(GetHeap())->CollectGarbage(
        GCConfig::PreciseAtomicConfig());
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
class GCed : public GarbageCollected<GCed<Size>> {
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
                                          const void* object) {
  internal::Heap::From(heap)->CollectGarbage(
      GCConfig::ConservativeAtomicConfig());
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

namespace {

class GCedWithFinalizer final : public GarbageCollected<GCedWithFinalizer> {
 public:
  static size_t destructor_counter;

  GCedWithFinalizer() { destructor_counter = 0; }
  ~GCedWithFinalizer() { destructor_counter++; }
  void Trace(Visitor* visitor) const {}
};
// static
size_t GCedWithFinalizer::destructor_counter = 0;

class LargeObjectGCDuringCtor final
    : public GarbageCollected<LargeObjectGCDuringCtor> {
 public:
  static constexpr size_t kDataSize = kLargeObjectSizeThreshold + 1;

  explicit LargeObjectGCDuringCtor(cppgc::Heap* heap)
      : child_(MakeGarbageCollected<GCedWithFinalizer>(
            heap->GetAllocationHandle())) {
    internal::Heap::From(heap)->CollectGarbage(
        GCConfig::ConservativeAtomicConfig());
  }

  void Trace(Visitor* visitor) const { visitor->Trace(child_); }

  char data[kDataSize];
  Member<GCedWithFinalizer> child_;
};

}  // namespace

TEST_F(GCHeapTest, ConservativeGCFromLargeObjectCtorFindsObject) {
  GCedWithFinalizer::destructor_counter = 0;
  MakeGarbageCollected<LargeObjectGCDuringCtor>(GetAllocationHandle(),
                                                GetHeap());
  EXPECT_EQ(0u, GCedWithFinalizer::destructor_counter);
}

TEST_F(GCHeapTest, ObjectPayloadSize) {
  static constexpr size_t kNumberOfObjectsPerArena = 16;
  static constexpr size_t kObjectSizes[] = {1, 32, 64, 128,
                                            2 * kLargeObjectSizeThreshold};

  EXPECT_EQ(0u, Heap::From(GetHeap())->ObjectPayloadSize());

  {
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

  PreciseGC();
  EXPECT_EQ(0u, Heap::From(GetHeap())->ObjectPayloadSize());
}

TEST_F(GCHeapTest, AllocateWithAdditionalBytes) {
  static constexpr size_t kBaseSize = sizeof(HeapObjectHeader) + sizeof(Foo);
  static constexpr size_t kAdditionalBytes = 10u * kAllocationGranularity;
  {
    Foo* object = MakeGarbageCollected<Foo>(GetAllocationHandle());
    EXPECT_LE(kBaseSize, HeapObjectHeader::FromObject(object).AllocatedSize());
  }
  {
    Foo* object = MakeGarbageCollected<Foo>(GetAllocationHandle(),
                                            AdditionalBytes(kAdditionalBytes));
    EXPECT_LE(kBaseSize + kAdditionalBytes,
              HeapObjectHeader::FromObject(object).AllocatedSize());
  }
  {
    Foo* object = MakeGarbageCollected<Foo>(
        GetAllocationHandle(),
        AdditionalBytes(kAdditionalBytes * kAdditionalBytes));
    EXPECT_LE(kBaseSize + kAdditionalBytes * kAdditionalBytes,
              HeapObjectHeader::FromObject(object).AllocatedSize());
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
  EXPECT_LT(HeapObjectHeader::FromObject(object).AllocatedSize(),
            HeapObjectHeader::FromObject(object_with_bytes).AllocatedSize());
  EXPECT_LT(
      HeapObjectHeader::FromObject(object_with_bytes).AllocatedSize(),
      HeapObjectHeader::FromObject(object_with_more_bytes).AllocatedSize());
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
  GCConfig config =
      GCConfig::PreciseIncrementalMarkingConcurrentSweepingConfig();
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
  GCConfig config =
      GCConfig::PreciseIncrementalMarkingConcurrentSweepingConfig();
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

class GCedExpectSweepingOnOwningThread final
    : public GarbageCollected<GCedExpectSweepingOnOwningThread> {
 public:
  explicit GCedExpectSweepingOnOwningThread(const HeapHandle& heap_handle)
      : heap_handle_(heap_handle) {}
  ~GCedExpectSweepingOnOwningThread() {
    EXPECT_TRUE(subtle::HeapState::IsSweepingOnOwningThread(heap_handle_));
  }

  void Trace(Visitor*) const {}

 private:
  const HeapHandle& heap_handle_;
};

}  // namespace

TEST_F(GCHeapTest, IsSweepingOnOwningThread) {
  GCConfig config =
      GCConfig::PreciseIncrementalMarkingConcurrentSweepingConfig();
  auto* heap = Heap::From(GetHeap());
  MakeGarbageCollected<GCedExpectSweepingOnOwningThread>(
      heap->GetAllocationHandle(), *heap);
  EXPECT_FALSE(subtle::HeapState::IsSweepingOnOwningThread(*heap));
  heap->StartIncrementalGarbageCollection(config);
  EXPECT_FALSE(subtle::HeapState::IsSweepingOnOwningThread(*heap));
  heap->FinalizeIncrementalGarbageCollectionIfRunning(config);
  EXPECT_FALSE(subtle::HeapState::IsSweepingOnOwningThread(*heap));
  heap->AsBase().sweeper().FinishIfRunning();
  EXPECT_FALSE(subtle::HeapState::IsSweepingOnOwningThread(*heap));
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
  GCConfig config = GCConfig::PreciseIncrementalConfig();
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

template <template <typename> class PersistentType>
class Cloner final : public GarbageCollected<Cloner<PersistentType>> {
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
  static PersistentType<Cloner> new_instance_;

  cppgc::AllocationHandle& handle_;
  size_t count_;
};

// static
template <template <typename> class PersistentType>
PersistentType<Cloner<PersistentType>> Cloner<PersistentType>::new_instance_;
// static
template <template <typename> class PersistentType>
size_t Cloner<PersistentType>::destructor_count;

}  // namespace

template <template <typename> class PersistentType>
void TerminateReclaimsNewState(std::shared_ptr<Platform> platform) {
  auto heap = cppgc::Heap::Create(platform);
  using ClonerImpl = Cloner<PersistentType>;
  Persistent<ClonerImpl> cloner = MakeGarbageCollected<ClonerImpl>(
      heap->GetAllocationHandle(), heap->GetAllocationHandle(), 1);
  ClonerImpl::destructor_count = 0;
  EXPECT_TRUE(cloner.Get());
  Heap::From(heap.get())->Terminate();
  EXPECT_FALSE(cloner.Get());
  EXPECT_EQ(2u, ClonerImpl::destructor_count);
}

TEST_F(GCHeapTest, TerminateReclaimsNewState) {
  TerminateReclaimsNewState<Persistent>(GetPlatformHandle());
  TerminateReclaimsNewState<WeakPersistent>(GetPlatformHandle());
  TerminateReclaimsNewState<cppgc::subtle::CrossThreadPersistent>(
      GetPlatformHandle());
  TerminateReclaimsNewState<cppgc::subtle::WeakCrossThreadPersistent>(
      GetPlatformHandle());
}

TEST_F(GCHeapDeathTest, TerminateProhibitsAllocation) {
  Heap::From(GetHeap())->Terminate();
  EXPECT_DEATH_IF_SUPPORTED(MakeGarbageCollected<Foo>(GetAllocationHandle()),
                            "");
}

template <template <typename> class PersistentType>
void LargeChainOfNewStates(cppgc::Heap& heap) {
  using ClonerImpl = Cloner<PersistentType>;
  Persistent<ClonerImpl> cloner = MakeGarbageCollected<ClonerImpl>(
      heap.GetAllocationHandle(), heap.GetAllocationHandle(), 1000);
  ClonerImpl::destructor_count = 0;
  EXPECT_TRUE(cloner.Get());
  // Terminate() requires destructors to stop creating new state within a few
  // garbage collections.
  EXPECT_DEATH_IF_SUPPORTED(Heap::From(&heap)->Terminate(), "");
}

TEST_F(GCHeapDeathTest, LargeChainOfNewStatesPersistent) {
  LargeChainOfNewStates<Persistent>(*GetHeap());
}

TEST_F(GCHeapDeathTest, LargeChainOfNewStatesCrossThreadPersistent) {
  LargeChainOfNewStates<subtle::CrossThreadPersistent>(*GetHeap());
}

}  // namespace internal
}  // namespace cppgc
