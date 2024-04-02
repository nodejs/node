// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(CPPGC_YOUNG_GENERATION)

#include <initializer_list>
#include <vector>

#include "include/cppgc/allocation.h"
#include "include/cppgc/explicit-management.h"
#include "include/cppgc/heap-consistency.h"
#include "include/cppgc/internal/caged-heap-local-data.h"
#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

bool IsHeapObjectYoung(void* obj) {
  return HeapObjectHeader::FromObject(obj).IsYoung();
}

bool IsHeapObjectOld(void* obj) { return !IsHeapObjectYoung(obj); }

class SimpleGCedBase : public GarbageCollected<SimpleGCedBase> {
 public:
  static size_t destructed_objects;

  virtual ~SimpleGCedBase() { ++destructed_objects; }

  virtual void Trace(Visitor* v) const { v->Trace(next); }

  Member<SimpleGCedBase> next;
};

size_t SimpleGCedBase::destructed_objects;

template <size_t Size>
class SimpleGCed : public SimpleGCedBase {
  char array[Size];
};

using Small = SimpleGCed<64>;
using Large = SimpleGCed<kLargeObjectSizeThreshold * 2>;

template <typename Type>
struct OtherType;
template <>
struct OtherType<Small> {
  using Type = Large;
};
template <>
struct OtherType<Large> {
  using Type = Small;
};

}  // namespace

class MinorGCTest : public testing::TestWithHeap {
 public:
  MinorGCTest() {
    CollectMajor();
    SimpleGCedBase::destructed_objects = 0;
  }

  static size_t DestructedObjects() {
    return SimpleGCedBase::destructed_objects;
  }

  void CollectMinor() {
    Heap::From(GetHeap())->CollectGarbage(
        Heap::Config::MinorPreciseAtomicConfig());
  }

  void CollectMajor() {
    Heap::From(GetHeap())->CollectGarbage(Heap::Config::PreciseAtomicConfig());
  }

  const auto& RememberedSlots() const {
    return Heap::From(GetHeap())->remembered_set().remembered_slots_;
  }

  const auto& RememberedSourceObjects() const {
    return Heap::From(GetHeap())->remembered_set().remembered_source_objects_;
  }
};

template <typename SmallOrLarge>
class MinorGCTestForType : public MinorGCTest {
 public:
  using Type = SmallOrLarge;
};

using ObjectTypes = ::testing::Types<Small, Large>;
TYPED_TEST_SUITE(MinorGCTestForType, ObjectTypes);

namespace {
template <typename... Args>
void RunMinorGCAndExpectObjectsPromoted(MinorGCTest& test, Args*... args) {
  ([args] { EXPECT_TRUE(IsHeapObjectYoung(args)); }(), ...);
  test.CollectMinor();
  ([args] { EXPECT_TRUE(IsHeapObjectOld(args)); }(), ...);
}

struct ExpectRememberedSlotsAdded final {
  ExpectRememberedSlotsAdded(
      const MinorGCTest& test,
      std::initializer_list<void*> slots_expected_to_be_remembered)
      : remembered_slots_(test.RememberedSlots()),
        slots_expected_to_be_remembered_(slots_expected_to_be_remembered),
        initial_number_of_slots_(remembered_slots_.size()) {
    // Check that the remembered set doesn't contain specified slots.
    EXPECT_FALSE(std::includes(remembered_slots_.begin(),
                               remembered_slots_.end(),
                               slots_expected_to_be_remembered_.begin(),
                               slots_expected_to_be_remembered_.end()));
  }

  ~ExpectRememberedSlotsAdded() {
    const size_t current_number_of_slots = remembered_slots_.size();
    EXPECT_EQ(
        initial_number_of_slots_ + slots_expected_to_be_remembered_.size(),
        current_number_of_slots);
    EXPECT_TRUE(std::includes(remembered_slots_.begin(),
                              remembered_slots_.end(),
                              slots_expected_to_be_remembered_.begin(),
                              slots_expected_to_be_remembered_.end()));
  }

 private:
  const std::set<void*>& remembered_slots_;
  std::set<void*> slots_expected_to_be_remembered_;
  const size_t initial_number_of_slots_ = 0;
};

struct ExpectRememberedSlotsRemoved final {
  ExpectRememberedSlotsRemoved(
      const MinorGCTest& test,
      std::initializer_list<void*> slots_expected_to_be_removed)
      : remembered_slots_(test.RememberedSlots()),
        slots_expected_to_be_removed_(slots_expected_to_be_removed),
        initial_number_of_slots_(remembered_slots_.size()) {
    DCHECK_GE(initial_number_of_slots_, slots_expected_to_be_removed_.size());
    // Check that the remembered set does contain specified slots to be removed.
    EXPECT_TRUE(std::includes(remembered_slots_.begin(),
                              remembered_slots_.end(),
                              slots_expected_to_be_removed_.begin(),
                              slots_expected_to_be_removed_.end()));
  }

  ~ExpectRememberedSlotsRemoved() {
    const size_t current_number_of_slots = remembered_slots_.size();
    EXPECT_EQ(initial_number_of_slots_ - slots_expected_to_be_removed_.size(),
              current_number_of_slots);
    EXPECT_FALSE(std::includes(remembered_slots_.begin(),
                               remembered_slots_.end(),
                               slots_expected_to_be_removed_.begin(),
                               slots_expected_to_be_removed_.end()));
  }

 private:
  const std::set<void*>& remembered_slots_;
  std::set<void*> slots_expected_to_be_removed_;
  const size_t initial_number_of_slots_ = 0;
};

struct ExpectNoRememberedSlotsAdded final {
  explicit ExpectNoRememberedSlotsAdded(const MinorGCTest& test)
      : remembered_slots_(test.RememberedSlots()),
        initial_remembered_slots_(remembered_slots_) {}

  ~ExpectNoRememberedSlotsAdded() {
    EXPECT_EQ(initial_remembered_slots_, remembered_slots_);
  }

 private:
  const std::set<void*>& remembered_slots_;
  std::set<void*> initial_remembered_slots_;
};

}  // namespace

TYPED_TEST(MinorGCTestForType, MinorCollection) {
  using Type = typename TestFixture::Type;

  MakeGarbageCollected<Type>(this->GetAllocationHandle());
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  MinorGCTest::CollectMinor();
  EXPECT_EQ(1u, TestFixture::DestructedObjects());

  {
    subtle::NoGarbageCollectionScope no_gc_scope(*Heap::From(this->GetHeap()));

    Type* prev = nullptr;
    for (size_t i = 0; i < 64; ++i) {
      auto* ptr = MakeGarbageCollected<Type>(this->GetAllocationHandle());
      ptr->next = prev;
      prev = ptr;
    }
  }

  MinorGCTest::CollectMinor();
  EXPECT_EQ(65u, TestFixture::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, StickyBits) {
  using Type = typename TestFixture::Type;

  Persistent<Type> p1 = MakeGarbageCollected<Type>(this->GetAllocationHandle());
  TestFixture::CollectMinor();
  EXPECT_FALSE(HeapObjectHeader::FromObject(p1.Get()).IsYoung());
  TestFixture::CollectMajor();
  EXPECT_FALSE(HeapObjectHeader::FromObject(p1.Get()).IsYoung());
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, OldObjectIsNotVisited) {
  using Type = typename TestFixture::Type;

  Persistent<Type> p = MakeGarbageCollected<Type>(this->GetAllocationHandle());
  TestFixture::CollectMinor();
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  EXPECT_FALSE(HeapObjectHeader::FromObject(p.Get()).IsYoung());

  // Check that the old deleted object won't be visited during minor GC.
  Type* raw = p.Release();
  TestFixture::CollectMinor();
  EXPECT_EQ(0u, TestFixture::DestructedObjects());
  EXPECT_FALSE(HeapObjectHeader::FromObject(raw).IsYoung());
  EXPECT_FALSE(HeapObjectHeader::FromObject(raw).IsFree());

  // Check that the old deleted object will be revisited in major GC.
  TestFixture::CollectMajor();
  EXPECT_EQ(1u, TestFixture::DestructedObjects());
}

template <typename Type1, typename Type2>
void InterGenerationalPointerTest(MinorGCTest* test, cppgc::Heap* heap) {
  auto* internal_heap = Heap::From(heap);
  Persistent<Type1> old =
      MakeGarbageCollected<Type1>(heap->GetAllocationHandle());
  test->CollectMinor();
  EXPECT_FALSE(HeapObjectHeader::FromObject(old.Get()).IsYoung());

  Type2* young = nullptr;

  {
    subtle::NoGarbageCollectionScope no_gc_scope(*Heap::From(heap));

    // Allocate young objects.
    for (size_t i = 0; i < 64; ++i) {
      auto* ptr = MakeGarbageCollected<Type2>(heap->GetAllocationHandle());
      ptr->next = young;
      young = ptr;
      EXPECT_TRUE(HeapObjectHeader::FromObject(young).IsYoung());
      const uintptr_t offset =
          internal_heap->caged_heap().OffsetFromAddress(young);
      // Age may be young or unknown.
      EXPECT_NE(
          AgeTable::Age::kOld,
          Heap::From(heap)->caged_heap().local_data().age_table.GetAge(offset));
    }
  }

  const auto& set = test->RememberedSlots();
  auto set_size_before = set.size();

  // Issue generational barrier.
  old->next = young;

  EXPECT_EQ(set_size_before + 1u, set.size());

  // Check that the remembered set is visited.
  test->CollectMinor();

  EXPECT_EQ(0u, MinorGCTest::DestructedObjects());
  EXPECT_TRUE(set.empty());

  for (size_t i = 0; i < 64; ++i) {
    EXPECT_FALSE(HeapObjectHeader::FromObject(young).IsFree());
    EXPECT_FALSE(HeapObjectHeader::FromObject(young).IsYoung());
    young = static_cast<Type2*>(young->next.Get());
  }

  old.Release();
  test->CollectMajor();
  EXPECT_EQ(65u, MinorGCTest::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerForSamePageTypes) {
  using Type = typename TestFixture::Type;
  InterGenerationalPointerTest<Type, Type>(this, this->GetHeap());
}

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerForDifferentPageTypes) {
  using Type = typename TestFixture::Type;
  InterGenerationalPointerTest<Type, typename OtherType<Type>::Type>(
      this, this->GetHeap());
}

TYPED_TEST(MinorGCTestForType, OmitGenerationalBarrierForOnStackObject) {
  using Type = typename TestFixture::Type;

  class StackAllocated : GarbageCollected<StackAllocated> {
    CPPGC_STACK_ALLOCATED();

   public:
    Type* ptr = nullptr;
  } stack_object;

  // Try issuing generational barrier for on-stack object.
  stack_object.ptr = MakeGarbageCollected<Type>(this->GetAllocationHandle());
  subtle::HeapConsistency::WriteBarrierParams params;
  EXPECT_EQ(subtle::HeapConsistency::WriteBarrierType::kNone,
            subtle::HeapConsistency::GetWriteBarrierType(
                reinterpret_cast<void*>(&stack_object.ptr), stack_object.ptr,
                params));
}

TYPED_TEST(MinorGCTestForType, OmitGenerationalBarrierForSentinels) {
  using Type = typename TestFixture::Type;

  Persistent<Type> old =
      MakeGarbageCollected<Type>(this->GetAllocationHandle());

  TestFixture::CollectMinor();
  EXPECT_FALSE(HeapObjectHeader::FromObject(old.Get()).IsYoung());

  {
    ExpectNoRememberedSlotsAdded _(*this);
    // Try issuing generational barrier for nullptr.
    old->next = static_cast<Type*>(nullptr);
  }
  {
    ExpectNoRememberedSlotsAdded _(*this);
    // Try issuing generational barrier for sentinel.
    old->next = static_cast<Type*>(kSentinelPointer);
  }
}

template <typename From, typename To>
void TestRememberedSetInvalidation(MinorGCTest& test) {
  Persistent<From> old = MakeGarbageCollected<From>(test.GetAllocationHandle());

  test.CollectMinor();

  auto* young = MakeGarbageCollected<To>(test.GetAllocationHandle());

  {
    ExpectRememberedSlotsAdded _(test, {old->next.GetSlotForTesting()});
    // Issue the generational barrier.
    old->next = young;
  }

  {
    ExpectRememberedSlotsRemoved _(test, {old->next.GetSlotForTesting()});
    // Release the persistent and free the old object.
    auto* old_raw = old.Release();
    subtle::FreeUnreferencedObject(test.GetHeapHandle(), *old_raw);
  }

  // Visiting remembered slots must not fail.
  test.CollectMinor();
}

TYPED_TEST(MinorGCTestForType, RememberedSetInvalidationOnPromptlyFree) {
  using Type1 = typename TestFixture::Type;
  using Type2 = typename OtherType<Type1>::Type;
  TestRememberedSetInvalidation<Type1, Type1>(*this);
  TestRememberedSetInvalidation<Type1, Type2>(*this);
}

TEST_F(MinorGCTest, RememberedSetInvalidationOnShrink) {
  using Member = Member<Small>;

  static constexpr size_t kTrailingMembers = 64;
  static constexpr size_t kBytesToAllocate = kTrailingMembers * sizeof(Member);

  static constexpr size_t kFirstMemberToInvalidate = kTrailingMembers / 2;
  static constexpr size_t kLastMemberToInvalidate = kTrailingMembers;

  // Create an object with additional kBytesToAllocate bytes.
  Persistent<Small> old = MakeGarbageCollected<Small>(
      this->GetAllocationHandle(), AdditionalBytes(kBytesToAllocate));

  auto get_member = [&old](size_t i) -> Member& {
    return *reinterpret_cast<Member*>(reinterpret_cast<uint8_t*>(old.Get()) +
                                      sizeof(Small) + i * sizeof(Member));
  };

  CollectMinor();

  auto* young = MakeGarbageCollected<Small>(GetAllocationHandle());

  const auto& set = RememberedSlots();
  const size_t set_size_before_barrier = set.size();

  // Issue the generational barriers.
  for (size_t i = kFirstMemberToInvalidate; i < kLastMemberToInvalidate; ++i) {
    // Construct the member.
    new (&get_member(i)) Member;
    // Issue the barrier.
    get_member(i) = young;
  }

  // Check that barriers hit (kLastMemberToInvalidate -
  // kFirstMemberToInvalidate) times.
  EXPECT_EQ(set_size_before_barrier +
                (kLastMemberToInvalidate - kFirstMemberToInvalidate),
            set.size());

  // Shrink the buffer for old object.
  subtle::Resize(*old, AdditionalBytes(kBytesToAllocate / 2));

  // Check that the reference was invalidated.
  EXPECT_EQ(set_size_before_barrier, set.size());

  // Visiting remembered slots must not fail.
  CollectMinor();
}

namespace {

template <typename Value>
struct InlinedObject {
  struct Inner {
    Inner() = default;
    explicit Inner(AllocationHandle& handle)
        : ref(MakeGarbageCollected<Value>(handle)) {}

    void Trace(Visitor* v) const { v->Trace(ref); }

    double d = -1.;
    Member<Value> ref;
  };

  InlinedObject() = default;
  explicit InlinedObject(AllocationHandle& handle)
      : ref(MakeGarbageCollected<Value>(handle)), inner(handle) {}

  void Trace(cppgc::Visitor* v) const {
    v->Trace(ref);
    v->Trace(inner);
  }

  int a_ = -1;
  Member<Value> ref;
  Inner inner;
};

template <typename Value>
class GCedWithInlinedArray
    : public GarbageCollected<GCedWithInlinedArray<Value>> {
 public:
  static constexpr size_t kNumObjects = 16;

  GCedWithInlinedArray(HeapHandle& heap_handle, AllocationHandle& alloc_handle)
      : heap_handle_(heap_handle), alloc_handle_(alloc_handle) {}

  using WriteBarrierParams = subtle::HeapConsistency::WriteBarrierParams;
  using HeapConsistency = subtle::HeapConsistency;

  void SetInPlaceRange(size_t from, size_t to) {
    DCHECK_GT(to, from);
    DCHECK_GT(kNumObjects, from);

    for (; from != to; ++from)
      new (&objects[from]) InlinedObject<Value>(alloc_handle_);

    GenerationalBarrierForSourceObject(&objects[from]);
  }

  void Trace(cppgc::Visitor* v) const {
    for (const auto& object : objects) v->Trace(object);
  }

  InlinedObject<Value> objects[kNumObjects];

 private:
  void GenerationalBarrierForSourceObject(void* object) {
    DCHECK(object);
    WriteBarrierParams params;
    const auto barrier_type = HeapConsistency::GetWriteBarrierType(
        object, params, [this]() -> HeapHandle& { return heap_handle_; });
    EXPECT_EQ(HeapConsistency::WriteBarrierType::kGenerational, barrier_type);
    HeapConsistency::GenerationalBarrierForSourceObject(params, object);
  }

  HeapHandle& heap_handle_;
  AllocationHandle& alloc_handle_;
};

}  // namespace

TYPED_TEST(MinorGCTestForType, GenerationalBarrierDeferredTracing) {
  using Type = typename TestFixture::Type;

  Persistent<GCedWithInlinedArray<Type>> array =
      MakeGarbageCollected<GCedWithInlinedArray<Type>>(
          this->GetAllocationHandle(), this->GetHeapHandle(),
          this->GetAllocationHandle());

  this->CollectMinor();

  EXPECT_TRUE(IsHeapObjectOld(array.Get()));

  const auto& remembered_objects = this->RememberedSourceObjects();
  {
    ExpectNoRememberedSlotsAdded _(*this);
    EXPECT_EQ(0u, remembered_objects.count(
                      &HeapObjectHeader::FromObject(array->objects)));

    array->SetInPlaceRange(2, 4);

    EXPECT_EQ(1u, remembered_objects.count(
                      &HeapObjectHeader::FromObject(array->objects)));
  }

  RunMinorGCAndExpectObjectsPromoted(
      *this, array->objects[2].ref.Get(), array->objects[2].inner.ref.Get(),
      array->objects[3].ref.Get(), array->objects[3].inner.ref.Get());

  EXPECT_EQ(0u, remembered_objects.size());
}

namespace {
class GCedWithCustomWeakCallback final
    : public GarbageCollected<GCedWithCustomWeakCallback> {
 public:
  static size_t custom_callback_called;

  void CustomWeakCallbackMethod(const LivenessBroker& broker) {
    custom_callback_called++;
  }

  void Trace(cppgc::Visitor* visitor) const {
    visitor->RegisterWeakCallbackMethod<
        GCedWithCustomWeakCallback,
        &GCedWithCustomWeakCallback::CustomWeakCallbackMethod>(this);
  }
};
size_t GCedWithCustomWeakCallback::custom_callback_called = 0;
}  // namespace

TEST_F(MinorGCTest, ReexecuteCustomCallback) {
  // Create an object with additional kBytesToAllocate bytes.
  Persistent<GCedWithCustomWeakCallback> old =
      MakeGarbageCollected<GCedWithCustomWeakCallback>(GetAllocationHandle());

  CollectMinor();
  EXPECT_EQ(1u, GCedWithCustomWeakCallback::custom_callback_called);

  CollectMinor();
  EXPECT_EQ(2u, GCedWithCustomWeakCallback::custom_callback_called);

  CollectMinor();
  EXPECT_EQ(3u, GCedWithCustomWeakCallback::custom_callback_called);

  CollectMajor();
  // The callback must be called only once.
  EXPECT_EQ(4u, GCedWithCustomWeakCallback::custom_callback_called);
}
}  // namespace internal
}  // namespace cppgc

#endif
